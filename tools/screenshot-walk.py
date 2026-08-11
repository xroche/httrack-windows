#!/usr/bin/env python3
"""Capture every documentation screen of WinHTTrack.

Drives the wizard end to end -- project, URL, the option tabs, then a real crawl of a
site this process serves -- and writes one PNG per screen. Controls are located by
resource ID read from WinHTTrack/resource.h, so a relabelled or resized dialog still
walks and a renumbered one fails loudly instead of shooting the wrong window.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import win32gui
from PIL import Image
from pywinauto.controls.common_controls import TabControlWrapper
from pywinauto.controls.win32_controls import ComboBoxWrapper

from wincapture import (Timeout, capture, click, content_rect, controls, find, grab,
                        nonblack, set_text, slug, top_level_windows, wait, window_titled)

# MFC's wizard buttons and the property-sheet page selector are standard.
ID_WIZBACK, ID_WIZNEXT, ID_WIZFINISH = 0x3023, 0x3024, 0x3025
IDOK, IDCANCEL = 1, 2
PSM_SETCURSEL = 0x0465
TCM_GETITEMCOUNT = 0x1304

PROJECT = "Demo Project"
NEEDED = ("IDC_lang", "IDC_STATIC_welcome", "IDC_projname", "IDC_projpath", "IDC_URL",
          "ID_setopt", "IDC_select_start", "IDC_inforun", "IDC_infoend", "IDC_i6",
          "IDC_connexion", "IDC_maxrate")


class Site(BaseHTTPRequestHandler):
    """Pages served in pieces, off a root that links to dozens of them at once. Both
    matter to the progress screen: a body that arrives gradually is what makes the
    per-file bars fill instead of jumping from nothing to done, and a wide root is what
    fills all eight connection slots straight away. A narrow root starves them, because
    the queue then only grows as fast as the pages it is waiting on arrive."""

    pages = 300
    spread = 60
    fanout = 4
    size = 240_000
    chunk = 8192
    pause = 0.06
    filler = "HTTrack copies a site by following its links, page by page. "

    def children(self):
        if self.path == "/":
            return range(1, self.spread + 1)
        m = re.fullmatch(r"/page(\d+)\.html", self.path)
        if not m or not 1 <= int(m.group(1)) <= self.pages:
            return None
        n = int(m.group(1))
        return [k for k in range(n * self.fanout + 1, (n + 1) * self.fanout + 1)
                if k <= self.pages]

    def body(self):
        kids = self.children()
        if kids is None:
            return None
        links = "".join(f'<li><a href="/page{k}.html">Page {k}</a></li>' for k in kids)
        head = (f"<html><head><title>Demo site</title></head><body><h1>Demo site</h1>"
                f'<ul>{links}</ul><p><a href="/">home</a><br>')
        tail = "</p></body></html>"
        pad = max(self.size - len(head) - len(tail), 0)
        return head + (self.filler * (pad // len(self.filler) + 1))[:pad] + tail

    def do_GET(self):
        body = self.body()
        if body is None:
            self.send_error(404)
            return
        raw = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        try:
            for i in range(0, len(raw), self.chunk):
                self.wfile.write(raw[i:i + self.chunk])
                self.wfile.flush()
                time.sleep(self.pause)
        except (BrokenPipeError, ConnectionResetError):
            pass  # the walk kills the app mid-crawl, so half the transfers end this way

    def log_message(self, *_):
        pass


def serve(host, port):
    """Serve on loopback, addressed as `host` so the shots read like a site rather than
    a developer's machine. `host` has to resolve to 127.0.0.1 for the crawl to reach it."""
    server = ThreadingHTTPServer(("127.0.0.1", port), Site)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    bound = server.socket.getsockname()[1]
    return server, "http://%s/" % (host if bound == 80 else f"{host}:{bound}")


def resource_ids(path):
    ids = {}
    for line in open(path, encoding="utf-8"):
        m = re.match(r"#define\s+(\w+)\s+(\d+)", line)
        if m:
            ids.setdefault(m.group(1), int(m.group(2)))
    missing = [n for n in NEEDED if n not in ids]
    if missing:
        raise SystemExit(f"{path} defines none of: {', '.join(missing)}")
    return ids


