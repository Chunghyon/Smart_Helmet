/*!
\file       smart_helmet_vitals.h
\brief      Motion-gated band-energy trend proxy (not clinical heart rate)

Uses LIS3DH (and optional SENS_IN / PIR event rate) to flag relative change
while the wearer is still. Output is a coarse trend, never a BPM value.

Pipeline:
  samples -> motion_gate -> (if calm) band_energy -> baseline EMA -> trend_flag
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
    smart_helmet_trend_unknown = 0, /*!< not enough calm data / motion */
    smart_helmet_trend_stable,
    smart_helmet_trend_rising,      /*!< band energy up vs baseline */
    smart_helmet_trend_falling
} smart_helmet_trend_flag_t;

typedef struct
{
    smart_helmet_motion_gate_t motion;
    smart_helmet_trend_flag_t  trend;
    uint16                     motion_rms_mg;   /*!< last short-window RMS */
    uint16                     band_energy;     /*!< arbitrary units */
    uint16                     baseline_energy; /*!< EMA baseline */
    uint16                     pir_events_win;  /*!< optional PD-V12 events */
    bool                       valid;           /*!< trend trustworthy */
} smart_helmet_vitals_status_t;

/*! \brief Reset windows, baseline, and flags. */
void SmartHelmet_VitalsInit(void);

/*!
 * \brief Feed one LIS3DH sample (mg). Call at ~SMART_HELMET_VITALS_FS_HZ.
 * \param x_mg,y_mg,z_mg  axis acceleration in milli-g
 */
void SmartHelmet_VitalsPushAccel(int16 x_mg, int16 y_mg, int16 z_mg);

/*!
 * \brief Optional: push SENS_IN millivolts (PD-V12 analog path) for band energy.
 * Ignored if not called; accel-only mode still works.
 */
void SmartHelmet_VitalsPushSensInMv(uint16 mv);

/*! \brief Optional: count a PIR_OUT edge in the current window. */
void SmartHelmet_VitalsPirEvent(void);

/*!
 * \brief Close a processing window (call once per window or from Poll).
 * Runs motion_gate -> band_energy -> trend_flag update.
 */
void SmartHelmet_VitalsProcess(void);

/*! \brief Latest status snapshot. */
const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void);

#endif /* SMART_HELMET_VITALS_H */
