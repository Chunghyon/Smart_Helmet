/*!
\file       smart_helmet_config.h
\brief      Board-level pin / bus map for Smart Helmet (QCC3044)

Derived from OrCAD schematic SMART_HELMET_260816_1.DSN net names:
  I2C0  SCL/SDA   -> CJMCU-8118 (CCS811+HDC1080), LIS3DH, GY-906-BAA (MLX90614), SSD1315(opt)
  I2C1  SCL1/SDA1 -> SSD1315(opt) alternate bus
  ADC   SENS_IN, CO, NH3, NO2  (MICS-6814 / air quality path)
  UART  TXD/RXD   -> Wi-SUN module

PIO numbers below are PLACEHOLDERS. Match them to the QCC3044 netlist
before building — schematic labels such as P3.4/P3.5 are not QCC PIO ids.
*/

#ifndef SMART_HELMET_CONFIG_H
#define SMART_HELMET_CONFIG_H

#include <csrtypes.h>
#include <adc.h>

/*! Feature compile switches */
#ifndef SMART_HELMET_ENABLE_I2C0
#define SMART_HELMET_ENABLE_I2C0           (1)
#endif
#ifndef SMART_HELMET_ENABLE_I2C1
#define SMART_HELMET_ENABLE_I2C1           (1)
#endif
#ifndef SMART_HELMET_ENABLE_ADC
#define SMART_HELMET_ENABLE_ADC            (1)
#endif
#ifndef SMART_HELMET_ENABLE_WISUN_UART
#define SMART_HELMET_ENABLE_WISUN_UART     (1)
#endif
/* CJMCU-8118 CCS811 (gas) on I2C0 */
#ifndef SMART_HELMET_ENABLE_CCS811
#define SMART_HELMET_ENABLE_CCS811         (1)
#endif
/* CJMCU-8118 HDC1080 (temp/RH) on I2C0 */
#ifndef SMART_HELMET_ENABLE_HDC1080
#define SMART_HELMET_ENABLE_HDC1080        (0)
#endif
/* GY-906-BAA = MLX90614 IR thermometer on I2C0 */
#ifndef SMART_HELMET_ENABLE_MLX90614
#define SMART_HELMET_ENABLE_MLX90614       (0)
#endif
/* LIS3DH on I2C0. Set to 0 while the part is depopulated or holding
 * SCL/SDA low; set back to 1 after the accelerometer is remounted. */
#ifndef SMART_HELMET_ENABLE_LIS3DH
#define SMART_HELMET_ENABLE_LIS3DH         (0)
#endif
/* SSD1315 128x64 OLED. 1 = probe + splash on I2C1 (default). */
#ifndef SMART_HELMET_ENABLE_SSD1315
#define SMART_HELMET_ENABLE_SSD1315        (0)
#endif
/*! Put SSD1315 on I2C1 when 1, else share I2C0 */
#ifndef SMART_HELMET_SSD1315_ON_I2C1
#define SMART_HELMET_SSD1315_ON_I2C1       (1)
#endif
/* Re-issue I2C probes every N ms until enabled devices return the expected
 * payload (CCS811 HW_ID, HDC1080 MFG/DEV ID, MLX TA, LIS3DH WHO_AM_I). */
#ifndef SMART_HELMET_I2C_PROBE_RETRY_MS
#define SMART_HELMET_I2C_PROBE_RETRY_MS    (1000)
#endif

/* -------------------------------------------------------------------------- */
/* I2C0 — SCL / SDA (bitserial block 0)                                       */
/* Schematic: SCL, SDA  (annotated P3.5 / P3.4 on reference sheet)            */
/* -------------------------------------------------------------------------- */
#define SMART_HELMET_I2C0_SCL_PIO          (20)
#define SMART_HELMET_I2C0_SDA_PIO          (19)
#define SMART_HELMET_I2C0_SPEED_KHZ        (100)
#define SMART_HELMET_I2C0_BITSERIAL_BLOCK  (BITSERIAL_BLOCK_0)

/* Device 7-bit addresses on I2C0 */
#define SMART_HELMET_ADDR_CCS811           (0x5B)  /* CJMCU-8118 CCS811; 0x5A if ADDR low */
#define SMART_HELMET_ADDR_HDC1080          (0x40)  /* CJMCU-8118 HDC1080 (fixed) */
#define SMART_HELMET_ADDR_LIS3DH           (0x18)  /* SA0=GND; 0x19 if high */
#define SMART_HELMET_ADDR_MLX90614         (0x5A)  /* GY-906-BAA; note: may clash
                                                    * with CCS811 if both 0x5A —
                                                    * verify PCB ADDR / SA pin. */
