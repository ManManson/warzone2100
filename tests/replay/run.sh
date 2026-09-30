#!/bin/sh
# Generates the fixture replays with the current build and runs the rewind harness on them.
#
# A .wzrp is bound to the build that recorded it, so the checked-in artefact is the recipe
# under data/mp/tests/. This script records it, then seeks. --check records each recipe twice
# and requires byte-identical CRC traces.
#
# data/ changes only take effect after a full `ninja -C build`. Building just the
# warzone2100 target leaves the packaged mp.wz stale.
set -e

WZ=${WZ:-build/src/warzone2100}
FIXTURES=${FIXTURES:-"replayfixture_4p_rush replayfixture_2p_highground"}
DURATION_MS=${DURATION_MS:-600000}
SEED=${SEED:-0x5EEDBEEF}
SEEK_TIMES=${SEEK_TIMES:-"60000 120000 300000 540000"}
N=${N:-150}

if [ ! -x "$WZ" ]; then
	echo "warzone2100 binary not found at $WZ (set WZ=path)" >&2
	exit 1
fi

CHECK=0
if [ "${1:-}" = "--check" ]; then
	CHECK=1
	shift
fi

gen() {  # $1 = recipe name, $2 = output name
	"$WZ" --autogame --headless --skirmish="$1.json" --game-seed="$SEED" --deterministic-latency \
	      --quit-at-gametime="$DURATION_MS" --record-replay-as="$2" --gamestate-crc-trace="/tmp/$2.crc" >/dev/null
}

fail=0
for f in $FIXTURES; do
	start=$(date +%s)
	gen "$f" "$f"
	echo "generated $f in $(($(date +%s) - start))s"
	if [ "$CHECK" -eq 1 ]; then
		gen "$f" "$f.check"
		cmp "/tmp/$f.crc" "/tmp/$f.check.crc"
		echo "reproducible $f"
	fi
	for T in $SEEK_TIMES; do
		out=$("$WZ" --autogame --headless --loadreplay="skirmish/$f.wzrp" --replay-selftest="$T,$N" 2>&1 | grep '\[replay-selftest\]') || true
		echo "$f T=$T: $out"
		echo "$out" | grep -q '^\[replay-selftest\] OK' || fail=1
	done
done
exit $fail
