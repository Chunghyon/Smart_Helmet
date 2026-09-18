# Smart Helmet interfaces (QCC3044)

Schematic reference: `schematic/SMART_HELMET_260816_1.DSN`

| Interface | Nets | Devices |
|-----------|------|---------|
| I2C0 | SCL, SDA | CJMCU-8118 (CCS811), LIS3DH, GY-906-BAA (MLX90614), SSD1315 optional |
| I2C1 | SCL1, SDA1 | SSD1315 optional |
| ADC | SENS_IN, CO, NH3, NO2 | Radar IF amp (SENS_IN), MICS-6814 (gas) |
| UART | TXD, RXD | Wi-SUN module |

## Vitals proxy (not clinical HR)

Goal: coarse **rising / falling / stable** band-energy trend while the wearer is still.

| Signal | Role | HW change? |
|--------|------|------------|
| **SENS_IN** | Primary — PD-V12 radar IF after board amp (~60 dB class) | No — already on ADC |
| **LIS3DH** | Optional motion gate / micro-motion | No — enable when mounted |
| **PIR_OUT** | Not required. Same chain as SENS_IN; SW edge on SENS residual approximates it | Avoid PIO rework |

Cadence:

- Gas full scan: `SMART_HELMET_ADC_PERIOD_MS` (default 1000 ms)
- SENS_IN-only: `SMART_HELMET_ADC_SENS_PERIOD_MS` (default 40 ms → 25 Hz)

Status: `SmartHelmet_VitalsGetStatus()` — includes `hr_bpm_peak`, `hr_bpm_fft`, fused `hr_bpm` (proxy only).

While calm, SENS residual also runs:
- **Peak detect** — local maxima / IBI → `hr_bpm_peak` (median IBI + regularity check)
- **64-pt fixed-point FFT** — dominant bin in ~40–180 BPM → `hr_bpm_fft` (Hann window, parabolic bin interpolation, fractional-bin EMA)
- **Autocorrelation** (optional) — strongest periodicity in the HR band → `hr_bpm_ac`, with `hr_ac_q8` as rhythm quality
- **Guards** — energy/sa band (`HR_MIN/MAX_ENERGY`, `HR_MIN/MAX_SENS_ABS`); SENS DC spike; post-ACTIVE/`tr=0` holdoff (`HR_DISTURB_HOLDOFF_WIN`); `hr_valid` needs estimator agreement + max ΔBPM step (re-syncs after `HR_DELTA_RESYNC_WIN` rejections so a genuine fast change is not locked out)

### Selectable options (`smart_helmet_config.h`)

Each one is an independent `#ifndef` switch, so a build can fall back to the
previous behaviour or trade CPU for robustness.

| Option | Default | Effect |
|--------|---------|--------|
| `SMART_HELMET_ENABLE_HR_BANDPASS` | 1 | Adds a 1-pole LP (`HR_LP_ALPHA_Q8`) after the HP so only the pulse band feeds the estimators. `band_energy` / `sa` stay on the HP signal, so existing thresholds are unchanged |
| `SMART_HELMET_HR_FFT_WINDOW_HANN` | 1 | Hann window before the FFT (less leakage) |
| `SMART_HELMET_HR_FFT_PRESCALE` | 1 | Scales the frame up before the FFT; without it a residual of a few tens of mV is truncated to zero by the per-stage ÷2 |
| `SMART_HELMET_HR_FFT_INTERP` | 1 | Parabolic peak interpolation + fractional-bin BPM. The raw bin grid is 23.4 BPM at 25 Hz / N=64, which is why the old estimate hopped between 47/70/94 BPM |
| `SMART_HELMET_ENABLE_HR_AUTOCORR` | 0 | Third estimator based on periodicity (`HR_AC_MIN_Q8` threshold). Most accurate of the three on synthetic tests, at the cost of ~3 k MACs per window |
| `SMART_HELMET_HR_PEAK_IBI_MEDIAN` | 1 | Median instead of mean IBI; `HR_IBI_SPREAD_PCT` drops irregular peak trains |
| `SMART_HELMET_HR_FUSE_MODE` | 0 | 0 = legacy pairwise peak/FFT agreement, 1 = consensus across every enabled estimator (`HR_FUSE_MIN_AGREE` must sit within `HR_AGREE_PCT` of the median) |
| `SMART_HELMET_LIS3DH_FS_G` / `_ODR_SEL` | 2 g / 100 Hz | LIS3DH full scale and ODR. Samples are converted to **mg** and BDU is set, so `MOTION_RMS_MG` is meaningful |

