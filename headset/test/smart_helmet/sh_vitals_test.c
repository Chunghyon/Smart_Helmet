/*!
\file       sh_vitals_test.c
\brief      Host regression harness for the pulse-change detector.

This is deliberately NOT a BPM-accuracy test. The module's product is the
hr_change flag, so every scenario is scored on events:

  - detections            : did hr_change fire when it should have?
  - detection latency     : how long after the true change?
  - false alarms          : did it fire when it should not have?
  - availability          : fraction of windows with hr_valid set

Run with no arguments to execute every scenario; pass a substring to filter.
Set SH_TEST_LOG=1 to see the firmware log lines.
*/
#define _GNU_SOURCE

#include "sh_signal.h"

#include <smart_helmet_config.h>
#include <smart_helmet_vitals.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Referenced by the vm.h stub so the firmware sees harness-controlled time. */
uint32 sh_test_now_us;

#define WIN_SAMPLES  SMART_HELMET_MOTION_WIN

/* ------------------------------------------------------------------ */
/* Scenario description                                                */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char     *name;
    const char     *what;        /*!< what the scenario is proving */
    double          dur_s;
    sh_signal_cfg_t sig;

    double          tick_ms;     /*!< nominal sample interval */
    double          tick_mod_hz; /*!< cadence modulation frequency (0 = none) */
    double          tick_mod_pct;/*!< cadence modulation depth, percent */

    /*! Expected change event. dir 0 = no change expected at all. */
    int             expect_dir;      /*!< +1 rising, -1 falling, 0 none */
    double          change_at_s;     /*!< when the true change happens */
    double          max_latency_s;   /*!< detection must land inside this */

    /*! False-alarm accounting window: [fa_from_s, fa_to_s). */
    double          fa_from_s;
    double          fa_to_s;
    int             max_false_alarms;

    /*! Minimum hr_valid availability over [avail_from_s, dur_s), percent.
     *  Negative disables the check. */
    double          avail_from_s;
    double          min_avail_pct;
} sh_scenario_t;

typedef struct
{
    int    windows;
    int    valid_windows;
    int    avail_windows;
    int    avail_valid;
    int    detections;
    int    first_dir;
    double first_change_s;
    int    false_alarms;
    int    pass;
} sh_result_t;

/* ------------------------------------------------------------------ */
/* Rate profiles                                                       */
/* ------------------------------------------------------------------ */

static double bpm_flat60(double t)   { (void)t; return 60.0; }
static double bpm_flat100(double t)  { (void)t; return 100.0; }

static double bpm_step_up(double t)   { return (t < 60.0) ? 60.0 : 100.0; }
static double bpm_step_down(double t) { return (t < 60.0) ? 100.0 : 60.0; }

/* 60 -> 70 BPM over 60 s, starting at t = 60 s. */
static double bpm_ramp(double t)
{
    if (t < 60.0)
    {
        return 60.0;
    }
    if (t > 120.0)
    {
        return 70.0;
    }
    return 60.0 + 10.0 * (t - 60.0) / 60.0;
}

static double bpm_none(double t) { (void)t; return 0.0; }

/* Subject leaves at 60 s and comes back at 90 s at a different rate. */
static double bpm_reentry(double t)
{
    if (t < 60.0)
    {
        return 60.0;
    }
    if (t < 90.0)
    {
        return 0.0;
    }
    return 62.0;
}

static double amp_reentry(double t)
{
    if (t >= 60.0 && t < 90.0)
    {
        return 0.0;
    }
    return 1.0;
}

/* ------------------------------------------------------------------ */
/* Baseline signal profile shared by most scenarios                    */
/* ------------------------------------------------------------------ */

#define SIG_MIX(pulse, noise)       \
    .pulse_mv  = (pulse),           \
    .resp_bpm  = 15.0,              \
    .resp_mv   = 25.0,              \
    .drift_mv  = 12.0,              \
    .noise_mv  = (noise),           \
    .dc_mv     = 900.0

/*! Nominal subject: harmonic-rich pulse + respiration + drift + noise. */
#define SIG_BASE SIG_MIX(40.0, 6.0)