class Shots:
    # Sixteen frames, close enough together to fit inside the crawl the walk already runs.
    # The panel refreshes every 100ms (HTS_SLEEP_WIN), so every frame is a distinct one.
    FRAMES, INTERVAL = 16, 0.3
    # Motion shown alongside body text needs a way to stop it past five seconds (WCAG
    # 2.2.2), and an APNG in an <img> offers none. One short play is outside that.
    PLAYS, SECONDS = 1, 5.0

    def __init__(self, outdir):
        self.outdir = outdir
        self.taken = []

    def take(self, hwnd, name):
        path = os.path.join(self.outdir, f"{name}.png")
        img = grab(hwnd)
        img.save(path)
        self.check(img, f"{name}.png")
        self.taken.append(path)

    def check(self, img, what):
        share = nonblack(img)
        print(f"  {what}  {share:.3f} non-black")
        # A window that is up but has not painted captures as a black rectangle, which
        # looks like a successful shot until someone opens the artifact.
        if share < 0.02:
            raise RuntimeError(f"{what} came out blank")

    def animate(self, hwnd, inner, name, over):
        """A window on a cadence, cropped to what a descendant of it has on it, as an
        APNG plus frame 1 as a PNG: only the pixels that move cost bytes, and a decoder
        with no APNG support shows frame 1."""
        box = content_rect(inner, hwnd)
        frames = []
        for i in range(self.FRAMES):
            if i:
                time.sleep(self.INTERVAL)
            if over():
                break
            frames.append(grab(hwnd).crop(box))
        if len(frames) < self.FRAMES:
            raise RuntimeError(f"{name}: {len(frames)} of {self.FRAMES} frames before the "
                               "mirror ended -- serve more pages, or serve them slower")
        self.check(frames[0], f"{name}.apng")
        frames = self.one_palette(frames)
        still = os.path.join(self.outdir, f"{name}.png")
        apng = os.path.join(self.outdir, f"{name}.apng")
        frames[0].save(still)
        frames[0].save(apng, format="PNG", save_all=True, append_images=frames[1:],
                       duration=int(self.INTERVAL * 1000), loop=self.PLAYS)
        # Report the file rather than the capture: two identical frames in a row are
        # written as one with their delays added, so the counts differ legitimately.
        stored, seconds = self.plays_once(apng, name)
        print(f"  {name}.apng  {stored} frames of {len(frames)}, {seconds:.1f}s, "
              f"{os.path.getsize(apng) / 1024:.0f} KB")
        self.taken += [still, apng]

    def one_palette(self, frames):
        """Every frame through a single palette, since an APNG carries one PLTE. A UI
        screen is a few hundred colours, so this halves the file and loses nothing;
        converting the frames together is what keeps their palettes identical."""
        union = set()
        for f in frames:
            union |= {colour for _, colour in f.getcolors(maxcolors=1 << 24)}
        if len(union) > 256:
            print(f"  {len(union)} colours: more than a palette holds, keeping them RGB")
            return frames
        w, h = frames[0].size
        tall = Image.new("RGB", (w, h * len(frames)))
        for i, f in enumerate(frames):
            tall.paste(f, (0, i * h))
        flat = tall.convert("P", palette=Image.ADAPTIVE, colors=256)
        if flat.convert("RGB").tobytes() != tall.tobytes():
            raise RuntimeError(f"the palette lost colours: {len(union)} in, "
                               f"{len(flat.getpalette()) // 3} out")
        print(f"  {len(union)} colours, one palette")
        return [flat.crop((0, i * h, w, (i + 1) * h)) for i in range(len(frames))]

    def plays_once(self, path, name):
        """Read back what was actually written, and return its frames and seconds. Four
        header bytes are all that separate a short play from one that never stops, and
        `loop` does not mean the same thing for every format Pillow writes."""
        shown = Image.open(path)
        plays, total = shown.info.get("loop"), 0
        for i in range(shown.n_frames):
            shown.seek(i)
            total += shown.info.get("duration", 0)
        if plays != self.PLAYS or total > self.SECONDS * 1000:
            raise RuntimeError(f"{name}.apng plays {plays}x for {total / 1000:.1f}s, and has "
                               f"to play {self.PLAYS}x within {self.SECONDS:.0f}s")
        return shown.n_frames, total / 1000


