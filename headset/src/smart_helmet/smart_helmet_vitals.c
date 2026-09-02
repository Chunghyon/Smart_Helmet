/*!
\file       smart_helmet_vitals.c
\brief      Skeleton: motion_gate + band_energy + trend_flag

Lightweight proxy only. Replace the band-energy path with a proper
band-pass (0.8–3 Hz) / Goertzel when CPU budget allows.
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

#include "smart_helmet_config.h"
#include "smart_helmet_vitals.h"

#include <logging.h>
#include <stdlib.h>
#include <string.h>

#if SMART_HELMET_ENABLE_VITALS_PROXY

DEBUG_LOG_DEFINE_LEVEL_VAR

#define MOTION_WIN   SMART_HELMET_MOTION_WIN
#define BAND_WIN     SMART_HELMET_BAND_WIN

static smart_helmet_vitals_status_t sh_vitals;

/* --- motion window (accel magnitude) ------------------------------------- */
static uint16 motion_mag[MOTION_WIN];
static uint8  motion_idx;
static uint8  motion_count;

/* --- band window: high-pass residual magnitude proxy --------------------- */
static int16  band_hp[BAND_WIN];
static uint8  band_idx;
static uint8  band_count;
static int32  hp_prev_x;   /* simple 1-pole HP state on |a| */
static int32  hp_prev_y;

/* optional radar / SENS_IN residual */
static int16  sens_hp[BAND_WIN];
static uint8  sens_idx;
static uint8  sens_count;
static int32  sens_prev_x;
static int32  sens_prev_y;
static uint16 pir_events;

static uint8  calm_windows;

static uint16 shSqrtU32(uint32 v)
{
    uint32 op = v;
    uint32 res = 0;
    uint32 one = 1u << 30;
    while (one > op)
    {
        one >>= 2;
    }
    while (one != 0)
    {
        if (op >= res + one)
        {
            op -= res + one;
            res = (res >> 1) + one;
        }
        else
        {
            res >>= 1;
        }
        one >>= 2;
    }
    return (uint16)res;
}

static uint16 shRmsU16(const uint16 *buf, uint8 n)
{
    uint32 sum = 0;
    uint8 i;
    if (!n)
    {
        return 0;
    }
    for (i = 0; i < n; i++)
    {
        sum += (uint32)buf[i] * buf[i];
    }
    return shSqrtU32(sum / n);
}

static uint16 shEnergyI16(const int16 *buf, uint8 n)
{
    uint32 sum = 0;
    uint8 i;
    if (!n)
    {
        return 0;
    }
    for (i = 0; i < n; i++)
    {
        int32 v = buf[i];
        sum += (uint32)(v * v);
    }
    /* mean power, scaled down */
    return (uint16)(sum / n);
}

/*! Very light high-pass: y[n] = x[n] - x[n-1] + a*y[n-1], a≈0.95 (Q8 243/256) */
static int16 shHighPass(int32 x, int32 *prev_x, int32 *prev_y)
{
    int32 y = x - *prev_x + ((*prev_y * 243) / 256);
    *prev_x = x;
    *prev_y = y;
    if (y > 32767)
    {
        y = 32767;
    }
    if (y < -32768)
    {
        y = -32768;
    }
    return (int16)y;
}

void SmartHelmet_VitalsInit(void)
{
    memset(&sh_vitals, 0, sizeof(sh_vitals));
    memset(motion_mag, 0, sizeof(motion_mag));
    memset(band_hp, 0, sizeof(band_hp));
    memset(sens_hp, 0, sizeof(sens_hp));
    motion_idx = motion_count = 0;
    band_idx = band_count = 0;
    sens_idx = sens_count = 0;
    hp_prev_x = hp_prev_y = 0;
    sens_prev_x = sens_prev_y = 0;
    pir_events = 0;
    calm_windows = 0;
    sh_vitals.trend = smart_helmet_trend_unknown;
    sh_vitals.motion = smart_helmet_motion_active;
	CC_LOGN("SmartHelmet Vitals: proxy init (motion_gate/band_energy/trend)");
}

void SmartHelmet_VitalsPushAccel(int16 x_mg, int16 y_mg, int16 z_mg)
{
    int32 ax = x_mg;
    int32 ay = y_mg;
    int32 az = z_mg;
    uint32 mag2 = (uint32)(ax * ax + ay * ay + az * az);
    uint16 mag = shSqrtU32(mag2);
    int16 hp;

    motion_mag[motion_idx] = mag;
    motion_idx = (uint8)((motion_idx + 1) % MOTION_WIN);
    if (motion_count < MOTION_WIN)
    {
        motion_count++;
    }

    /* remove gravity-ish DC via HP on magnitude; residual ~ motion + micro */
    hp = shHighPass((int32)mag, &hp_prev_x, &hp_prev_y);
    band_hp[band_idx] = hp;
    band_idx = (uint8)((band_idx + 1) % BAND_WIN);
    if (band_count < BAND_WIN)
    {
        band_count++;
    }
}