#### Acquisition / time base

| Option | Default | Effect |
|--------|---------|--------|
| `SMART_HELMET_ADC_SENS_PRIORITY` | 1 | A SENS tick that collides with the 1 s gas scan is **dropped and reported** instead of being deferred and fired immediately afterwards. The old behaviour bunched two samples together once per second, modulating the sampling interval at exactly 1 Hz — a 60 BPM artefact in the middle of the search band. Also stops the gas scan's own SENS channel from inserting an off-grid sample |
| `SMART_HELMET_ENABLE_VITALS_TIMESTAMP` | 1 | Each SENS sample carries `VmGetTimerTime()`. The window's real sample rate and mean jitter are measured and used for **every** BPM conversion, and windows outside `VITALS_FS_TOL_PCT` / `VITALS_JITTER_PCT` are rejected (`why=17`). On a synthetic 10 % timer drift this returns 65 BPM against a true 66, where the untimestamped path reports 72 |
| `SMART_HELMET_VITALS_FS_TOL_PCT` | 25 | Max deviation of the measured rate from `VITALS_FS_HZ` before the window is rejected |
| `SMART_HELMET_VITALS_JITTER_PCT` | 20 | Max mean sampling jitter (percent of the mean interval) before rejection |
| `SMART_HELMET_ENABLE_SENS_OVERSAMPLE` | 0 | Takes `SENS_OVERSAMPLE_N` back-to-back conversions per tick and averages them. Trades ADC time for ~√N less quantisation/white noise; the delivered rate is unchanged |
| `SMART_HELMET_SENS_OVERSAMPLE_N` | 4 | Conversions averaged per delivered sample |

#### Respiration

| Option | Default | Effect |
|--------|---------|--------|
| `SMART_HELMET_ENABLE_RESP` | 1 | Decimates SENS by `RESP_DECIM` into a `RESP_WIN` buffer (~20 s) and autocorrelates over `RESP_BPM_MIN..MAX`. Respiration is a far stronger radar return than pulse, so it is both useful on its own and a cheap sanity check that a torso is actually in the beam. Reported as `resp_bpm` / `resp_q8` / `resp_valid` |
| `SMART_HELMET_RESP_BPM_MIN` / `_MAX` | 8 / 30 | Respiration search band (breaths/min) |
| `SMART_HELMET_RESP_DECIM` | 8 | Decimation factor (25 Hz → ~3.1 Hz) |
| `SMART_HELMET_RESP_WIN` | 64 | Decimated buffer length |
| `SMART_HELMET_RESP_MIN_Q8` | 64 | Minimum autocorrelation quality for `resp_valid` |
| `SMART_HELMET_HR_REQUIRE_RESP` | 0 | When 1, `hr_valid` additionally requires a plausible respiration (`why=18`). Strongest false-positive rejection, at the cost of needing ~20 s of calm data |

#### Estimator robustness