def warmed_up(main, ids, files=20, seconds=25):
    """Six seconds in, the engine is still parsing the first page and the counters are
    barely off zero. Wait for them to climb before filming, and film anyway if they do
    not -- the wait is the warm-up either way."""
    written = ids["IDC_i6"]

    def enough():
        count = win32gui.GetWindowText(find(main, control_id=written))
        return count.isdigit() and int(count) >= files

    try:
        wait(enough, f"{files} files written", seconds)
    except Timeout:
        print(f"  under {files} files after {seconds}s: filming the sequence regardless")


def pane(main, anchor, what, timeout=30):
    """Wait for a wizard pane: its own anchor control turns visible only once the page
    is on screen, since the sheet keeps every page's controls alive from the start."""
    return wait(lambda: find(main, control_id=anchor), what, timeout)


def open_options(main, pid, ids):
    """The options sheet, with its tab control and the captions of its pages."""
    before = set(top_level_windows(pid))
    # By class too: ID_setopt is 3, which is also IDABORT, so a bare ID is not distinctive.
    click(find(main, control_id=ids["ID_setopt"], class_name="Button"))
    sheet = wait(lambda: next((h for h in top_level_windows(pid) if h not in before), None),
                 "the options sheet", 30)
    tabs = wait(lambda: find(sheet, class_name="SysTabControl32"), "the option tabs", 15)
    return sheet, tabs, TabControlWrapper(tabs)


def close_options(sheet, pid, button):
    click(find(sheet, control_id=button, class_name="Button"))
    wait(lambda: sheet not in top_level_windows(pid), "the options sheet to close", 15)


def options(main, pid, ids, shots, connections=8, rate=2_000_000):
    """The eleven option tabs, switched through the sheet rather than by clicking the tab
    control, so a two-row tab layout cannot put a tab under the mouse of another.

    The connection count is set on the way out, after every tab has been shot with its
    defaults, because the sheet only opens once: a second "Set options..." throws
    "Encountered an improper argument" inside MFC and leaves a message box on screen."""
    sheet, tabs, captions = open_options(main, pid, ids)
    count = win32gui.SendMessage(tabs, TCM_GETITEMCOUNT, 0, 0)
    print(f"  options sheet {sheet:#x}: {count} tabs")
    at = {}
    for i in range(count):
        win32gui.SendMessage(sheet, PSM_SETCURSEL, i, 0)
        time.sleep(0.6)
        page = slug(captions.get_tab_text(i))
        at[page] = i
        shots.take(sheet, f"{4 + i:02d}_options_{page}")
    # Eight parallel transfers, so the animation shows a mirror working rather than one
    # file trickling in. Eight is the engine's ceiling; it clamps anything higher. The
    # rate cap goes up with it: eight transfers sharing the engine's 100 KB/s default
    # would still crawl. Each on its own page, since a control on a page that is not on
    # top is not visible to find().
    for page, control, value in (("limits", "IDC_maxrate", rate),
                                 ("flow_control", "IDC_connexion", connections)):
        if page not in at:
            raise RuntimeError(f"no {page} tab to set the mirror up on")
        win32gui.SendMessage(sheet, PSM_SETCURSEL, at[page], 0)
        time.sleep(0.6)
        set_text(find(sheet, control_id=ids[control]), str(value))
    close_options(sheet, pid, IDOK)
    print(f"  {connections} connections, {rate} B/s")


