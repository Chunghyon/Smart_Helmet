/*!
\file       sh_signal.h
\brief      Synthetic SENS_IN / accelerometer signal generator for the host
            regression harness.

The generator deliberately produces more than a sine wave: a pulse waveform
with harmonics, a respiration component, 1/f-like baseline drift and white
noise. Options such as SMART_HELMET_HR_FFT_HPS only show their real
behaviour against harmonic-rich input.
*/
#ifndef SH_SIGNAL_H
#define SH_SIGNAL_H

typedef struct
{
    /*! Instantaneous pulse rate at time t (seconds). Return 0 for "no
     *  subject present" - the pulse component is then omitted. */
    double (*bpm_at)(double t);
    /*! Pulse amplitude scale at time t, 0..1. NULL means constant 1. */
    double (*amp_at)(double t);

    double pulse_mv;      /*!< peak pulse deviation, millivolts */
    double resp_bpm;      /*!< respiration rate, breaths/min (0 = none) */
    double resp_mv;       /*!< respiration amplitude, millivolts */
    double drift_mv;      /*!< 1/f-ish baseline wander amplitude */
    double noise_mv;      /*!< white noise standard deviation */
    double dc_mv;         /*!< DC operating point */

    double sway_hz;       /*!< head sway frequency (0 = still) */
    double sway_mg;       /*!< head sway amplitude in milli-g */
    double sway_mv;       /*!< how much sway couples into SENS_IN */
} sh_signal_cfg_t;

typedef struct
{
    const sh_signal_cfg_t *cfg;
    double phase;         /*!< pulse phase accumulator, cycles */
    double resp_phase;
    double drift[3];      /*!< three decaying random walks -> 1/f-like */
    unsigned rng;
} sh_signal_t;

void ShSignal_Init(sh_signal_t *s, const sh_signal_cfg_t *cfg, unsigned seed);

/*!
 * \brief Advance to absolute time \p t and produce one sample set.
 * \param dt  Time since the previous sample, seconds.
 * \param mv  Receives the SENS_IN sample in millivolts (clamped >= 0).
 * \param mg  Receives x/y/z accelerometer values in milli-g, gravity on z.
 */
void ShSignal_Step(sh_signal_t *s, double t, double dt,
                   double *mv, double mg[3]);

#endif /* SH_SIGNAL_H */
