# digitalClock - the time in digits on Wayland (mwc look)

PREFIX ?= /usr/local
PKG_CONFIG ?= pkg-config
MTK_DIR = mtk

all: digitalClock

include $(MTK_DIR)/mtk.mk

# CPPFLAGS, CFLAGS, LDFLAGS and LDLIBS stay the user's, also on the command line:
# `make CFLAGS=-O0` must not lose the include paths.
CFLAGS ?= -O2 -g

MODS = timefmt.c
MODS_H = timefmt.h

digitalClock: clock.c $(MODS) $(MODS_H) version.h $(MTK_DEPS)
	$(CC) $(MTK_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ clock.c $(MODS) \
		$(MTK_SRC) $(MTK_LIBS) $(LDLIBS)

# The modules without Wayland, each under ASan and UBSan. mtk's are tested in mtk.
UNIT_CFLAGS = -std=c11 -Wall -Wextra -g -O1 -fsanitize=address,undefined \
	-fno-sanitize-recover=all
UNITS = build/test_timefmt

# clock_gettime wrapped: the test steps the wall clock, which needs root for real.
build/test_timefmt: tests/test_timefmt.c timefmt.c timefmt.h | build
	$(CC) $(UNIT_CFLAGS) -I. $(shell $(PKG_CONFIG) --cflags glib-2.0) -o $@ \
		tests/test_timefmt.c timefmt.c $(shell $(PKG_CONFIG) --libs glib-2.0) \
		-Wl,--wrap=clock_gettime

check: $(UNITS)
	@for t in $(UNITS); do echo "$$t"; ./$$t || exit 1; done

build:
	mkdir -p build

install: digitalClock
	install -Dm755 digitalClock $(DESTDIR)$(PREFIX)/bin/digitalClock
	install -d $(DESTDIR)$(PREFIX)/share/applications
	sed 's|^Exec=digitalClock$$|Exec=$(PREFIX)/bin/digitalClock|' data/digitalClock.desktop \
		>$(DESTDIR)$(PREFIX)/share/applications/digitalClock.desktop
	chmod 644 $(DESTDIR)$(PREFIX)/share/applications/digitalClock.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/digitalClock \
		$(DESTDIR)$(PREFIX)/share/applications/digitalClock.desktop

# Needs a built mwc next to this directory, or in MWC_DIR, plus grim. Compares the
# height with deskControl and pulseControl if they are built next to it.
test: digitalClock
	tests/ui.sh

clean:
	rm -rf digitalClock build

.PHONY: all install uninstall check test clean
