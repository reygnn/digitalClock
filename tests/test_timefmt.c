/* test_timefmt.c - the time as one line, and its timer: make check */
#define _GNU_SOURCE
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "timefmt.h"

static int failures, checks;

#define CHECK(c) do { checks++; if (!(c)) { failures++; \
	fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* Friday, 20 November 2026, 14:05:09 */
static const struct tm fri = {
	.tm_year = 126, .tm_mon = 10, .tm_mday = 20, .tm_wday = 5, .tm_yday = 323,
	.tm_hour = 14, .tm_min = 5, .tm_sec = 9,
};

static int fmt(const char *f, char *out)
{
	memset(out, '?', MAX_TEXT);
	return format_tm(&fri, f, out);
}

static void test_format(void)
{
	char out[MAX_TEXT], f[MAX_TEXT + 8];
	CHECK(fmt("%H:%M", out) == 0 && !strcmp(out, "14:05"));
	CHECK(fmt("%a %d.%m.", out) == 0 && !strcmp(out, "Fri 20.11."));
	CHECK(fmt("", out) == 0 && !strcmp(out, ""));
	memset(out, '?', MAX_TEXT);
	CHECK(format_tm(NULL, "%H", out) == 0 && !strcmp(out, ""));
	/* one line: a newline, a tab and other control characters are spaces */
	CHECK(fmt("%H\n%M%t%S\r", out) == 0 && !strcmp(out, "14 05 09 "));
	/* MAX_TEXT - 1 bytes fit, one more does not */
	memset(f, 'a', MAX_TEXT - 1);
	f[MAX_TEXT - 1] = 0;
	CHECK(fmt(f, out) == 0 && strlen(out) == MAX_TEXT - 1);
	f[MAX_TEXT - 1] = 'a';
	f[MAX_TEXT] = 0;
	CHECK(fmt(f, out) == 1 && !strcmp(out, ""));
	CHECK(fmt("%c%c%c%c%c%c%c%c%c%c%c%c", out) == 1 && !strcmp(out, ""));
}

/* The wall clock as a fake around the real one: from call number step_at on, it
 * is step seconds behind, as if it had been set back just then, and back more
 * seconds with every call after. A step cannot be made real without
 * CAP_SYS_TIME. */
int __real_clock_gettime(clockid_t c, struct timespec *ts);
static int calls, step_at = -1;
static time_t step, back;

int __wrap_clock_gettime(clockid_t c, struct timespec *ts)
{
	int r = __real_clock_gettime(c, ts);
	if (c == CLOCK_REALTIME && step_at >= 0) {
		if (calls >= step_at)
			ts->tv_sec -= step + back * (calls - step_at);
		calls++;
	}
	return r;
}

static void test_timer(void)
{
	int fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	struct itimerspec its;
	CHECK(fd >= 0 && arm_timer(fd) == 0);
	CHECK(timerfd_gettime(fd, &its) == 0);
	/* the next full second, then every second */
	CHECK(its.it_interval.tv_sec == 1 && its.it_interval.tv_nsec == 0);
	CHECK(its.it_value.tv_sec == 0 && its.it_value.tv_nsec > 0);
	close(fd);
	CHECK(arm_timer(fd) == -1);

	/* Set back an hour between reading the clock and arming: armed on the
	 * second after the new time, it is due at once, and an hour of seconds
	 * have passed for the real clock. Not still an hour away. */
	fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	calls = 0;
	step_at = 1;
	step = 3600;
	CHECK(arm_timer(fd) == 0);
	uint64_t n = 0;
	CHECK(read(fd, &n, sizeof n) == sizeof n && n > 3000);
	/* A clock that goes back on every reading: it gives up and stays armed. */
	calls = 0;
	step_at = 0;
	step = 0;
	back = 10;
	CHECK(arm_timer(fd) == 0 && calls == 16);
	CHECK(timerfd_gettime(fd, &its) == 0 && its.it_interval.tv_sec == 1);
	step_at = -1;
	close(fd);
}

int main(void)
{
	setlocale(LC_ALL, "C");
	test_format();
	test_timer();
	printf("test_timefmt: %d/%d\n", checks - failures, checks);
	return failures != 0;
}