#define SMART_HELMET_ADDR_SSD1315          (0x3C)

/* -------------------------------------------------------------------------- */
/* I2C1 — SCL1 / SDA1 (bitserial block 1, optional OLED)                      */
/* -------------------------------------------------------------------------- */
#define SMART_HELMET_I2C1_SCL_PIO          (16)
#define SMART_HELMET_I2C1_SDA_PIO          (15)
#define SMART_HELMET_I2C1_SPEED_KHZ        (100)
#define SMART_HELMET_I2C1_BITSERIAL_BLOCK  (BITSERIAL_BLOCK_1)

/* -------------------------------------------------------------------------- */
/* ADC inputs — gas / air quality                                             */
/* Map each net to a vm_adc_source_type available on the pad used.            */
/* Default uses LED-pad ADCs; change to match PCB.                            */
/* -------------------------------------------------------------------------- */
#define SMART_HELMET_ADC_SENS_IN           (adcsel_led4)  /* SENS_IN */
#define SMART_HELMET_ADC_CO                (adcsel_led0)  /* CO (MICS-6814) */
#define SMART_HELMET_ADC_NH3               (adcsel_led1)  /* NH3 */
#define SMART_HELMET_ADC_NO2               (adcsel_led2)  /* NO2 */

/*! Extra settling time after enabling sensor bias (ms), if any */
#define SMART_HELMET_ADC_SETTLE_MS         (5)
/*! Full gas scan period (SENS_IN + CO/NH3/NO2). Slow OK for air quality. */
#define SMART_HELMET_ADC_PERIOD_MS         (1000)
/*!
 * SENS_IN-only sample period for vitals proxy.
 * 40 ms => 25 Hz, matches SMART_HELMET_VITALS_FS_HZ. No HW change: same ADC pad.
 * PIR_OUT PIO interrupt is NOT required — SENS_IN is the analog parent signal.
 */
#define SMART_HELMET_ADC_SENS_PERIOD_MS    (40)

/* -------------------------------------------------------------------------- */
/* Wi-SUN UART — TXD / RXD                                                    */
/* Stream UART on QCC uses dedicated UART_TX / UART_RX pin functions.         */
/* -------------------------------------------------------------------------- */
#define SMART_HELMET_WISUN_UART_TX_PIO     (14)
#define SMART_HELMET_WISUN_UART_RX_PIO     (13)
#define SMART_HELMET_WISUN_UART_BAUD       (VM_UART_RATE_115K2)

/*! RX assemble buffer */
#define SMART_HELMET_WISUN_RX_BUF_SIZE     (256)
#define SMART_HELMET_WISUN_TX_BUF_MIN      (64)

/* -------------------------------------------------------------------------- */
/* Vitals proxy (LIS3DH + PD-V12 / SENS_IN) — trend only, not clinical HR    */
/* Primary path: SENS_IN ADC @ VITALS_FS_HZ. LIS3DH optional motion gate.     */
/* PIR_OUT optional; SW edge on SENS_IN residual approximates it (no PIO).   */
/* -------------------------------------------------------------------------- */
#ifndef SMART_HELMET_ENABLE_VITALS_PROXY
#define SMART_HELMET_ENABLE_VITALS_PROXY   (1)
#endif

/*! Sample rate assumed by the proxy (Hz). Match SENS_IN ADC cadence. */
#define SMART_HELMET_VITALS_FS_HZ          (25)

/*! Motion gate: accel magnitude RMS above this (mg) => ACTIVITY */
#define SMART_HELMET_MOTION_RMS_MG         (80)

/*!
 * When LIS3DH is absent: SENS residual energy (mean |hp|) above this
 * (mV-ish units after HP) => treat as ACTIVITY (helmet/head motion).
 */
#define SMART_HELMET_SENS_MOTION_ABS_MV    (80)

/*! Samples in short motion window (~1 s at 25 Hz) */
#define SMART_HELMET_MOTION_WIN            (25)

/*! Samples in band-energy window (~4 s) */
#define SMART_HELMET_BAND_WIN              (100)

/*! Baseline EMA time constant in windows (larger = slower baseline) */
#define SMART_HELMET_BASELINE_ALPHA_Q8     (16)  /* alpha = 16/256 ≈ 0.06 */

