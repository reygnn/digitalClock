#!/bin/bash
# Black-box test of digitalClock: a headless mwc, the clock in its dock, with
# deskControl, pulseControl and osDisplay beside it when they are built next to this
# directory.
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
OSD=${OSD:-$CK_DIR/../osDisplay/osDisplay}
SKIP_SISTERS=${SKIP_SISTERS:-0}   # lib.sh sets -u
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
unset MWC_INACTIVE MWC_ACTIVE CLOCK_FORMAT CLOCK_DATE PULSE_SERVER
# The dock's font is fixed: the variables that once set it are ignored by all four.
export CLOCK_FONT="Sans 20" DC_FONT="Sans 20" SC_FONT="Sans 20" OSD_FONT="Sans 20"
SISTERS_LOG=$CK_DIR/build/ui-sisters.log
: >"$CK_LOG"
: >"$SISTERS_LOG"
SHOT=$SMOKE_DIR/ui.png
ERR_SHOT=$CK_DIR/build/ui-last.png
BG=aeb2c3      # Motif's default background: the frame without the keyboard
SEL=7387ad     # activeBackground: the frame with the keyboard
INK=000000     # the digits on Motif's default background
DIM=3d3e44     # the date: dimmed towards the ink as far as WCAG AA's 4.5:1 allows
SEL_DIM=171b23 # ... and on activeBackground, where that takes more
IN=$((3 + 6 + 2))       # frame, padding, the field's bevel

