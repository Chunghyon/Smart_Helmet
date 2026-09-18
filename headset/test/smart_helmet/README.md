# Pulse-change detection — host regression harness (Tier 1)

This directory holds the automated safety net for `smart_helmet_vitals.c`.

The module does **not** report a clinical heart rate. Its product is the
`hr_change` flag, so nothing here scores BPM accuracy. Every scenario is
scored on events:

| metric | meaning |
|---|---|
| `valid%`  | share of windows where `hr_valid` was set (availability) |
| `det`     | number of `hr_change` transitions observed |
| `dir`     | direction of the first transition (`+1` rising, `-1` falling) |
| `latency` | first detection relative to the true change time |
| `false`   | transitions inside a window where none was expected |

A scenario only passes if it gets the expected event, inside its latency
budget, with no false alarms, and (where asserted) with enough availability.

## Running

```sh
./run_tests.sh                 # whole option matrix
./run_tests.sh 7-dropout       # one scenario across the matrix
SH_TEST_LOG=1 ./run_tests.sh 7-dropout   # plus the firmware log lines
CC=clang ./run_tests.sh
```

Only a C compiler is needed. The firmware source is compiled directly against
the stub headers in `stub/` (`csrtypes.h`, `adc.h`, `logging.h`, `message.h`,
`vm.h`, `panic.h`), so the code under test is the real code, not a copy.

`run_tests.sh` exits non-zero if any **gating** configuration fails.
Configurations marked `[info]` exist to show what an option is worth and are
allowed to fail.

### Option matrix

| config | why it is in the matrix |
|---|---|
| `default` | the shipped build (`SMART_HELMET_ENABLE_LIS3DH` is 0) |
| `lis3dh` | accelerometer present, polled irregularly |
| `lis3dh-fifo-adapt` | FIFO stream + NLMS motion cancellation |
| `no-timestamp` | measured-`fs` path disabled |
| `telemetry-on` | CSV + raw dump paths compiled and exercised |
| `legacy-cusum` *(info)* | change detection before the stale-baseline / proportional-slack fixes |
| `legacy-adapt-ref` *(info)* | NLMS driven by the accel magnitude instead of per-axis |
| `autocorr-hps` *(info)* | autocorrelation + harmonic product spectrum |

`run_tests.sh` also syntax-checks `smart_helmet_adc.c`,
`smart_helmet_sensors.c`, `smart_helmet.c` and the
`SMART_HELMET_ENABLE_VITALS_PROXY=0` build, so no configuration can be left
broken.

## Scenarios

| # | input | what it proves |
|---|---|---|
| 1 | 60 BPM held for 5 min | no change flag; availability floor |
| 2 | 60 → 100 BPM step | `rising` inside the latency budget |
| 3 | 100 → 60 BPM step | `falling`, and no false alarm at the higher rate |
| 4 | 60 → 70 BPM ramp over 60 s | CUSUM slack/limit are not so slow that a real drift is missed |
| 5 | 60 BPM with heavy noise | zero false alarms |
| 6 | no subject, noise only | `hr_valid` stays clear |
| 7 | subject leaves at 60 s, returns at 90 s at 62 BPM | re-acquisition is not reported as a rate change |
| 8 | 2.3 Hz head sway over 60 BPM | motion handling; with `MOTION_ADAPT` the window must stay usable |
| 9 | 1 Hz, 25 % sampling-interval modulation | the cadence artefact must not become a 60 BPM line |

The synthetic signal is deliberately not a sine wave. `sh_signal.c` builds a
pulse waveform from three harmonics, then adds respiration, a 1/f-like
baseline wander and white noise. Options such as `SMART_HELMET_HR_FFT_HPS`
behave completely differently on a pure tone, so testing against one would be
misleading.

## What the harness found

Three real defects, all now fixed and all still reproducible by flipping the
corresponding option back:

1. **Stale CUSUM baseline** (scenario 7). The baseline survived an
   arbitrarily long signal loss, so a subject who stepped away and came back
   produced a false `rising`. Fixed by
   `SMART_HELMET_HR_BASELINE_STALE_WIN`.
2. **Fixed CUSUM slack** (scenario 3). A 3 BPM slack that is right at 60 BPM
   is too tight at 100 BPM and fired spuriously. Fixed by
   `SMART_HELMET_HR_CUSUM_SLACK_PCT`.
3. **Magnitude-based NLMS reference** (scenario 8). The canceller was driven
   by the high-passed accelerometer *magnitude*. Because gravity dominates
   the magnitude, a lateral sway appears rectified and at twice its real
   frequency, which a linear filter cannot subtract — availability under sway
   was 0.4 %. Driving the filter from the per-axis high-passed signals
   (`SMART_HELMET_HR_ADAPT_REF_AXES=3`) raises it to ~71 %.

## Replaying a real recording

`SMART_HELMET_ENABLE_SENS_DUMP` makes the firmware emit every SENS sample as
`seq,mv,time_us`. Capture that over the Wi-SUN UART, and the same samples can
be pushed through `SmartHelmet_VitalsPushSensInMvAt()` here — algorithm tuning
against real waveforms then needs no hardware.

## CSV telemetry format

`SMART_HELMET_ENABLE_VITALS_CSV` emits one record per processed window. The
column order is fixed; **append new columns at the end** rather than
inserting them, so existing captures stay parsable.

```
seq, active, hr_valid, hr_reason, hr_bpm, hr_bpm_peak, hr_bpm_fft,
hr_bpm_ac, hr_baseline_bpm, hr_delta_bpm, hr_cusum_up, hr_cusum_dn,
hr_change, resp_bpm, resp_q8, fs_x100, fs_jitter_pct, sens_dropped,
band_energy, baseline_energy, sens_abs, motion_rms_mg,
adapt_removed_pct, sens_mv
```

`hr_change` uses `smart_helmet_trend_flag_t`: 0 unknown, 1 stable, 2 rising,
3 falling. `hr_reason` uses `smart_helmet_hr_reason_t` (see
`../../src/smart_helmet/README.md`).

Both telemetry options are inert unless a sink is registered with
`SmartHelmet_VitalsSetSink()`; `smart_helmet.c` registers one that writes the
records to the Wi-SUN UART.

## Adding a scenario

Add an entry to `sh_scenarios[]` in `sh_vitals_test.c` with a rate profile
function and the event expectations. Keep the expectation event-based; if you
find yourself asserting on a BPM value, the test is measuring the wrong
thing.
