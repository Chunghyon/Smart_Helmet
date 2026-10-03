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
/* Query WS8856FLS over UART (param / ip) and log whether the link answers. */
#ifndef SMART_HELMET_ENABLE_WISUN_LINK_CHECK
#define SMART_HELMET_ENABLE_WISUN_LINK_CHECK (1)
#endif
/* CJMCU-8118 CCS811 (gas) on I2C0 */
#ifndef SMART_HELMET_ENABLE_CCS811
#define SMART_HELMET_ENABLE_CCS811         (0)
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
/*! LIS3DH output data rate selector (CTRL_REG1 ODR field, 0x5 = 100 Hz). */
#ifndef SMART_HELMET_LIS3DH_ODR_SEL
#define SMART_HELMET_LIS3DH_ODR_SEL        (0x5)
#endif
/*! LIS3DH full scale in g: 2, 4, 8 or 16. Sets CTRL_REG4 FS and mg/LSB. */
#ifndef SMART_HELMET_LIS3DH_FS_G
#define SMART_HELMET_LIS3DH_FS_G           (2)
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
#define SMART_HELMET_WISUN_RX_BUF_SIZE     (512)
#define SMART_HELMET_WISUN_TX_BUF_MIN      (64)

/* WS8856FLS (Silent Smart, same CLI family as WS8854FLS):
 * text commands, lowercase, terminated by CR LF. No AT+ prefix.
 *   param  -> role, network status, TX power, PHY
 *   ip     -> module IPv6
 * Boot can take tens of seconds before status=5 (online). The check only
 * proves the UART and that a WS8856-family CLI answered. */
#ifndef SMART_HELMET_WISUN_LINK_BOOT_MS
#define SMART_HELMET_WISUN_LINK_BOOT_MS    (800)
#endif
#ifndef SMART_HELMET_WISUN_LINK_TIMEOUT_MS
#define SMART_HELMET_WISUN_LINK_TIMEOUT_MS (1500)
#endif
#ifndef SMART_HELMET_WISUN_LINK_RETRIES
#define SMART_HELMET_WISUN_LINK_RETRIES    (8)
#endif

/* AT_CommandTXT (repo root): lowercase CLI, CR LF, no AT+ prefix.
 * Bring-up sends reset, waits for "Router start", then probes read commands
 * only (version/role/param/mac/ip/fstat/domain/cca/txpower/pan/chrate/
 * chconfig/neighbor). clear/clrst/save/svrst/exit/udps/ping are not sent. */
#ifndef SMART_HELMET_WISUN_AT_RESET_FIRST
#define SMART_HELMET_WISUN_AT_RESET_FIRST  (1)
#endif
#ifndef SMART_HELMET_WISUN_RESET_WAIT_MS
#define SMART_HELMET_WISUN_RESET_WAIT_MS   (20000)
#endif
#ifndef SMART_HELMET_WISUN_RESET_RETRIES
#define SMART_HELMET_WISUN_RESET_RETRIES   (2)
#endif
/* param/ip replies arrive in several UART chunks. Wait this long after the
 * last byte before judging or sending the next command. */
#ifndef SMART_HELMET_WISUN_REPLY_SETTLE_MS
#define SMART_HELMET_WISUN_REPLY_SETTLE_MS (400)
#endif
/* Drain the UART source even if MORE_DATA was missed. */
#ifndef SMART_HELMET_WISUN_RX_POLL_MS
#define SMART_HELMET_WISUN_RX_POLL_MS      (50)
#endif

/* Join profile of the live border/router (param dump 2026-10-03).
 * MAC and IPv6 stay device-unique and are never written. */
#ifndef SMART_HELMET_WISUN_PROVISION
#define SMART_HELMET_WISUN_PROVISION       (1)
#endif
#define SMART_HELMET_WISUN_NETNAME         "ws_wisun_net"
#define SMART_HELMET_WISUN_PAN_HEX         "abcd"
#define SMART_HELMET_WISUN_DOMAIN          "1"
#define SMART_HELMET_WISUN_CHRATE_KBPS     "50"
#define SMART_HELMET_WISUN_CLASS           "1"
#define SMART_HELMET_WISUN_TXPOWER_DBM     "20"
#define SMART_HELMET_WISUN_CCA_DBM         "-83"
#define SMART_HELMET_WISUN_UDP_PORT        "1234"

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

