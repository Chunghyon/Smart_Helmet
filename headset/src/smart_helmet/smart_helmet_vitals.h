/*!
\file       smart_helmet_vitals.h
\brief      Motion-gated vitals proxy: band energy, peak BPM, FFT BPM

Primary input: SENS_IN (PD-V12 radar IF). Optional LIS3DH motion gate.
Not a clinical heart-rate monitor — coarse proxy only while calm.

Pipeline:
  samples -> motion_gate -> band_energy / peak_detect / FFT -> trend + bpm
*/

#ifndef SMART_HELMET_VITALS_H
#define SMART_HELMET_VITALS_H

#include <csrtypes.h>
#include <stdbool.h>

typedef enum
{
    smart_helmet_motion_calm = 0,
    smart_helmet_motion_active
} smart_helmet_motion_gate_t;

typedef enum
{
    smart_helmet_trend_unknown = 0,
    smart_helmet_trend_stable,
    smart_helmet_trend_rising,
    smart_helmet_trend_falling
} smart_helmet_trend_flag_t;

/*!
 * Why hr_valid is set/cleared this calm window (proxy quality).
 * Logged as why=<tag>; also in hr_reason.
 */
typedef enum
{
	hr_ok = 0,          /*!< peak+FFT agree, guards passed */
	hr_warm,            /*!< calm_windows / buffer not ready */
	hr_e_low,           /*!< band_energy < HR_MIN_ENERGY */
	hr_e_high,          /*!< band_energy > HR_MAX_ENERGY */
	hr_sa_low,          /*!< mean|SENS HP| too small */
	hr_sa_high,         /*!< mean|SENS HP| motion-like */
	hr_dc_spike,        /*!< |Δsens_mv| too large */
	hr_holdoff,         /*!< post-disturbance holdoff */
	hr_no_peak,         /*!< peak detector empty */
	hr_no_fft,          /*!< FFT empty / SNR fail */
	hr_disagree,        /*!< peak vs FFT outside agree % */
	hr_peak_only,       /*!< peak only (need FFT if REQUIRE_AGREE) */
	hr_fft_only,        /*!< FFT only — never hr_valid */
	hr_delta,           /*!< |ΔBPM| vs last valid too large */
	hr_none,            /*!< no estimate this window */
	hr_disabled,        /*!< HR peak/FFT compile-off */
	hr_motion,          /*!< motion gate active (accel RMS / SENS residual) */
	hr_timebase,        /*!< sample timing too irregular / wrong rate */
	hr_no_resp          /*!< no plausible respiration => not measurable */
} smart_helmet_hr_reason_t;

typedef struct
{
    smart_helmet_motion_gate_t motion;
    smart_helmet_trend_flag_t  trend;
    uint16                     motion_rms_mg;
    uint16                     motion_min_mg;
    uint16                     motion_peak_mg;
    uint16                     band_energy;
    uint16                     baseline_energy;
    uint16                     pir_events_win;
    uint16                     sens_mv;
    bool                       valid;           /*!< trend trustworthy */

    /*! Time-domain peak estimate (BPM). 0 if unavailable. */
    uint16                     hr_bpm_peak;
    /*! FFT dominant-bin estimate (BPM). 0 if unavailable. */
    uint16                     hr_bpm_fft;
    /*! Autocorrelation estimate (BPM). 0 if unavailable / compiled out. */
    uint16                     hr_bpm_ac;
    /*! Autocorrelation peak / r(0), Q8 (rhythm quality). */
    uint16                     hr_ac_q8;

    /*! Measured sample rate x100 (Hz). 0 when not timestamped. */
    uint16                     fs_x100;
    /*! Mean sampling jitter as percent of the mean interval. */
    uint8                      fs_jitter_pct;
    /*! SENS samples dropped since the previous window (cadence conflicts). */
    uint8                      sens_dropped;

    /*! Respiration estimate (breaths/min). 0 if unavailable. */
    uint16                     resp_bpm;
    /*! Respiration autocorrelation quality, Q8. */
    uint16                     resp_q8;
    /*! Respiration estimate usable this window. */
    bool                       resp_valid;

    /*! Slow personal BPM baseline. 0 until enough valid windows. */
    uint16                     hr_baseline_bpm;
    /*! Signed deviation of hr_bpm from the baseline (BPM). */
    int16                      hr_delta_bpm;
    /*! Two-sided CUSUM accumulators (rise / fall). */
    uint16                     hr_cusum_up;
    uint16                     hr_cusum_dn;
    /*! Sustained pulse-rate change detected (CUSUM past limit). */
    smart_helmet_trend_flag_t  hr_change;

    /*! Motion energy removed by the adaptive filter, percent. */
    uint8                      adapt_removed_pct;
    /*! Fused BPM when peak/FFT agree or one is strong; else 0. */
    uint16                     hr_bpm;
    uint8                      hr_peak_count;   /*!< peaks in last analysis window */
    uint16                     hr_fft_mag;      /*!< dominant bin |X|^2 (arb) */
    bool                       hr_valid;        /*!< hr_bpm usable this window */
    /*! Primary reason pulse proxy is meaningful or not (see enum). */
    smart_helmet_hr_reason_t   hr_reason;
	uint8                      hr_win_left;      /*!< windows left before hv allowed */
} smart_helmet_vitals_status_t;

void SmartHelmet_VitalsInit(void);
void SmartHelmet_VitalsPushAccel(int16 x_mg, int16 y_mg, int16 z_mg);
void SmartHelmet_VitalsPushSensInMv(uint16 mv);

/*!
 * \brief Push a SENS_IN sample together with its capture time.
 * \param mv     Sample in millivolts.
 * \param time_us Capture timestamp (microseconds, may wrap).
 *
 * Used when SMART_HELMET_ENABLE_VITALS_TIMESTAMP is set so the effective
 * sample rate is measured rather than assumed. SmartHelmet_VitalsPushSensInMv
 * is equivalent to passing no timestamp.
 */
void SmartHelmet_VitalsPushSensInMvAt(uint16 mv, uint32 time_us);

/*! \brief Record SENS samples that could not be taken (cadence conflict). */
void SmartHelmet_VitalsNoteSensDropped(void);

/*!
 * \brief Telemetry sink for CSV records and raw sample dumps.
 * \param line NUL-terminated record, without a trailing newline.
 * \param len  Number of bytes in \p line, excluding the terminator.
 * \param ctx  Opaque pointer supplied at registration time.
 */
typedef void (*smart_helmet_vitals_sink_t)(const char *line, uint16 len,
                                           void *ctx);

/*!
 * \brief Register where test telemetry is delivered.
 *
 * Only used when SMART_HELMET_ENABLE_VITALS_CSV or
 * SMART_HELMET_ENABLE_SENS_DUMP is set; without a sink those options are
 * inert. Pass NULL to detach.
 */
void SmartHelmet_VitalsSetSink(smart_helmet_vitals_sink_t sink, void *ctx);
void SmartHelmet_VitalsPirEvent(void);
void SmartHelmet_VitalsOnSensSample(void);
void SmartHelmet_VitalsProcess(void);
const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void);

#endif /* SMART_HELMET_VITALS_H */
