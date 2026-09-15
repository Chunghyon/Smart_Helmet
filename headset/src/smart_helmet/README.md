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
- **Peak detect** — local maxima / IBI → `hr_bpm_peak`
- **64-pt fixed-point FFT** — dominant bin in ~40–180 BPM → `hr_bpm_fft` (bin EMA smoothed)
- **Guards** — energy/sa band (`HR_MIN/MAX_ENERGY`, `HR_MIN/MAX_SENS_ABS`); SENS DC spike; post-ACTIVE/`tr=0` holdoff (`HR_DISTURB_HOLDOFF_WIN`); `hr_valid` needs peak↔FFT agree + max ΔBPM step

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
