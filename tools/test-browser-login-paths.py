#!/usr/bin/env python3
"""Compile and run tools/browser-login-paths-test.cpp, then check what no C code here can
check about itself: that --selftest still runs the same cases and still fails on one, that
every caller pins the same count, that nothing defines these functions twice, and that the
Log in button reaches them.

The GUI only builds under MSVC, so the --selftest run cannot be rerun in review and cannot
be mutated in a pull request. This is the half that runs on ubuntu.

hts_scan_token() decides which address the browser opens, so the engine has to be checked
out. --engine says where, and defaults to the sibling directory CI uses.
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EXPECTED_LINE = "browser login helpers ok on 107 checks"


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def engine_token_function(htscore):
    """The engine's hts_scan_token(), copied out rather than reproduced here, so the
    address box ends an address where the crawl ends it."""
    m = re.search(r"^hts_boolean hts_scan_token\(.*?^\}$", htscore, re.M | re.S)
    if m is None:
        sys.exit("no hts_scan_token() in the engine's htscore.c")
    return m.group(0)


def engine_opt_function(htscoremain):
    """rules-split.cpp calls it, so linking the splitter here needs the engine's own
    definition rather than a stub of ours."""
    m = re.search(
        r"^HTSEXT_API hts_boolean hts_rule_would_be_read_as_opt\(const char \*arg\) "
        r"\{.*?^\}$",
        htscoremain,
        re.M | re.S,
    )
    if m is None:
        sys.exit("no hts_rule_would_be_read_as_opt() in the engine's htscoremain.c")
    return m.group(0)


def engine_bundle_name_rule(htsbundle_c, htsbundle_h):
    """The engine's bundle name rule, copied out so the harness refuses what the crawl
    refuses."""
    parts = []
    for source, pat in (
        (htsbundle_h, r"^#define HTS_BUNDLE_NAME_MAX \d+$"),
        (
            htsbundle_c,
            r"^static hts_boolean bundle_name_is_device\(const char \*name\) \{.*?^\}$",
        ),
        (
            htsbundle_c,
            r"^static const char \*bundle_name_refusal\(const char \*name\) \{.*?^\}$",
        ),
        (
            htsbundle_c,
            r"^HTSEXT_API const char \*hts_bundle_name_refusal\(const char \*name\) "
            r"\{.*?^\}$",
        ),
    ):
        m = re.search(pat, source, re.M | re.S)
        if m is None:
            sys.exit("the engine's htsbundle.c or .h no longer matches %r" % pat)
        parts.append(m.group(0))
    return "\n".join(parts)


# The public suffix list stays the engine's to test, since htspsl.c needs the whole
# engine. This stands in for it on the hosts the cases use, with co.uk and github.io.
REGISTRABLE_STUB = r"""
int hts_registrable_domain(const char *host, char *out, size_t size) {
  const size_t len = strlen(host);
  const char *reg = NULL, *p;
  int labels = 2;

  out[0] = '\0';
  if (host[0] == '[' || strcmp(host, "co.uk") == 0
      || strcmp(host, "github.io") == 0 || strspn(host, "0123456789.") == len)
    return 0;
  if ((len > 6 && strcmp(host + len - 6, ".co.uk") == 0)
      || (len > 10 && strcmp(host + len - 10, ".github.io") == 0))
    labels = 3;
  for (p = host + len; p > host && reg == NULL; p--)
    if (p[-1] == '.' && --labels == 0)
      reg = p;
  if (reg == NULL && labels == 1)
    reg = host;
  if (reg == NULL || strlen(reg) >= size)
    return 0;
  strcpy(out, reg);
  return 1;
}
"""


# The engine's own rule is the engine's to test, and the Windows --selftest runs the
# cases against it. This keeps a host's registrable domain, and records what it was given.
SCOPE_STUB = r"""
char bl_scope_seen[4][1024];
size_t bl_scope_seen_n;
int hts_bundle_cookie_in_scope(const char *const *urls, size_t nurls,
                               const char *domain) {
  char host[256], reg[256];
  size_t i;

  bl_scope_seen_n = nurls;
  for (i = 0; i < nurls && i < 4; i++)
    snprintf(bl_scope_seen[i], sizeof(bl_scope_seen[i]), "%s", urls[i]);
  if (domain[0] == '.')
    domain++;
  for (i = 0; i < nurls; i++) {
    const char *p = strstr(urls[i], "://");
    const size_t len = strcspn(p = p != NULL ? p + 3 : urls[i], "/?#:");
    const size_t d = strlen(domain);
    size_t r;

    if (len == 0 || len >= sizeof(host))
      return 0;
    memcpy(host, p, len);
    host[len] = '\0';
    if (!hts_registrable_domain(host, reg, sizeof(reg))) {
      if (strcmp(domain, host) == 0)
        return 1;
      continue;
    }
    r = strlen(reg);
    if (strcmp(domain, reg) == 0
        || (d > r && domain[d - r - 1] == '.' && strcmp(domain + d - r, reg) == 0))
      return 1;
  }
  return 0;
}
"""


def engine_space_set(htssafe):
    """The byte set that function splits on, copied out for the same reason."""
    m = re.search(r"^#\s*define\s+HTS_REALSPACES\s+[^\n]*$", htssafe, re.M)
    if m is None:
        sys.exit("no HTS_REALSPACES in the engine's htssafe.h")
    return m.group(0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--engine",
        default=os.path.join(os.path.dirname(ROOT), "httrack"),
        help="the engine checkout holding src/htscore.c and src/htssafe.h",
    )
    ap.add_argument("--cxx", default=os.environ.get("CXX", "g++"))
    args = ap.parse_args()

    htscore = os.path.join(args.engine, "src", "htscore.c")
    htscoremain = os.path.join(args.engine, "src", "htscoremain.c")
    htssafe = os.path.join(args.engine, "src", "htssafe.h")
    htsbundle_c = os.path.join(args.engine, "src", "htsbundle.c")
    htsbundle_h = os.path.join(args.engine, "src", "htsbundle.h")
    for path in (htscore, htscoremain, htssafe, htsbundle_c, htsbundle_h):
        if not os.path.exists(path):
            sys.exit(
                "%s: no engine source, so the token scanner cannot be the engine's. "
                "Check the engine out, or pass --engine." % path
            )

    checked = 0

    # 1. --selftest has to still run these cases, and still stop the run when one of
    #    them fails. A call whose verdict is dropped reports coverage nobody has.
    selftest = read(os.path.join(ROOT, "WinHTTrack", "WinHTTrack.cpp"))
    block = re.search(r"browserLoginCheckCases\(&err\);(.*?)\n    \}", selftest, re.S)
    if block is None:
        sys.exit(
            "WinHTTrack.cpp no longer calls browserLoginCheckCases(): --selftest "
            "stopped running the browser-login path cases"
        )
    checked += 1
    if block.group(1).count("ExitProcess(3)") != 2:
        sys.exit(
            "the --selftest block does not stop the run on both a failed case and a "
            "wrong count, so a red case would go unnoticed"
        )
    checked += 1

    # 2. Every count has to agree, or one of them stops being a pin.
    want = re.search(r"browser login helpers ran %d checks, expected (\d+)", selftest)
    # The comparison as well as the message, because only the comparison stops the run
    # and the two drifted apart once.
    guard = re.search(
        r"if \(nchecks != (\d+)\) \{\n\s*fprintf\(stderr,\n?\s*"
        r'"FATAL: browser login helpers ran',
        selftest,
    )
    if guard is None or (want is not None and guard.group(1) != want.group(1)):
        sys.exit(
            "the --selftest count it compares against and the count it reports differ, "
            "so the message can read right while the guard lets a lost case through"
        )
    checked += 1
    mine = re.search(
        r"define BL_EXPECTED_CHECKS (\d+)",
        read(os.path.join(ROOT, "tools", "browser-login-paths-test.cpp")),
    )
    if want is None or mine is None:
        sys.exit("cannot read the expected check count from both callers")
    line = re.search(r"ok on (\d+) checks", EXPECTED_LINE)
    if want.group(1) != mine.group(1) or mine.group(1) != line.group(1):
        sys.exit(
            "the expected check count differs between --selftest (%s), this harness "
            "(%s) and the line CI matches (%s)"
            % (want.group(1), mine.group(1), EXPECTED_LINE)
        )
    checked += 1

    text = read(os.path.join(ROOT, ".github", "workflows", "windows-build.yml"))
    counts = set(re.findall(r"browser login helpers ok on (\d+) checks", text))
    if not counts:
        sys.exit("windows-build.yml no longer matches the browser-login helper line")
    if counts != {mine.group(1)}:
        sys.exit(
            "windows-build.yml matches %s checks, where the code runs %s"
            % (", ".join(sorted(counts)), mine.group(1))
        )
    checked += 1

    # 3. One definition only: a copy left behind would mean the Windows build and this
    #    run test different code. Every unit, because a fixed file list cannot see a
    #    copy that lands in the next one.
    unit = os.path.join(ROOT, "WinHTTrack", "browser-login-paths.cpp")
    others = [
        other
        for other in sorted(glob.glob(os.path.join(ROOT, "WinHTTrack", "*.cpp")))
        if os.path.basename(other) != "browser-login-paths.cpp"
    ]
    if len(others) < 20:
        sys.exit("found %d other units: this check would prove nothing" % len(others))
    for name in (
        "browserLoginStartUrl",
        "browserLoginProfileDir",
        "browserLoginProfileRoot",
        "browserLoginStaleProfile",
        "browserLoginSafeUrl",
        "browserLoginMovedHost",
        "browserLoginBundleName",
        "browserLoginBundleFile",
        "browserLoginBundleUrl",
        "browserLoginUnbundledUrl",
        "browserLoginBundleText",
        "browserLoginUtf8",
        "browserLoginBundleUrls",
        "browserLoginBundleCookies",
    ):
        for other in others:
            if re.search(r"^\w[^;()\n]*\b%s\s*\(" % name, read(other), re.M):
                sys.exit(
                    "%s is defined in %s as well as browser-login-paths.cpp"
                    % (name, os.path.basename(other))
                )
        if (
            re.search(
                r"^(CString|BOOL|CSimpleArray<\w+>) %s\(" % name, read(unit), re.M
            )
            is None
        ):
            sys.exit("browser-login-paths.cpp no longer defines %s()" % name)
        checked += 1

    # A profile holds a signed-in session, so the only two things that ever remove one
    # have to stay wired: the sweep at startup and the delete at exit.
    main = read(os.path.join(ROOT, "WinHTTrack", "WinHTTrack.cpp"))
    init = re.search(
        r"^BOOL CWinHTTrackApp::InitInstance\(\)(.*?)^\}", main, re.M | re.S
    )
    exit_ = re.search(
        r"^int CWinHTTrackApp::ExitInstance\(\)(.*?)^\}", main, re.M | re.S
    )
    if init is None or "browserLoginSweepProfiles()" not in init.group(1):
        sys.exit(
            "InitInstance() no longer sweeps abandoned browser profiles, so a crash "
            "would leave a signed-in session on disk for ever"
        )
    checked += 1
    if exit_ is None or "browserLoginForgetProfile(" not in exit_.group(1):
        sys.exit("ExitInstance() no longer removes this run's browser profile")
    checked += 1

    # 4. The button has to reach them, so the calls have to sit inside the handler the
    #    message map names, and the harvest has to be applied when the box is accepted.
    #    Naming a function anywhere in a file proves neither.
    # The engine scopes a bundle from the addresses that name it, so the cut runs before
    # the file is written, and again once the wizard finishes, edits included.
    wid1 = read(os.path.join(ROOT, "WinHTTrack", "Wid1.cpp"))
    if not re.search(
        r"^static BOOL applyBrowserLogin\(const BrowserLoginHarvest &login,\s*"
        r"const CSimpleArray<CString> &urls, BOOL again\)\n\{\s*"
        r"BrowserLoginHarvest harvest = login;\s*"
        r"harvest\.cookies = browserLoginBundleCookies\(login\.cookies, urls\);"
        r"[^}]*browserLoginSaveBundle\(harvest,",
        wid1,
        re.M,
    ):
        sys.exit(
            "Wid1.cpp no longer cuts a login's cookies to the URL list before saving"
        )
    checked += 1
    finish = re.search(
        r"^void finishBrowserLogins\(const CString &urls, const CString &listFile\)\n"
        r"\{(.*?)^\}",
        wid1,
        re.M | re.S,
    )
    if finish is None or not re.search(
        r"const CString list = readUrlList\(listFile\);[\s\S]*"
        r"browserLoginBundleUrls\(urls, list, heldLogins\[i\]\.bundleName\);\s*"
        r"/\*[^*]*\*/\s*if \(named\.GetSize\(\) == 0\) \{[^}]*DeleteFileA\(file\);"
        r"\s*\} else \{\s*applyBrowserLogin\(heldLogins\[i\], named, TRUE\);"
        r"[\s\S]*heldLogins\.RemoveAll\(\);",
        finish.group(1),
    ):
        sys.exit(
            "finishBrowserLogins() no longer cuts each held login to the final "
            "addresses, or no longer deletes one that no address names"
        )
    checked += 1
    trans = read(os.path.join(ROOT, "WinHTTrack", "trans.cpp"))
    done = re.search(
        r"^BOOL Ctrans::OnWizardFinish\( \)\n\{(.*?)^\}", trans, re.M | re.S
    )
    if done is None or not re.search(
        r"\n  compute_options\(\) ;\s*/\*[^*]*\*/\s*"
        r"finishBrowserLogins\(ShellOptions->url, ShellOptions->filelist\);"
        r"[\s\S]*Save_current_profile\(0\);[\s\S]*LaunchMirror\(\);",
        done.group(1),
    ):
        sys.exit(
            "the wizard no longer cuts its logins to the final addresses after "
            "compute_options() and before it saves or launches the project"
        )
    checked += 1
    insert = read(os.path.join(ROOT, "WinHTTrack", "InsertUrl.cpp"))
    if "ON_BN_CLICKED(IDC_browserlogin, OnBrowserLogin)" not in insert:
        sys.exit("the Log in button no longer calls OnBrowserLogin()")
    checked += 1
    handler = re.search(
        r"^void CInsertUrl::OnBrowserLogin\(\)\n\{(.*?)^\}", insert, re.M | re.S
    )
    if handler is None:
        sys.exit("InsertUrl.cpp no longer defines OnBrowserLogin()")
    checked += 1
    for name in (
        "browserLoginStartUrl",
        "browserLoginProfileDir",
        "browserLoginRun",
        "browserLoginMovedHost",
        "browserLoginBundleUrl",
    ):
        if name + "(" not in handler.group(1):
            sys.exit(
                "OnBrowserLogin() no longer calls %s(), so the button bypasses it"
                % name
            )
        checked += 1
    # The address the user landed on has to reach the field, or the wizard has no URL
    # to go on and the login leads nowhere. That shipped once and Xavier hit it.
    if not re.search(
        r"CString landed = browserLoginSafeUrl\(got\.finalUrl\)", handler.group(1)
    ) or not re.search(r"SetDlgItemTextCP\([^)]*IDC_urladr, named\)", handler.group(1)):
        sys.exit(
            "OnBrowserLogin() no longer puts a sanitised landed address in "
            "IDC_urladr, so a login either leaves the address box empty or writes a "
            "token from the URL into winprofile.ini"
        )
    checked += 1

    # Reading the verdict is not telling the user: the warning has to be what the test
    # guards, or a login that wandered off would fill the box in silence.
    if not re.search(
        r"if \(browserLoginMovedHost\([\s\S]{0,400}?AfxMessageBox\(",
        handler.group(1),
    ):
        sys.exit(
            "OnBrowserLogin() asks browserLoginMovedHost() without putting a box up, "
            "so a landed address on another host reaches winprofile.ini unannounced"
        )
    checked += 1

    # Calling browserLoginBundleUrl() is not using it: the box has to get the suffixed
    # address, or the login writes a bundle whose name no URL carries.
    if not re.search(
        r"CString named =[\s\S]{0,120}?browserLoginBundleUrl\(landed, name\)",
        handler.group(1),
    ):
        sys.exit(
            "OnBrowserLogin() no longer derives the address box from "
            "browserLoginBundleUrl(), so the bundle it writes would go unnamed"
        )
    checked += 1

    # The address box and the file must name the SAME bundle. The name is carried in the
    # harvest for that reason, so one site setting it and the other reading it is the
    # coupling, and deriving it twice would let the two drift apart.
    if not re.search(
        r"CString name = browserLoginBundleName\(got\.finalUrl\)", handler.group(1)
    ) or not re.search(r"harvest\.bundleName = name;", handler.group(1)):
        sys.exit(
            "OnBrowserLogin() no longer sets harvest.bundleName, so the address box and "
            "the bundle file could name different bundles"
        )
    checked += 1

    # A login the box cannot name changes nothing, so an earlier one stays what the box
    # names, and the user hears it before any box claims cookies were saved.
    if re.search(
        r"harvest\.cookies\.RemoveAll\(\)|haveHarvest = FALSE", handler.group(1)
    ) or not (
        re.search(
            r"if \(named\.IsEmpty\(\)\) \{[^}]*LANG_BROWSERLOGINNONAME[^}]*return;\s*\}\s*"
            r"harvest = got;\s*harvest\.bundleName = name;\s*haveHarvest = TRUE;"
            r"[\s\S]*LANG_BROWSERLOGINGOT",
            handler.group(1),
        )
    ):
        sys.exit(
            "OnBrowserLogin() no longer keeps the earlier login and warns when the box "
            "cannot name a new one"
        )
    checked += 1

    # The harvest is deliberately not written until the box is accepted, so the apply
    # has to be on the OK path. Without this the login would silently do nothing.
    wid1 = read(os.path.join(ROOT, "WinHTTrack", "Wid1.cpp"))
    apply = re.search(
        r"^static BOOL applyBrowserLogin\(.*?\)\n\{(.*?)^\}", wid1, re.M | re.S
    )
    if apply is None:
        sys.exit("Wid1.cpp no longer defines applyBrowserLogin()")
    checked += 1
    # TRUE only at the end, so every early exit tells Onlogin2() no file was written.
    if apply.group(1).count("return TRUE;") != 1 or not apply.group(
        1
    ).rstrip().endswith("return TRUE;"):
        sys.exit("applyBrowserLogin() can report a bundle written on a failure path")
    checked += 1
    for name in ("browserLoginBundleFile", "browserLoginSaveBundle"):
        if name + "(" not in apply.group(1):
            sys.exit("applyBrowserLogin() no longer calls %s()" % name)
        checked += 1
    # The other half of that coupling: it must READ the carried name, never recompute one.
    if "harvest.bundleName" not in apply.group(1):
        sys.exit(
            "applyBrowserLogin() no longer uses harvest.bundleName, so the file it "
            "writes could be named differently from the address box"
        )
    checked += 1
    accepted = re.search(r"^void Wid1::Onlogin2\(\).*?^\}", wid1, re.M | re.S)
    if accepted is None:
        sys.exit("Wid1.cpp no longer defines Onlogin2()")
    checked += 1
    # Inside the accepted branch and under the flag, not merely somewhere in the
    # function: either slip writes a cancelled login to disk, which is the whole
    # reason the harvest is held back.
    taken = re.search(
        r"if \(url\.DoModal\(\) == IDOK\) \{(.*?)\n  \}", accepted.group(0), re.S
    )
    if taken is None:
        sys.exit("Onlogin2() no longer has an IDOK branch to apply the harvest in")
    checked += 1
    # Written before the URL is added, so a failed save leaves no suffix naming a file,
    # and held only once written, so the finish never writes a login OK refused.
    if not re.search(
        r"GetDlgItemText\(IDC_URL,st\);\s*GetDlgItemText\(IDC_filelist,listFile\);"
        r"\s*listFile\.TrimLeft\(\); listFile\.TrimRight\(\);\s*/\*[^*]*\*/\s*"
        r"if \(url\.haveHarvest\) \{\s*const CSimpleArray<CString> named =\s*"
        r"browserLoginBundleUrls\(st \+ \"\\r\\n\" \+ url\.m_urladr, "
        r"readUrlList\(listFile\),\s*url\.harvest\.bundleName\);\s*"
        r"if \(applyBrowserLogin\(url\.harvest, named, FALSE\)\)\s*"
        r"holdBrowserLogin\(url\.harvest\);\s*else\s*"
        r"url\.m_urladr = browserLoginUnbundledUrl\(url\.m_urladr, "
        r"url\.harvest\.bundleName\);\s*\}\s*if \(url\.m_urladr\.Left",
        taken.group(1),
    ):
        sys.exit(
            "Onlogin2() no longer saves the login before adding its URL, under "
            "haveHarvest and inside the IDOK branch, and drops the suffix on failure"
        )
    checked += 1

    print("%d source checks pass" % checked)

    with tempfile.TemporaryDirectory() as tmp:
        gen = os.path.join(tmp, "engine-token.cpp")
        with open(gen, "w", encoding="utf-8") as f:
            f.write(
                "/* Copied by tools/test-browser-login-paths.py out of the engine's "
                "src/htscore.c, src/htssafe.h, src/htsbundle.c and src/htsbundle.h. */\n"
                "#include <assert.h>\n#include <ctype.h>\n#include <stddef.h>\n"
                "#include <stdio.h>\n#include <string.h>\n"
                "typedef int hts_boolean;\n#define HTSEXT_API\n"
                "#define HTS_TRUE 1\n#define HTS_FALSE 0\n"
                "#define assertf(x) assert(x)\n"
                "static void strlncatbuff(char *d, const char *s, size_t size, "
                "size_t n) {\n"
                "  const size_t len = strlen(d);\n"
                "  assert(n < size - len);\n  strncat(d, s, n);\n}\n"
                "static void hts_lowcase(char *s) {\n"
                "  for (; *s; s++) *s = (char) tolower((unsigned char) *s);\n}\n"
                "%s\n"
                'extern "C" {\n%s\n%s\n%s\n%s\n%s\n}\n'
                % (
                    engine_space_set(read(htssafe)),
                    engine_token_function(read(htscore)),
                    engine_opt_function(read(htscoremain)),
                    engine_bundle_name_rule(read(htsbundle_c), read(htsbundle_h)),
                    REGISTRABLE_STUB,
                    SCOPE_STUB,
                )
            )
        # Built and run twice: once as the Windows build sees it, once with the
        # sanitizers, because the engine tokenizer this links walks a token buffer by
        # hand and a one-byte overrun there passes every case.
        for label, extra in (
            ("plain", []),
            ("sanitized", ["-fsanitize=address,undefined"]),
        ):
            exe = os.path.join(tmp, "browser-login-paths-test-" + label)
            subprocess.run(
                [args.cxx, "-std=c++14", "-Wall", "-Wextra", "-Werror", "-g", "-o", exe]
                + extra
                + [
                    os.path.join(ROOT, "tools", "browser-login-paths-test.cpp"),
                    os.path.join(ROOT, "WinHTTrack", "browser-login-paths.cpp"),
                    os.path.join(ROOT, "WinHTTrack", "cookie-file.cpp"),
                    os.path.join(ROOT, "WinHTTrack", "rules-split.cpp"),
                    gen,
                    "-DBROWSER_LOGIN_PATHS_TEST",
                    "-DCOOKIE_FILE_TEST",
                    "-DRULES_SPLIT_TEST",
                    "-I" + os.path.join(ROOT, "tools"),
                    "-I" + os.path.join(ROOT, "WinHTTrack"),
                ],
                check=True,
            )
            try:
                run = subprocess.run([exe], capture_output=True, text=True, timeout=300)
            except subprocess.TimeoutExpired:
                sys.exit(
                    "the %s cases did not finish in 300 seconds, so one of them loops"
                    % label
                )
            if run.returncode != 0:
                sys.exit("%s: %s" % (label, (run.stdout + run.stderr).strip()))
            if EXPECTED_LINE not in run.stdout:
                sys.exit(
                    "the %s harness printed %r, expected %r"
                    % (label, run.stdout.strip(), EXPECTED_LINE)
                )
            for line in run.stdout.splitlines():
                print("%s: %s" % (label, line))


if __name__ == "__main__":
    main()
