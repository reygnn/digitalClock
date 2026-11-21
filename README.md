# digitalClock

digitalClock shows the time in digits, with the date below, in the look of the Motif
window manager. It is made for the dock of [mwc](https://github.com/reygnn/mwc), beside
[deskControl](https://github.com/reygnn/deskControl) and
[pulseControl](https://github.com/reygnn/pulseControl), and runs on any Wayland
compositor with xdg-shell. C on [mtk](https://github.com/reygnn/mtk), the toolkit of
mwc's clients, vendored in `mtk/`: libwayland, xkbcommon, Cairo and Pango.


## Build

On Ubuntu 26.04:

    sudo apt install build-essential pkg-config libwayland-dev wayland-protocols \
        libxkbcommon-dev libcairo2-dev libpango1.0-dev fonts-dejavu-core
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

`-v` prints the version, which is set in `version.h`,
and the commit built, as `VERSION+DATE.TIME.HASH` with its commit time in UTC
and `.dirty` for changes not committed; `-h` the usage.


## Look

The time sits in a sunken field, as large as the field allows, the date below it in
the smaller font, dimmed as far as a contrast of 4.5:1 allows. The font is fixed,
DejaVu Sans 10, the same in deskControl, pulseControl and osDisplay, so the window is
as high as theirs and the four line up in the dock. Without DejaVu Sans fontconfig
takes another font, and digitalClock says so on stderr.
It is as wide as its text and grows when the text does, but never shrinks, so the
dock does not move every minute.

It draws only mwm's thin border, no title bar, buttons or resize handles. The border
takes the active colour while the clock has the keyboard. The colours are mwc's:
`MWC_INACTIVE` and `MWC_ACTIVE` from the environment, else `Mwm*background` and
`Mwm*activeBackground` from the mwmrc, else the Motif defaults. The text is
`Mwm*foreground`, else black or white as Motif picks it, as mwc does for its titles.

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
| `MWC_INACTIVE`, `MWC_ACTIVE` | `#aaaaaa`, `#c6c1aa` | background, border with the keyboard |
| `LC_TIME`, `TZ` | | names of days and months, the zone |


## How this is built

digitalClock is developed with Claude (Anthropic) as a coding assistant. I set the
scope and the design and review the changes; Claude writes most of the code and the
tests. What is open is written down in CAVEATS.


## Testing

    make check

runs the unit tests of `timefmt.c`, the time as one line and its timer, under ASan
and UBSan. It needs no compositor. The colours, the mwmrc, the cursor and the keys
are mtk's and tested there.

    make test

runs digitalClock in the dock of a headless mwc beside deskControl, pulseControl and
osDisplay and checks in screenshots its border and focus, the height against theirs, the
digits, the date and its contrast, that it ticks, a format with letters, the colours it
reads from the environment and the mwmrc, the keys, the cursor and sharpness on a
scaled output, the warning without DejaVu Sans, `-v` and `-h`. It needs a built mwc
with `build/vinput`, `build/pngtool` and `build/xdgc` in `MWC_DIR` (by default
`../mwc`), grim, the Adwaita cursor theme, and the sisters built next to this
directory; without them set `SKIP_SISTERS=1`. mwc's development box has all of it:

    distrobox enter mwc-dev -- bash -lc 'cd ~/data/C-Projects/digitalClock && make test'

The log is in `build/ui.log`, the clock's own in `build/ui-digitalClock.log`, the
sisters' in `build/ui-sisters.log`, its protocol log on a scaled output in
`build/ui-wayland.log`, a failure's screenshot in `build/ui-last.png`.
