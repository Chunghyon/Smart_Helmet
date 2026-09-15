/*!
\file       smart_helmet_vitals.h
\brief      Motion-gated band-energy trend proxy (not clinical heart rate)

Primary input: SENS_IN (PD-V12 radar IF via ~60 dB amp, ADC).
Optional: LIS3DH motion gate. PIR_OUT PIO is not required — a software
edge on the SENS residual approximates the comparator path (same chain).

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
    uint16                     motion_rms_mg;   /*!< accel RMS, or 0 if unused */
    uint16                     band_energy;     /*!< arbitrary units */
    uint16                     baseline_energy; /*!< EMA baseline */
    uint16                     pir_events_win;  /*!< SW SENS edges (or HW PIR) */
    uint16                     sens_mv;         /*!< last SENS_IN mV */
    bool                       valid;           /*!< trend trustworthy */
} smart_helmet_vitals_status_t;

/*! \brief Reset windows, baseline, and flags. */
void SmartHelmet_VitalsInit(void);

/*!
 * \brief Feed one LIS3DH sample (mg). Optional motion gate / micro-motion.
 * \param x_mg,y_mg,z_mg  axis acceleration in milli-g
 */
void SmartHelmet_VitalsPushAccel(int16 x_mg, int16 y_mg, int16 z_mg);

/*!
 * \brief Push SENS_IN millivolts (PD-V12 analog). Primary band-energy path.
 * Also applies optional SW edge detect (PIR_OUT proxy, no extra pin).
 */
void SmartHelmet_VitalsPushSensInMv(uint16 mv);

/*! \brief Optional: count a real PIR_OUT edge if HW ever wires it. */
void SmartHelmet_VitalsPirEvent(void);

/*!
 * \brief After each SENS sample: bump counter; every MOTION_WIN samples run Process.
 * Call from ADC SENS path (preferred over PollSensors-only).
 */
void SmartHelmet_VitalsOnSensSample(void);

/*!
 * \brief Close a processing window.
 * Runs motion_gate -> band_energy -> trend_flag update.
 */
void SmartHelmet_VitalsProcess(void);

/*! \brief Latest status snapshot. */
const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void);

#endif /* SMART_HELMET_VITALS_H */
