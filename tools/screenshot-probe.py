#!/usr/bin/env python3
"""Can a hosted Windows runner render and capture the GUI?

Launches WinHTTrack, captures every top-level window it opens both off the screen
and via PrintWindow, and measures how much of each image is not black. Notepad gets
the same treatment as a control: black everywhere means the session has no rendering
desktop, black for WinHTTrack alone means the app is what fails to draw.
"""

import argparse
import ctypes
import os
import re
import subprocess
import sys
import time

import win32gui
import win32process
import win32ui
from PIL import Image, ImageGrab

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32
PW_RENDERFULLCONTENT = 2


def describe_session():
    """Session 0 is the service session and has no desktop to draw on."""
    sid = ctypes.c_ulong()
    kernel32.ProcessIdToSessionId(os.getpid(), ctypes.byref(sid))
    try:
        dpi = user32.GetDpiForSystem()
    except AttributeError:
        dpi = "?"
    print(f"session={sid.value} screen={user32.GetSystemMetrics(0)}x{user32.GetSystemMetrics(1)} dpi={dpi}")
    print(f"desktop window={user32.GetDesktopWindow():#x} foreground={user32.GetForegroundWindow():#x}")


def top_level_windows(pid):
    found = []

    def visit(hwnd, _):
        if win32gui.IsWindowVisible(hwnd) and win32process.GetWindowThreadProcessId(hwnd)[1] == pid:
            found.append(hwnd)
        return True

    win32gui.EnumWindows(visit, None)
    return found


def controls(hwnd):
    """Descendant control IDs and classes -- what a pywinauto walk would navigate by."""
    out = []

    def visit(child, _):
        out.append((win32gui.GetDlgCtrlID(child), win32gui.GetClassName(child),
                    win32gui.GetWindowText(child)[:40]))
        return True

    win32gui.EnumChildWindows(hwnd, visit, None)
    return out


def print_window(hwnd):
    """Render into a memory DC: unlike a screen grab this survives occlusion."""
    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    w, h = right - left, bottom - top
    if w <= 0 or h <= 0:
        return None, 0
    src = win32gui.GetWindowDC(hwnd)
    dc = win32ui.CreateDCFromHandle(src)
    mem = dc.CreateCompatibleDC()
    bmp = win32ui.CreateBitmap()
    bmp.CreateCompatibleBitmap(dc, w, h)
    mem.SelectObject(bmp)
    ok = user32.PrintWindow(hwnd, mem.GetSafeHdc(), PW_RENDERFULLCONTENT)
    img = Image.frombuffer("RGB", (w, h), bmp.GetBitmapBits(True), "raw", "BGRX", 0, 1)
    win32gui.DeleteObject(bmp.GetHandle())
    mem.DeleteDC()
    dc.DeleteDC()
    win32gui.ReleaseDC(hwnd, src)
    return img, ok


def grab_screen(hwnd):
    try:
        return ImageGrab.grab(bbox=win32gui.GetWindowRect(hwnd), all_screens=True)
    except Exception as e:  # a headless session can refuse the desktop DC outright
        print(f"    screen grab failed: {e}")
        return None


def liveness(img):
    """Distinct colours and non-black fraction; a dead capture is uniformly black."""
    hist = img.convert("L").histogram()
    n = img.width * img.height
    colors = img.convert("RGB").getcolors(1 << 18)
    return (len(colors) if colors else -1), 1.0 - sum(hist[:16]) / n


def capture(hwnd, tag, outdir, results):
    cls = win32gui.GetClassName(hwnd)
    title = win32gui.GetWindowText(hwnd)
    print(f"  {hwnd:#x} class={cls!r} title={title!r} rect={win32gui.GetWindowRect(hwnd)}")
    try:
        user32.SetForegroundWindow(hwnd)
    except Exception:
        pass
    time.sleep(0.3)
    slug = re.sub(r"[^A-Za-z0-9]+", "_", f"{tag}_{cls}_{title}").strip("_")[:60]
    printed, ok = print_window(hwnd)
    for how, img in (("printwindow", printed), ("screen", grab_screen(hwnd))):
        if img is None:
            print(f"    {how}: no image")
            continue
        path = os.path.join(outdir, f"{slug}_{how}.png")
        img.save(path)
        colors, nonblack = liveness(img)
        print(f"    {how}: {img.width}x{img.height} colors={colors} nonblack={nonblack:.3f}"
              f"{f' PrintWindow={ok}' if how == 'printwindow' else ''} -> {os.path.basename(path)}")
        results.setdefault((tag, how), []).append(nonblack)


def walk(exe, tag, outdir, checkpoints, results):
    print(f"\n=== {tag}: {exe} ===")
    proc = subprocess.Popen([exe])
    seen = set()
    try:
        for t in checkpoints:
            time.sleep(t)
            for hwnd in top_level_windows(proc.pid):
                if hwnd in seen:
                    continue
                seen.add(hwnd)
                capture(hwnd, tag, outdir, results)
                for cid, cls, text in controls(hwnd)[:40]:
                    print(f"      id={cid} {cls!r} {text!r}")
        if not seen:
            print(f"  {tag} opened no visible top-level window (exit code {proc.poll()})")
    finally:
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return bool(seen)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--out", default="shots")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    describe_session()
    try:
        import pywinauto
        print(f"pywinauto {pywinauto.__version__}")
    except Exception as e:
        print(f"pywinauto unavailable: {e}")

    results = {}
    got_app = walk(os.path.abspath(args.exe), "app", args.out, [6, 6, 8], results)
    walk(os.path.join(os.environ["WINDIR"], "system32", "notepad.exe"), "notepad",
         args.out, [4], results)

    print("\n=== verdict ===")
    for (tag, how), values in sorted(results.items()):
        best = max(values)
        print(f"{tag}/{how}: best nonblack={best:.3f} -> {'renders' if best > 0.02 else 'BLANK'}")
    if not got_app:
        print("the app never showed a window: the walk cannot be built on this runner")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
