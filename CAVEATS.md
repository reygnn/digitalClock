# Caveats

Limits of digitalClock that are known and left as they are.


## Size

The width only grows: after a long day name the window stays that wide until the
clock restarts.

deskControl is one row high with a single desktop and then no longer matches the
clock and pulseControl in the dock; from two desktops on it is two rows, as they are.

It needs wl_compositor 4. It draws at the integer scale the compositor asks for with
wl_compositor 6. A fractional scale is drawn at the next integer and scaled down by
the compositor; a compositor with wl_compositor 4 or 5 never asks, and there the
window stays at scale 1 and is blurred.

The cursor is loaded from the theme at its size times the scale, the theme's nearest
size. When that is no multiple of the scale, it is shown at the largest scale below
that divides it, and so a little larger; when the theme has nothing that large, it
is smaller. `XCURSOR_SIZE` above 512 counts as unset.

It binds the first seat. A seat removed and added again is not taken up until the
clock restarts.


## Text

The time and the date are one line each; a newline in a format becomes a space. A
format that makes more than 255 bytes shows nothing and says so once on stderr.

Digits are centred by the ink of all ten. Fonts with old style figures, whose digits
differ in height, sit a little high.


## Drawing

While the window is hidden, it gets no frames and does not draw. Shown again, it may
show the old time for one frame.
