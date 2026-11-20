/* theme.h - the colours of digitalClock without Wayland: mwc's rules for the
 * bevels, the text and the date, colour names, and the mwmrc. test_theme checks
 * them. */
#ifndef THEME_H
#define THEME_H

#include <stddef.h>
#include <stdint.h>

/* rgb moved towards to by f, per channel, rounded. */
uint32_t mix(uint32_t rgb, uint32_t to, float f);

/* WCAG relative luminance of 0xrrggbb. */
float luminance(uint32_t c);

/* Black or white, whichever has the higher WCAG contrast on bg. */
uint32_t contrast(uint32_t bg);

/* The date's colour: bg moved towards fg as far as WCAG AA's 4.5:1 for small text
 * needs, at least 12/20 of the way; fg itself if even that falls short. */
uint32_t dimmed(uint32_t bg, uint32_t fg);

/* What mwc reads: "#rrggbb", names like "gold", and rgb:r/g/b with 1 to 4 hex
 * digits per channel. 0 for NULL, "" and anything else; *out is then untouched. */
int parse_color(const char *s, uint32_t *out);

/* The value of the last global "Mwm*key: value" line of $XDG_CONFIG_HOME/mwc/mwmrc,
 * else ~/.config/mwc/mwmrc, in val[n]; NULL if there is none, or it is too long. */
char *mwmrc_get(const char *key, char *val, size_t n);

#endif