/*!
 * Motion gate: RMS of the accel magnitude around its own mean (i.e. with the
 * ~1000 mg gravity component removed) above this (mg) => ACTIVITY.
 */
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

/*!
 * Consecutive windows rejected by the ΔBPM guard before the reference is
 * dropped and the estimator re-syncs. Without it a genuine fast change
 * (e.g. 65 -> 105 BPM) would be rejected forever. 0 disables the re-sync.
 */
#ifndef SMART_HELMET_HR_DELTA_RESYNC_WIN
#define SMART_HELMET_HR_DELTA_RESYNC_WIN   (3)
#endif

/* --- Optional DSP improvements (each independently selectable) ----------- */

/*!
 * Band-pass the SENS residual instead of high-pass only.
 * 1 = HP (0.2 Hz) + 1-pole LP so respiration harmonics / wideband noise
 * outside the pulse band do not feed the peak / FFT / autocorrelation stages.
 * 0 = legacy high-pass only.
 */
#ifndef SMART_HELMET_ENABLE_HR_BANDPASS
#define SMART_HELMET_ENABLE_HR_BANDPASS    (1)
#endif

/*!
 * LP smoothing factor (Q8): y += (x - y) * ALPHA / 256.
 * 96/256 ≈ 0.375 => fc ≈ 1.9 Hz at 25 Hz (keeps 40–180 BPM, cuts >3 Hz).
 */
#ifndef SMART_HELMET_HR_LP_ALPHA_Q8
#define SMART_HELMET_HR_LP_ALPHA_Q8        (96)
#endif

/*!
 * Hann window before the FFT. Reduces spectral leakage from the
 * non-integer number of beats inside the 64-sample frame.
 * 0 = rectangular (legacy).
 */
#ifndef SMART_HELMET_HR_FFT_WINDOW_HANN
#define SMART_HELMET_HR_FFT_WINDOW_HANN    (1)
#endif

/*!
 * Normalise the FFT input to use the full int32 headroom.
 * The radix-2 butterfly scales by 1/2 per stage, so small residuals
 * (tens of mV) would otherwise be truncated to zero after 6 stages.
 * 0 = legacy (no pre-scaling).
 */
#ifndef SMART_HELMET_HR_FFT_PRESCALE
#define SMART_HELMET_HR_FFT_PRESCALE       (1)
#endif

/*!
 * Parabolic interpolation of the dominant FFT bin plus fractional-bin BPM.
 * Without it the BPM grid is fs*60/N = 23.4 BPM at 25 Hz / N=64, which is
 * why the estimate hops between 47/70/94 BPM.
 * 0 = legacy integer-bin BPM.
 */
#ifndef SMART_HELMET_HR_FFT_INTERP
#define SMART_HELMET_HR_FFT_INTERP         (1)
#endif

/*!
 * Autocorrelation estimator on the residual. Periodicity based, so it is
 * robust for "is the pulse changing?" even when individual peaks are weak.
 * Adds a third opinion to the fusion stage.
 */
#ifndef SMART_HELMET_ENABLE_HR_AUTOCORR
#define SMART_HELMET_ENABLE_HR_AUTOCORR    (0)
#endif

/*! Min autocorrelation peak vs r(0) (Q8) before the AC BPM is accepted. */
#ifndef SMART_HELMET_HR_AC_MIN_Q8
#define SMART_HELMET_HR_AC_MIN_Q8          (77)   /* ~0.30 */
#endif

/*!
 * Peak-detector inter-beat interval statistic.
 * 1 = median IBI (robust to one missed / doubled beat)
 * 0 = mean IBI (legacy)
 */
#ifndef SMART_HELMET_HR_PEAK_IBI_MEDIAN
#define SMART_HELMET_HR_PEAK_IBI_MEDIAN    (1)
#endif

/*!
 * Max IBI dispersion (percent of the chosen IBI) still considered a regular
 * rhythm. Above it the peak estimate is dropped. 0 disables the check.
 */
#ifndef SMART_HELMET_HR_IBI_SPREAD_PCT
#define SMART_HELMET_HR_IBI_SPREAD_PCT     (35)
#endif

/*!
 * Fusion strategy:
 *  0 = legacy pairwise peak/FFT agreement
 *  1 = consensus across every enabled estimator (peak / FFT / autocorrelation):
 *      take the median candidate and require HR_FUSE_MIN_AGREE of them to sit
 *      within HR_AGREE_PCT of it.
 */
