/*
 * digitalClock - the time in digits on Wayland, in the Motif look of mwc
 * (https://github.com/reygnn/mwc), mfm, pulseControl and deskControl.
 *
 * The window, the seat, the cursor, the colours and the bevels come from mtk
 * (mtk/), the time and its timer from timefmt.c. The time sits in a sunken field,
 * its digits as high as the field allows, the date below it in the font of the
 * sister tools' labels, dimmed. The font is fixed, DejaVu Sans 10, as in
 * pulseControl, deskControl and osDisplay, so the window is as high as theirs and
 * the four line up in mwc's dock. Its colours are mwc's: MWC_INACTIVE /
 * MWC_ACTIVE from the environment, else Mwm*background and Mwm*activeBackground
 * from ~/.config/mwc/mwmrc, else the Motif defaults. The text is Mwm*foreground,
 * else black or white as Motif picks it, as mwc's titles.
 *
 * It asks for no server-side decoration and draws only mwm's thin border, which
 * takes the active colour while it has the keyboard, as mwc's frames do.
 *
 * A realtime timerfd wakes it at every full second, and at once when the clock is
 * set; it redraws only when a text changes.
 *
 * Keys:   Esc / q / Ctrl+Q / Ctrl+W  quit
 *
 * In mwc it belongs in the dock: Mwm*digitalClock*dock: true.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "mtk.h"
#include "timefmt.h"
#include "version.h"

#define PAD 6
#define FRAME MTK_BORDER        /* mwm's border without resize handles */
#define FIELD_PAD 10    /* left and right of the digits */
#define INK_PAD 5       /* above and below them */
#define DATE_GAP 5      /* between the time's ink and the date's */
#ifndef MAX            /* glib has them too */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* ---- state -------------------------------------------------------------- */

static const char *fmt = "%H:%M";
static const char *date_fmt = "%a %d.%m.";     /* "": no date line */
static char shown[MAX_TEXT];    /* the time in the field */
static char shown_date[MAX_TEXT];
static int tfd = -1;

static struct mtk *m;
static struct mtk_window *win;
static struct mtk_theme theme;
static uint32_t xc_fg, xc_dim;  /* the text, and the date's */
static PangoLayout *plo;        /* the digits */
static PangoLayout *plo_date;   /* the date, in the dock's font */
static int H;
static int barh;                /* a button's height in the sister tools */
static int date_h;              /* the date's digits and the gap, 0 without one */
static int ink_y, ink_h;        /* where the time's ink can be in its line */
static int date_ink_y;          /* ... and the date's, by its digits too */
static int text_w;              /* the widest text so far: the field never shrinks */

/* The height comes from the font as in pulseControl and deskControl: two of their
 * rows and the gap between. The time is in the same font, as large as that height
 * allows beside the date line. A time of digits is measured by their ink; one with
 * letters by the whole line, room for accents and descenders included. */
static void theme_init(void)
{
	mtk_theme_load(&theme);
	/* The text is always on the background: the active colour is only the
	 * border's. */
	xc_fg = theme.inactive.fg;
	xc_dim = theme.dim;

	PangoFontDescription *desc = mtk_dock_font("digitalClock");
	plo_date = mtk_layout_new(desc);
	PangoContext *pctx = pango_layout_get_context(plo_date);
	PangoFontMetrics *fm = pango_context_get_metrics(pctx, desc, NULL);
	int asc = PANGO_PIXELS_CEIL(pango_font_metrics_get_ascent(fm));
	int dsc = PANGO_PIXELS_CEIL(pango_font_metrics_get_descent(fm));
	pango_font_metrics_unref(fm);
	barh = asc + dsc + 10;
	H = 2 * (FRAME + PAD) + 2 * barh + 4;

	if (*date_fmt) {
		PangoRectangle dink;
		pango_layout_set_text(plo_date, "0123456789", -1);
		pango_layout_get_pixel_extents(plo_date, &dink, NULL);
		date_ink_y = dink.y;
		date_h = DATE_GAP + dink.height;
	}

	plo = mtk_layout_new(NULL);
	int room = 2 * barh + 4 - 2 * MTK_BEVEL - 2 * INK_PAD - date_h;
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
}

/* As wide as the texts need; it only grows, so that a narrower minute or day name
 * does not move the dock. */
static int width(void)
{
	text_w = MAX(text_w, MAX(mtk_text_width(plo, shown),
		mtk_text_width(plo_date, shown_date)));
	return 2 * (FRAME + PAD) + 2 * MTK_BEVEL + 2 * FIELD_PAD + MAX(text_w, ink_h);
}

/* ---- the time ------------------------------------------------------------ */

static void arm_or_die(void)
{
	if (arm_timer(tfd) < 0) {
		fprintf(stderr, "digitalClock: cannot set the timer: %s\n",
			strerror(errno));
		exit(1);
	}
}

/* Too long, it says so once per *warned. */
static void show_tm(const struct tm *tm, const char *f, char *out, int *warned)
{
	if (format_tm(tm, f, out) && !*warned) {
		*warned = 1;
		fprintf(stderr, "digitalClock: \"%s\" makes more than %d bytes\n", f,
			MAX_TEXT - 1);
	}
}

