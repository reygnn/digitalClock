#!/bin/bash
# Black-box test of digitalClock: a headless mwc, the clock in its dock, with
# deskControl and pulseControl beside it when they are built next to this directory.
# What it shows is read from screenshots. It has its own sockets and touches no
# running session.
#
# Needs a built mwc with build/vinput, build/pngtool and build/xdgc in MWC_DIR (by
# default ../mwc), grim, and the sisters built next to this directory (or
# SKIP_SISTERS=1). Output goes to build/ui.log, digitalClock's own to
# build/ui-digitalClock.log, the sisters' to build/ui-sisters.log.
CK_DIR=$(cd "$(dirname "$0")/.." && pwd)
CK=${CK:-$CK_DIR/digitalClock}
LOG=$CK_DIR/build/ui.log
CK_LOG=$CK_DIR/build/ui-digitalClock.log
MWC_DIR=${MWC_DIR:-$CK_DIR/../mwc}
DESK=${DESK:-$CK_DIR/../deskControl/deskControl}
PULSE=${PULSE:-$CK_DIR/../pulseControl/pulseControl}
[ -x "$CK" ] || { echo "FAIL: $CK not built"; exit 1; }
[ -d "$MWC_DIR" ] || { echo "FAIL: no mwc in $MWC_DIR (set MWC_DIR)"; exit 1; }
mkdir -p "$CK_DIR/build"
cd "$MWC_DIR" || exit 1
. tests/lib.sh
require_tools grim
for tool in build/vinput build/pngtool build/xdgc; do
	[ -x "$tool" ] || { echo "FAIL: $MWC_DIR/$tool not built (make $tool)"; exit 1; }
done

export MWC_TEST_STATE=$SMOKE_DIR/state
export LC_ALL=C.UTF-8
unset MWC_INACTIVE MWC_ACTIVE CLOCK_FONT CLOCK_FORMAT CLOCK_DATE DC_FONT SC_FONT \
	PULSE_SERVER
SISTERS_LOG=$CK_DIR/build/ui-sisters.log
: >"$CK_LOG"
: >"$SISTERS_LOG"
SHOT=$SMOKE_DIR/ui.png
ERR_SHOT=$CK_DIR/build/ui-last.png
BG=aeb2c3      # Motif's default background: the frame without the keyboard
SEL=7387ad     # activeBackground: the frame with the keyboard
INK=000000     # the digits on Motif's default background
DIM=46474e     # the date: the background mixed 60 % towards the ink
IN=$((3 + 6 + 2))       # frame, padding, the field's bevel

mkdir -p "$XDG_CONFIG_HOME/mwc"
printf '%s\n' 'Mwm*digitalClock*dock: true' 'Mwm*deskControl*dock: true' \
	'Mwm*pulseControl*dock: true' 'Mwm*dockPlacement: right bottom' \
	>"$XDG_CONFIG_HOME/mwc/mwmrc"
start_mwc 'exec sleep 300'
start_vinput
refresh

shoot() { WAYLAND_DISPLAY=wayland-0 grim "$SHOT" 2>/dev/null; }
# count X Y W H RRGGBB in the last screenshot
count() { build/pngtool count "$SHOT" "$@"; }
where() {
	X0=$(get digitalClock x) Y0=$(get digitalClock y)
	CW=$(get digitalClock w) CH=$(get digitalClock h)
}
ink() { count $((X0 + IN)) $((Y0 + IN)) $((CW - 2 * IN)) $((CH - 2 * IN)) $INK; }
# The strip along the field's bottom where the date is, and only the date.
strip() { count $((X0 + IN)) $((Y0 + CH - IN - 16)) $((CW - 2 * IN)) 16 "$1"; }
frame_is() { # description rrggbb
	local got= i
	for ((i = 0; i < 30; i++)); do
		shoot && got=$(build/pngtool pixel "$SHOT" $((X0 + 2)) $((Y0 + CH / 2)))
		[ "$got" = "$2" ] && { echo "ok: $1"; return 0; }
		sleep 0.1
	done
	cp "$SHOT" "$ERR_SHOT" 2>/dev/null
	fail "$1: the frame is $got, expected $2 (screenshot in $ERR_SHOT)"
}
start_clock() {
	WAYLAND_DISPLAY=wayland-0 "$CK" >>"$CK_LOG" 2>&1 &
	PID_ck=$!
	check "digitalClock is mapped" "[ -n \"\$(get digitalClock x)\" ]"
	sleep 0.3
	refresh
	where
}
quit_clock() {
	local i
	do_input key q
	for ((i = 0; i < 50; i++)); do
		kill -0 "$PID_ck" 2>/dev/null || break
		sleep 0.1
	done
	kill -0 "$PID_ck" 2>/dev/null && fail "q doesn't quit"
	wait "$PID_ck"
	[ $? = 0 ] || fail "digitalClock didn't exit with 0"
	check "it is gone" "[ -z \"\$(get digitalClock x)\" ]"
}

