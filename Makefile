# digitalClock - the time in digits on Wayland (mwc look)

PREFIX ?= /usr/local
PKG_CONFIG ?= pkg-config
PKGS = wayland-client wayland-cursor xkbcommon cairo pangocairo

WAYLAND_PROTOCOLS := $(shell $(PKG_CONFIG) --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER := $(shell $(PKG_CONFIG) --variable=wayland_scanner wayland-scanner)
XDG_SHELL = $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml

PROTO_H = build/xdg-shell-client-protocol.h
PROTO_C = build/xdg-shell-protocol.c

# CFLAGS and LDLIBS stay the user's, also on the command line: `make CFLAGS=-O0`
# must not lose the include paths.
CFLAGS ?= -O2 -g
CK_CFLAGS = -std=c11 -Wall -Wextra -Wno-unused-parameter -Ibuild \
	$(shell $(PKG_CONFIG) --cflags $(PKGS))
CK_LIBS = $(shell $(PKG_CONFIG) --libs $(PKGS))

all: digitalClock

digitalClock: clock.c version.h $(PROTO_H) $(PROTO_C)
	$(CC) $(CK_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ clock.c $(PROTO_C) $(CK_LIBS) $(LDLIBS)

build:
	mkdir -p build

build/xdg-shell-client-protocol.h: | build
	$(WAYLAND_SCANNER) client-header $(XDG_SHELL) $@

build/xdg-shell-protocol.c: | build
	$(WAYLAND_SCANNER) private-code $(XDG_SHELL) $@

install: digitalClock
	install -Dm755 digitalClock $(DESTDIR)$(PREFIX)/bin/digitalClock
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications
	sed 's|^Exec=digitalClock$$|Exec=$(PREFIX)/bin/digitalClock|' data/digitalClock.desktop \
		>$(DESTDIR)$(PREFIX)/share/applications/digitalClock.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/digitalClock \
		$(DESTDIR)$(PREFIX)/share/applications/digitalClock.desktop

# Needs a built mwc next to this directory, or in MWC_DIR, plus grim. Compares the
# height with deskControl and pulseControl if they are built next to it.
test: digitalClock
	tests/ui.sh

clean:
	rm -rf digitalClock build

.PHONY: all install uninstall test clean
