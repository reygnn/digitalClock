# Caveats

Limits of digitalClock that are known and left as they are.


## Size

The width only grows: after a long day name the window stays that wide until the
clock restarts.

It needs wl_compositor 4. It draws at the integer scale the compositor asks for with
wl_compositor 6. A fractional scale is drawn at the next integer and scaled down by
the compositor; a compositor with wl_compositor 4 or 5 never asks, and there the
window stays at scale 1 and is blurred. A scale above 16 is drawn at 16.

The cursor is loaded from the theme at its size times the scale, the theme's nearest
size; when the theme has nothing that large, it is smaller. When that size is no
multiple of the scale, a viewport shows it at its size over the scale. Without
wp_viewporter it is shown at the largest scale below that divides it, and so a
little larger. `XCURSOR_SIZE` above 256 counts as 256.


## Text

The time and the date are one line each; a newline in a format becomes a space. A
format that makes more than 255 bytes shows nothing and says so once on stderr.

Digits are centred by the ink of all ten. Fonts with old style figures, whose digits
differ in height, sit a little high.


## Drawing

While the window is hidden, it gets no frames and does not draw. Shown again, it may
show the old time for one frame.


## Input

It keeps the first eight seats the compositor announces. One beyond that is taken
only if it is announced again after the others went.


## Colours

Without `Mwm*foreground` the text is black or white by Motif's rule, as mwc's titles.
On a mid-tone that can fall short of WCAG's 4.5:1: on mwc's activeBackground
`#7387ad` the text is white at about 3.8:1, and the date, which can't be dimmed
there, as white as the time.
