/*!
\file       sh_signal.c
\brief      Synthetic signal generator (see sh_signal.h).
*/
#define _GNU_SOURCE
#include "sh_signal.h"

#include <math.h>
#include <stddef.h>

/* Deterministic LCG so scenario results are reproducible across machines. */
static double shRand(sh_signal_t *s)
{
    s->rng = s->rng * 1103515245u + 12345u;
    return (double)((s->rng >> 8) & 0xFFFFu) / 65535.0;
}

/* Approximately Gaussian: sum of uniforms, mean 0, unit-ish variance. */
static double shNoise(sh_signal_t *s)
{
    return (shRand(s) + shRand(s) + shRand(s) + shRand(s) - 2.0) * 0.866;
}

void ShSignal_Init(sh_signal_t *s, const sh_signal_cfg_t *cfg, unsigned seed)
{
    int i;

    s->cfg = cfg;
    s->phase = 0.0;
    s->resp_phase = 0.0;
    s->rng = seed ? seed : 1u;
    for (i = 0; i < 3; i++)
    {
        s->drift[i] = 0.0;
    }
}

/*
 * Pulse shape: a systolic peak plus a smaller dicrotic-like second peak,
 * built from the first three harmonics. This is what a Doppler IF signal
 * from chest/head wall motion actually looks like - not a pure sinusoid.
 */
static double shPulseShape(double phase_cycles)
{
    double w = 2.0 * M_PI * phase_cycles;

    return 1.00 * sin(w)
         + 0.45 * sin(2.0 * w + 0.6)
         + 0.20 * sin(3.0 * w + 1.2);
}

void ShSignal_Step(sh_signal_t *s, double t, double dt,
                   double *mv, double mg[3])
{
    const sh_signal_cfg_t *c = s->cfg;
    double bpm = c->bpm_at ? c->bpm_at(t) : 0.0;
    double amp = c->amp_at ? c->amp_at(t) : 1.0;
    double v = c->dc_mv;
    double sway = 0.0;
    int i;

    if (bpm > 0.0)
    {
        s->phase += (bpm / 60.0) * dt;
        v += c->pulse_mv * amp * shPulseShape(s->phase);
    }

    if (c->resp_bpm > 0.0)
    {
        s->resp_phase += (c->resp_bpm / 60.0) * dt;
        v += c->resp_mv * sin(2.0 * M_PI * s->resp_phase);
    }

    /*
     * Three first-order random walks with decade-spaced time constants sum
     * to a rough 1/f baseline wander, which is what the analogue front end
     * and thermal drift contribute.
     */
    if (c->drift_mv > 0.0)
    {
        static const double tau[3] = { 0.5, 5.0, 50.0 };
        double sum = 0.0;

        for (i = 0; i < 3; i++)
        {
            double a = dt / (tau[i] + dt);
            s->drift[i] += a * (shNoise(s) * 3.0 - s->drift[i]);
            sum += s->drift[i];
        }
        v += c->drift_mv * sum / 3.0;
    }

    if (c->sway_hz > 0.0)
    {
        sway = sin(2.0 * M_PI * c->sway_hz * t);
        v += c->sway_mv * sway;
    }

    if (c->noise_mv > 0.0)
    {
        v += c->noise_mv * shNoise(s);
    }

    if (v < 0.0)
    {
        v = 0.0;
    }
    *mv = v;

    /* Gravity sits on z; sway modulates x. The vitals code must remove the
     * static component itself, which is exactly what we want to exercise. */
    mg[0] = c->sway_mg * sway;
    mg[1] = 0.0;
    mg[2] = 1000.0;
}