#ifndef SMART_HELMET_HR_FUSE_MODE
#define SMART_HELMET_HR_FUSE_MODE          (0)
#endif

/*! Estimators that must agree in consensus mode (HR_FUSE_MODE == 1). */
#ifndef SMART_HELMET_HR_FUSE_MIN_AGREE
#define SMART_HELMET_HR_FUSE_MIN_AGREE     (2)
#endif

/* ========================================================================== */
/* Priority 1 — sampling integrity + presence sanity check                    */
/* ========================================================================== */

/*!
 * P1-1: give the SENS_IN (vitals) conversion priority over the gas scan.
 *
 * Legacy behaviour (0): a SENS tick that lands while the 4-channel gas scan is
 * running is deferred and then fired immediately after the scan finishes. The
 * gas scan repeats every SMART_HELMET_ADC_PERIOD_MS (1000 ms), so the SENS
 * sample spacing is modulated at exactly 1 Hz — i.e. 60 BPM, right in the
 * middle of the HR search band. The estimators can lock onto that artifact.
 *
 * With 1 the gas scan is postponed instead, and a SENS tick that still cannot
 * run is reported as a dropped sample rather than being bunched up.
 */
#ifndef SMART_HELMET_ADC_SENS_PRIORITY
#define SMART_HELMET_ADC_SENS_PRIORITY     (1)
#endif

/*!
 * P1-2: timestamp every SENS sample and derive the effective sample rate from
 * the measured intervals instead of trusting SMART_HELMET_VITALS_FS_HZ.
 * MessageSendLater is not a precision timer, so the nominal 40 ms can drift.
 */
#ifndef SMART_HELMET_ENABLE_VITALS_TIMESTAMP
#define SMART_HELMET_ENABLE_VITALS_TIMESTAMP (1)
#endif

/*!
 * Max mean sampling-interval error (percent of nominal) still usable.
 * Beyond this the BPM scale would be wrong, so the window is rejected.
 */
#ifndef SMART_HELMET_VITALS_FS_TOL_PCT
#define SMART_HELMET_VITALS_FS_TOL_PCT     (25)
#endif

/*!
 * Max mean absolute jitter (percent of the measured interval) still usable.
 * Above this the time base is too irregular for a frequency estimate.
 */
#ifndef SMART_HELMET_VITALS_JITTER_PCT
#define SMART_HELMET_VITALS_JITTER_PCT     (20)
#endif

/*!
 * P1-3: respiration estimate from the SENS envelope (0.15-0.5 Hz).
 * Respiration is 10-100x stronger than the pulse component in a Doppler radar
 * IF signal, so it is both a useful vital on its own and the cheapest possible
 * "is a person actually in front of the sensor and measurable" check.
 */
#ifndef SMART_HELMET_ENABLE_RESP
#define SMART_HELMET_ENABLE_RESP           (1)
#endif

/*! Respiration search band (breaths per minute). */
#define SMART_HELMET_RESP_BPM_MIN          (8)
#define SMART_HELMET_RESP_BPM_MAX          (30)

/*! SENS decimation factor feeding the respiration buffer (25 Hz / 8 ~= 3 Hz). */
#define SMART_HELMET_RESP_DECIM            (8)

/*! Respiration analysis buffer length (64 @ ~3 Hz ~= 20 s). */
#define SMART_HELMET_RESP_WIN              (64)

/*! Min normalised autocorrelation (Q8) before a respiration rate is accepted. */
#ifndef SMART_HELMET_RESP_MIN_Q8
#define SMART_HELMET_RESP_MIN_Q8           (64)   /* ~0.25 */
#endif

/*!
 * 1 = hr_valid additionally requires a plausible respiration rate.
 * Blocks pulse reports when nobody is in front of the radar or the wearer is
 * sitting in a Doppler null, both of which otherwise look like "weak pulse".
 */
#ifndef SMART_HELMET_HR_REQUIRE_RESP
#define SMART_HELMET_HR_REQUIRE_RESP       (0)
#endif

/* ========================================================================== */
/* Priority 2 — signal conditioning + estimator / output quality              */
/* ========================================================================== */

