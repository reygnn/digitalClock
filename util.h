/* util.h - small pieces of clock.c without Wayland: the cursor's size and how it
 * is shown at a scale, and which keys quit. test_util checks them. */
#ifndef UTIL_H
#define UTIL_H

/* XCURSOR_SIZE as given in s: 24 if NULL or bad, at most 256, so that times the
 * scale it does not overflow. A number past LONG_MAX is the largest too. */
int cursor_size(const char *s);

/* A theme has only some sizes: Adwaita gives 30 for 28. A buffer scale that
 * doesn't divide the size is a protocol error. A w x h image loaded for scale s
 * is shown at *buf_scale; with a viewport (has_vp) at *vw x *vh, its size over
 * the scale, else they are -1 and the buffer scale is the largest up to s that
 * divides it, and the cursor that much larger. */
void cursor_fit(int w, int h, int s, int has_vp, int *buf_scale, int *vw, int *vh);

/* Whether the key quits: Esc, q or Q, with Ctrl also w and W. With Alt or Super
 * held no key does. ks is an xkb keysym. */
int key_quits(unsigned ks, int ctrl, int alt, int logo);

#endif
