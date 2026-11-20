/* test_util.c - the cursor's size and scale, and the keys that quit: make check */
#include <stdio.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "util.h"

static int failures, checks;

#define CHECK(c) do { checks++; if (!(c)) { failures++; \
	fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void test_cursor_size(void)
{
	CHECK(cursor_size(NULL) == 24);
	CHECK(cursor_size("32") == 32);
	CHECK(cursor_size("1") == 1);
	CHECK(cursor_size("256") == 256);
	CHECK(cursor_size("257") == 256);
	CHECK(cursor_size("99999999999999999999999") == 256);  /* past LONG_MAX */
	CHECK(cursor_size(" 32") == 32);                       /* as strtol reads it */
	static const char *bad[] = { "", "0", "-5", "abc", "32x", "32 ", "3.5" };
	for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		checks++;
		if (cursor_size(bad[i]) != 24) {
			failures++;
			fprintf(stderr, "FAIL: XCURSOR_SIZE \"%s\" taken\n", bad[i]);
		}
	}
}

static int fit(int w, int h, int s, int vp, int bs, int vw, int vh)
{
	int b = -7, x = -7, y = -7;
	cursor_fit(w, h, s, vp, &b, &x, &y);
	return b == bs && x == vw && y == vh;
}

static void test_cursor_fit(void)
{
	/* a size the scale divides: that scale, no viewport size */
	CHECK(fit(48, 48, 2, 1, 2, -1, -1));
	CHECK(fit(48, 48, 2, 0, 2, -1, -1));
	CHECK(fit(24, 24, 1, 0, 1, -1, -1));
	/* Adwaita's 30 at scale 4: the viewport shows it at 30/4, rounded */
	CHECK(fit(30, 30, 4, 1, 1, 8, 8));
	CHECK(fit(30, 28, 4, 1, 1, 8, 7));
	CHECK(fit(1, 1, 4, 1, 1, 1, 1));                /* never 0 */
	/* without one, the largest scale below that divides it */
	CHECK(fit(30, 30, 4, 0, 3, -1, -1));
	CHECK(fit(30, 30, 8, 0, 6, -1, -1));
	CHECK(fit(30, 20, 4, 0, 2, -1, -1));
	CHECK(fit(7, 7, 4, 0, 1, -1, -1));
}

static void test_keys(void)
{
	CHECK(key_quits(XKB_KEY_q, 0, 0, 0));
	CHECK(key_quits(XKB_KEY_Q, 0, 0, 0));
	CHECK(key_quits(XKB_KEY_Escape, 0, 0, 0));
	CHECK(!key_quits(XKB_KEY_w, 0, 0, 0));
	CHECK(!key_quits(XKB_KEY_a, 0, 0, 0));
	CHECK(key_quits(XKB_KEY_q, 1, 0, 0));
	CHECK(key_quits(XKB_KEY_Q, 1, 0, 0));
	CHECK(key_quits(XKB_KEY_w, 1, 0, 0));
	CHECK(key_quits(XKB_KEY_W, 1, 0, 0));
	CHECK(!key_quits(XKB_KEY_Escape, 1, 0, 0));
	CHECK(!key_quits(XKB_KEY_a, 1, 0, 0));
	/* with Alt or Super held none */
	CHECK(!key_quits(XKB_KEY_q, 0, 1, 0));
	CHECK(!key_quits(XKB_KEY_q, 0, 0, 1));
	CHECK(!key_quits(XKB_KEY_Escape, 0, 1, 0));
	CHECK(!key_quits(XKB_KEY_w, 1, 1, 0));
	CHECK(!key_quits(XKB_KEY_q, 1, 0, 1));
}

int main(void)
{
	test_cursor_size();
	test_cursor_fit();
	test_keys();
	printf("test_util: %d/%d\n", checks - failures, checks);
	return failures != 0;
}
