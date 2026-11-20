# digitalClock - the time in digits on Wayland (mwc look)

PREFIX ?= /usr/local
PKG_CONFIG ?= pkg-config
PKGS = wayland-client wayland-cursor xkbcommon cairo pangocairo

WAYLAND_PROTOCOLS := $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER := $(shell $(PKG_CONFIG) --variable=wayland_scanner wayland-scanner)
XDG_SHELL = $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml
VIEWPORTER = $(WAYLAND_PROTOCOLS)/stable/viewporter/viewporter.xml

PROTO_H = build/xdg-shell-client-protocol.h build/viewporter-client-protocol.h
PROTO_C = build/xdg-shell-protocol.c build/viewporter-protocol.c

# CPPFLAGS, CFLAGS, LDFLAGS and LDLIBS stay the user's, also on the command line:
# `make CFLAGS=-O0` must not lose the include paths.
CFLAGS ?= -O2 -g
CK_CFLAGS = -std=c11 -Wall -Wextra -Wno-unused-parameter -Ibuild \
	$(shell $(PKG_CONFIG) --cflags $(PKGS))
CK_LIBS = $(shell $(PKG_CONFIG) --libs $(PKGS)) -lm

MODS = theme.c timefmt.c util.c
MODS_H = theme.h timefmt.h util.h

all: digitalClock

digitalClock: clock.c $(MODS) $(MODS_H) version.h $(PROTO_H) $(PROTO_C)
	$(CC) $(CK_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ clock.c $(MODS) \
		$(PROTO_C) $(CK_LIBS) $(LDLIBS)

# The modules without Wayland, each under ASan and UBSan.
UNIT_CFLAGS = -std=c11 -Wall -Wextra -g -O1 -fsanitize=address,undefined \
	-fno-sanitize-recover=all
UNITS = build/test_theme build/test_timefmt build/test_util

build/test_theme: tests/test_theme.c theme.c theme.h | build
	$(CC) $(UNIT_CFLAGS) -I. $(shell $(PKG_CONFIG) --cflags pango) -o $@ \
		tests/test_theme.c theme.c $(shell $(PKG_CONFIG) --libs pango) -lm

# clock_gettime wrapped: the test steps the wall clock, which needs root for real.
build/test_timefmt: tests/test_timefmt.c timefmt.c timefmt.h | build
	$(CC) $(UNIT_CFLAGS) -I. $(shell $(PKG_CONFIG) --cflags glib-2.0) -o $@ \
		tests/test_timefmt.c timefmt.c $(shell $(PKG_CONFIG) --libs glib-2.0) \
		-Wl,--wrap=clock_gettime

build/test_util: tests/test_util.c util.c util.h | build
	$(CC) $(UNIT_CFLAGS) -I. -o $@ tests/test_util.c util.c

check: $(UNITS)
	@for t in $(UNITS); do echo "$$t"; ./$$t || exit 1; done

build:
	mkdir -p build

build/xdg-shell-client-protocol.h: $(XDG_SHELL) | build
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL) $@

build/xdg-shell-protocol.c: $(XDG_SHELL) | build
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL) $@

build/viewporter-client-protocol.h: $(VIEWPORTER) | build
	$(WAYLAND_SCANNER) client-header $(VIEWPORTER) $@

build/viewporter-protocol.c: $(VIEWPORTER) | build
	$(WAYLAND_SCANNER) private-code $(VIEWPORTER) $@

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