static const sh_scenario_t sh_scenarios[] =
{
    {
        .name = "1-steady60",
        .what = "60 BPM held: no change flag, good availability",
        .dur_s = 300.0,
        .sig = { .bpm_at = bpm_flat60, SIG_BASE },
        .tick_ms = 40.0,
        .expect_dir = 0,
        .fa_from_s = 40.0, .fa_to_s = 300.0, .max_false_alarms = 0,
        .avail_from_s = 60.0, .min_avail_pct = 60.0,
    },
    {
        .name = "2-step-up",
        .what = "60 -> 100 BPM step must be reported as rising",
        .dur_s = 180.0,
        .sig = { .bpm_at = bpm_step_up, SIG_BASE },
        .tick_ms = 40.0,
        .expect_dir = 1, .change_at_s = 60.0, .max_latency_s = 60.0,
        .fa_from_s = 40.0, .fa_to_s = 60.0, .max_false_alarms = 0,
        .avail_from_s = -1.0,
    },
    {
        .name = "3-step-down",
        .what = "100 -> 60 BPM step must be reported as falling",
        .dur_s = 180.0,
        .sig = { .bpm_at = bpm_step_down, SIG_BASE },
        .tick_ms = 40.0,
        .expect_dir = -1, .change_at_s = 60.0, .max_latency_s = 60.0,
        .fa_from_s = 40.0, .fa_to_s = 60.0, .max_false_alarms = 0,
        .avail_from_s = -1.0,
    },
    {
        .name = "4-slow-ramp",
        .what = "60 -> 70 BPM ramp: CUSUM slack/limit tuning check",
        .dur_s = 240.0,
        .sig = { .bpm_at = bpm_ramp, SIG_BASE },
        .tick_ms = 40.0,
        .expect_dir = 1, .change_at_s = 60.0, .max_latency_s = 150.0,
        .fa_from_s = 40.0, .fa_to_s = 60.0, .max_false_alarms = 0,
        .avail_from_s = -1.0,
    },
    {
        .name = "5-noise-only-jitter",
        .what = "60 BPM with heavy noise: zero false alarms",
        .dur_s = 300.0,
        .sig = { .bpm_at = bpm_flat60, SIG_MIX(40.0, 20.0) },
        .tick_ms = 40.0,
        .expect_dir = 0,
        .fa_from_s = 40.0, .fa_to_s = 300.0, .max_false_alarms = 0,
        .avail_from_s = -1.0,
    },
    {
        .name = "6-no-subject",
        .what = "noise only: hr_valid stays clear, no change flag",
        .dur_s = 180.0,
        .sig = { .bpm_at = bpm_none, .dc_mv = 900.0,
                 .drift_mv = 12.0, .noise_mv = 4.0 },
        .tick_ms = 40.0,
        .expect_dir = 0,
        .fa_from_s = 0.0, .fa_to_s = 180.0, .max_false_alarms = 0,
        .avail_from_s = -1.0,
    },
    {
        .name = "7-dropout-reentry",
        .what = "subject leaves then returns: re-learning must not look "
                "like a real rate change (top false-alarm risk)",
        .dur_s = 240.0,
        .sig = { .bpm_at = bpm_reentry, .amp_at = amp_reentry, SIG_BASE },
        .tick_ms = 40.0,
        .expect_dir = 0,
        .fa_from_s = 40.0, .fa_to_s = 240.0, .max_false_alarms = 0,
        .avail_from_s = -1.0,
    },
    {
        .name = "8-head-sway",
        .what = "2.3 Hz head sway on top of 60 BPM (MOTION_ADAPT axis)",
        .dur_s = 240.0,
        .sig = { .bpm_at = bpm_flat60, SIG_BASE,
                 .sway_hz = 2.3, .sway_mg = 120.0, .sway_mv = 55.0 },
        .tick_ms = 40.0,
        .expect_dir = 0,
        .fa_from_s = 40.0, .fa_to_s = 240.0, .max_false_alarms = 0,
#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
        /* The adaptive canceller is supposed to keep the window usable
         * through light sway instead of discarding it. */
        .avail_from_s = 60.0, .min_avail_pct = 50.0,
#else
        .avail_from_s = -1.0,
#endif
    },
    {
        .name = "9-cadence-1hz",
        .what = "1 Hz sampling-interval modulation (TIMESTAMP / "
                "SENS_PRIORITY axis) must not synthesise a 60 BPM line",
        .dur_s = 240.0,
        .sig = { .bpm_at = bpm_flat100, SIG_BASE },
        .tick_ms = 40.0, .tick_mod_hz = 1.0, .tick_mod_pct = 25.0,
        .expect_dir = 0,
        .fa_from_s = 40.0, .fa_to_s = 240.0, .max_false_alarms = 0,
        .avail_from_s = -1.0,
    },
};

#define SH_N_SCENARIOS ((int)(sizeof(sh_scenarios) / sizeof(sh_scenarios[0])))

/* ------------------------------------------------------------------ */
/* Execution                                                           */
/* ------------------------------------------------------------------ */

static int sh_dir_of(smart_helmet_trend_flag_t f)
{
    if (f == smart_helmet_trend_rising)
    {
        return 1;
    }
    if (f == smart_helmet_trend_falling)
    {
        return -1;
    }
    return 0;
}

#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
/*
 * Smoke check for the stage-0 telemetry path: count the records and keep the
 * last one so the column count can be sanity-checked.
 */
static unsigned long sh_telem_records;
static char sh_telem_last[256];

static void sh_telem_sink(const char *line, uint16 len, void *ctx)
{
    (void)ctx;
    sh_telem_records++;
    if (len < sizeof(sh_telem_last))
    {
        memcpy(sh_telem_last, line, len);
        sh_telem_last[len] = '\0';
    }
}
#endif

