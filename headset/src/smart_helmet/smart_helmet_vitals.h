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
    smart_helmet_hr_ok = 0,          /*!< peak+FFT agree, guards passed */
    smart_helmet_hr_warm,            /*!< calm_windows / buffer not ready */
    smart_helmet_hr_e_low,           /*!< band_energy < HR_MIN_ENERGY */
    smart_helmet_hr_e_high,          /*!< band_energy > HR_MAX_ENERGY */
    smart_helmet_hr_sa_low,          /*!< mean|SENS HP| too small */
    smart_helmet_hr_sa_high,         /*!< mean|SENS HP| motion-like */
    smart_helmet_hr_dc_spike,        /*!< |Δsens_mv| too large */
    smart_helmet_hr_holdoff,         /*!< post-disturbance holdoff */
    smart_helmet_hr_no_peak,         /*!< peak detector empty */
    smart_helmet_hr_no_fft,          /*!< FFT empty / SNR fail */
    smart_helmet_hr_disagree,        /*!< peak vs FFT outside agree % */
    smart_helmet_hr_peak_only,       /*!< peak only (need FFT if REQUIRE_AGREE) */
    smart_helmet_hr_fft_only,        /*!< FFT only — never hr_valid */
    smart_helmet_hr_delta,           /*!< |ΔBPM| vs last valid too large */
    smart_helmet_hr_none,            /*!< no estimate this window */
    smart_helmet_hr_disabled         /*!< HR peak/FFT compile-off */
} smart_helmet_hr_reason_t;

typedef struct
{
    smart_helmet_motion_gate_t motion;
    smart_helmet_trend_flag_t  trend;
    uint16                     motion_rms_mg;
    uint16                     band_energy;
    uint16                     baseline_energy;
    uint16                     pir_events_win;
    uint16                     sens_mv;
    bool                       valid;           /*!< trend trustworthy */

    /*! Time-domain peak estimate (BPM). 0 if unavailable. */
    uint16                     hr_bpm_peak;
    /*! FFT dominant-bin estimate (BPM). 0 if unavailable. */
    uint16                     hr_bpm_fft;
    /*! Fused BPM when peak/FFT agree or one is strong; else 0. */
    uint16                     hr_bpm;
    uint8                      hr_peak_count;   /*!< peaks in last analysis window */
    uint16                     hr_fft_mag;      /*!< dominant bin |X|^2 (arb) */
    bool                       hr_valid;        /*!< hr_bpm usable this window */
    /*! Primary reason pulse proxy is meaningful or not (see enum). */
    smart_helmet_hr_reason_t   hr_reason;
    uint8                      hr_holdoff;      /*!< windows left before hv allowed */
} smart_helmet_vitals_status_t;

void SmartHelmet_VitalsInit(void);
void SmartHelmet_VitalsPushAccel(int16 x_mg, int16 y_mg, int16 z_mg);
void SmartHelmet_VitalsPushSensInMv(uint16 mv);
void SmartHelmet_VitalsPirEvent(void);
void SmartHelmet_VitalsOnSensSample(void);
void SmartHelmet_VitalsProcess(void);
const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void);

#endif /* SMART_HELMET_VITALS_H */
