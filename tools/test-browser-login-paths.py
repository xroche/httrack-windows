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

EXPECTED_LINE = "browser login helpers ok on 80 checks"


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


def engine_space_set(htssafe):
    """The byte set that function splits on, copied out for the same reason."""
    m = re.search(r"^#\s*define\s+HTS_REALSPACES\s+[^\n]*$", htssafe, re.M)
    if m is None:
        sys.exit("no HTS_REALSPACES in the engine's htssafe.h")
    return m.group(0)


def engine_url_max(htsglobal):
    """The engine's address cap, which decides when a long address names no bundle."""
    m = re.search(r"^#define HTS_URLMAXSIZE (\d+)$", htsglobal, re.M)
    if m is None:
        sys.exit("no HTS_URLMAXSIZE in the engine's htsglobal.h")
    return m.group(1)


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
    htsglobal = os.path.join(args.engine, "src", "htsglobal.h")
    htsbundle = os.path.join(args.engine, "src", "htsbundle.c")
    for path in (htscore, htscoremain, htssafe, htsglobal, htsbundle):
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
        "browserLoginUtf8",
        "browserLoginBundlesToWrite",
        "browserLoginWriteHeld",
        "browserLoginPasteText",
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
    #    message map names, and the harvest has to be held when the box is accepted.
    #    Naming a function anywhere in a file proves neither.
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
        "browserLoginMakeBundle",
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
    ) or not re.search(
        r"SetDlgItemTextCP\([^)]*IDC_urladr, made\.url\)", handler.group(1)
    ):
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

    # The engine names the bundle without writing it, and a refusal keeps the earlier
    # login, so the box and the held login name the same bundle or nothing changes.
    if re.search(
        r"harvest\.cookies\.RemoveAll\(\)|haveHarvest = FALSE", handler.group(1)
    ) or not re.search(
        r"const BrowserLoginBundleInfo made = "
        r"browserLoginMakeBundle\(got, BrowserLoginCheckOnly\);\s*"
        r"/\*[^*]*\*/\s*if \(!made\.ok\) \{"
        r"[^}]*LANG_BROWSERLOGINNONAME[^}]*made\.error[^}]*return;\s*\}\s*"
        r"harvest = got;\s*harvest\.bundleName = made\.name;\s*haveHarvest = TRUE;"
        r"[\s\S]*LANG_BROWSERLOGINGOTFINISH",
        handler.group(1),
    ):
        sys.exit(
            "OnBrowserLogin() no longer asks the engine for the bundle without "
            "writing it, or no longer keeps the earlier login on a refusal"
        )
    checked += 1

    # A cookie left out before the engine sees it may be the one carrying the login.
    if not re.search(
        r"if \(made\.dropped > 0\) \{[^}]*LANG_BROWSERLOGINDROPPED[^}]*AfxMessageBox\(",
        handler.group(1),
    ):
        sys.exit("OnBrowserLogin() no longer says how many cookies were left out")
    checked += 1

    # The harvest is held on the OK path and written only when the wizard finishes, into
    # the final project, after compute_options() and before the profile is saved.
    wid1 = read(os.path.join(ROOT, "WinHTTrack", "Wid1.cpp"))
    accepted = re.search(r"^void Wid1::Onlogin2\(\).*?^\}", wid1, re.M | re.S)
    taken = accepted and re.search(
        r"if \(url\.DoModal\(\) == IDOK\) \{(.*?)\n  \}", accepted.group(0), re.S
    )
    if not taken or not re.search(
        r"^\s*if \(url\.haveHarvest\)\s*heldLogins\.Add\(url\.harvest\);",
        taken.group(1),
    ):
        sys.exit("Onlogin2() no longer holds the login under haveHarvest on OK")
    checked += 1
    write = re.search(r"^BOOL Wid1::WriteBrowserLogins\(.*?^\}", wid1, re.M | re.S)
    if write is None or not re.search(
        r"HeldLoginWrite w = \{ &heldLogins, folder \};[\s\S]*"
        r"return browserLoginWriteHeld\(names, urls, listFile, readUrlList, "
        r"writeHeldLogin, &w\);",
        write.group(0),
    ) or not re.search(
        r"^static BOOL writeHeldLogin\(void \*ctx, int index\)\n\{[^}]*"
        r"browserLoginMakeBundle\(\(\*w->held\)\[index\], BrowserLoginWriteFile, "
        r"w->folder\);",
        wid1,
        re.M,
    ):
        sys.exit(
            "WriteBrowserLogins() no longer writes, into the folder it is given, "
            "the held logins browserLoginBundlesToWrite() picks"
        )
    checked += 1
    trans = read(os.path.join(ROOT, "WinHTTrack", "trans.cpp"))
    done = re.search(
        r"^BOOL Ctrans::OnWizardFinish\( \)\n\{(.*?)^\}", trans, re.M | re.S
    )
    if done is None or not re.search(
        r"\n  compute_options\(\) ;\s*/\*[^*]*\*/\s*"
        r"if \(!dialog1->WriteBrowserLogins\(ShellOptions->path, ShellOptions->url,"
        r"\s*ShellOptions->filelist\)\)\s*return 0;"
        r"[\s\S]*Save_current_profile\(0\);[\s\S]*LaunchMirror\(\);",
        done.group(1),
    ):
        sys.exit(
            "the wizard no longer writes its logins into the final project after "
            "compute_options(), stopping on a failure, before it saves or launches"
        )
    checked += 1

    # A login with no cookie is kept, so neither the handler nor the writer may drop it.
    login = read(os.path.join(ROOT, "WinHTTrack", "browser-login.cpp"))
    if "HTS_PASTE_KEEP_EMPTY" not in login or "hts_bundle_from_paste(" in login:
        sys.exit("browserLoginMakeBundle() no longer keeps a login with no cookie")
    if re.search(r"got\.cookies\.GetSize\(\) == 0", handler.group(1)):
        sys.exit("OnBrowserLogin() refuses a login with no cookie again")
    checked += 1

    # The paste stops where the engine would refuse it, so the two caps must agree.
    engine_paste = re.search(
        r"^#define PASTE_MAX (.+)$", read(htsbundle), re.M
    )
    ours = re.search(r"^#define BROWSER_PASTE_MAX (.+)$", read(unit), re.M)
    if engine_paste is None or ours is None or engine_paste.group(1) != ours.group(1):
        sys.exit("BROWSER_PASTE_MAX no longer matches the engine's PASTE_MAX")
    checked += 1

    print("%d source checks pass" % checked)

    with tempfile.TemporaryDirectory() as tmp:
        gen = os.path.join(tmp, "engine-token.cpp")
        with open(gen, "w", encoding="utf-8") as f:
            f.write(
                "/* Copied by tools/test-browser-login-paths.py out of the engine's "
                "src/htscore.c, src/htscoremain.c and src/htssafe.h. */\n"
                "#include <assert.h>\n#include <ctype.h>\n#include <stddef.h>\n"
                "#include <string.h>\n"
                "typedef int hts_boolean;\n#define HTSEXT_API\n"
                "#define HTS_TRUE 1\n#define HTS_FALSE 0\n"
                "#define assertf(x) assert(x)\n"
                "%s\n"
                'extern "C" {\n%s\n%s\n}\n'
                % (
                    engine_space_set(read(htssafe)),
                    engine_token_function(read(htscore)),
                    engine_opt_function(read(htscoremain)),
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
                    "-DHTS_URLMAXSIZE=" + engine_url_max(read(htsglobal)),
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
