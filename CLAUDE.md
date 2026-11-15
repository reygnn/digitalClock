# digitalClock

Project rules. The rules for all C projects, mwc as their reference, are in the global
CLAUDE.md.


## Build and test

Test inside mwc's `mwc-dev` distrobox; the host lacks grim.
`distrobox enter mwc-dev -- bash -lc 'cd ~/data/C-Projects/mClock && make test'`.

| Target | |
|---|---|
| `make` | digitalClock, one C file (`clock.c`) |
| `make test` | `tests/ui.sh`: the clock in a headless mwc's dock, checked in screenshots; log in `build/ui.log`, the clock's own in `build/ui-digitalClock.log`, the sisters' in `build/ui-sisters.log`, a failure's screenshot in `build/ui-last.png` |

`make test` needs a built mwc with `build/vinput`, `build/pngtool` and `build/xdgc`
in `MWC_DIR` (default `../mwc`) and deskControl and pulseControl built next to this
directory, or `SKIP_SISTERS=1`. Polling helpers come from mwc's `tests/lib.sh`.


## Rules

- The window is as high as deskControl and pulseControl with the same font; a change
  to the layout keeps that, and the test compares it.
- Colours, contrast and bevels follow mwc and the sisters; a fix there is checked here
  too.
