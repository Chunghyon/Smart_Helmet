#!/bin/sh
#
# Tier 1 host regression runner for the pulse-change detector.
#
# Builds the firmware DSP source against the stubs in stub/ and runs every
# scenario across the option matrix. Nothing here touches hardware; it is the
# safety net that every algorithm change has to pass before it goes near a
# board.
#
# Usage:  ./run_tests.sh [scenario-name-substring]
#         CC=clang ./run_tests.sh
#         SH_TEST_LOG=1 ./run_tests.sh 7-dropout   # show firmware log lines
#
set -eu

here=$(cd "$(dirname "$0")" && pwd)
src="$here/../../src/smart_helmet"
bin=${TMPDIR:-/tmp}/sh_vitals_test.$$
filter=${1:-}

CC=${CC:-gcc}
CFLAGS="-O1 -std=gnu99 -Wall -Wextra -Werror -I$here/stub -I$src"

failures=0

# $1 = label, $2 = "gate" (required to pass) or "info" (comparison only),
# $3.. = extra -D flags.
run_config()
{
    label=$1
    gate=$2
    shift 2

    echo
    echo "=============================================================="
    echo "config: $label  ${*:-(defaults)}  [$gate]"
    echo "=============================================================="

    $CC $CFLAGS "$@" -o "$bin" \
        "$here/sh_vitals_test.c" "$here/sh_signal.c" \
        "$src/smart_helmet_vitals.c" -lm

    if "$bin" $filter; then
        :
    elif [ "$gate" = gate ]; then
        failures=$((failures + 1))
    else
        echo "  (comparison configuration - failures here are expected and"
        echo "   document what the option buys)"
    fi
    rm -f "$bin"
}

# The default axis comes first: SMART_HELMET_ENABLE_LIS3DH defaults to 0, so
# that is the shipped configuration and it must pass on its own.
run_config default gate
run_config lis3dh gate \
    -DSMART_HELMET_ENABLE_LIS3DH=1
run_config lis3dh-fifo-adapt gate \
    -DSMART_HELMET_ENABLE_LIS3DH=1 \
    -DSMART_HELMET_LIS3DH_USE_FIFO=1 \
    -DSMART_HELMET_ENABLE_HR_MOTION_ADAPT=1
run_config no-timestamp gate \
    -DSMART_HELMET_ENABLE_VITALS_TIMESTAMP=0
run_config telemetry-on gate \
    -DSMART_HELMET_ENABLE_VITALS_CSV=1 \
    -DSMART_HELMET_ENABLE_SENS_DUMP=1

# Comparison axes: these exist to show the effect of an option, not to gate
# the build.
run_config legacy-cusum info \
    -DSMART_HELMET_HR_BASELINE_STALE_WIN=0 \
    -DSMART_HELMET_HR_CUSUM_SLACK_PCT=0
run_config legacy-adapt-ref info \
    -DSMART_HELMET_ENABLE_LIS3DH=1 \
    -DSMART_HELMET_LIS3DH_USE_FIFO=1 \
    -DSMART_HELMET_ENABLE_HR_MOTION_ADAPT=1 \
    -DSMART_HELMET_HR_ADAPT_REF_AXES=1
run_config autocorr-hps info \
    -DSMART_HELMET_ENABLE_HR_AUTOCORR=1 \
    -DSMART_HELMET_HR_FFT_HPS=1

echo
echo "Compile-only checks (these cannot be linked standalone)."
$CC $CFLAGS -DSMART_HELMET_ENABLE_VITALS_PROXY=0 -fsyntax-only \
    "$src/smart_helmet_vitals.c" && echo "  ok  vitals proxy disabled"
for f in smart_helmet_adc.c smart_helmet_sensors.c smart_helmet.c; do
    $CC $CFLAGS -fsyntax-only "$src/$f" && echo "  ok  $f"
done

echo
if [ "$failures" -eq 0 ]; then
    echo "ALL GATING CONFIGURATIONS PASSED"
else
    echo "$failures GATING CONFIGURATION(S) FAILED"
fi
exit "$failures"