mkdir -p "$XDG_CONFIG_HOME/mwc"
printf '%s\n' 'Mwm*digitalClock*dock: true' 'Mwm*deskControl*dock: true' \
	'Mwm*pulseControl*dock: true' 'Mwm*osDisplay*dock: true' \
	'Mwm*dockPlacement: right bottom' \
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
# The time alone: the field above the date's strip.
time_ink() { count $((X0 + IN)) $((Y0 + IN)) $((CW - 2 * IN)) $((CH - 2 * IN - 16)) "$@"; }
# The strip along the field's bottom where the date is, and only the date.
strip() { count $((X0 + IN)) $((Y0 + CH - IN - 16)) $((CW - 2 * IN)) 16 "$@"; }
# The date's own colour, nearly exact: count's tolerance would take the old one too.
DATE_TOL=6
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
	refresh
	where
}
quit_clock() { # [key]
	local i key=${1:-q}
	do_input key "$key"
	for ((i = 0; i < 50; i++)); do
		kill -0 "$PID_ck" 2>/dev/null || break
		sleep 0.1
	done
	kill -0 "$PID_ck" 2>/dev/null && fail "$key doesn't quit"
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

# As high as its sisters, with the same fixed font: the point of its layout.
PIDS_sisters=
for sister in "$DESK" "$PULSE" "$OSD"; do
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
date=$(strip $DIM $DATE_TOL) black=$(strip $INK)
[ "${date:-0}" -gt 8 ] && [ "${black:-1}" = 0 ] ||
	{ cp "$SHOT" "$ERR_SHOT" 2>/dev/null; fail "the date strip: $date grey, $black black pixels"; }
echo "ok: the date below the time ($date pixels)"
# A change with digits still there, and the clock still running: not a crash.
for ((i = 0; i < 40; i++)); do
	kill -0 "$PID_ck" 2>/dev/null || fail "digitalClock died"
	shoot && n=$(ink) && [ "${n:-0}" -gt 50 ] && [ "$n" != "$first" ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] && [ "$n" != "$first" ] || { cp "$SHOT" "$ERR_SHOT" 2>/dev/null; fail "it doesn't tick"; }
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
# Not gone within a second: what q takes is a tenth of that.
for ((i = 0; i < 10; i++)); do
	kill -0 "$PID_ck" 2>/dev/null || fail "Alt+Q quits"
	sleep 0.1
done
echo "ok: Alt+Q does not quit"
quit_clock
echo "ok: q quits"
for key in Escape ctrl+q ctrl+w; do
	start_clock
	do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
	do_input click left
	check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
	quit_clock $key
	echo "ok: $key quits"
done

# ---- letters ---------------------------------------------------------------------------

# Accents and descenders stay inside the field and off the date: the time is fitted
# to its whole line once it has letters.
CLOCK_FORMAT='Ägypten %H' start_clock
for ((i = 0; i < 30; i++)); do
	shoot && n=$(ink) && [ "${n:-0}" -gt 50 ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] || { cp "$SHOT" "$ERR_SHOT" 2>/dev/null; fail "no letters: $n ink pixels"; }
# The field's top rows inside its bevel, where the text is clipped: an accent cut
# off there would leave ink, so they are all face. And the date's strip.
top=$(count $((X0 + IN)) $((Y0 + IN)) $((CW - 2 * IN)) 2 $BG)
black=$(strip $INK)
[ "$top" = $((2 * (CW - 2 * IN))) ] && [ "$black" = 0 ] ||
	{ cp "$SHOT" "$ERR_SHOT" 2>/dev/null
	  fail "letters: $top of $((2 * (CW - 2 * IN))) face pixels at the top, $black on the date"; }
echo "ok: letters stay in the field and off the date"
do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
do_input click left
check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
quit_clock
echo "ok: and it quits"

# ---- contrast ----------------------------------------------------------------------------

# On a mid-tone such as mwc's activeBackground black reads better than white: the
# digits are black and nothing in the field is white. The date is darker than on
# the default background, to keep 4.5:1.
MWC_INACTIVE="#$SEL" CLOCK_FORMAT=%H:%M start_clock
for ((i = 0; i < 30; i++)); do
	# The date's 171b23 is within count's tolerance of black: the time alone.
	shoot && n=$(time_ink $INK 6) &&
		white=$(count $((X0 + IN)) $((Y0 + IN)) $((CW - 2 * IN)) $((CH - 2 * IN)) ffffff) &&
		date=$(strip $SEL_DIM $DATE_TOL) &&
		[ "${n:-0}" -gt 50 ] && [ "$white" = 0 ] && [ "${date:-0}" -gt 8 ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] && [ "$white" = 0 ] && [ "${date:-0}" -gt 8 ] ||
	{ cp "$SHOT" "$ERR_SHOT" 2>/dev/null
	  fail "on $SEL: $n black, $white white, $date $SEL_DIM pixels"; }
echo "ok: black digits on a mid-tone, the date in $SEL_DIM"
do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
do_input click left
check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
quit_clock

# rgb: takes hex digits only, as mwc does: a sign is no colour, and the default
# stays, as in mwc's frames.
MWC_INACTIVE=rgb:-1/0/0 start_clock
frame_is "rgb:-1/0/0 is no colour" $BG
do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
do_input click left
check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
quit_clock

# ---- the mwmrc ---------------------------------------------------------------------------

MWMRC=$XDG_CONFIG_HOME/mwc/mwmrc
cp "$MWMRC" "$SMOKE_DIR/mwmrc.saved"
restore_mwmrc() { cp "$SMOKE_DIR/mwmrc.saved" "$MWMRC"; }
focus_clock() {
	do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
	do_input click left
	check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
}

# The text in Mwm*foreground, as mwc's titles, not black.
FG=800000
echo "Mwm*foreground: #$FG" >>"$MWMRC"
start_clock
for ((i = 0; i < 30; i++)); do
	shoot && n=$(count $((X0 + IN)) $((Y0 + IN)) $((CW - 2 * IN)) $((CH - 2 * IN)) $FG 6) &&
		[ "${n:-0}" -gt 50 ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] ||
	{ cp "$SHOT" "$ERR_SHOT" 2>/dev/null; fail "Mwm*foreground: $n pixels in $FG"; }
echo "ok: the digits in Mwm*foreground"
focus_clock
quit_clock
restore_mwmrc

# Whole lines: a comment longer than a line buffer whose rest looks like a setting
# stays a comment.
{ printf '!%.0s' {1..511}; echo 'Mwm*activeBackground: #ff0000'; } >>"$MWMRC"
start_clock
focus_clock
frame_is "a long comment line sets nothing" $SEL
quit_clock
restore_mwmrc

# The last line wins, as in mwc, and one too long to keep is no value: the default.
{ echo 'Mwm*activeBackground: #ff0000'; printf 'Mwm*activeBackground: #'
  printf '0%.0s' {1..300}; echo; } >>"$MWMRC"
start_clock
focus_clock
frame_is "the last activeBackground, too long, leaves the default" $SEL
quit_clock
restore_mwmrc

# A format too long says so, the time and the date each, also beyond 1023 bytes.
long=$SMOKE_DIR/long.log
CLOCK_FORMAT=$(printf 'a%.0s' {1..300}) CLOCK_DATE=$(printf 'b%.0s' {1..2000}) \
	WAYLAND_DISPLAY=wayland-0 "$CK" >"$long" 2>&1 &
PID_long=$!
check "a long time and a long date warn" \
	"[ \"\$(grep -c 'makes more than 255 bytes' \"$long\")\" = 2 ]"
kill "$PID_long" 2>/dev/null
wait "$PID_long" 2>/dev/null
check "the long clock is gone" "[ -z \"\$(get digitalClock x)\" ]"
# Once: with the seconds as the time, it draws every second, and two draws later
# the date has still warned once.
CLOCK_FORMAT=%S CLOCK_DATE=$(printf 'b%.0s' {1..300}) WAYLAND_DEBUG=client \
	WAYLAND_DISPLAY=wayland-0 "$CK" >"$long" 2>&1 &
PID_long=$!
check "a long date warns" "grep -q 'makes more than 255 bytes' \"$long\""
check "the clock with the long date is mapped" "[ -n \"\$(get digitalClock x)\" ]"
# Two seconds on the wall clock and two commits after: two ticks at least, not
# only the commits of the start.
commits() { grep -c 'wl_surface#[0-9]*\.commit()' "$long"; }
C0=$(commits) T0=$(date +%s)
check "it ticks twice" \
	"[ \$(date +%s) -ge $((T0 + 2)) ] && [ \"\$(commits)\" -ge $((C0 + 2)) ]"
[ "$(grep -c 'makes more than 255 bytes' "$long")" = 1 ] ||
	fail "the long date warned $(grep -c 'makes more than 255 bytes' "$long") times"
echo "ok: and only once"
kill "$PID_long" 2>/dev/null
wait "$PID_long" 2>/dev/null
check "the long clock is gone" "[ -z \"\$(get digitalClock x)\" ]"

# Without a date the time takes the whole field, down into the date's strip, where
# with one there is no black.
CLOCK_DATE= CLOCK_FORMAT=%H:%M start_clock
for ((i = 0; i < 30; i++)); do
	shoot && n=$(strip $INK) && [ "${n:-0}" -gt 50 ] && break
	sleep 0.1
done
[ "${n:-0}" -gt 50 ] ||
	{ cp "$SHOT" "$ERR_SHOT" 2>/dev/null
	  fail "CLOCK_DATE empty: $n black pixels in the date's strip"; }
echo "ok: without a date the time fills the field"
focus_clock
quit_clock

# ---- a scaled output ---------------------------------------------------------------------

# The pointer rests on the clock while the scale changes: its cursor follows at once,
# without a new enter, and its buffer is one the scale divides.
# The dock top left, so that the pointer stays on the clock when the output shrinks.
# Adwaita has cursors of 24, 30, 36, 48, 72 and 96 pixels; at size 7 the clock gets
# 24 at scales 1 and 2, and 30 at scale 4, which 4 doesn't divide.
cursor() { refresh; CW_=$(global cursor cw) CH_=$(global cursor ch) CS_=$(global cursor cscale); }
# What the clock asks for, from its protocol log.
WL_LOG=$CK_DIR/build/ui-wayland.log
: >"$WL_LOG"
# The last size the cursor's viewport was given, "w h"; -1 -1 is none.
cursor_dest() {
	grep -o 'wp_viewport#[0-9]*\.set_destination(-\?[0-9]*, -\?[0-9]*)' "$WL_LOG" | tail -1 |
		sed 's/.*(\(.*\), \(.*\))/\1 \2/'
}
# Buffers destroyed while the cursor surface still showed them: a theme gone too early.
cursor_kept() {
	awk '
	match($0, /wl_pointer#[0-9]+\.set_cursor\([0-9]+, wl_surface#[0-9]+/) {
		split(substr($0, RSTART, RLENGTH), f, /[#(, ]+/); cur = f[5]
	}
	match($0, /wl_surface#[0-9]+\.attach\((wl_buffer#[0-9]+|nil)/) {
		split(substr($0, RSTART, RLENGTH), f, /[#.()]+/)
		att[f[2]] = f[4] == "nil" ? "" : f[5]
	}
	match($0, /wl_buffer#[0-9]+\.destroy\(\)/) {
		split(substr($0, RSTART, RLENGTH), f, /[#.()]+/)
		if (cur != "" && att[cur] == f[2]) print f[2]
	}' "$WL_LOG"
}
# A size past LONG_MAX is the largest, 256, as in the other docks: Adwaita's 96.
XCURSOR_THEME=Adwaita XCURSOR_SIZE=99999999999999999999 start_clock
do_input move $((X0 + CW / 2)) $((Y0 + CH / 2))
check "XCURSOR_SIZE past LONG_MAX gives the largest cursor" "cursor; [ \"\${CW_:-0}\" = 96 ]"
do_input click left
check "it has the focus" "[ \"\$(get digitalClock kbfocus)\" = 1 ]"
quit_clock
sed -i 's/dockPlacement: right bottom$/dockPlacement: left top/' "$XDG_CONFIG_HOME/mwc/mwmrc"
reload_mwc
CK_LOG=$WL_LOG WAYLAND_DEBUG=client XCURSOR_THEME=Adwaita XCURSOR_SIZE=7 start_clock
do_input move $((X0 + 20)) $((Y0 + 20))
check "the clock's cursor at scale 1" \
	"cursor; [ \"\$CS_\" = 1 ] && [ \"\${CW_:-0}\" -gt 0 ]"
echo "ok: its cursor, ${CW_}x$CH_ at scale 1"

# At scale 2 it draws twice the pixels: the first one of the face beside the bevel is
# the face's colour, not a blend of the two from a buffer scaled up.
echo 'Mwm*HEADLESS-1*outputScale: 2' >>"$XDG_CONFIG_HOME/mwc/mwmrc"
reload_mwc
check "the output is scaled" "[ \"\$(global output_HEADLESS-1 scale)\" = 200 ]"
check "the cursor follows the scale without a new enter" \
	"cursor; [ \"\$CS_\" = 2 ]"
echo "ok: its cursor, ${CW_}x$CH_ at scale 2"
[ "$(cursor_dest)" = '-1 -1' ] || fail "at scale 2 a viewport of $(cursor_dest)"
echo "ok: there the scale divides it, and no viewport"
refresh
where
sharp=
for ((i = 0; i < 30; i++)); do
	shoot && sharp=$(build/pngtool pixel "$SHOT" $((2 * (X0 + 2))) $((2 * (Y0 + CH / 2))))
	[ "$sharp" = $BG ] && break
	sleep 0.1
done
[ "$sharp" = $BG ] || { cp "$SHOT" "$ERR_SHOT" 2>/dev/null
	fail "at scale 2 the face beside the bevel is $sharp, not $BG (screenshot in $ERR_SHOT)"; }
echo "ok: sharp at scale 2"

# At scale 4 the theme's 30 pixels are no multiple of 4: the cursor's buffer scale
# must divide its size, or a compositor may end the client with invalid_size. The
# buffer goes at scale 1, and a viewport shows it at 30 / 4, rounded 8, as large as
# mwc shows its own cursor.
FROM=$(($(wc -l <"$WL_LOG") + 1))
sed -i 's/outputScale: 2$/outputScale: 4/' "$XDG_CONFIG_HOME/mwc/mwmrc"
reload_mwc
check "the output is at scale 4" "[ \"\$(global output_HEADLESS-1 scale)\" = 400 ]"
check "the cursor is loaded for scale 4" "cursor; [ \"\${CW_:-0}\" = 30 ]"
[ "$CS_" -gt 0 ] && [ $((CW_ % CS_)) = 0 ] && [ $((CH_ % CS_)) = 0 ] ||
	fail "a cursor of ${CW_}x$CH_ at buffer scale $CS_"
echo "ok: its cursor, ${CW_}x$CH_ at buffer scale $CS_"
check "and a viewport shows it at 8" "[ \"\$(cursor_dest)\" = '8 8' ]"
# With the pointer on the clock the new cursor replaces the old one: no attach(nil)
# from the scale change on, which would flicker. One at startup is fine.
CURSOR_SURF=$(grep -o 'wl_pointer#[0-9]*\.set_cursor([0-9]*, wl_surface#[0-9]*' "$WL_LOG" |
	tail -1 | sed 's/.*#//')
tail -n +"$FROM" "$WL_LOG" | grep -q "wl_surface#$CURSOR_SURF\.attach(nil" &&
	fail "the cursor surface went empty while the pointer was on the clock"
echo "ok: the cursor is swapped without going empty"
# The pointer leaves, the scale changes: the cursor isn't shown, and the surface
# lets go of the old theme's buffer before that goes.
do_input move 1000 600
check "the pointer leaves the clock" "grep -q 'wl_pointer#[0-9]*\.leave(' \"$WL_LOG\""
# The buffer the cursor showed last is the old theme's; the window's own go too.
OLD_BUF=$(grep -o "wl_surface#$CURSOR_SURF\.attach(wl_buffer#[0-9]*" "$WL_LOG" | tail -1 |
	sed 's/.*#//')
[ -n "$OLD_BUF" ] || fail "no buffer on the cursor surface"
FROM=$(($(wc -l <"$WL_LOG") + 1))
sed -i 's/outputScale: 4$/outputScale: 2/' "$XDG_CONFIG_HOME/mwc/mwmrc"
reload_mwc
check "the output is at scale 2 again" "[ \"\$(global output_HEADLESS-1 scale)\" = 200 ]"
check "the old theme is gone" \
	"tail -n +$FROM \"$WL_LOG\" | grep -q 'wl_buffer#$OLD_BUF\.destroy()'"
[ -z "$(cursor_kept)" ] || fail "the cursor surface showed destroyed buffers: $(cursor_kept)"
echo "ok: no cursor buffer is destroyed while shown"
kill -0 "$PID_ck" 2>/dev/null || fail "digitalClock died at scale 4"
kill "$PID_ck"
wait "$PID_ck" 2>/dev/null

VERSION=$(sed -n 's/^#define DIGITALCLOCK_VERSION "\(.*\)"/\1/p' "$CK_DIR/version.h")
[ "$("$CK" -v)" = "digitalClock $VERSION" ] || fail "-v: $("$CK" -v)"
echo "ok: -v prints the version"
out=$("$CK" -h 2>/dev/null) || fail "-h: exit $?"
echo "$out" | grep -q '^usage: digitalClock' || fail "-h: no usage on stdout"
echo "ok: -h prints the usage on stdout"
for a in -v -h; do
	"$CK" $a >/dev/full 2>/dev/null && fail "$a: exit 0 on a full disk"
	"$CK" $a >&- 2>/dev/null && fail "$a: exit 0 without stdout"
	echo "ok: $a fails when stdout does"
done
for a in "-v junk" "-h junk" "--help" "-x"; do
	"$CK" $a >/dev/null 2>&1 && fail "$a: exit 0"
	echo "ok: $a is refused"
done

# Without DejaVu Sans fontconfig takes another font; the clock says so.
FC_DIR=$SMOKE_DIR/fc
mkdir -p "$FC_DIR/fonts"
cp "$(fc-match -f '%{file}' 'DejaVu Sans Mono')" "$FC_DIR/fonts/"
printf '<?xml version="1.0"?>\n<fontconfig><dir>%s</dir><cachedir>%s</cachedir></fontconfig>\n' \
	"$FC_DIR/fonts" "$FC_DIR/cache" >"$FC_DIR/fonts.conf"
FC_LOG=$SMOKE_DIR/fc.log
FONTCONFIG_FILE=$FC_DIR/fonts.conf WAYLAND_DISPLAY=wayland-0 "$CK" >"$FC_LOG" 2>&1 &
PID_fc=$!
check "it warns without DejaVu Sans" \
	"grep -q '^digitalClock: font DejaVu Sans not found, using DejaVu Sans Mono\$' \"$FC_LOG\""
kill "$PID_fc"
wait "$PID_fc" 2>/dev/null
grep -q 'not found' "$CK_LOG" && fail "the font is missing in the test: $(grep 'not found' "$CK_LOG")"
echo "ok: DejaVu Sans is there for the rest"

for p in $PIDS_sisters; do
	kill "$p" 2>/dev/null
	wait "$p" 2>/dev/null
done
kill -0 "$pid" || fail "mwc died"
stop_vinput
stop_mwc
echo "PASS: digitalClock"
