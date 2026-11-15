/*
 * digitalClock - the time in digits on Wayland, in the Motif look of mwc
 * (https://github.com/reygnn/mwc), mfm, pulseControl and deskControl.
 *
 * libwayland, xkbcommon, Pango and Cairo, one file, no toolkit. The time sits in a
 * sunken field, its digits as high as the field allows, the date below it in the
 * font of the sister tools' labels, dimmed. The window is as high as
 * pulseControl and deskControl with the same font, so the three line up in mwc's
 * dock. It reads the same colours as mwc: MWC_INACTIVE / MWC_ACTIVE from the
 * environment, else Mwm*background and Mwm*activeBackground from
 * ~/.config/mwc/mwmrc, else the Motif defaults.
 *
 * It asks for no server-side decoration and draws its own frame: only mwm's thin
 * border, no title bar, no buttons, no resize handles. The border takes the active
 * colour while it has the keyboard, as mwc's frames do.
 *
 * A realtime timerfd wakes it at every full second, and at once when the clock is
 * set; it redraws only when a text changes.
 *
 * Keys:   Esc / q / Ctrl+Q / Ctrl+W  quit
 *
 * In mwc it belongs in the dock: Mwm*digitalClock*dock: true.
 */
#define _GNU_SOURCE
#include <cairo.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "version.h"
#include "xdg-shell-client-protocol.h"

#define BEV 2           /* bevel width, as in mwm */
#define PAD 6
#define FRAME (BEV + 1) /* mwm's border without resize handles, as mwc draws it */
#define FIELD_PAD 10    /* left and right of the digits */
#define INK_PAD 5       /* above and below them */
#define DATE_GAP 5      /* between the time's ink and the date's */
#define MAX_TEXT 256
#ifndef MAX            /* glib has them too */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

struct rect { int x, y, w, h; };

/* ---- state -------------------------------------------------------------- */

static const char *fmt = "%H:%M";
static const char *date_fmt = "%a %d.%m.";     /* "": no date line */
static char shown[MAX_TEXT];    /* the time in the field */
static char shown_date[MAX_TEXT];
static int tfd = -1;
static int dirty = 1;

/* ---- Wayland ------------------------------------------------------------ */

/* One wl_shm buffer with a Cairo context on it. */
struct shmbuf {
	struct wl_buffer *wl;
	cairo_surface_t *cs;
	cairo_t *cr;
	void *data;
	size_t size;
	int w, h;
	int busy;       /* the compositor still reads it */
	int stale;      /* wrong size: free it as soon as it is released */
};

static struct wl_display *dpy;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct xdg_wm_base *wm_base;
static struct wl_surface *surface;
static struct wl_callback *frame;       /* a frame is on its way */
static struct shmbuf *bufs[2];
static struct xdg_surface *main_xdg;
static struct xdg_toplevel *toplevel;
static int configured;          /* the first configure has arrived */
static int need_commit;         /* a configure waits for its commit */
static int scale = 1;           /* buffer pixels per surface pixel */

static struct wl_pointer *pointer;
static struct wl_surface *cursor_surf;
static struct wl_cursor_theme *cursor_theme;
static struct wl_cursor_image *cursor_img;
static int cursor_scale;         /* the scale the theme was loaded for */
static uint32_t ptr_serial;     /* the last enter's, for set_cursor */
static int ptr_in;              /* the pointer is on the window */
static void cursor_load(void);
static void cursor_show(void);

static struct wl_keyboard *keyboard;
static struct xkb_context *xkb;
static struct xkb_keymap *kb_map;
static struct xkb_state *kb_state;

static int W, H, sized_w, sized_h;
static int focused;             /* it has the keyboard: the active frame */
static uint32_t px_bg, px_top, px_bot, px_sel;
static uint32_t xc_fg, xc_dim;
static cairo_t *cr;             /* what fill, bevel and text draw on */
static PangoLayout *plo;        /* the digits */
static PangoLayout *plo_date;   /* the date, in the font as given */
static int barh;                /* a button's height in the sister tools */
static int date_h;              /* the date's digits and the gap, 0 without one */
static int ink_y, ink_h;        /* where the time's ink can be in its line */
static int date_ink_y;          /* ... and the date's, by its digits too */
static int text_w;              /* the widest text so far: the field never shrinks */

/* ---- buffers ------------------------------------------------------------ */

