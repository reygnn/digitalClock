# Caveats

Limits of digitalClock that are known and left as they are.


## Size

The width only grows: after a long day name the window stays that wide until the
clock restarts.

deskControl is two rows high only with three or four desktops. With fewer or more it
no longer matches the clock and pulseControl in the dock.

It draws at the integer scale the compositor asks for with wl_compositor 6. A
fractional scale is drawn at the next integer and scaled down by the compositor; an
older compositor never asks, and there the window stays at scale 1 and is blurred.


## Text

The time and the date are one line each; a newline in a format becomes a space. A
format that makes more than 255 bytes shows nothing and says so once on stderr.

Digits are centred by the ink of all ten. Fonts with old style figures, whose digits
differ in height, sit a little high.


## Drawing

While the window is hidden, it gets no frames and does not draw. Shown again, it may
show the old time for one frame.