def run(pid, ids, shots, url, base_path):
    # Only a first run offers the language, so a VM replay skips this shot rather than
    # failing on it.
    try:
        lang = wait(lambda: window_titled(pid, "About WinHTTrack"), "the language dialog", 25)
        shots.take(lang, "00_language_preference")
        combo = ComboBoxWrapper(find(lang, control_id=ids["IDC_lang"]))
        print(f"  languages: {combo.selected_text()!r} of {len(combo.item_texts())}")
        combo.select("English")
        click(find(lang, control_id=IDOK, class_name="Button"))
    except Timeout:
        print("  no language dialog: not a first run")

    main = wait(lambda: window_titled(pid, r"WinHTTrack Website Copier - \["), "the main window", 60)

    pane(main, ids["IDC_STATIC_welcome"], "the welcome pane")
    shots.take(main, "01_startup")
    click(find(main, control_id=ID_WIZNEXT))

    pane(main, ids["IDC_projname"], "the project pane")
    set_text(find(main, control_id=ids["IDC_projname"]), PROJECT)
    set_text(find(main, control_id=ids["IDC_projpath"]), base_path)
    shots.take(main, "02_project_name")
    click(find(main, control_id=ID_WIZNEXT))

    pane(main, ids["IDC_URL"], "the URL pane")
    set_text(find(main, control_id=ids["IDC_URL"]), url)
    shots.take(main, "03_project_setup")

    options(main, pid, ids, shots)

    click(find(main, control_id=ID_WIZNEXT))
    pane(main, ids["IDC_select_start"], "the ready-to-start pane")
    shots.take(main, "15_ready_to_start")

    started = time.monotonic()
    click(find(main, control_id=ID_WIZFINISH))
    inforun = pane(main, ids["IDC_inforun"], "the progress pane", timeout=60)
    time.sleep(6)  # let the counters fill, or the shot is a screen of zeros
    shots.take(main, "16_mirror_progress")
    warmed_up(main, ids)
    # Framed on the pane's own controls, read from the live layout: the panel is what the
    # website shows, and a crop box written down here would go stale when the layout moves.
    shots.animate(main, win32gui.GetParent(inforun), "16_mirror_progress_panel",
                  lambda: find(main, control_id=ids["IDC_infoend"]))

    pane(main, ids["IDC_infoend"], "the finished pane", timeout=420)
    print(f"  the mirror ran {time.monotonic() - started:.0f}s; the sequence has to fit in it")
    time.sleep(1)
    shots.take(main, "17_mirror_finished")


def diagnose(pid, outdir):
    """On failure, shoot and dump every window the app has open: which one it is stuck
    on, and what is on it, is the whole diagnosis."""
    print("--- diagnostics ---")
    for hwnd in top_level_windows(pid):
        print(f"  {hwnd:#x} {win32gui.GetClassName(hwnd)!r} {win32gui.GetWindowText(hwnd)!r}")
        try:
            capture(hwnd, os.path.join(outdir, f"diag_{hwnd:#x}.png"))
        except Exception as e:
            print(f"    capture failed: {e}")
        for child, cid, cls, text in controls(hwnd):
            if win32gui.IsWindowVisible(child) and (text or cls != "Static"):
                print(f"    id={cid} {cls!r} {text[:50]!r}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", default="shots")
    ap.add_argument("--resource-h", default=os.path.join("WinHTTrack", "resource.h"))
    ap.add_argument("--site-host", default="127.0.0.1",
                    help="name the crawled site answers to; must resolve to 127.0.0.1")
    ap.add_argument("--site-port", type=int, default=8099,
                    help="port to serve it on (0 picks a free one)")
    ap.add_argument("--base-path", default=r"C:\shots-mirror")
    args = ap.parse_args()

    ids = resource_ids(args.resource_h)
    os.makedirs(args.out, exist_ok=True)
    # An existing mirror finishes from cache in no time, which turns the progress shot
    # into a second copy of the finished screen.
    shutil.rmtree(args.base_path, ignore_errors=True)
    _, site_url = serve(args.site_host, args.site_port)

    shots = Shots(args.out)
    exe = os.path.abspath(args.exe)
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    try:
        run(proc.pid, ids, shots, site_url, args.base_path)
    except Exception as e:
        print(f"FAILED: {e}")
        diagnose(proc.pid, args.out)
        return 1
    finally:
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print(f"\n{len(shots.taken)} screens captured in {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