static void buf_free(struct shmbuf *b)
{
	cairo_destroy(b->cr);
	cairo_surface_destroy(b->cs);
	wl_buffer_destroy(b->wl);
	munmap(b->data, b->size);
	free(b);
}

static void buf_release(void *data, struct wl_buffer *wl)
{
	struct shmbuf *b = data;
	b->busy = 0;
	if (b->stale)
		buf_free(b);
}

static const struct wl_buffer_listener buf_listener = { buf_release };

static struct shmbuf *buf_new(int w, int h)
{
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, w);
	size_t size = (size_t)stride * h;
	struct shmbuf *b = calloc(1, sizeof *b);
	int fd = memfd_create("digitalClock", MFD_CLOEXEC);
	void *data = MAP_FAILED;
	if (b && fd >= 0 && ftruncate(fd, size) == 0)
		data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "digitalClock: no memory for a %dx%d buffer: %s\n", w, h,
			strerror(errno));
		exit(1);
	}
	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	b->wl = wl_shm_pool_create_buffer(pool, 0, w, h, stride,
		WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	wl_buffer_add_listener(b->wl, &buf_listener, b);
	b->data = data;
	b->size = size;
	b->w = w;
	b->h = h;
	b->cs = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24, w, h,
		stride);
	b->cr = cairo_create(b->cs);
	cairo_set_antialias(b->cr, CAIRO_ANTIALIAS_NONE);       /* not the text */
	return b;
}

/* A buffer of this size to draw on, or NULL while the compositor holds both;
 * its release wakes the main loop. */
static struct shmbuf *get_buffer(int w, int h)
{
	for (int i = 0; i < 2; i++) {
		struct shmbuf *b = bufs[i];
		if (!b || (b->w == w && b->h == h))
			continue;
		if (b->busy)
			b->stale = 1;
		else
			buf_free(b);
		bufs[i] = NULL;
	}
	for (int i = 0; i < 2; i++)
		if (bufs[i] && !bufs[i]->busy)
			return bufs[i];
	for (int i = 0; i < 2; i++)
		if (!bufs[i])
			return bufs[i] = buf_new(w, h);
	return NULL;
}

static void frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	wl_callback_destroy(cb);
	frame = NULL;
}

static const struct wl_callback_listener frame_listener = { frame_done };

/* ---- colours (same rules as mwm's draw.c) ------------------------------- */

static uint32_t mix(uint32_t rgb, uint32_t to, float f)
{
	uint32_t out = 0;
	for (int sh = 0; sh <= 16; sh += 8) {
		float c = (rgb >> sh) & 0xff, t = (to >> sh) & 0xff;
		out |= (uint32_t)(c + (t - c) * f + 0.5f) << sh;
	}
	return out;
}

/* An sRGB channel as linear light. */
static float linear(unsigned c)
{
	float v = c / 255.0f;
	return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}

/* Black or white, whichever has the higher WCAG contrast on bg. The two are equal
 * at a relative luminance of sqrt(1.05 * 0.05) - 0.05, a grey of about 0x76: a
 * mid-tone such as mwc's activeBackground gets black. */
static uint32_t contrast(uint32_t bg)
{
	float l = 0.2126f * linear((bg >> 16) & 0xff) + 0.7152f * linear((bg >> 8) & 0xff) +
		0.0722f * linear(bg & 0xff);
	return (l + 0.05f) * (l + 0.05f) > 1.05f * 0.05f ? 0x000000 : 0xffffff;
}

/* What mwc reads: "#rrggbb", names like "gold", and rgb:r/g/b with 1 to 4 hex
 * digits per channel. */
static int parse_color(const char *s, uint32_t *out)
{
	PangoColor c;
	if (!s || !*s)
		return 0;
	if (!strncasecmp(s, "rgb:", 4)) {
		uint32_t rgb = 0;
		s += 4;
		for (int i = 0; i < 3; i++) {
			char *end;
			unsigned long v = strtoul(s, &end, 16);
			int digits = end - s;
			if (digits < 1 || digits > 4 || *end != (i < 2 ? '/' : '\0'))
				return 0;
			unsigned long max = (1ul << (4 * digits)) - 1;
			rgb = rgb << 8 | (v * 255 + max / 2) / max;
			s = end + 1;
		}
		*out = rgb;
		return 1;
	}
	if (!pango_color_parse(&c, s))
		return 0;
	*out = (uint32_t)(c.red >> 8) << 16 | (c.green >> 8) << 8 | (c.blue >> 8);
	return 1;
}

