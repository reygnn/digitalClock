/* test_theme.c - mwc's colour rules, colour names and the mwmrc: make check */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "theme.h"

static int failures, checks;

#define CHECK(c) do { checks++; if (!(c)) { failures++; \
	fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_mix(void)
{
	/* mwc's bevels on its default background */
	CHECK(mix(0xaeb2c3, 0xffffff, 0.55f) == 0xdbdce4);
	CHECK(mix(0xaeb2c3, 0x000000, 0.5f) == 0x575962);
	CHECK(mix(0x123456, 0xabcdef, 0) == 0x123456);
	CHECK(mix(0x123456, 0xabcdef, 1) == 0xabcdef);
}

static void test_contrast(void)
{
	CHECK(contrast(0xaeb2c3) == 0x000000);
	CHECK(contrast(0x7387ad) == 0x000000);  /* a mid-tone gets black */
	CHECK(contrast(0x202020) == 0xffffff);
	CHECK(contrast(0x000000) == 0xffffff);
	CHECK(contrast(0xffffff) == 0x000000);
	/* the two are equal at a grey of about 0x76 */
	CHECK(contrast(0x777777) == 0x000000);
	CHECK(contrast(0x737373) == 0xffffff);
	CHECK(luminance(0x000000) == 0 && luminance(0xffffff) > 0.999f);
}

static void test_dimmed(void)
{
	/* the date's colours that tests/ui.sh sees */
	CHECK(dimmed(0xaeb2c3, 0x000000) == 0x3d3e44);
	CHECK(dimmed(0x7387ad, 0x000000) == 0x171b23);
	/* 4.5:1 out of reach: fg itself */
	CHECK(dimmed(0x808080, 0x606060) == 0x606060);
}

static int color(const char *s, uint32_t *c)
{
	*c = 0xdeadbe;
	return parse_color(s, c);
}

static void test_parse_color(void)
{
	uint32_t c;
	CHECK(color("#ff8000", &c) && c == 0xff8000);
	CHECK(color("gold", &c) && c == 0xffd700);
	CHECK(color("rgb:f/80/ff00", &c) && c == 0xff80fe);
	CHECK(color("RGB:0/0/0", &c) && c == 0x000000);
	CHECK(color("rgb:123/fff/0", &c) && c == 0x12ff00);
	CHECK(color("rgb:aB/Cd/eF", &c) && c == 0xabcdef);
	static const char *bad[] = {
		"", "rgb:", "rgb:1/2", "rgb:1/2/3/4", "rgb:1/2/3 ", "rgb: 1/2/3",
		"rgb:+1/2/3", "rgb:0x1/2/3", "rgb:12345/1/1", "rgb:1//3", "rgb:g/0/0",
		"no such colour", "#ff80",
	};
	for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		checks++;
		if (color(bad[i], &c) || c != 0xdeadbe) {
			failures++;
			fprintf(stderr, "FAIL: \"%s\" taken as a colour\n", bad[i]);
		}
	}
	CHECK(!color(NULL, &c) && c == 0xdeadbe);
}

static char dir[] = "/tmp/test_theme.XXXXXX";

/* text as base/mwc/mwmrc */
static void rc(const char *base, const char *text)
{
	char path[256];
	snprintf(path, sizeof path, "%s/mwc", base);
	mkdir(path, 0700);
	snprintf(path, sizeof path, "%s/mwc/mwmrc", base);
	FILE *f = fopen(path, "w");
	if (!f || fputs(text, f) == EOF || fclose(f)) {
		perror(path);
		exit(1);
	}
}

static int got(const char *key, const char *want)
{
	char v[16];
	const char *r = mwmrc_get(key, v, sizeof v);
	return want ? r == v && !strcmp(v, want) : !r;
}

static void test_mwmrc(void)
{
	setenv("XDG_CONFIG_HOME", dir, 1);
	CHECK(got("background", NULL));         /* no file */
	rc(dir, "Mwm*background: #111111\n"
	   "  mwm.BACKGROUND \t:  #222222  \n"  /* any case, "." or "*", blanks */
	   "Mwm*backgroundX: #333333\n"
	   "Mwm*foo*background: #444444\n"      /* not global */
	   "Xwm*background: #555555\n"
	   "Mwm*background #666666\n"           /* no colon */
	   "Mwm*foreground:\n"
	   "Mwm*activeBackground: 0123456789abcdef\n"
	   "Mwm*long: 0123456789abcdef\n"
	   "Mwm*long: short\n"
	   "Mwm*short: short\n"
	   "Mwm*short: 0123456789abcdef\n");
	CHECK(got("background", "#222222"));    /* the last line wins */
	CHECK(got("Background", "#222222"));
	CHECK(got("foreground", ""));
	CHECK(got("activeBackground", NULL));   /* too long: none, not a cut one */
	CHECK(got("long", "short"));
	CHECK(got("short", NULL));              /* not the one before it either */
	CHECK(got("foo*background", "#444444"));
	CHECK(got("backgroun", NULL));

	/* A line longer than any buffer is read whole: a comment of setting-like
	 * pieces 32 bytes apart, so that a reader cutting it into pieces of most
	 * sizes gets one starting with "Mwm*background:". */
	enum { UNIT = 32, UNITS = 300 };
	char *big = malloc(UNIT * (UNITS + 2));
	int at = sprintf(big, "Mwm*background: #777777\n");
	at += sprintf(big + at, "%-*s", UNIT, "!");
	for (int i = 0; i < UNITS; i++)
		at += sprintf(big + at, "%-*s", UNIT, "Mwm*background: #999999");
	strcpy(big + at, "\n");
	rc(dir, big);
	free(big);
	CHECK(got("background", "#777777"));

	/* Without XDG_CONFIG_HOME it is ~/.config; without HOME there is none. */
	unsetenv("XDG_CONFIG_HOME");
	setenv("HOME", "/nonexistent", 1);
	CHECK(got("background", NULL));
	char home[64], cfg[128];
	snprintf(home, sizeof home, "%s/home", dir);
	snprintf(cfg, sizeof cfg, "%s/.config", home);
	mkdir(home, 0700);
	mkdir(cfg, 0700);
	rc(cfg, "Mwm*background: #888888\n");
	setenv("HOME", home, 1);
	CHECK(got("background", "#888888"));
	setenv("XDG_CONFIG_HOME", "", 1);       /* empty is unset */
	CHECK(got("background", "#888888"));
	unsetenv("HOME");
	unsetenv("XDG_CONFIG_HOME");
	CHECK(got("background", NULL));
}

int main(void)
{
	if (!mkdtemp(dir)) {
		perror("mkdtemp");
		return 1;
	}
	test_mix();
	test_contrast();
	test_dimmed();
	test_parse_color();
	test_mwmrc();
	char cmd[64];
	snprintf(cmd, sizeof cmd, "rm -r %s", dir);
	if (system(cmd))
		fprintf(stderr, "cannot remove %s\n", dir);
	printf("test_theme: %d/%d\n", checks - failures, checks);
	return failures != 0;
}