| Option | Default | Effect |
|--------|---------|--------|
| `SMART_HELMET_HR_FFT_HPS` | 0 | Scores FFT bins by the geometric mean of `P(k)` and `P(2k)` (harmonic product spectrum) instead of `P(k)` alone. Fixes the case where the pulse waveform's first harmonic is stronger than its fundamental — on a synthetic 60 BPM signal whose 2nd harmonic dominates, plain FFT reports 117 BPM while HPS recovers 65. **Deliberately penalises harmonic-free lines**, so leave it off for near-sinusoidal returns |
| `SMART_HELMET_ENABLE_HR_CUSUM` | 1 | Tracks a slow personal BPM baseline and runs a two-sided CUSUM on the deviation. This is what actually answers "has the pulse *changed*" — a small but sustained shift is flagged (`hr_change`) even though the absolute BPM is only a proxy. Reported as `hr_baseline_bpm`, `hr_delta_bpm`, `hr_cusum_up/dn`, `hr_change` |
| `SMART_HELMET_HR_BASELINE_ALPHA_Q8` | 12 | Baseline EMA rate once learned |
| `SMART_HELMET_HR_BASELINE_MIN_WIN` | 8 | Valid windows to learn the baseline before change events are raised |
| `SMART_HELMET_HR_CUSUM_SLACK_BPM` | 3 | Deviation absorbed before the CUSUM accumulates (noise dead-band) |
| `SMART_HELMET_HR_CUSUM_LIMIT` | 24 | CUSUM threshold for a change event |
| `SMART_HELMET_HR_CUSUM_SLACK_PCT` | 5 | Rate-proportional slack component; the effective slack is `max(SLACK_BPM, baseline * PCT / 100)`. Estimator scatter grows with rate, so a fixed 3 BPM dead-band that is correct at 60 BPM fires spuriously at 100 BPM. 5 % reproduces the old slack at 60 BPM and widens it above. Set to 0 for the fixed slack only |
| `SMART_HELMET_HR_BASELINE_STALE_WIN` | 15 | Consecutive windows without a usable estimate after which the baseline is discarded and re-learned. Without it the baseline outlives an arbitrarily long signal loss, so a subject who steps away and returns at a slightly different rate trips an immediate false `rising` — the single largest false-alarm source found in testing. 0 restores the old behaviour |

#### Motion handling (requires `SMART_HELMET_ENABLE_LIS3DH=1`)

Both options below are inert when the accelerometer is depopulated or not
detected; the SENS-only build behaves exactly as before.

| Option | Default | Effect |
|--------|---------|--------|
| `SMART_HELMET_LIS3DH_USE_FIFO` | 0 | Puts the LIS3DH in **stream** mode and drains up to `LIS3DH_FIFO_BURST` samples per poll. The part then buffers at its own ODR, so the motion reference has an even time base even though `SmartHelmet_PollSensors()` is called irregularly. **Precondition for a fully valid `ENABLE_HR_MOTION_ADAPT`** |
| `SMART_HELMET_LIS3DH_FIFO_BURST` | 16 | Max samples drained per poll |
| `SMART_HELMET_ENABLE_HR_MOTION_ADAPT` | 0 | NLMS adaptive filter (`HR_ADAPT_TAPS`, `HR_ADAPT_MU_Q8`) that subtracts the accel-correlated component from the SENS residual instead of discarding the window. The motion gate then relaxes to `MOTION_RMS_MG_ADAPT`, and raw accel energy is no longer folded into `band_energy`. On a synthetic 2.3 Hz head-sway test it removes 82 % of the disturbance energy and recovers the true 66 BPM, where the un-cancelled path rejects the window outright (`why=16`). Without FIFO mode the accel/SENS alignment is only approximate |
| `SMART_HELMET_HR_ADAPT_REF_AXES` | 3 | Reference channels for the canceller. `3` uses the per-axis high-passed x/y/z, which is signed and linear in the disturbance. `1` uses the high-passed vector magnitude (original behaviour) — gravity dominates the magnitude, so a lateral sway appears rectified and at twice its real frequency and a linear filter cannot subtract it. On the 2.3 Hz sway scenario, availability is 0.4 % with `1` and ~71 % with `3` |
| `SMART_HELMET_HR_ADAPT_TAPS` | 8 | Adaptive filter length |
| `SMART_HELMET_HR_ADAPT_MU_Q8` | 32 | NLMS step size (Q8). Larger converges faster but tracks noise |
| `SMART_HELMET_MOTION_RMS_MG_ADAPT` | 250 | Relaxed motion gate used while the canceller is converged |