/* Last global "Mwm*key: value" line of ~/.config/mwc/mwmrc. */
static char *mwmrc_get(const char *key, char *val, size_t n)
{
	char path[PATH_MAX], line[512];
	const char *home = getenv("HOME"), *xdg = getenv("XDG_CONFIG_HOME");
	int len;
	if (xdg && *xdg)
		len = snprintf(path, sizeof path, "%s/mwc/mwmrc", xdg);
	else
		len = snprintf(path, sizeof path, "%s/.config/mwc/mwmrc",
			home ? home : "");
	if (len < 0 || (size_t)len >= sizeof path)
		return NULL;
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	char *found = NULL;
	size_t kl = strlen(key);
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		while (isspace((unsigned char)*p))
			p++;
		/* As mwc reads it: any case, "." or "*", blanks before the ":". */
		char *colon = strchr(p, ':');
		if (!colon)
			continue;
		char *ke = colon;
		while (ke > p && isspace((unsigned char)ke[-1]))
			ke--;
		if (strncasecmp(p, "mwm", 3) || (p[3] != '*' && p[3] != '.') ||
		    (size_t)(ke - (p + 4)) != kl || strncasecmp(p + 4, key, kl))
			continue;
		p = colon + 1;
		while (isspace((unsigned char)*p))
			p++;
		char *e = p + strlen(p);
		while (e > p && isspace((unsigned char)e[-1]))
			*--e = 0;
		if ((size_t)snprintf(val, n, "%s", p) < n)
			found = val;
	}
	fclose(f);
	return found;
}

static uint32_t setting(const char *env, const char *res, uint32_t def)
{
	uint32_t rgb;
	char v[256];
	if (parse_color(getenv(env), &rgb))
		return rgb;
	if (parse_color(mwmrc_get(res, v, sizeof v), &rgb))
		return rgb;
	return def;
}

/* The height comes from the font as in pulseControl and deskControl: two of their
 * rows and the gap between. The time is in the same font, as large as that height
 * allows beside the date line. A time of digits is measured by their ink; one with
 * letters by the whole line, room for accents and descenders included. */
static void theme_init(void)
{
	uint32_t bg = setting("MWC_INACTIVE", "background", 0xaeb2c3);

	px_bg = bg;
	px_top = mix(bg, 0xffffff, 0.55f);
	px_bot = mix(bg, 0x000000, 0.5f);
	px_sel = setting("MWC_ACTIVE", "activeBackground", 0x7387ad);
	xc_fg = contrast(bg);
	xc_dim = mix(bg, xc_fg, 0.6f);

	const char *f = getenv("CLOCK_FONT");
	PangoFontDescription *desc =
		pango_font_description_from_string(f && *f ? f : "Sans 10");
	/* "Monospace" has no size, and metrics without one are all zero. */
	if (pango_font_description_get_size(desc) <= 0)
		pango_font_description_set_size(desc, 10 * PANGO_SCALE);
	PangoContext *pctx = pango_font_map_create_context(
		pango_cairo_font_map_get_default());
	PangoFontMetrics *m = pango_context_get_metrics(pctx, desc, NULL);
	int asc = PANGO_PIXELS_CEIL(pango_font_metrics_get_ascent(m));
	int dsc = PANGO_PIXELS_CEIL(pango_font_metrics_get_descent(m));
	pango_font_metrics_unref(m);
	barh = asc + dsc + 10;
	H = 2 * (FRAME + PAD) + 2 * barh + 4;

	plo_date = pango_layout_new(pctx);
	pango_layout_set_single_paragraph_mode(plo_date, TRUE);
	pango_layout_set_font_description(plo_date, desc);
	if (*date_fmt) {
		PangoRectangle dink;
		pango_layout_set_text(plo_date, "0123456789", -1);
		pango_layout_get_pixel_extents(plo_date, &dink, NULL);
		date_ink_y = dink.y;
		date_h = DATE_GAP + dink.height;
	}

	plo = pango_layout_new(pctx);
	pango_layout_set_single_paragraph_mode(plo, TRUE);
	int room = 2 * barh + 4 - 2 * BEV - 2 * INK_PAD - date_h;
	int letters = 0;
	for (const char *c = shown; *c; c++)
		if (!strchr("0123456789 :.,-/", *c))
			letters = 1;
	PangoRectangle ink = { 0 }, line = { 0 };
	for (int px = MAX(room * 2, 1); px >= 1; px--) {
		pango_font_description_set_absolute_size(desc, px * PANGO_SCALE);
		pango_layout_set_font_description(plo, desc);
		pango_layout_set_text(plo, "0123456789", -1);
		pango_layout_get_pixel_extents(plo, &ink, &line);
		if (letters)
			ink = line;
		if (ink.height <= room)
			break;
	}
	ink_y = ink.y;
	ink_h = MIN(ink.height, MAX(room, 0));     /* what does not fit is clipped */
	pango_font_description_free(desc);
	g_object_unref(pctx);
}

