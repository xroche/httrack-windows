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
from pywinauto.controls.common_controls import TabControlWrapper
from pywinauto.controls.win32_controls import ComboBoxWrapper

from wincapture import (Timeout, capture, click, controls, find, grab, nonblack, set_text,
                        slug, top_level_windows, wait, window_titled)

# MFC's wizard buttons and the property-sheet page selector are standard.
ID_WIZBACK, ID_WIZNEXT, ID_WIZFINISH = 0x3023, 0x3024, 0x3025
IDOK, IDCANCEL = 1, 2
PSM_SETCURSEL = 0x0465
TCM_GETITEMCOUNT = 0x1304

PROJECT = "Demo Project"
NEEDED = ("IDC_lang", "IDC_STATIC_welcome", "IDC_projname", "IDC_projpath", "IDC_URL",
          "ID_setopt", "IDC_select_start", "IDC_inforun", "IDC_infoend")


class Site(BaseHTTPRequestHandler):
    """Interlinked pages served slowly, so the crawl is still running with real numbers
    on it when the progress screen is captured."""

    pages = 24
    delay = 0.35
    filler = "HTTrack copies a site by following its links, page by page. " * 90

    def body(self):
        if self.path == "/":
            links = "".join(f'<li><a href="page{i}.html">Page {i}</a></li>'
                            for i in range(1, self.pages + 1))
            return ("<html><head><title>Demo site</title></head><body>"
                    f"<h1>Demo site</h1><ul>{links}</ul></body></html>")
        m = re.fullmatch(r"/page(\d+)\.html", self.path)
        if not m or not 1 <= int(m.group(1)) <= self.pages:
            return None
        n = int(m.group(1))
        nxt = n % self.pages + 1
        return (f"<html><head><title>Page {n}</title></head><body><h1>Page {n}</h1>"
                f'<p>{self.filler}</p><a href="page{nxt}.html">next</a> <a href="/">home</a>'
                "</body></html>")

    def do_GET(self):
        time.sleep(self.delay)
        body = self.body()
        if body is None:
            self.send_error(404)
            return
        raw = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def log_message(self, *_):
        pass


def serve(port):
    server = ThreadingHTTPServer(("127.0.0.1", port), Site)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


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
    # Ten frames, close enough together to fit inside the crawl the walk already runs.
    # The panel refreshes every 100ms (HTS_SLEEP_WIN), so every frame is a distinct one.
    FRAMES, INTERVAL = 10, 0.6

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

    def animate(self, hwnd, name, over):
        """A window on a cadence, as an APNG plus frame 1 as an ordinary PNG: only the
        pixels that move cost bytes, and a decoder with no APNG support shows frame 1."""
        frames = []
        for i in range(self.FRAMES):
            if i:
                time.sleep(self.INTERVAL)
            if over():
                break
            frames.append(grab(hwnd))
        if len(frames) < self.FRAMES:
            raise RuntimeError(f"{name}: {len(frames)} of {self.FRAMES} frames before the "
                               "mirror ended -- serve more pages, or serve them slower")
        self.check(frames[0], f"{name}.apng")
        still = os.path.join(self.outdir, f"{name}.png")
        apng = os.path.join(self.outdir, f"{name}.apng")
        frames[0].save(still)
        frames[0].save(apng, format="PNG", save_all=True, append_images=frames[1:],
                       duration=int(self.INTERVAL * 1000), loop=0)
        print(f"  {name}.apng  {len(frames)} frames, {os.path.getsize(apng) / 1024:.0f} KB")
        self.taken += [still, apng]


def pane(main, anchor, what, timeout=30):
    """Wait for a wizard pane: its own anchor control turns visible only once the page
    is on screen, since the sheet keeps every page's controls alive from the start."""
    return wait(lambda: find(main, control_id=anchor), what, timeout)


def options(main, pid, ids, shots):
    """The eleven option tabs, switched through the sheet rather than by clicking the
    tab control, so a two-row tab layout cannot put a tab under the mouse of another."""
    before = set(top_level_windows(pid))
    # By class too: ID_setopt is 3, which is also IDABORT, so a bare ID is not distinctive.
    click(find(main, control_id=ids["ID_setopt"], class_name="Button"))
    sheet = wait(lambda: next((h for h in top_level_windows(pid) if h not in before), None),
                 "the options sheet", 30)
    tabs = wait(lambda: find(sheet, class_name="SysTabControl32"), "the option tabs", 15)
    count = win32gui.SendMessage(tabs, TCM_GETITEMCOUNT, 0, 0)
    captions = TabControlWrapper(tabs)
    print(f"  options sheet {sheet:#x}: {count} tabs")
    for i in range(count):
        win32gui.SendMessage(sheet, PSM_SETCURSEL, i, 0)
        time.sleep(0.6)
        shots.take(sheet, f"{4 + i:02d}_options_{slug(captions.get_tab_text(i))}")
    click(find(sheet, control_id=IDCANCEL))
    wait(lambda: sheet not in top_level_windows(pid), "the options sheet to close", 15)


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
    # The pane's own hwnd rather than a crop of the window: a crop box would go stale the
    # next time the layout moves, and the panel is what the website shows.
    shots.animate(win32gui.GetParent(inforun), "16_mirror_progress_panel",
                  lambda: find(main, control_id=ids["IDC_infoend"]))

    pane(main, ids["IDC_infoend"], "the finished pane", timeout=300)
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
    ap.add_argument("--port", type=int, default=8099)
    ap.add_argument("--base-path", default=r"C:\shots-mirror")
    args = ap.parse_args()

    ids = resource_ids(args.resource_h)
    os.makedirs(args.out, exist_ok=True)
    # An existing mirror finishes from cache in no time, which turns the progress shot
    # into a second copy of the finished screen.
    shutil.rmtree(args.base_path, ignore_errors=True)
    serve(args.port)

    shots = Shots(args.out)
    exe = os.path.abspath(args.exe)
    proc = subprocess.Popen([exe], cwd=os.path.dirname(exe))
    try:
        run(proc.pid, ids, shots, f"http://127.0.0.1:{args.port}/", args.base_path)
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