void SmartHelmet_VitalsPushSensInMv(uint16 mv)
{
    int16 hp = shHighPass((int32)mv, &sens_prev_x, &sens_prev_y);
    sens_hp[sens_idx] = hp;
    sens_idx = (uint8)((sens_idx + 1) % BAND_WIN);
    if (sens_count < BAND_WIN)
    {
        sens_count++;
    }
}

void SmartHelmet_VitalsPirEvent(void)
{
    if (pir_events < 0xffff)
    {
        pir_events++;
    }
}

void SmartHelmet_VitalsProcess(void)
{
    uint16 rms;
    uint16 e_accel;
    uint16 e_sens;
    uint16 energy;
    uint16 baseline;
    int32  delta_pct;
    smart_helmet_trend_flag_t trend;

    rms = shRmsU16(motion_mag, motion_count);
    sh_vitals.motion_rms_mg = rms;
    sh_vitals.pir_events_win = pir_events;

    if (rms >= SMART_HELMET_MOTION_RMS_MG)
    {
        sh_vitals.motion = smart_helmet_motion_active;
        sh_vitals.trend = smart_helmet_trend_unknown;
        sh_vitals.valid = FALSE;
        calm_windows = 0;
        /* do not update baseline while moving */
        pir_events = 0;
        DEBUG_LOG_VERBOSE("Vitals: ACTIVE rms=%u", rms);
        return;
    }

    sh_vitals.motion = smart_helmet_motion_calm;

    e_accel = shEnergyI16(band_hp, band_count);
    e_sens = shEnergyI16(sens_hp, sens_count);
    /* blend: accel primary, SENS_IN optional contribution */
    energy = e_accel;
    if (sens_count > (BAND_WIN / 4))
    {
        energy = (uint16)(e_accel + (e_sens / 4));
    }
    /* small boost from PIR event rate (motion near helmet rear) */
    energy = (uint16)(energy + pir_events * 8u);

    sh_vitals.band_energy = energy;

    baseline = sh_vitals.baseline_energy;
    if (baseline == 0)
    {
        baseline = energy;
    }
    else
    {
        /* EMA: baseline += alpha * (energy - baseline) */
        baseline = (uint16)(baseline +
            (((int32)energy - (int32)baseline) * SMART_HELMET_BASELINE_ALPHA_Q8) / 256);
    }
    sh_vitals.baseline_energy = baseline;

    if (calm_windows < 0xff)
    {
        calm_windows++;
    }

    if (calm_windows < SMART_HELMET_CALM_WINDOWS_MIN || baseline == 0)
    {
        trend = smart_helmet_trend_unknown;
        sh_vitals.valid = FALSE;
    }
    else
    {
        delta_pct = (((int32)energy - (int32)baseline) * 100) / (int32)baseline;
        if (delta_pct >= (int32)SMART_HELMET_TREND_UP_PCT)
        {
            trend = smart_helmet_trend_rising;
        }
        else if (delta_pct <= -(int32)SMART_HELMET_TREND_DOWN_PCT)
        {
            trend = smart_helmet_trend_falling;
        }
        else
        {
            trend = smart_helmet_trend_stable;
        }
        sh_vitals.valid = TRUE;
    }

    sh_vitals.trend = trend;
    pir_events = 0;

    DEBUG_LOG_VERBOSE("Vitals: calm e=%u base=%u trend=%u valid=%u",
                      energy, baseline, (unsigned)trend, sh_vitals.valid);
}

const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void)
{
    return &sh_vitals;
}

#else /* !SMART_HELMET_ENABLE_VITALS_PROXY */

void SmartHelmet_VitalsInit(void) {}
void SmartHelmet_VitalsPushAccel(int16 x_mg, int16 y_mg, int16 z_mg)
{
    UNUSED(x_mg); UNUSED(y_mg); UNUSED(z_mg);
}
void SmartHelmet_VitalsPushSensInMv(uint16 mv) { UNUSED(mv); }
void SmartHelmet_VitalsPirEvent(void) {}
void SmartHelmet_VitalsProcess(void) {}
const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void)
{
    static smart_helmet_vitals_status_t empty;
    return &empty;
}

#endif /* SMART_HELMET_ENABLE_VITALS_PROXY */
