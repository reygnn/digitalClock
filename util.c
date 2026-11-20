/* util.c - small pieces of clock.c without Wayland; see util.h. */
#include <stdlib.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "util.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

int cursor_size(const char *s)
{
	char *end;
	long v = s ? strtol(s, &end, 10) : 0;
	if (!s || end == s || *end || v <= 0)
		return 24;
	return v > 256 ? 256 : (int)v;
}

void cursor_fit(int w, int h, int s, int has_vp, int *buf_scale, int *vw, int *vh)
{
	*buf_scale = 1;
	*vw = *vh = -1;
	if (w % s == 0 && h % s == 0) {
		*buf_scale = s;
	} else if (has_vp) {
		*vw = MAX((w + s / 2) / s, 1);
		*vh = MAX((h + s / 2) / s, 1);
	} else {
		for (int d = s - 1; d > 1; d--)
			if (w % d == 0 && h % d == 0) {
				*buf_scale = d;
				break;
			}
	}
}

int key_quits(unsigned ks, int ctrl, int alt, int logo)
{
	if (alt || logo)
		return 0;       /* Alt+Q must not quit */
	if (ctrl)
		return ks == XKB_KEY_q || ks == XKB_KEY_Q || ks == XKB_KEY_w ||
			ks == XKB_KEY_W;
	return ks == XKB_KEY_q || ks == XKB_KEY_Q || ks == XKB_KEY_Escape;
}