/*! Relative rise/fall thresholds vs baseline (percent) */
#define SMART_HELMET_TREND_UP_PCT          (25)
#define SMART_HELMET_TREND_DOWN_PCT        (20)

/*! Consecutive calm windows required before trend is trusted */
#define SMART_HELMET_CALM_WINDOWS_MIN      (3)

/*!
 * Software threshold on |SENS HP| (mV residual) to emulate PIR_OUT edges.
 * 0 disables. No extra pin / HW change.
 */
#define SMART_HELMET_SENS_EDGE_ABS_MV      (40)

/* --- HR peak + FFT proxy (not clinical) ---------------------------------- */
#ifndef SMART_HELMET_ENABLE_HR_PEAK
#define SMART_HELMET_ENABLE_HR_PEAK        (1)
#endif
#ifndef SMART_HELMET_ENABLE_HR_FFT
#define SMART_HELMET_ENABLE_HR_FFT         (1)
#endif

/*! BPM search band */
#define SMART_HELMET_HR_BPM_MIN            (40)
#define SMART_HELMET_HR_BPM_MAX            (180)

/*! Min samples between peaks (~0.33 s at 25 Hz) */
#define SMART_HELMET_HR_MIN_PEAK_DIST      (8)

/*! Peak must exceed mean|hp| * this / 256 */
#define SMART_HELMET_HR_PEAK_THR_Q8        (384)  /* 1.5x */

/*! Power-of-two FFT length on latest SENS HP samples ( <= BAND_WIN ) */
#define SMART_HELMET_HR_FFT_N              (64)

/*! Min FFT bin power vs mean band power (Q8 ratio) for hr_fft accept */
#define SMART_HELMET_HR_FFT_SNR_Q8         (320)  /* ~1.25x mean */

/*! Peak vs FFT agreement window (percent of fused value) */
#define SMART_HELMET_HR_AGREE_PCT          (20)

/*!
 * Min band_energy for any HR estimate. Below this, hr_* cleared (SNR too low).
 * Field logs: usable ~700+, noisy hops ~100–250.
 */
#define SMART_HELMET_HR_MIN_ENERGY         (400)

/*! Optional min mean|SENS HP|; 0 disables. */
#define SMART_HELMET_HR_MIN_SENS_ABS       (12)

/*!
 * FFT bin EMA: new_bin weight = ALPHA/256, hold = (256-ALPHA)/256.
 * Lower ALPHA = stronger temporal smoothing (less bin hop).
 */
#define SMART_HELMET_HR_FFT_SMOOTH_ALPHA_Q8 (48)

/*! Max |Δbin| accepted in one window before treating as hop (then heavier smooth). */
#define SMART_HELMET_HR_FFT_MAX_BIN_JUMP   (2)

/*!
 * Fuse weights when both peak and FFT present: peak * W + fft * (256-W).
 * 192 => 75% peak / 25% FFT.
 */
#define SMART_HELMET_HR_FUSE_PEAK_W_Q8     (192)

/*! Min peaks in window to accept peak-led fuse as hr_valid */
#define SMART_HELMET_HR_MIN_PEAKS_VALID    (3)

/*!
 * Upper bounds: above these = motion/contact residual, not pulse band.
 * Field: good e~700–3000 sa~18–35; spikes e>6k sa>50.
 */
#define SMART_HELMET_HR_MAX_ENERGY         (3500)
#define SMART_HELMET_HR_MAX_SENS_ABS       (45)

/*! |Δsens_mv| vs previous process window => disturbance (DC jump) */
#define SMART_HELMET_HR_SENS_DC_SPIKE_MV   (120)

/*!
 * After ACTIVE / high-e / sens spike / tr unknown exit: skip hr_valid
 * for this many calm process windows (~1 s each).
 */
#define SMART_HELMET_HR_DISTURB_HOLDOFF_WIN (5)

/*!
 * 1 = hr_valid only when peak and FFT agree within HR_AGREE_PCT.
 * Peak-only / disagree may still fill hr_bpm for debug but hv=0.
 */
#define SMART_HELMET_HR_VALID_REQUIRE_AGREE (1)

/*! Max |ΔBPM| from last hr_valid sample; larger step => hv=0 this window */
#define SMART_HELMET_HR_MAX_DELTA_BPM      (25)

#endif /* SMART_HELMET_CONFIG_H */
