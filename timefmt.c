/* timefmt.c - the time as digitalClock shows it, and its timer, without
 * Wayland; see timefmt.h. */
#define _GNU_SOURCE
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <time.h>

#include "timefmt.h"

int format_tm(const struct tm *tm, const char *f, char *out)
{
	size_t n = MAX_TEXT;
	out[0] = 0;
	if (!tm || !*f)
		return 0;
	if (!strftime(out, n, f, tm)) {
		/* 0 is also an empty result, as %p in some locales. With one byte in
		 * front, only a result that does not fit makes 0. */
		size_t fl = strlen(f);
		char *xf = malloc(fl + 2), big[MAX_TEXT + 1];
		int too_long = 0;
		if (xf) {
			xf[0] = 'x';
			memcpy(xf + 1, f, fl + 1);
			too_long = !strftime(big, sizeof big, xf, tm);
			free(xf);
		}
		out[0] = 0;
		return too_long;
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
	return 0;
}

int arm_timer(int fd)
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	struct itimerspec its = {
		.it_interval = { 1, 0 },
		.it_value = { now.tv_sec + 1, 0 },
	};
	return timerfd_settime(fd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &its,
		NULL);
}