static void sh_run(const sh_scenario_t *sc, sh_result_t *r)
{
    sh_signal_t gen;
    const smart_helmet_vitals_status_t *st;
    double t = 0.0;
    double prev_t = 0.0;
    int n = 0;
    int prev_dir = 0;

    memset(r, 0, sizeof(*r));
    r->first_change_s = -1.0;

    sh_test_now_us = 0;
    ShSignal_Init(&gen, &sc->sig, 0xC0FFEEu);
    SmartHelmet_VitalsInit();
#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
    SmartHelmet_VitalsSetSink(sh_telem_sink, NULL);
#endif

    while (t < sc->dur_s)
    {
        double dt_ms = sc->tick_ms;
        double mv;
        double mg[3];

        if (sc->tick_mod_hz > 0.0)
        {
            dt_ms *= 1.0 + (sc->tick_mod_pct / 100.0) *
                           sin(2.0 * M_PI * sc->tick_mod_hz * t);
        }

        ShSignal_Step(&gen, t, t - prev_t, &mv, mg);
        prev_t = t;

        sh_test_now_us = (uint32)(t * 1e6);
#if SMART_HELMET_ENABLE_LIS3DH
        SmartHelmet_VitalsPushAccel((int16)mg[0], (int16)mg[1], (int16)mg[2]);
#else
        (void)mg;
#endif
        SmartHelmet_VitalsPushSensInMvAt((uint16)(mv + 0.5), sh_test_now_us);
        SmartHelmet_VitalsOnSensSample();

        if (++n >= WIN_SAMPLES)
        {
            int dir;

            n = 0;
            st = SmartHelmet_VitalsGetStatus();
            r->windows++;
            if (st->hr_valid)
            {
                r->valid_windows++;
            }
            if (sc->avail_from_s >= 0.0 && t >= sc->avail_from_s)
            {
                r->avail_windows++;
                if (st->hr_valid)
                {
                    r->avail_valid++;
                }
            }

            dir = sh_dir_of(st->hr_change);
            if (dir && dir != prev_dir)
            {
                r->detections++;
                if (r->first_change_s < 0.0)
                {
                    r->first_change_s = t;
                    r->first_dir = dir;
                }
                if (t >= sc->fa_from_s && t < sc->fa_to_s)
                {
                    /* An expected event inside its own latency budget is
                     * not a false alarm. */
                    int expected = sc->expect_dir &&
                                   dir == sc->expect_dir &&
                                   t >= sc->change_at_s &&
                                   t <= sc->change_at_s + sc->max_latency_s;
                    if (!expected)
                    {
                        r->false_alarms++;
                    }
                }
            }
            prev_dir = dir;
        }

        t += dt_ms / 1000.0;
    }

    /* Scoring. */
    r->pass = 1;
    if (r->false_alarms > sc->max_false_alarms)
    {
        r->pass = 0;
    }
    if (sc->expect_dir)
    {
        double lat = r->first_change_s - sc->change_at_s;

        if (r->first_dir != sc->expect_dir || r->first_change_s < 0.0 ||
            lat < 0.0 || lat > sc->max_latency_s)
        {
            r->pass = 0;
        }
    }
    else if (r->detections > sc->max_false_alarms)
    {
        r->pass = 0;
    }
    if (sc->avail_from_s >= 0.0 && sc->min_avail_pct > 0.0)
    {
        double pct = r->avail_windows
                   ? (100.0 * r->avail_valid / r->avail_windows) : 0.0;
        if (pct < sc->min_avail_pct)
        {
            r->pass = 0;
        }
    }
}

int main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    int failures = 0;
    int i;

    printf("scenario              win  valid%%  det  dir  latency  false  result\n");
    printf("-------------------------------------------------------------------\n");

    for (i = 0; i < SH_N_SCENARIOS; i++)
    {
        const sh_scenario_t *sc = &sh_scenarios[i];
        sh_result_t r;
        char lat[16];

        if (filter && !strstr(sc->name, filter))
        {
            continue;
        }

        sh_run(sc, &r);

        if (r.first_change_s >= 0.0)
        {
            snprintf(lat, sizeof(lat), "%6.1fs",
                     r.first_change_s - sc->change_at_s);
        }
        else
        {
            snprintf(lat, sizeof(lat), "%7s", "-");
        }

        printf("%-20s %4d  %5.1f  %3d  %3d  %s  %5d  %s\n",
               sc->name, r.windows,
               r.windows ? (100.0 * r.valid_windows / r.windows) : 0.0,
               r.detections, r.first_dir, lat, r.false_alarms,
               r.pass ? "PASS" : "FAIL");

        if (!r.pass)
        {
            failures++;
            printf("    expected: %s\n", sc->what);
        }
    }

    printf("-------------------------------------------------------------------\n");
#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
    printf("telemetry: %lu records, last=\"%s\"\n",
           sh_telem_records, sh_telem_last);
    if (!sh_telem_records)
    {
        printf("telemetry enabled but no records were emitted\n");
        failures++;
    }
#endif
    printf("%s (%d failing)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