/* ---- the time ------------------------------------------------------------ */

/* tm in format f as one line of UTF-8; "" if it makes none. */
static void format_tm(const struct tm *tm, const char *f, char *out, size_t n)
{
	static int warned;
	out[0] = 0;
	if (!tm || !*f)
		return;
	if (!strftime(out, n, f, tm)) {
		/* 0 is also an empty result, as %p in some locales: warn only when
		 * it does not fit. */
		char big[4 * MAX_TEXT];
		if (strftime(big, sizeof big, f, tm) && !warned++)
			fprintf(stderr, "digitalClock: \"%s\" makes more than %d bytes\n",
				f, MAX_TEXT - 1);
		out[0] = 0;
		return;
	}
	if (!g_utf8_validate(out, -1, NULL)) {       /* a locale not in UTF-8 */
		gchar *u = g_locale_to_utf8(out, -1, NULL, NULL, NULL);
		if (!u || strlen(u) >= n)
			out[0] = 0;
		else
			memcpy(out, u, strlen(u) + 1);
		g_free(u);
	}
	/* A layout of one paragraph shows a newline as a glyph. */
	for (char *c = out; *c; c++)
		if ((unsigned char)*c < 0x20)
			*c = ' ';
}

/* Every full second on the wall clock. TFD_TIMER_CANCEL_ON_SET wakes it at once
 * when the clock is set, also after a resume; then it is armed anew. */
static void arm_timer(void)
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	struct itimerspec its = {
		.it_interval = { 1, 0 },
		.it_value = { now.tv_sec + 1, 0 },
	};
	if (timerfd_settime(tfd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &its,
	    NULL) < 0) {
		fprintf(stderr, "digitalClock: cannot set the timer: %s\n",
			strerror(errno));
		exit(1);
	}
}

static void tick(void)
{
	char s[MAX_TEXT], d[MAX_TEXT];
	/* The timer's clock: time() may still say the second before when it fires. */
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	time_t t = ts.tv_sec;
	struct tm tm, *now;
	tzset();        /* the zone may have changed since the last tick */
	now = localtime_r(&t, &tm);
	format_tm(now, fmt, s, sizeof s);
	format_tm(now, date_fmt, d, sizeof d);
	if (strcmp(s, shown) || strcmp(d, shown_date)) {
		memcpy(shown, s, sizeof shown);
		memcpy(shown_date, d, sizeof shown_date);
		dirty = 1;
	}
}

static void timer_fired(void)
{
	uint64_t n;
	if (read(tfd, &n, sizeof n) < 0 && errno == ECANCELED)
		arm_timer();
	tick();
}

/* ---- drawing ------------------------------------------------------------ */

static void set_rgb(uint32_t c)
{
	cairo_set_source_rgb(cr, ((c >> 16) & 0xff) / 255.0,
		((c >> 8) & 0xff) / 255.0, (c & 0xff) / 255.0);
}

static void fill(uint32_t px, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	set_rgb(px);
	cairo_rectangle(cr, x, y, w, h);
	cairo_fill(cr);
}

static void bevel_in(struct rect r, uint32_t t, uint32_t b)
{
	for (int i = 0; i < BEV; i++) {
		fill(t, r.x + i, r.y + i, r.w - 2 * i, 1);
		fill(t, r.x + i, r.y + i, 1, r.h - 2 * i);
		fill(b, r.x + i + 1, r.y + r.h - 1 - i, r.w - 2 * i - 1, 1);
		fill(b, r.x + r.w - 1 - i, r.y + i + 1, 1, r.h - 2 * i - 1);
	}
}