/*!
 * P2-1: oversample SENS_IN and average before handing samples to the vitals
 * chain. Acts as an anti-aliasing filter and drops ADC noise by ~sqrt(N).
 * The effective vitals rate stays SMART_HELMET_VITALS_FS_HZ.
 */
#ifndef SMART_HELMET_ENABLE_SENS_OVERSAMPLE
#define SMART_HELMET_ENABLE_SENS_OVERSAMPLE (0)
#endif

/*! Conversions averaged per delivered vitals sample (1 = off). */
#ifndef SMART_HELMET_SENS_OVERSAMPLE_N
#define SMART_HELMET_SENS_OVERSAMPLE_N     (4)
#endif

/*!
 * P2-2: harmonic product spectrum. Score each candidate bin with
 * P(k) * P(2k) so a strong second harmonic cannot be mistaken for the
 * fundamental (and vice versa). Reuses the existing FFT output.
 */
#ifndef SMART_HELMET_HR_FFT_HPS
#define SMART_HELMET_HR_FFT_HPS            (0)
#endif

/*!
 * P2-3: track a slow personal BPM baseline and run a two-sided CUSUM on the
 * deviation. This is the actual "has the pulse changed?" output — far more
 * sensitive to small sustained drifts than a fixed threshold, and far less
 * prone to false alarms than comparing single windows.
 */
#ifndef SMART_HELMET_ENABLE_HR_CUSUM
#define SMART_HELMET_ENABLE_HR_CUSUM       (1)
#endif

/*! BPM baseline EMA weight (Q8) applied per valid window. */
#ifndef SMART_HELMET_HR_BASELINE_ALPHA_Q8
#define SMART_HELMET_HR_BASELINE_ALPHA_Q8  (12)  /* ~0.05 */
#endif

/*! Valid windows required before the BPM baseline is trusted. */
#ifndef SMART_HELMET_HR_BASELINE_MIN_WIN
#define SMART_HELMET_HR_BASELINE_MIN_WIN   (8)
#endif

/*! CUSUM slack (BPM). Deviations below this are treated as noise. */
#ifndef SMART_HELMET_HR_CUSUM_SLACK_BPM
#define SMART_HELMET_HR_CUSUM_SLACK_BPM    (3)
#endif

/*! CUSUM alarm level (BPM-windows accumulated past the slack). */
#ifndef SMART_HELMET_HR_CUSUM_LIMIT
#define SMART_HELMET_HR_CUSUM_LIMIT        (24)
#endif

/* ========================================================================== */
/* Priority 3 — accelerometer time base + motion cancellation                 */
/* Both degrade to no-ops when SMART_HELMET_ENABLE_LIS3DH is 0.               */
/* ========================================================================== */

/*!
 * P3-1: read the LIS3DH through its FIFO (stream mode) instead of taking one
 * sample per poll. Gives the accelerometer a regular time base, which is a
 * precondition for using it as a motion reference signal.
 */
#ifndef SMART_HELMET_LIS3DH_USE_FIFO
#define SMART_HELMET_LIS3DH_USE_FIFO       (0)
#endif

/*! Samples drained from the FIFO per poll (LIS3DH FIFO holds 32). */
#ifndef SMART_HELMET_LIS3DH_FIFO_BURST
#define SMART_HELMET_LIS3DH_FIFO_BURST     (16)
#endif

/*!
 * P3-2: NLMS adaptive filter that subtracts the accelerometer-correlated part
 * of the SENS residual, instead of simply discarding every window with motion.
 * Recovers usable windows during light movement, which is most of the time for
 * a helmet. No effect when the LIS3DH is disabled or not detected.
 */
#ifndef SMART_HELMET_ENABLE_HR_MOTION_ADAPT
#define SMART_HELMET_ENABLE_HR_MOTION_ADAPT (0)
#endif

/*!
 * Number of accelerometer reference channels fed to the NLMS canceller.
 *
 *   3 - per-axis high-passed x/y/z (default). Signed and linear in the
 *       disturbance, which is what an adaptive filter needs.
 *   1 - the high-passed vector magnitude (original behaviour). Because
 *       gravity dominates the magnitude, a lateral sway shows up rectified
 *       and at twice its real frequency, so the canceller cannot subtract
 *       it. Kept only for comparison.
 */
#ifndef SMART_HELMET_HR_ADAPT_REF_AXES
#define SMART_HELMET_HR_ADAPT_REF_AXES     (3)
#endif

