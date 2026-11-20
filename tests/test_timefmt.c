/* test_timefmt.c - the time as one line, and its timer: make check */
#define _GNU_SOURCE
#include <locale.h>
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
}

int main(void)
{
	setlocale(LC_ALL, "C");
	test_format();
	test_timer();
	printf("test_timefmt: %d/%d\n", checks - failures, checks);
	return failures != 0;
}
