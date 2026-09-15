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
- **64-pt fixed-point FFT** — dominant bin in ~40–180 BPM → `hr_bpm_fft`

FFT helps when beats are irregular or buried in noise (frequency peak vs missed time peaks). Not medical-grade.

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