/*! NLMS filter length (taps over the accel reference history). */
#ifndef SMART_HELMET_HR_ADAPT_TAPS
#define SMART_HELMET_HR_ADAPT_TAPS         (8)
#endif

/*! NLMS step size (Q8). Larger = faster tracking, less stable. */
#ifndef SMART_HELMET_HR_ADAPT_MU_Q8
#define SMART_HELMET_HR_ADAPT_MU_Q8        (32)  /* ~0.125 */
#endif

/*!
 * With motion cancellation active the motion gate can be relaxed, since light
 * movement is now removed rather than rejected. RMS (mg) above this still
 * forces ACTIVITY. Only used when SMART_HELMET_ENABLE_HR_MOTION_ADAPT is 1.
 */
#ifndef SMART_HELMET_MOTION_RMS_MG_ADAPT
#define SMART_HELMET_MOTION_RMS_MG_ADAPT   (250)
#endif

/* -------------------------------------------------------------------------
 * Test / observability options (stage 0 of the pulse-change test plan).
 *
 * These exist so the change-detection behaviour can actually be measured on
 * hardware. All default to off except the extra log line, so a production
 * build is unaffected.
 * ---------------------------------------------------------------------- */

/*!
 * Second calm-log line carrying the change-detection state (hr_change,
 * baseline, CUSUM accumulators, respiration, measured fs / jitter, dropped
 * SENS samples). Split from the main line because the QCC log macro has a
 * limited argument count.
 */
#ifndef SMART_HELMET_ENABLE_VITALS_LOG2
#define SMART_HELMET_ENABLE_VITALS_LOG2    (1)
#endif

/*!
 * Emit one CSV record per processed window through the telemetry sink
 * registered with SmartHelmet_VitalsSetSink(). Intended for long recordings
 * that are analysed offline; see headset/test/smart_helmet/README.md.
 */
#ifndef SMART_HELMET_ENABLE_VITALS_CSV
#define SMART_HELMET_ENABLE_VITALS_CSV     (0)
#endif

/*!
 * Raw SENS sample dump. Every sample (millivolts + capture timestamp) is
 * pushed to the telemetry sink, so a real recording can be replayed against
 * the host harness and the DSP retuned without the hardware.
 *
 * This is the highest-value option for tuning, but it is also the highest
 * bandwidth one: at the default 25 Hz it produces a line every 40 ms.
 */
#ifndef SMART_HELMET_ENABLE_SENS_DUMP
#define SMART_HELMET_ENABLE_SENS_DUMP      (0)
#endif

/*! Max bytes of a single telemetry record (CSV line or dump line). */
#ifndef SMART_HELMET_TELEMETRY_LINE_MAX
#define SMART_HELMET_TELEMETRY_LINE_MAX    (160)
#endif

/* -------------------------------------------------------------------------
 * Change-detection robustness (found by the host regression harness, see
 * headset/test/smart_helmet).
 * ---------------------------------------------------------------------- */

/*!
 * Number of consecutive windows without a usable pulse estimate after which
 * the CUSUM baseline is discarded and re-learned from scratch.
 *
 * Without this the baseline survives an arbitrarily long signal loss, so a
 * subject who steps away and returns at a slightly different rate trips an
 * immediate false "rising". At the default 1 s window, 15 means ~15 s of
 * signal loss. Set to 0 to keep the baseline forever (old behaviour).
 */
#ifndef SMART_HELMET_HR_BASELINE_STALE_WIN
#define SMART_HELMET_HR_BASELINE_STALE_WIN (15)
#endif

/*!
 * Rate-proportional component of the CUSUM slack, in percent of the current
 * baseline. The effective slack is the larger of
 * SMART_HELMET_HR_CUSUM_SLACK_BPM and baseline * this / 100.
 *
 * Estimator scatter grows with rate, so a fixed 3 BPM slack that is correct
 * at 60 BPM is too tight at 100 BPM and produces false alarms there. 5 %
 * reproduces the old slack at 60 BPM and widens it above that. Set to 0 to
 * use the fixed slack only.
 */
#ifndef SMART_HELMET_HR_CUSUM_SLACK_PCT
#define SMART_HELMET_HR_CUSUM_SLACK_PCT    (5)
#endif

#endif /* SMART_HELMET_CONFIG_H */