static int textw(PangoLayout *l, const char *s)
{
	int w;
	pango_layout_set_text(l, s, -1);
	pango_layout_get_pixel_size(l, &w, NULL);
	return w;
}

/* As wide as the texts need; it only grows, so that a narrower minute or day name
 * does not move the dock. */
static void layout(void)
{
	text_w = MAX(text_w, MAX(textw(plo, shown), textw(plo_date, shown_date)));
	int want_w = 2 * (FRAME + PAD) + 2 * BEV + 2 * FIELD_PAD + MAX(text_w, ink_h);
	if (want_w != sized_w || H != sized_h) {
		sized_w = want_w;
		sized_h = H;
		if (toplevel) {
			xdg_toplevel_set_min_size(toplevel, want_w, H);
			xdg_toplevel_set_max_size(toplevel, want_w, H);
		}
	}
	W = want_w;
}

/* mwm's thin border: a raised bevel and one line of the face inside it. */
static void draw_frame(void)
{
	uint32_t face = focused ? px_sel : px_bg;
	fill(face, 0, 0, W, FRAME);
	fill(face, 0, H - FRAME, W, FRAME);
	fill(face, 0, 0, FRAME, H);
	fill(face, W - FRAME, 0, FRAME, H);
	bevel_in((struct rect){ 0, 0, W, H }, mix(face, 0xffffff, 0.55f),
		mix(face, 0x000000, 0.5f));
}

static void paint(void)
{
	fill(px_bg, 0, 0, W, H);
	draw_frame();
	struct rect field = { FRAME + PAD, FRAME + PAD, W - 2 * (FRAME + PAD),
		H - 2 * (FRAME + PAD) };
	bevel_in(field, px_bot, px_top);
	cairo_save(cr);
	cairo_rectangle(cr, field.x + BEV, field.y + BEV, field.w - 2 * BEV,
		field.h - 2 * BEV);
	cairo_clip(cr);
	/* Time and date as one block, centred by the ink of their digits: a font's
	 * line has room for accents above and descenders below, which would put
	 * the block too high. */
	int top = field.y + BEV + (field.h - 2 * BEV - ink_h - date_h) / 2;
	if (date_h) {
		int w = textw(plo_date, shown_date);
		set_rgb(xc_dim);
		cairo_move_to(cr, field.x + (field.w - w) / 2,
			top + ink_h + DATE_GAP - date_ink_y);
		pango_cairo_show_layout(cr, plo_date);
	}
	int w = textw(plo, shown);
	set_rgb(xc_fg);
	cairo_move_to(cr, field.x + (field.w - w) / 2, top - ink_y);
	pango_cairo_show_layout(cr, plo);
	cairo_restore(cr);
}

/* 0 if the compositor holds both buffers: try again after a release. */
static int draw(void)
{
	layout();
	struct shmbuf *b = get_buffer(W * scale, H * scale);
	if (!b)
		return 0;
	cr = b->cr;
	cairo_identity_matrix(cr);
	cairo_scale(cr, scale, scale);
	paint();
	cairo_surface_flush(b->cs);
	if (frame)
		wl_callback_destroy(frame);
	frame = wl_surface_frame(surface);
	wl_callback_add_listener(frame, &frame_listener, NULL);
	wl_surface_set_buffer_scale(surface, scale);
	wl_surface_attach(surface, b->wl, 0, 0);
	wl_surface_damage_buffer(surface, 0, 0, b->w, b->h);
	wl_surface_commit(surface);
	b->busy = 1;
	return 1;
}

/* ---- input -------------------------------------------------------------- */

static int mod_on(const char *name)
{
	return xkb_state_mod_name_is_active(kb_state, name,
		XKB_STATE_MODS_EFFECTIVE) > 0;
}

static void key_action(xkb_keysym_t ks)
{
	int ctrl = mod_on(XKB_MOD_NAME_CTRL), alt = mod_on(XKB_MOD_NAME_ALT),
		logo = mod_on(XKB_MOD_NAME_LOGO);
	if (alt || logo)
		return;         /* Alt+Q must not quit */
	if (ctrl && (ks == XKB_KEY_q || ks == XKB_KEY_Q || ks == XKB_KEY_w ||
	    ks == XKB_KEY_W))
		exit(0);
	if (!ctrl && (ks == XKB_KEY_q || ks == XKB_KEY_Q || ks == XKB_KEY_Escape))
		exit(0);
}

