"""Win32 helpers shared by the screenshot tools: find a window, its controls, capture it."""

import ctypes
import re
import time

import win32gui
import win32process
import win32ui
from PIL import Image

user32 = ctypes.windll.user32
PW_RENDERFULLCONTENT = 2
WM_SETTEXT = 0x000C
BM_CLICK = 0x00F5


class Timeout(Exception):
    pass


def wait(fn, what, timeout=30, poll=0.4):
    """Poll fn until it returns something truthy, or give up with a named error."""
    deadline = time.monotonic() + timeout
    while True:
        value = fn()
        if value:
            return value
        if time.monotonic() > deadline:
            raise Timeout(f"timed out after {timeout}s waiting for {what}")
        time.sleep(poll)


def top_level_windows(pid):
    found = []

    def visit(hwnd, _):
        if win32gui.IsWindowVisible(hwnd) and win32process.GetWindowThreadProcessId(hwnd)[1] == pid:
            found.append(hwnd)
        return True

    win32gui.EnumWindows(visit, None)
    return found


def window_titled(pid, pattern):
    """The process's visible top-level window whose title matches, if it is up yet."""
    for hwnd in top_level_windows(pid):
        if re.search(pattern, win32gui.GetWindowText(hwnd)):
            return hwnd
    return None


def controls(hwnd):
    """Every descendant, as (hwnd, control id, class, text)."""
    out = []

    def visit(child, _):
        out.append((child, win32gui.GetDlgCtrlID(child), win32gui.GetClassName(child),
                    win32gui.GetWindowText(child)))
        return True

    win32gui.EnumChildWindows(hwnd, visit, None)
    return out


def find(parent, control_id=None, class_name=None, visible=True):
    """First matching descendant. Visible by default: a property sheet keeps the
    controls of every page alive, so existence alone does not mean it is on screen."""
    for hwnd, cid, cls, _ in controls(parent):
        if control_id is not None and cid != control_id:
            continue
        if class_name is not None and cls != class_name:
            continue
        if visible and not win32gui.IsWindowVisible(hwnd):
            continue
        return hwnd
    return None


def click(hwnd):
    """Posted, not sent: a button that opens a modal dialog would block a SendMessage."""
    win32gui.PostMessage(hwnd, BM_CLICK, 0, 0)


def set_text(hwnd, text):
    win32gui.SendMessage(hwnd, WM_SETTEXT, 0, text)


def grab(hwnd):
    """PrintWindow into a memory DC, so the shot carries no taskbar and nothing that
    happens to overlap."""
    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    w, h = right - left, bottom - top
    if w <= 0 or h <= 0:
        raise ValueError(f"window {hwnd:#x} has no area")
    src = win32gui.GetWindowDC(hwnd)
    dc = win32ui.CreateDCFromHandle(src)
    mem = dc.CreateCompatibleDC()
    bmp = win32ui.CreateBitmap()
    bmp.CreateCompatibleBitmap(dc, w, h)
    old = mem.SelectObject(bmp)
    user32.PrintWindow(hwnd, mem.GetSafeHdc(), PW_RENDERFULLCONTENT)
    img = Image.frombuffer("RGB", (w, h), bmp.GetBitmapBits(True), "raw", "BGRX", 0, 1)
    # Select the original bitmap back before deleting ours: deleting a bitmap that is
    # still selected fails, and so then does deleting the DC holding it.
    mem.SelectObject(old)
    win32gui.DeleteObject(bmp.GetHandle())
    mem.DeleteDC()
    dc.DeleteDC()
    win32gui.ReleaseDC(hwnd, src)
    return img


def nonblack(img):
    """Fraction of the image that is not black."""
    w, h = img.size
    return 1.0 - sum(img.convert("L").histogram()[:16]) / (w * h)


def capture(hwnd, path):
    """Shoot a window to a PNG. Returns the fraction of the image that is not black."""
    img = grab(hwnd)
    img.save(path)
    return nonblack(img)


def relative_rect(hwnd, parent):
    """A window's rectangle in the coordinates of a shot of one of its ancestors."""
    left, top, right, bottom = win32gui.GetWindowRect(hwnd)
    x, y = win32gui.GetWindowRect(parent)[:2]
    return left - x, top - y, right - x, bottom - y


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")
