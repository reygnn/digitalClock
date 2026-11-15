# digitalClock

digitalClock shows the time in digits, with the date below, in the look of the Motif
window manager. It is made for the dock of [mwc](https://github.com/reygnn/mwc), beside
[deskControl](https://github.com/reygnn/deskControl) and
[pulseControl](https://github.com/reygnn/pulseControl), and runs on any Wayland
compositor with xdg-shell. One C file on libwayland, xkbcommon, Cairo and Pango, no
toolkit.


## Build

On Ubuntu 26.04:

    sudo apt install build-essential pkg-config libwayland-dev wayland-protocols \
        libxkbcommon-dev libcairo2-dev libpango1.0-dev
    make
    sudo make install

`make install` puts `digitalClock` in `/usr/local/bin` and adds a desktop entry.


## Run

In `~/.config/mwc/mwmrc`:

    Mwm*digitalClock*dock:   true

and in `~/.config/mwc/autostart`:

    digitalClock &

The order of the `dock` lines is the order in the dock; with the default
`dockPlacement`, `right bottom`, the first one sits in the corner.

`-v` prints the version, which is set in `version.h`; `-h` the usage.


## Look

The time sits in a sunken field, as large as the field allows, the date below it in
the smaller font, dimmed as far as a contrast of 4.5:1 allows. The window is as high
as deskControl and pulseControl with the same font, so the three line up in the dock.
It is as wide as its text and grows when the text does, but never shrinks, so the
dock does not move every minute.

It draws only mwm's thin border, no title bar, buttons or resize handles. The border
takes the active colour while the clock has the keyboard. The colours are mwc's:
`MWC_INACTIVE` and `MWC_ACTIVE` from the environment, else `Mwm*background` and
`Mwm*activeBackground` from the mwmrc, else the Motif defaults.

The display changes on the full second, and at once when the system clock is set or
the machine resumes.


## Keys

| Keys | Action |
|---|---|
| Esc, q, Ctrl+Q, Ctrl+W | quit |

With Alt or Super held no key acts.


## Configuration

| Variable | Default | |
|---|---|---|
| `CLOCK_FORMAT` | `%H:%M` | the time, as for `strftime`, e.g. `%H:%M:%S`; with letters it is sized for their accents and descenders |
| `CLOCK_DATE` | `%a %d.%m.` | the date; empty for none |
| `CLOCK_FONT` | `Sans 10` | the date's Pango font, as `DC_FONT` and `SC_FONT` of the sisters; its size sets the window's height, the time is the same font, larger |
| `MWC_INACTIVE`, `MWC_ACTIVE` | `#aeb2c3`, `#7387ad` | background, border with the keyboard |
| `LC_TIME`, `TZ` | | names of days and months, the zone |


## How this is built

digitalClock is developed with Claude (Anthropic) as a coding assistant. I set the
scope and review the changes; Claude writes most of the code and the tests. What is
open is written down in CAVEATS.


## Testing

    make test

runs digitalClock in the dock of a headless mwc beside deskControl and pulseControl
and checks in screenshots its border and focus, the height against both, the digits,
the date and its contrast, that it ticks, a format with letters, the colours it reads,
the keys, the cursor and sharpness on a scaled output, and `-v`. It needs a built
mwc with `build/vinput`, `build/pngtool` and `build/xdgc` in `MWC_DIR` (by default
`../mwc`), grim, and the sisters built next to this directory; without them set
`SKIP_SISTERS=1`. mwc's development box has all of it:

    distrobox enter mwc-dev -- bash -lc 'cd ~/data/C-Projects/mClock && make test'

The log is in `build/ui.log`, the clock's own in `build/ui-digitalClock.log`, the
sisters' in `build/ui-sisters.log`, a failure's screenshot in `build/ui-last.png`.