static void kb_keymap(void *data, struct wl_keyboard *k, uint32_t format,
	int32_t fd, uint32_t size)
{
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}
	char *s = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (s == MAP_FAILED)
		return;
	struct xkb_keymap *map = xkb_keymap_new_from_string(xkb, s,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(s, size);
	struct xkb_state *state = map ? xkb_state_new(map) : NULL;
	if (!state) {
		xkb_keymap_unref(map);
		return;
	}
	xkb_state_unref(kb_state);
	xkb_keymap_unref(kb_map);
	kb_state = state;
	kb_map = map;
}

static void kb_enter(void *data, struct wl_keyboard *k, uint32_t serial,
	struct wl_surface *s, struct wl_array *keys)
{
	focused = 1;
	dirty = 1;
}

static void kb_leave(void *data, struct wl_keyboard *k, uint32_t serial,
	struct wl_surface *s)
{
	focused = 0;
	dirty = 1;
}

static void kb_key(void *data, struct wl_keyboard *k, uint32_t serial,
	uint32_t time, uint32_t code, uint32_t state)
{
	if (kb_state && state == WL_KEYBOARD_KEY_STATE_PRESSED)
		key_action(xkb_state_key_get_one_sym(kb_state, code + 8));
}

static void kb_modifiers(void *data, struct wl_keyboard *k, uint32_t serial,
	uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
	if (kb_state)
		xkb_state_update_mask(kb_state, depressed, latched, locked, 0, 0,
			group);
}

static void kb_repeat_info(void *data, struct wl_keyboard *k, int32_t rate,
	int32_t delay)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = kb_keymap,
	.enter = kb_enter,
	.leave = kb_leave,
	.key = kb_key,
	.modifiers = kb_modifiers,
	.repeat_info = kb_repeat_info,
};

/* The pointer only for its cursor: without one the last shape stays on it. */
static void ptr_enter(void *data, struct wl_pointer *p, uint32_t serial,
	struct wl_surface *s, wl_fixed_t x, wl_fixed_t y)
{
	ptr_serial = serial;
	ptr_in = 1;
	cursor_load();
	cursor_show();
}

static void ptr_leave(void *data, struct wl_pointer *p, uint32_t serial,
	struct wl_surface *s)
{
	ptr_in = 0;
}

static void ptr_motion(void *data, struct wl_pointer *p, uint32_t time,
	wl_fixed_t x, wl_fixed_t y)
{
}

static void ptr_button(void *data, struct wl_pointer *p, uint32_t serial,
	uint32_t time, uint32_t code, uint32_t state)
{
}

static void ptr_axis(void *data, struct wl_pointer *p, uint32_t time,
	uint32_t axis, wl_fixed_t value)
{
}

static void ptr_frame(void *data, struct wl_pointer *p)
{
}

static void ptr_axis_source(void *data, struct wl_pointer *p, uint32_t source)
{
}

static void ptr_axis_stop(void *data, struct wl_pointer *p, uint32_t time,
	uint32_t axis)
{
}

static void ptr_axis_discrete(void *data, struct wl_pointer *p, uint32_t axis,
	int32_t steps)
{
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = ptr_enter,
	.leave = ptr_leave,
	.motion = ptr_motion,
	.button = ptr_button,
	.axis = ptr_axis,
	.frame = ptr_frame,
	.axis_source = ptr_axis_source,
	.axis_stop = ptr_axis_stop,
	.axis_discrete = ptr_axis_discrete,
};

static void seat_capabilities(void *data, struct wl_seat *s, uint32_t caps)
{
	int want_ptr = caps & WL_SEAT_CAPABILITY_POINTER;
	int want_kb = caps & WL_SEAT_CAPABILITY_KEYBOARD;
	if (want_ptr && !pointer) {
		pointer = wl_seat_get_pointer(s);
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	} else if (!want_ptr && pointer) {
		wl_pointer_release(pointer);
		pointer = NULL;
		ptr_in = 0;
	}
	if (want_kb && !keyboard) {
		keyboard = wl_seat_get_keyboard(s);
		wl_keyboard_add_listener(keyboard, &keyboard_listener, NULL);
	} else if (!want_kb && keyboard) {
		wl_keyboard_release(keyboard);
		keyboard = NULL;
		focused = 0;    /* in case no leave came first */
		dirty = 1;
	}
}