/* 1 if a text changed. */
static int tick(void)
{
	static int warned_time, warned_date;
	char s[MAX_TEXT], d[MAX_TEXT];
	/* The timer's clock: time() may still say the second before when it fires. */
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	time_t t = ts.tv_sec;
	struct tm tm, *now;
	tzset();        /* the zone may have changed since the last tick */
	now = localtime_r(&t, &tm);
	show_tm(now, fmt, s, &warned_time);
	show_tm(now, date_fmt, d, &warned_date);
	if (!strcmp(s, shown) && !strcmp(d, shown_date))
		return 0;
	memcpy(shown, s, sizeof shown);
	memcpy(shown_date, d, sizeof shown_date);
	return 1;
}

static void timer_fired(struct mtk *mm, int fd, short revents, void *data)
{
	uint64_t n;
	if (read(tfd, &n, sizeof n) < 0 && errno == ECANCELED)
		arm_or_die();
	if (tick()) {
		mtk_window_set_size(win, width(), H);
		mtk_window_redraw(win);
	}
}

/* ---- the window ---------------------------------------------------------- */

static void paint(struct mtk_window *w, cairo_t *cr, int W, int h, void *data)
{
	mtk_fill(cr, theme.inactive.bg, 0, 0, W, h);
	mtk_border(cr, W, h, mtk_window_focused(w) ? &theme.active : &theme.inactive);
	int fx = FRAME + PAD, fy = FRAME + PAD, fw = W - 2 * (FRAME + PAD),
		fh = h - 2 * (FRAME + PAD);
	mtk_shadow(cr, fx, fy, fw, fh, MTK_BEVEL, &theme.inactive, 1);
	cairo_rectangle(cr, fx + MTK_BEVEL, fy + MTK_BEVEL, fw - 2 * MTK_BEVEL,
		fh - 2 * MTK_BEVEL);
	cairo_clip(cr);
	/* Time and date as one block, centred by the ink of their digits: a font's
	 * line has room for accents above and descenders below, which would put
	 * the block too high. */
	int top = fy + MTK_BEVEL + (fh - 2 * MTK_BEVEL - ink_h - date_h) / 2;
	if (date_h)
		mtk_text(cr, plo_date, shown_date, xc_dim,
			fx + (fw - mtk_text_width(plo_date, shown_date)) / 2,
			top + ink_h + DATE_GAP - date_ink_y);
	mtk_text(cr, plo, shown, xc_fg, fx + (fw - mtk_text_width(plo, shown)) / 2,
		top - ink_y);
}

static void focus(struct mtk_window *w, int in, void *data)
{
	mtk_window_redraw(w);
}

static void key(struct mtk_window *w, const struct mtk_key *k, void *data)
{
	if (mtk_key_quits(k->latin, k->mods))
		mtk_quit(m, 0);
}

/* No key does anything held. */
static int key_repeats(struct mtk_window *w, const struct mtk_key *k, void *data)
{
	return 0;
}

static const struct mtk_window_listener listener = {
	.paint = paint,
	.focus = focus,
	.key = key,
	.key_repeats = key_repeats,
};

int main(int argc, char **argv)
{
	if (argc > 1) {
		int version = argc == 2 && !strcmp(argv[1], "-v");
		/* Asked for, the usage goes to stdout, as mwc's. */
		int help = argc == 2 && !strcmp(argv[1], "-h");
		if (version)
			printf("digitalClock %s\n", DIGITALCLOCK_VERSION);
		else
			fprintf(help ? stdout : stderr, "usage: digitalClock [-v]\n"
				"Environment: CLOCK_FORMAT (default \"%%H:%%M\"), "
				"CLOCK_DATE (default \"%%a %%d.%%m.\", empty for none),\n"
				"MWC_INACTIVE, MWC_ACTIVE.\n");
		/* A full disk or a closed stdout is no success: fully buffered, the
		 * write only fails here. */
		if (fflush(stdout) == EOF || ferror(stdout)) {
			fprintf(stderr, "digitalClock: stdout: %s\n", strerror(errno));
			return 1;
		}
		return version || help ? 0 : 1;
	}
	setlocale(LC_ALL, "");
	const char *f = getenv("CLOCK_FORMAT");
	if (f && *f)
		fmt = f;
	const char *d = getenv("CLOCK_DATE");
	if (d)
		date_fmt = d;   /* set but empty: no date */

	if (!(m = mtk_open(&(struct mtk_config){ .name = "digitalClock" })))
		return 1;
	tfd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	if (tfd < 0) {
		fprintf(stderr, "digitalClock: no timer: %s\n", strerror(errno));
		return 1;
	}
	tick();         /* theme_init looks at the text */
	theme_init();
	arm_or_die();
	win = mtk_window_new(m, &(struct mtk_window_config){
		.width = width(), .height = H, .fixed = 1,
	}, &listener, NULL);
	mtk_watch_fd(m, tfd, POLLIN, timer_fired, NULL);
	int status = mtk_run(m);
	g_object_unref(plo);
	g_object_unref(plo_date);
	mtk_close(m);
	close(tfd);
	return status;
}
