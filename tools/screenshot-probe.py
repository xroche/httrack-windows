#!/usr/bin/env python3
"""Can this machine render and capture the GUI at all?

Launches WinHTTrack, captures every window it opens both off the screen and via
PrintWindow, and measures how much of each image is not black. Notepad gets the same
treatment as a control: black everywhere means no rendering desktop, black for
WinHTTrack alone means the app is what fails to draw. Run this when the runner image
changes or when moving the walk to a VM; the walk itself diagnoses its own failures.
"""

import argparse
import ctypes
import os
import subprocess
import sys
import time

import win32gui
from PIL import ImageGrab

from wincapture import capture, controls, slug, top_level_windows

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32


def describe_session():
    """Session 0 is the service session and has no desktop to draw on."""
    sid = ctypes.c_ulong()
    kernel32.ProcessIdToSessionId(os.getpid(), ctypes.byref(sid))
    dpi = user32.GetDpiForSystem() if hasattr(user32, "GetDpiForSystem") else "?"
    print(f"session={sid.value} screen={user32.GetSystemMetrics(0)}x{user32.GetSystemMetrics(1)} dpi={dpi}")


def screen_grab(hwnd, path):
    """The naive route, kept for comparison: it also picks up whatever overlaps."""
    img = ImageGrab.grab(bbox=win32gui.GetWindowRect(hwnd), all_screens=True)
    img.save(path)
    return 1.0 - sum(img.convert("L").histogram()[:16]) / (img.width * img.height)


def probe(exe, tag, outdir, checkpoints, results):
    print(f"\n=== {tag}: {exe} ===")
    proc = subprocess.Popen([exe])
    seen = set()
    try:
        for pause in checkpoints:
            time.sleep(pause)
            for hwnd in [h for h in top_level_windows(proc.pid) if h not in seen]:
                seen.add(hwnd)
                cls, title = win32gui.GetClassName(hwnd), win32gui.GetWindowText(hwnd)
                print(f"  {hwnd:#x} class={cls!r} title={title!r} rect={win32gui.GetWindowRect(hwnd)}")
                user32.SetForegroundWindow(hwnd)
                time.sleep(0.3)
                name = slug(f"{tag}_{cls}_{title}")[:60]
                for how, shoot in (("printwindow", capture), ("screen", screen_grab)):
                    try:
                        nonblack = shoot(hwnd, os.path.join(outdir, f"{name}_{how}.png"))
                    except Exception as e:
                        print(f"    {how}: failed: {e}")
                        continue
                    print(f"    {how}: nonblack={nonblack:.3f}")
                    results.setdefault((tag, how), []).append(nonblack)
                for _, cid, cls, text in controls(hwnd)[:40]:
                    print(f"      id={cid} {cls!r} {text[:40]!r}")
        if not seen:
            print(f"  {tag} opened no visible window (exit code {proc.poll()})")
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
    results = {}
    started = probe(os.path.abspath(args.exe), "app", args.out, [6, 6, 8], results)
    probe(os.path.join(os.environ["WINDIR"], "system32", "notepad.exe"), "notepad",
          args.out, [4], results)

    print("\n=== verdict ===")
    for (tag, how), values in sorted(results.items()):
        best = max(values)
        print(f"{tag}/{how}: best nonblack={best:.3f} -> {'renders' if best > 0.02 else 'BLANK'}")
    if not started:
        print("the app never showed a window: the walk cannot run here")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