static void seat_name(void *data, struct wl_seat *s, const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

/* ---- window setup ------------------------------------------------------- */

static void wm_ping(void *data, struct xdg_wm_base *b, uint32_t serial)
{
	xdg_wm_base_pong(b, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = { wm_ping };

static void registry_global(void *data, struct wl_registry *r, uint32_t name,
	const char *iface, uint32_t ver)
{
	if (!strcmp(iface, wl_compositor_interface.name)) {
		compositor = wl_registry_bind(r, name, &wl_compositor_interface,
			MIN(ver, 6));
	} else if (!strcmp(iface, wl_shm_interface.name)) {
		shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
	} else if (!strcmp(iface, wl_seat_interface.name) && !seat) {
		seat = wl_registry_bind(r, name, &wl_seat_interface, MIN(ver, 7));
		wl_seat_add_listener(seat, &seat_listener, NULL);
	} else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface,
			MIN(ver, 2));
		xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
	}
}

static void registry_remove(void *data, struct wl_registry *r, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_remove,
};

static void main_xdg_configure(void *data, struct xdg_surface *s,
	uint32_t serial)
{
	xdg_surface_ack_configure(s, serial);
	configured = 1;
	need_commit = 1;
	dirty = 1;
}

static const struct xdg_surface_listener main_xdg_listener = {
	main_xdg_configure,
};

/* Whatever size it is offered, it keeps its own: that of its text. */
static void top_configure(void *data, struct xdg_toplevel *t, int32_t w,
	int32_t h, struct wl_array *states)
{
}

static void top_close(void *data, struct xdg_toplevel *t)
{
	exit(0);
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = top_configure,
	.close = top_close,
};

/* The theme's cursor at the window's scale, loaded again when that changes. */
static void cursor_load(void)
{
	if (cursor_scale == scale)
		return;
	if (cursor_theme)
		wl_cursor_theme_destroy(cursor_theme);
	cursor_theme = NULL;
	cursor_img = NULL;
	cursor_scale = scale;
	const char *s = getenv("XCURSOR_SIZE");
	int size = s && atoi(s) > 0 ? atoi(s) : 24;
	cursor_theme = wl_cursor_theme_load(getenv("XCURSOR_THEME"), size * scale, shm);
	if (!cursor_theme)
		return;
	struct wl_cursor *c = wl_cursor_theme_get_cursor(cursor_theme, "default");
	if (!c)
		c = wl_cursor_theme_get_cursor(cursor_theme, "left_ptr");
	if (c)
		cursor_img = c->images[0];
	if (!cursor_surf)
		cursor_surf = wl_compositor_create_surface(compositor);
}

/* The cursor on the window, if the pointer is there. A theme has only some sizes:
 * Adwaita's 64 at scale 3 would be a buffer the scale doesn't divide, which a
 * compositor may refuse with invalid_size. Then the largest scale below that
 * divides it, and the cursor is a little larger. */
static void cursor_show(void)
{
	if (!pointer || !ptr_in || !cursor_img)
		return;
	int bs = cursor_scale;
	while (bs > 1 && (cursor_img->width % bs || cursor_img->height % bs))
		bs--;
	wl_pointer_set_cursor(pointer, ptr_serial, cursor_surf,
		cursor_img->hotspot_x / bs, cursor_img->hotspot_y / bs);
	wl_surface_set_buffer_scale(cursor_surf, bs);
	wl_surface_attach(cursor_surf, wl_cursor_image_get_buffer(cursor_img), 0, 0);
	wl_surface_damage_buffer(cursor_surf, 0, 0, cursor_img->width,
		cursor_img->height);
	wl_surface_commit(cursor_surf);
}

/* Sharp on a scaled output: wl_compositor 6 says which scale the output it is on
 * has. An older compositor never does, and it stays at 1. */
static void surf_enter(void *data, struct wl_surface *s, struct wl_output *o)
{
}

static void surf_leave(void *data, struct wl_surface *s, struct wl_output *o)
{
}

static void surf_preferred_scale(void *data, struct wl_surface *s, int32_t factor)
{
	if (factor < 1 || factor == scale)
		return;
	scale = factor;
	dirty = 1;
	/* Now, not at the next enter: the pointer may be resting on the window. */
	cursor_load();
	cursor_show();
}

static void surf_preferred_transform(void *data, struct wl_surface *s,
	uint32_t transform)
{
}

static const struct wl_surface_listener surf_listener = {
	.enter = surf_enter,
	.leave = surf_leave,
	.preferred_buffer_scale = surf_preferred_scale,
	.preferred_buffer_transform = surf_preferred_transform,
};

static void setup_window(void)
{
	surface = wl_compositor_create_surface(compositor);
	wl_surface_add_listener(surface, &surf_listener, NULL);
	main_xdg = xdg_wm_base_get_xdg_surface(wm_base, surface);
	xdg_surface_add_listener(main_xdg, &main_xdg_listener, NULL);
	toplevel = xdg_surface_get_toplevel(main_xdg);
	xdg_toplevel_add_listener(toplevel, &toplevel_listener, NULL);
	xdg_toplevel_set_app_id(toplevel, "digitalClock");
	xdg_toplevel_set_title(toplevel, "digitalClock");
	sized_w = sized_h = 0;
	layout();
	/* No xdg-decoration: a compositor that offers it would add a title bar. */
}

/* ---- main loop ---------------------------------------------------------- */

static void lost_connection(void)
{
	fprintf(stderr, "digitalClock: lost the connection to the compositor: %s\n",
		strerror(wl_display_get_error(dpy)));
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc > 1) {
		if (!strcmp(argv[1], "-v")) {
			printf("digitalClock %s\n", DIGITALCLOCK_VERSION);
			return 0;
		}
		int help = !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help");
		fprintf(stderr, "usage: digitalClock [-v]\n"
			"Environment: CLOCK_FORMAT (default \"%%H:%%M\"), CLOCK_DATE "
			"(default \"%%a %%d.%%m.\", empty for none),\n"
			"CLOCK_FONT (default \"Sans 10\"), MWC_INACTIVE, MWC_ACTIVE.\n");
		return help ? 0 : 1;
	}
	setlocale(LC_ALL, "");
	const char *f = getenv("CLOCK_FORMAT");
	if (f && *f)
		fmt = f;
	const char *d = getenv("CLOCK_DATE");
	if (d)
		date_fmt = d;   /* set but empty: no date */

	if (!(dpy = wl_display_connect(NULL))) {
		fprintf(stderr, "digitalClock: cannot connect to a Wayland compositor\n");
		return 1;
	}
	if (!(xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS))) {
		fprintf(stderr, "digitalClock: cannot set up xkbcommon\n");
		return 1;
	}
	tfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (tfd < 0) {
		fprintf(stderr, "digitalClock: no timer: %s\n", strerror(errno));
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(dpy);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	if (wl_display_roundtrip(dpy) < 0)
		lost_connection();
	if (!compositor || !shm || !wm_base) {
		fprintf(stderr, "digitalClock: the compositor lacks wl_compositor, wl_shm "
			"or xdg_wm_base\n");
		return 1;
	}
	cursor_load();
	tick();         /* theme_init looks at the text */
	theme_init();
	arm_timer();
	setup_window();
	wl_surface_commit(surface);

	for (;;) {
		if (wl_display_dispatch_pending(dpy) < 0)
			lost_connection();
		/* A frame at a time, except that a configure wants its commit even if
		 * the window is hidden and gets no frames. */
		if (dirty && configured && (!frame || need_commit) && draw()) {
			dirty = 0;
			need_commit = 0;
		}
		if (wl_display_prepare_read(dpy) != 0)
			continue;       /* events arrived in the meantime */
		struct pollfd pfd[2] = {
			{ wl_display_get_fd(dpy), POLLIN, 0 },
			{ tfd, POLLIN, 0 },
		};
		if (wl_display_flush(dpy) < 0) {
			/* EPIPE: the compositor has closed, but may have said why first.
			 * Reading that error makes libwayland report it. */
			if (errno != EAGAIN && errno != EPIPE) {
				wl_display_cancel_read(dpy);
				lost_connection();
			}
			if (errno == EAGAIN)
				pfd[0].events |= POLLOUT;
		}
		if (poll(pfd, 2, -1) < 0 && errno != EINTR) {
			wl_display_cancel_read(dpy);
			lost_connection();
		}
		if (pfd[0].revents & (POLLIN | POLLERR | POLLHUP)) {
			if (wl_display_read_events(dpy) < 0)
				lost_connection();
		} else {
			wl_display_cancel_read(dpy);
		}
		if (pfd[1].revents & POLLIN)
			timer_fired();
	}
}
