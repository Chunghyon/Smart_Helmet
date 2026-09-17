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

Recommended for best change-detection: `ENABLE_HR_AUTOCORR=1` with
`HR_FUSE_MODE=1` (two of three estimators must agree).

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

Not medical-grade.

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