Recommended for best change-detection: `ENABLE_HR_AUTOCORR=1` with
`HR_FUSE_MODE=1` (two of three estimators must agree), plus
`ENABLE_VITALS_TIMESTAMP=1`, `ADC_SENS_PRIORITY=1` and `ENABLE_HR_CUSUM=1`.
Note that the *change* flag (`hr_change`) is the meaningful output of this
module — the absolute `hr_bpm` remains a proxy.

Calm log: `ok=` (1=meaningful pulse proxy) `why=` reason code + second line text.

| why | meaning |
|-----|---------|
| 0 OK | peak+FFT agree, guards passed |
| 1 warm | calm buffer / windows not ready |
| 2 E_low | energy below min SNR |
| 3 E_high | energy above max (motion-like) |
| 4 sa_low | SENS residual too weak |
| 5 sa_high | SENS residual motion-like |
| 6 dc_spk | SENS DC jump |
| 7 hold | post-disturbance holdoff |
| 8 no_pk | no peaks |
| 9 no_fft | no FFT bin |
| 10 disagr | peak vs FFT disagree |
| 11 pk_only | peak without FFT agree |
| 12 fft_only | FFT without peaks |
| 13 dBPM | jump vs last valid BPM |
| 14 none | no estimate |
| 15 off | HR compile-disabled |
| 16 motion | motion gate active (accel RMS / SENS residual) |
| 17 timebase | measured sample rate / jitter out of tolerance |
| 18 no_resp | no plausible respiration (`HR_REQUIRE_RESP`) |

Not medical-grade.

### Test / observability

See [TESTING.md](TESTING.md) for the full test plan and
[`headset/test/smart_helmet`](../../test/smart_helmet/README.md) for the
automated host regression suite (`run_tests.sh`).

| Option | Default | Effect |
|--------|---------|--------|
| `SMART_HELMET_ENABLE_VITALS_LOG2` | 1 | Second calm-log line carrying the change-detection state: `chg`, `base`, `d`, `cup`, `cdn`, `resp`, `rq`, `fs`, `jit`, `drop`, `rem`, `act`. Split from the main line because the log macro takes a limited argument count |
| `SMART_HELMET_ENABLE_VITALS_CSV` | 0 | One CSV record per processed window through the sink registered with `SmartHelmet_VitalsSetSink()`. Column order is fixed and documented in the test README |
| `SMART_HELMET_ENABLE_SENS_DUMP` | 0 | Raw SENS samples as `seq,mv,time_us`. A capture can be replayed through the host harness, so the DSP can be retuned against real waveforms without hardware. High bandwidth — not for production builds |
| `SMART_HELMET_TELEMETRY_LINE_MAX` | 160 | Maximum telemetry record size |

## Integration

1. Edit **PIO / ADC source** placeholders in `smart_helmet_config.h` to match the QCC3044 netlist.
2. Add the `.c` files under this folder to `headset.x2p` (or SCons source list).
3. From headset init:

```c
#include "smart_helmet.h"

SmartHelmet_Init(appGetAppTask());
/* later */
SmartHelmet_PollSensors();
```

4. CCS811 and MLX90614 both commonly use address `0x5A` — confirm PCB addressing if both are on I2C0.
5. `SMART_HELMET_ENABLE_LIS3DH` stays 0 while the accelerometer is depopulated; vitals still run on SENS_IN alone.
6. The motion gate uses the RMS of the accel magnitude **around its own mean**, so the 1 g gravity component does not force a permanent ACTIVE state.