# Seconds, so that the test sees it tick.
CLOCK_FORMAT=%H:%M:%S start_clock

# Only its own thin frame: mwc adds no border and no title bar.
[ "$(get digitalClock bw)" = 0 ] && [ "$(get digitalClock th)" = 0 ] ||
	fail "mwc decorates it: border $(get digitalClock bw), title $(get digitalClock th)"
echo "ok: no server-side decoration"
check "it is on every desktop, as a docked window" \
	"[ \"\$(get digitalClock everywhere)\" = 1 ]"

# As high as its sisters, with the same default font: the point of its layout.
PIDS_sisters=
for sister in "$DESK" "$PULSE"; do
	name=$(basename "$sister")
	if [ ! -x "$sister" ]; then
		[ "$SKIP_SISTERS" = 1 ] || fail "$sister not built (or SKIP_SISTERS=1)"
		echo "skip: $sister not built, the height not compared"
		continue
	fi
	WAYLAND_DISPLAY=wayland-0 "$sister" >>"$SISTERS_LOG" 2>&1 &
	PIDS_sisters="$PIDS_sisters $!"
	check "$name is mapped" "[ -n \"\$(get $name x)\" ]"
	check "digitalClock is as high as $name" \
		"[ \"\$(get digitalClock h)\" = \"\$(get $name h)\" ] && \
		 [ \"\$(get digitalClock y)\" = \"\$(get $name y)\" ]"
done
refresh
where
echo "ok: ${CW}x$CH"

# The digits: ink in the field, and a different picture a second later.
for ((i = 0; i < 30; i++)); do
	shoot && first=$(ink) && [ "${first:-0}" -gt 50 ] && break
	sleep 0.1
done
[ "${first:-0}" -gt 50 ] ||
	{ cp "$SHOT" "$ERR_SHOT" 2>/dev/null; fail "no digits: $first ink pixels"; }
echo "ok: digits in the field ($first ink pixels)"
# The date below them, dimmed: grey in its strip, and none of the digits' black.
date=$(strip $DIM) black=$(strip $INK)
[ "${date:-0}" -gt 20 ] && [ "${black:-1}" = 0 ] ||
	{ cp "$SHOT" "$ERR_SHOT"; fail "the date strip: $date grey, $black black pixels"; }
echo "ok: the date below the time ($date pixels)"
# A change with digits still there, and the clock still running: not a crash.
for ((i = 0; i < 40; i++)); do
	kill -0 "$PID_ck" 2>/dev/null || fail "digitalClock died"
	shoot && n=$(ink) && [ "${n:-0}" -gt 50 ] && [ "$n" != "$first" ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] && [ "$n" != "$first" ] || { cp "$SHOT" "$ERR_SHOT"; fail "it doesn't tick"; }
echo "ok: it ticks"

# ---- the frame ---------------------------------------------------------------------------

# Docked, it maps without the keyboard; a click gives it, another window takes it.
frame_is "the frame is inactive at first" $BG
do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
do_input click left
check "digitalClock has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
frame_is "the frame is in the active colour" $SEL
WAYLAND_DISPLAY=wayland-0 build/xdgc app=thief size=200x100 >"$SMOKE_DIR/thief.log" 2>&1 &
PID_thief=$!
check "another window takes the keyboard" \
	"[ \"\$(get thief kbfocus)\" = 1 ] && [ \"\$(get digitalClock kbfocus)\" = 0 ]"
