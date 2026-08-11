# Documentation screenshots

`screenshot-walk.py` drives the GUI and captures one PNG per documentation screen:
the first-run language dialog, the welcome, project and URL panes, all eleven option
tabs, the ready-to-start pane, and a mirror in progress and finished. The crawl is
real — the script serves a small site to itself, so the progress screen carries live
counters rather than zeros.

The progress pane comes out a second time as a ten-frame animation,
`16_mirror_progress_panel.apng`, with its first frame beside it as a PNG. The website's
front page uses it to show a mirror actually running rather than a frozen one. It is a
shot of the pane's own window rather than a crop, so it follows the layout instead of
going stale with it.

## Replay

Run the **screenshots** workflow (Actions → Run workflow) and download the
`screenshots` artifact. It takes the binaries from the last green `windows-build` on
the branch you dispatch it from, falling back to master, so a new option page is shot
from the build that added it. `run-id` overrides that; `mode: probe` runs
`screenshot-probe.py` instead, which only answers whether the machine renders and
captures at all — reach for it when the runner image changes or when moving to a VM.

On a Windows box or VM, against any staged build directory:

```
pip install pywin32 pillow pywinauto
python tools\screenshot-walk.py --exe path\to\WinHTTrack.exe --out shots
```

## Adding an option page

Nothing to edit: the tabs are enumerated from the sheet and named after their caption,
so a new page appears in the set on the next run. A new *wizard* pane does need a line
in `run()`, anchored on a control ID from that pane's dialog.

## Traps

- Controls come from `WinHTTrack/resource.h` at run time, so the checkout and the
  binaries have to be the same version; the workflow warns when it falls back to
  another branch's build.
- A pane is only shot once its own anchor control is *visible*. The property sheet
  creates every page up front, so the controls of a page that is not on screen exist
  and would otherwise satisfy the wait.
- An existing mirror at the base path finishes from cache in no time, which turns the
  progress shot into a second copy of the finished screen. The script wipes it first.
- Shots of the main window include the file tree, so on a runner they show that
  machine's `C:\`. A VM with a tidy drive gives nicer full-window shots; the option
  tabs are separate dialogs and carry none of it.
- Comparing a new set against the previous one byte for byte is what makes the walk
  worth running before a layout change. The animation cannot take part — its counters
  differ on every run — so `16_mirror_progress.png` stays the guarded shot of that
  screen, and the animation is eyeballed.
- The sequence has to fit inside the crawl: the walk fails rather than write a short
  animation, and prints how long the mirror ran so the cadence can be matched to it.
