/* timefmt.h - the time as digitalClock shows it, and its timer, without Wayland.
 * test_timefmt checks them. */
#ifndef TIMEFMT_H
#define TIMEFMT_H

#include <time.h>

#define MAX_TEXT 256

/* tm in format f as one line of UTF-8 in out[MAX_TEXT], a newline or other control
 * character as a space; "" if it makes none. 1 if the result does not fit, else
 * 0: an empty result, as %p in some locales, is no error. */
int format_tm(const struct tm *tm, const char *f, char *out);

/* The timerfd fd armed for every full second on the wall clock. With
 * TFD_TIMER_CANCEL_ON_SET it wakes at once when the clock is set, also after a
 * resume; then it is to be armed anew. -1 with errno on failure. */
int arm_timer(int fd);

#endif