frame_is "the frame is inactive again" $BG
kill "$PID_thief" 2>/dev/null
wait "$PID_thief" 2>/dev/null

# ---- quitting ----------------------------------------------------------------------------

do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
do_input click left
check "digitalClock has the focus again" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
do_input key alt+q
sleep 0.3
kill -0 "$PID_ck" 2>/dev/null || fail "Alt+Q quits"
echo "ok: Alt+Q does not quit"
quit_clock
echo "ok: q quits"

# ---- letters ---------------------------------------------------------------------------

# Accents and descenders stay inside the field and off the date: the time is fitted
# to its whole line once it has letters.
CLOCK_FORMAT='Ägypten %H' start_clock
for ((i = 0; i < 30; i++)); do
	shoot && n=$(ink) && [ "${n:-0}" -gt 50 ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] || { cp "$SHOT" "$ERR_SHOT" 2>/dev/null; fail "no letters: $n ink pixels"; }
# The field's top bevel and the strip above it, and the date's strip.
bevel=$(count $((X0 + IN)) $((Y0 + 3 + 6)) $((CW - 2 * IN)) 2 $INK)
black=$(strip $INK)
[ "$bevel" = 0 ] && [ "$black" = 0 ] ||
	{ cp "$SHOT" "$ERR_SHOT"; fail "letters: $bevel on the bevel, $black on the date"; }
echo "ok: letters stay in the field and off the date"
do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
do_input click left
check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
quit_clock
echo "ok: and it quits"

# ---- contrast ----------------------------------------------------------------------------

# On a mid-tone such as mwc's activeBackground black reads better than white: the
# digits are black and nothing in the field is white.
MWC_INACTIVE=$SEL CLOCK_FORMAT=%H:%M start_clock
for ((i = 0; i < 30; i++)); do
	shoot && n=$(ink) &&
		white=$(count $((X0 + IN)) $((Y0 + IN)) $((CW - 2 * IN)) $((CH - 2 * IN)) ffffff) &&
		[ "${n:-0}" -gt 50 ] && [ "$white" = 0 ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] && [ "$white" = 0 ] ||
	{ cp "$SHOT" "$ERR_SHOT" 2>/dev/null; fail "on $SEL: $n black, $white white pixels"; }
echo "ok: black digits on a mid-tone"
do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
do_input click left
check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
quit_clock

# ---- a scaled output ---------------------------------------------------------------------

# At scale 2 it draws twice the pixels: the first one of the face beside the bevel is
# the face's colour, not a blend of the two from a buffer scaled up.
echo 'Mwm*HEADLESS-1*outputScale: 2' >>"$XDG_CONFIG_HOME/mwc/mwmrc"
reload_mwc
check "the output is scaled" "[ \"\$(global output_HEADLESS-1 scale)\" = 200 ]"
start_clock
sharp=
for ((i = 0; i < 30; i++)); do
	shoot && sharp=$(build/pngtool pixel "$SHOT" $((2 * (X0 + 2))) $((2 * (Y0 + CH / 2))))
	[ "$sharp" = $BG ] && break
	sleep 0.1
done
[ "$sharp" = $BG ] || { cp "$SHOT" "$ERR_SHOT" 2>/dev/null
	fail "at scale 2 the face beside the bevel is $sharp, not $BG (screenshot in $ERR_SHOT)"; }
echo "ok: sharp at scale 2"
kill "$PID_ck"
wait "$PID_ck" 2>/dev/null

VERSION=$(sed -n 's/^#define DIGITALCLOCK_VERSION "\(.*\)"/\1/p' "$CK_DIR/version.h")
[ "$("$CK" -v)" = "digitalClock $VERSION" ] || fail "-v: $("$CK" -v)"
echo "ok: -v prints the version"

for p in $PIDS_sisters; do
	kill "$p" 2>/dev/null
	wait "$p" 2>/dev/null
done
kill -0 "$pid" || fail "mwc died"
stop_vinput
stop_mwc
echo "PASS: digitalClock"
