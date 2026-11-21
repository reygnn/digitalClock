# digitalClock

Project rules. The rules for all C projects, mwc as their reference, are in the global
CLAUDE.md.


## Build and test

Test inside mwc's `mwc-dev` distrobox; the host lacks grim.
`distrobox enter mwc-dev -- bash -lc 'cd ~/data/C-Projects/digitalClock && make test'`.

| Target | |
|---|---|
| `make` | digitalClock: `clock.c`, `timefmt.c` without Wayland, and mtk from `mtk/` |
| `make check` | `tests/test_timefmt.c` under ASan and UBSan, no compositor |
| `make test` | `tests/ui.sh`: the clock in a headless mwc's dock, checked in screenshots; log in `build/ui.log`, the clock's own in `build/ui-digitalClock.log`, the sisters' in `build/ui-sisters.log`, the protocol log in `build/ui-wayland.log`, a failure's screenshot in `build/ui-last.png` |

`make test` needs a built mwc with `build/vinput`, `build/pngtool` and `build/xdgc`
in `MWC_DIR` (default `../mwc`), the Adwaita cursor theme, and deskControl,
pulseControl and osDisplay built next to this directory, or `SKIP_SISTERS=1`. Polling helpers come from mwc's `tests/lib.sh`.


## Rules

- `mtk/` is mtk, vendored with git subtree: changed in mtk's repository and taken
  with `git subtree pull --prefix=mtk`, never edited here.
- The font is fixed, DejaVu Sans 10, the same `dock_font()` in deskControl,
  pulseControl and osDisplay; the window is as high as theirs. A change to the font or
  the layout keeps that in all four, and the test compares it.
- Colours, contrast and bevels follow mwc and the sisters; a fix there is checked here
  too.
