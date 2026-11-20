/* theme.c - the colours of digitalClock without Wayland; see theme.h. */
#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <pango/pango.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "theme.h"

/* ---- colours (same rules as mwm's draw.c) ------------------------------- */

uint32_t mix(uint32_t rgb, uint32_t to, float f)
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
float luminance(uint32_t c)
{
	return 0.2126f * linear((c >> 16) & 0xff) + 0.7152f * linear((c >> 8) & 0xff) +
		0.0722f * linear(c & 0xff);
}

uint32_t contrast(uint32_t bg)
{
	float l = luminance(bg);
	return (l + 0.05f) * (l + 0.05f) > 1.05f * 0.05f ? 0x000000 : 0xffffff;
}

/* The date's colour: dimmed towards fg as the sisters' labels are, but no further
 * than WCAG AA's 4.5:1 for small text allows; fg itself if even that falls short. */
uint32_t dimmed(uint32_t bg, uint32_t fg)
{
	float lb = luminance(bg) + 0.05f;
	for (int k = 12; k < 20; k++) {
		uint32_t c = mix(bg, fg, k / 20.0f);
		float lc = luminance(c) + 0.05f;
		if ((lb > lc ? lb / lc : lc / lb) >= 4.5f)
			return c;
	}
	return fg;
}

/* What mwc reads: "#rrggbb", names like "gold", and rgb:r/g/b with 1 to 4 hex
 * digits per channel. */
int parse_color(const char *s, uint32_t *out)
{
	PangoColor c;
	if (!s || !*s)
		return 0;
	if (!strncasecmp(s, "rgb:", 4)) {
		uint32_t rgb = 0;
		s += 4;
		for (int i = 0; i < 3; i++) {
			/* Only hex digits, as mwc: strtoul alone also takes spaces,
			 * signs and 0x. */
			int digits = strspn(s, "0123456789abcdefABCDEF");
			if (digits < 1 || digits > 4 || s[digits] != (i < 2 ? '/' : '\0'))
				return 0;
			unsigned long v = strtoul(s, NULL, 16);
			unsigned long max = (1ul << (4 * digits)) - 1;
			rgb = rgb << 8 | (v * 255 + max / 2) / max;
			s += digits + 1;
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
char *mwmrc_get(const char *key, char *val, size_t n)
{
	char path[PATH_MAX];
	const char *home = getenv("HOME"), *xdg = getenv("XDG_CONFIG_HOME");
	int len;
	if (xdg && *xdg)
		len = snprintf(path, sizeof path, "%s/mwc/mwmrc", xdg);
	else if (home && *home)
		len = snprintf(path, sizeof path, "%s/.config/mwc/mwmrc", home);
	else
		return NULL;    /* as mwc: no HOME, no mwmrc, not /.config */
	if (len < 0 || (size_t)len >= sizeof path)
		return NULL;
	FILE *f = fopen(path, "r");
	if (!f)
		return NULL;
	char *found = NULL, *line = NULL;       /* whole lines, however long */
	size_t kl = strlen(key), cap = 0;
	while (getline(&line, &cap, f) > 0) {
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
		/* The last line wins, as in mwc: one too long to keep is no value,
		 * not a cut one and not the line before it. */
		found = NULL;
		if ((size_t)(e - p) < n) {
			memcpy(val, p, e - p + 1);
			found = val;
		}
	}
	free(line);
	fclose(f);
	return found;
}
