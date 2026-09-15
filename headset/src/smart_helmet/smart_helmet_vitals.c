/*!
\file       smart_helmet_vitals.c
\brief      motion_gate + band_energy + peak BPM + fixed-point FFT BPM

SENS_IN primary. Peak detect gives beat intervals; FFT finds dominant
frequency in ~0.8–3 Hz. Both are proxies, not clinical HR.
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
#define FFT_N        SMART_HELMET_HR_FFT_N
#define FS_HZ        SMART_HELMET_VITALS_FS_HZ

static smart_helmet_vitals_status_t sh_vitals;

static uint16 motion_mag[MOTION_WIN];
static uint8  motion_idx;
static uint8  motion_count;

static int16  band_hp[BAND_WIN];
static uint8  band_idx;
static uint8  band_count;
static int32  hp_prev_x;
static int32  hp_prev_y;

static int16  sens_hp[BAND_WIN];
static uint8  sens_idx;
static uint8  sens_count;
static int32  sens_prev_x;
static int32  sens_prev_y;
static uint16 pir_events;
static bool   sens_edge_armed;
static uint8  sens_since_process;
static uint8  calm_windows;
static uint8  hr_holdoff;          /* windows left before hr_valid allowed */
static uint16 prev_sens_mv;        /* last process-window SENS DC */
static uint8  prev_sens_mv_valid;
static uint16 last_hr_valid_bpm;   /* last fused BPM with hr_valid */
static uint8  last_hr_valid_have;

/* Chronological scratch for peak/FFT (max BAND_WIN) */
static int16  sh_ordered[BAND_WIN];

#if SMART_HELMET_ENABLE_HR_FFT
/* In-place FFT buffers */
static int32  fft_re[FFT_N];
static int32  fft_im[FFT_N];
/* Q8 fixed bin index EMA to reduce 46/70/93 hop */
static uint16 fft_k_smooth_q8;
static uint8  fft_k_smooth_valid;
#endif

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
    return (uint16)(sum / n);
}

static uint16 shMeanAbsI16(const int16 *buf, uint8 n)
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
        if (v < 0)
        {
            v = -v;
        }
        sum += (uint32)v;
    }
    return (uint16)(sum / n);
}

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

/*! Copy last n SENS HP samples oldest->newest into sh_ordered. */
static uint8 shCopySensOrdered(uint8 n)
{
    uint8 i;
    uint8 start;
    if (n > sens_count)
    {
        n = sens_count;
    }
    if (n > BAND_WIN)
    {
        n = BAND_WIN;
    }
    if (!n)
    {
        return 0;
    }
    start = (uint8)((sens_idx + BAND_WIN - n) % BAND_WIN);
    for (i = 0; i < n; i++)
    {
        sh_ordered[i] = sens_hp[(start + i) % BAND_WIN];
    }
    return n;
}

#if SMART_HELMET_ENABLE_HR_PEAK
/*!
 * Local-maxima peak detect on ordered residual.
 * Returns peak count; writes BPM estimate via mean IBI when >=2 peaks.
 */
static uint16 shPeakBpm(const int16 *x, uint8 n, uint8 *peak_count_out)
{
    uint8 i;
    uint8 peaks = 0;
    uint8 last_peak = 0xff;
    uint16 mean_abs;
    int32 thr;
    uint32 ibi_sum = 0;
    uint8 ibi_n = 0;
    uint16 bpm;
    uint16 min_dist = SMART_HELMET_HR_MIN_PEAK_DIST;
    uint16 max_ibi = (uint16)((FS_HZ * 60u) / SMART_HELMET_HR_BPM_MIN);
    uint16 min_ibi = (uint16)((FS_HZ * 60u) / SMART_HELMET_HR_BPM_MAX);

    if (peak_count_out)
    {
        *peak_count_out = 0;
    }
    if (n < (min_dist * 2u + 3u))
    {
        return 0;
    }

    mean_abs = shMeanAbsI16(x, n);
    thr = ((int32)mean_abs * SMART_HELMET_HR_PEAK_THR_Q8) / 256;
    if (thr < 5)
    {
        thr = 5;
    }

    for (i = 1; i + 1 < n; i++)
    {
        int32 yi = x[i];
        if (yi <= x[i - 1] || yi < x[i + 1])
        {
            continue;
        }
        if (yi < thr)
        {
            continue;
        }
        if (last_peak != 0xff)
        {
            uint16 ibi = (uint16)(i - last_peak);
            if (ibi < min_dist)
            {
                /* keep stronger peak */
                if (yi > x[last_peak])
                {
                    last_peak = i;
                }
                continue;
            }
            if (ibi >= min_ibi && ibi <= max_ibi)
            {
                ibi_sum += ibi;
                ibi_n++;
            }
        }
        last_peak = i;
        peaks++;
    }

    if (peak_count_out)
    {
        *peak_count_out = peaks;
    }
    if (ibi_n == 0)
    {
        return 0;
    }
    /* BPM = 60 * fs / mean_ibi */
    bpm = (uint16)((60u * (uint32)FS_HZ * (uint32)ibi_n) / ibi_sum);
    if (bpm < SMART_HELMET_HR_BPM_MIN || bpm > SMART_HELMET_HR_BPM_MAX)
    {
        return 0;
    }
    return bpm;
}
#endif /* PEAK */

#if SMART_HELMET_ENABLE_HR_FFT
#if SMART_HELMET_HR_FFT_N != 64
#error "smart_helmet_vitals: FFT LUT requires SMART_HELMET_HR_FFT_N == 64"
#endif
/* cos/sin(2*pi*k/64) Q15, k=0..32 — LUT assumes FFT_N==64 */
static const int16 sh_fft_cos64_q15[33] =
{
     32767,  32609,  32137,  31356,  30273,  28898,  27245,  25329,
     23170,  20787,  18204,  15446,  12539,   9512,   6393,   3212,
         0,  -3212,  -6393,  -9512, -12539, -15446, -18204, -20787,
    -23170, -25329, -27245, -28898, -30273, -31356, -32137, -32609,
    -32767
};
static const int16 sh_fft_sin64_q15[33] =
{
         0,   3212,   6393,   9512,  12539,  15446,  18204,  20787,
     23170,  25329,  27245,  28898,  30273,  31356,  32137,  32609,
     32767,  32609,  32137,  31356,  30273,  28898,  27245,  25329,
     23170,  20787,  18204,  15446,  12539,   9512,   6393,   3212,
         0
};

/*! W = exp(-j*2*pi*k/N) as (cw, sw) with sw = -sin */
static void shFftTwiddle(uint16 k, int16 *c, int16 *s)
{
    uint16 kk = (uint16)(k % FFT_N);
    int16 cosv;
    int16 sinv;
    if (kk <= 32)
    {
        cosv = sh_fft_cos64_q15[kk];
        sinv = sh_fft_sin64_q15[kk];
    }
    else
    {
        uint16 t = (uint16)(FFT_N - kk);
        cosv = sh_fft_cos64_q15[t];
        sinv = (int16)(-sh_fft_sin64_q15[t]);
    }
    *c = cosv;
    *s = (int16)(-sinv);
}

static void shFftRadix2(int32 *re, int32 *im)
{
    uint16 n = FFT_N;
    uint16 i, j, k, m;
    uint16 len, half;
    int32 tr, ti;

    /* Bit reverse */
    j = 0;
    for (i = 0; i < n - 1; i++)
    {
        if (i < j)
        {
            tr = re[i]; re[i] = re[j]; re[j] = tr;
            ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
        k = n >> 1;
        while (k <= j)
        {
            j = (uint16)(j - k);
            k >>= 1;
        }
        j = (uint16)(j + k);
    }

    for (len = 2; len <= n; len <<= 1)
    {
        half = len >> 1;
        for (i = 0; i < n; i += len)
        {
            for (m = 0; m < half; m++)
            {
                int16 cw, sw;
                int32 ur, ui;
                uint16 p = (uint16)(i + m);
                uint16 q = (uint16)(p + half);
                uint16 tw = (uint16)((m * n) / len);
                shFftTwiddle(tw, &cw, &sw);
                /* t = W * a[q] */
                /* Q15 mul; >>1 per stage keeps headroom on int32 */
                tr = ((re[q] / 2) * (int32)cw - (im[q] / 2) * (int32)sw) / 16384;
                ti = ((re[q] / 2) * (int32)sw + (im[q] / 2) * (int32)cw) / 16384;
                ur = re[p] / 2;
                ui = im[p] / 2;
                re[p] = ur + tr;
                im[p] = ui + ti;
                re[q] = ur - tr;
                im[q] = ui - ti;
            }
        }
    }
}

static uint16 shFftBpm(const int16 *x, uint8 n, uint16 *mag_out)
{
    uint16 i;
    uint16 k;
    uint16 k_lo;
    uint16 k_hi;
    uint16 best_k = 0;
    uint32 best_p = 0;
    uint32 sum_p = 0;
    uint16 band_bins = 0;
    uint16 bpm;
    int32 mean = 0;

    if (mag_out)
    {
        *mag_out = 0;
    }
    if (n < (FFT_N / 2))
    {
        return 0;
    }

    /* Use last FFT_N samples (or pad) */
    for (i = 0; i < FFT_N; i++)
    {
        int16 s = 0;
        if (n >= FFT_N)
        {
            s = x[n - FFT_N + i];
        }
        else if (i < n)
        {
            s = x[i];
        }
        mean += s;
    }
    mean /= (int32)FFT_N;

    for (i = 0; i < FFT_N; i++)
    {
        int16 s = 0;
        if (n >= FFT_N)
        {
            s = x[n - FFT_N + i];
        }
        else if (i < n)
        {
            s = x[i];
        }
        fft_re[i] = (int32)s - mean;
        fft_im[i] = 0;
    }

    shFftRadix2(fft_re, fft_im);

    /* HR band: f = k * fs / N; BPM = f*60 = k * fs * 60 / N */
    k_lo = (uint16)(((uint32)SMART_HELMET_HR_BPM_MIN * FFT_N + (60u * FS_HZ / 2)) /
                    (60u * FS_HZ));
    k_hi = (uint16)(((uint32)SMART_HELMET_HR_BPM_MAX * FFT_N) / (60u * FS_HZ));
    if (k_lo < 1)
    {
        k_lo = 1;
    }
    if (k_hi >= (FFT_N / 2))
    {
        k_hi = (uint16)(FFT_N / 2 - 1);
    }
    if (k_hi < k_lo)
    {
        return 0;
    }

    for (k = k_lo; k <= k_hi; k++)
    {
        int32 pr = fft_re[k] / 8;
        int32 pi = fft_im[k] / 8;
        uint32 p = (uint32)(pr * pr + pi * pi);
        sum_p += p;
        band_bins++;
        if (p > best_p)
        {
            best_p = p;
            best_k = k;
        }
    }

    if (!band_bins || best_k == 0)
    {
        return 0;
    }
    /* Require peak above mean band power */
    {
        uint32 mean_p = sum_p / band_bins;
        if (best_p < (mean_p * SMART_HELMET_HR_FFT_SNR_Q8) / 256u)
        {
            return 0;
        }
    }

    if (mag_out)
    {
        *mag_out = (best_p > 0xffffu) ? 0xffffu : (uint16)best_p;
    }

    /* Temporal smooth on bin index, then map to BPM */
    {
        uint16 k_use = best_k;
        uint16 alpha = SMART_HELMET_HR_FFT_SMOOTH_ALPHA_Q8;
        if (alpha > 256)
        {
            alpha = 256;
        }
        if (!fft_k_smooth_valid)
        {
            fft_k_smooth_q8 = (uint16)(best_k << 8);
            fft_k_smooth_valid = 1;
        }
        else
        {
            uint16 prev = (uint16)((fft_k_smooth_q8 + 128u) >> 8);
            uint16 jump = (best_k > prev) ? (uint16)(best_k - prev)
                                          : (uint16)(prev - best_k);
            /* Large hop: pull gently (half alpha) so one noisy frame cannot flip BPM */
            if (jump > SMART_HELMET_HR_FFT_MAX_BIN_JUMP)
            {
                alpha = (uint16)(alpha / 2u);
                if (alpha < 16)
                {
                    alpha = 16;
                }
            }
            fft_k_smooth_q8 = (uint16)(
                ((uint32)fft_k_smooth_q8 * (256u - alpha) +
                 ((uint32)best_k << 8) * alpha) / 256u);
        }
        k_use = (uint16)((fft_k_smooth_q8 + 128u) >> 8);
        if (k_use < 1)
        {
            k_use = 1;
        }
        bpm = (uint16)(((uint32)k_use * 60u * (uint32)FS_HZ) / (uint32)FFT_N);
    }
    if (bpm < SMART_HELMET_HR_BPM_MIN || bpm > SMART_HELMET_HR_BPM_MAX)
    {
        return 0;
    }
    return bpm;
}

static void shFftSmoothReset(void)
{
    fft_k_smooth_q8 = 0;
    fft_k_smooth_valid = 0;
}
#endif /* FFT */

static void shHrDisturb(uint8 holdoff_win)
{
    if (holdoff_win > hr_holdoff)
    {
        hr_holdoff = holdoff_win;
    }
    last_hr_valid_have = 0;
    last_hr_valid_bpm = 0;
#if SMART_HELMET_ENABLE_HR_FFT
    shFftSmoothReset();
#endif
}

/*! Second log line: human reason (literal strings — QCC log has no %s). */
static void shHrLogWhy(smart_helmet_hr_reason_t r, uint16 energy, uint16 sa,
                       uint8 ho, uint16 pk, uint16 fft, uint16 hr)
{
    switch (r)
    {
        case smart_helmet_hr_ok:
            CC_LOGN("Vitals: pulse MEANINGFUL hr=%u pk=%u fft=%u e=%u sa=%u",
                    hr, pk, fft, energy, sa);
            break;
        case smart_helmet_hr_warm:
            CC_LOGN("Vitals: pulse N/A warm-up (need more calm windows) ho=%u",
                    (unsigned)ho);
            break;
        case smart_helmet_hr_e_low:
            CC_LOGN("Vitals: pulse N/A E_low e=%u < min (SNR too weak)", energy);
            break;
        case smart_helmet_hr_e_high:
            CC_LOGN("Vitals: pulse N/A E_high e=%u > max (motion-like energy)", energy);
            break;
        case smart_helmet_hr_sa_low:
            CC_LOGN("Vitals: pulse N/A sa_low sa=%u (residual too weak)", sa);
            break;
        case smart_helmet_hr_sa_high:
            CC_LOGN("Vitals: pulse N/A sa_high sa=%u (motion-like residual)", sa);
            break;
        case smart_helmet_hr_dc_spike:
            CC_LOGN("Vitals: pulse N/A dc_spike sens jump (contact/motion) ho=%u",
                    (unsigned)ho);
            break;
        case smart_helmet_hr_holdoff:
            CC_LOGN("Vitals: pulse N/A holdoff ho=%u after disturb (pk=%u fft=%u)",
                    (unsigned)ho, pk, fft);
            break;
        case smart_helmet_hr_no_peak:
            CC_LOGN("Vitals: pulse N/A no_peak (fft=%u)", fft);
            break;
        case smart_helmet_hr_no_fft:
            CC_LOGN("Vitals: pulse N/A no_fft (pk=%u)", pk);
            break;
        case smart_helmet_hr_disagree:
            CC_LOGN("Vitals: pulse N/A disagree pk=%u vs fft=%u", pk, fft);
            break;
        case smart_helmet_hr_peak_only:
            CC_LOGN("Vitals: pulse N/A peak_only pk=%u (need FFT agree)", pk);
            break;
        case smart_helmet_hr_fft_only:
            CC_LOGN("Vitals: pulse N/A fft_only fft=%u (need peaks)", fft);
            break;
        case smart_helmet_hr_delta:
            CC_LOGN("Vitals: pulse N/A dBPM jump hr=%u vs last valid", hr);
            break;
        case smart_helmet_hr_none:
            CC_LOGN("Vitals: pulse N/A none (no peak/FFT estimate)");
            break;
        case smart_helmet_hr_disabled:
            CC_LOGN("Vitals: pulse N/A HR compile-disabled");
            break;
        default:
            CC_LOGN("Vitals: pulse N/A why=%u e=%u sa=%u", (unsigned)r, energy, sa);
            break;
    }
}

/*! Classify energy/sa band (0 = in band). */
static smart_helmet_hr_reason_t shHrSignalReason(uint16 energy, uint16 sens_abs)
{
    if (energy < SMART_HELMET_HR_MIN_ENERGY)
    {
        return smart_helmet_hr_e_low;
    }
    if (energy > SMART_HELMET_HR_MAX_ENERGY)
    {
        return smart_helmet_hr_e_high;
    }
#if SMART_HELMET_HR_MIN_SENS_ABS > 0
    if (sens_abs < SMART_HELMET_HR_MIN_SENS_ABS)
    {
        return smart_helmet_hr_sa_low;
    }
#endif
#if SMART_HELMET_HR_MAX_SENS_ABS > 0
    if (sens_abs > SMART_HELMET_HR_MAX_SENS_ABS)
    {
        return smart_helmet_hr_sa_high;
    }
#else
    UNUSED(sens_abs);
#endif
    return smart_helmet_hr_ok;
}

/*! Disturbance class for holdoff arming; ok means no disturb. */
static smart_helmet_hr_reason_t shHrDisturbReason(uint16 energy, uint16 sens_abs,
                                                  uint16 sens_mv)
{
    int32 ddc;

    if (energy > SMART_HELMET_HR_MAX_ENERGY)
    {
        return smart_helmet_hr_e_high;
    }
#if SMART_HELMET_HR_MAX_SENS_ABS > 0
    if (sens_abs > SMART_HELMET_HR_MAX_SENS_ABS)
    {
        return smart_helmet_hr_sa_high;
    }
#else
    UNUSED(sens_abs);
#endif
    if (prev_sens_mv_valid && SMART_HELMET_HR_SENS_DC_SPIKE_MV > 0)
    {
        ddc = (int32)sens_mv - (int32)prev_sens_mv;
        if (ddc < 0)
        {
            ddc = -ddc;
        }
        if (ddc >= (int32)SMART_HELMET_HR_SENS_DC_SPIKE_MV)
        {
            return smart_helmet_hr_dc_spike;
        }
    }
    return smart_helmet_hr_ok;
}

/*!
 * Fuse peak/FFT. pre_reason is already known blocker (holdoff/signal), or ok.
 * Sets hr_* and hr_reason (most specific failure wins if not valid).
 */
static void shFuseHr(uint16 bpm_peak, uint16 bpm_fft, uint8 peaks, uint16 fft_mag,
                     bool allow_valid, smart_helmet_hr_reason_t pre_reason)
{
    uint16 fused = 0;
    bool ok = FALSE;
    smart_helmet_hr_reason_t why = pre_reason;
    uint16 w_peak = SMART_HELMET_HR_FUSE_PEAK_W_Q8;
    uint8 min_peaks = SMART_HELMET_HR_MIN_PEAKS_VALID;

    if (w_peak > 256)
    {
        w_peak = 256;
    }

    sh_vitals.hr_bpm_peak = bpm_peak;
    sh_vitals.hr_bpm_fft = bpm_fft;
    sh_vitals.hr_peak_count = peaks;
    sh_vitals.hr_fft_mag = fft_mag;

    if (bpm_peak && bpm_fft)
    {
        int32 a = (int32)bpm_peak;
        int32 b = (int32)bpm_fft;
        int32 diff = (a > b) ? (a - b) : (b - a);
        int32 avg = (a + b) / 2;
        fused = (uint16)(((uint32)bpm_peak * w_peak +
                          (uint32)bpm_fft * (256u - w_peak)) / 256u);
        if (avg > 0 && (diff * 100) <= (avg * (int32)SMART_HELMET_HR_AGREE_PCT))
        {
            fused = (uint16)avg;
            ok = TRUE;
            if (why == smart_helmet_hr_ok)
            {
                why = smart_helmet_hr_ok;
            }
        }
        else
        {
#if !SMART_HELMET_HR_VALID_REQUIRE_AGREE
            if (peaks >= min_peaks)
            {
                fused = bpm_peak;
                ok = TRUE;
            }
            else
            {
                why = (why == smart_helmet_hr_ok) ? smart_helmet_hr_disagree : why;
            }
#else
            UNUSED(min_peaks);
            ok = FALSE;
            if (why == smart_helmet_hr_ok)
            {
                why = smart_helmet_hr_disagree;
            }
#endif
        }
    }
    else if (bpm_peak && peaks >= min_peaks)
    {
        fused = bpm_peak;
#if SMART_HELMET_HR_VALID_REQUIRE_AGREE
        ok = FALSE;
        if (why == smart_helmet_hr_ok)
        {
            why = smart_helmet_hr_peak_only;
        }
#else
        ok = TRUE;
#endif
    }
    else if (bpm_fft)
    {
        fused = bpm_fft;
        ok = FALSE;
        if (why == smart_helmet_hr_ok)
        {
            why = smart_helmet_hr_fft_only;
        }
    }
    else if (bpm_peak)
    {
        fused = bpm_peak;
        ok = FALSE;
        if (why == smart_helmet_hr_ok)
        {
            why = smart_helmet_hr_no_fft; /* weak peaks */
        }
    }
    else
    {
        if (why == smart_helmet_hr_ok)
        {
            why = smart_helmet_hr_none;
        }
    }

    if (!allow_valid)
    {
        ok = FALSE;
        if (pre_reason != smart_helmet_hr_ok)
        {
            why = pre_reason;
        }
        else if (hr_holdoff > 0 || pre_reason == smart_helmet_hr_holdoff)
        {
            why = smart_helmet_hr_holdoff;
        }
    }

    /* Reject sudden BPM jumps even when peak/FFT agree (post-glitch) */
    if (ok && last_hr_valid_have && SMART_HELMET_HR_MAX_DELTA_BPM > 0)
    {
        int32 d = (int32)fused - (int32)last_hr_valid_bpm;
        if (d < 0)
        {
            d = -d;
        }
        if (d > (int32)SMART_HELMET_HR_MAX_DELTA_BPM)
        {
            ok = FALSE;
            why = smart_helmet_hr_delta;
        }
    }

    if (ok)
    {
        why = smart_helmet_hr_ok;
        last_hr_valid_bpm = fused;
        last_hr_valid_have = 1;
    }

    sh_vitals.hr_bpm = fused;
    sh_vitals.hr_valid = ok;
    sh_vitals.hr_reason = why;
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
    sens_edge_armed = TRUE;
    sens_since_process = 0;
    calm_windows = 0;
    hr_holdoff = SMART_HELMET_HR_DISTURB_HOLDOFF_WIN;
    prev_sens_mv = 0;
    prev_sens_mv_valid = 0;
    last_hr_valid_bpm = 0;
    last_hr_valid_have = 0;
#if SMART_HELMET_ENABLE_HR_FFT
    shFftSmoothReset();
#endif
    sh_vitals.trend = smart_helmet_trend_unknown;
    sh_vitals.motion = smart_helmet_motion_active;
    CC_LOGN("SmartHelmet Vitals: SENS peak=%u FFT=%u (proxy BPM)",
            (unsigned)SMART_HELMET_ENABLE_HR_PEAK,
            (unsigned)SMART_HELMET_ENABLE_HR_FFT);
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
    int16 hp;
    int32 a;

    sh_vitals.sens_mv = mv;
    hp = shHighPass((int32)mv, &sens_prev_x, &sens_prev_y);
    sens_hp[sens_idx] = hp;
    sens_idx = (uint8)((sens_idx + 1) % BAND_WIN);
    if (sens_count < BAND_WIN)
    {
        sens_count++;
    }

    a = hp;
    if (a < 0)
    {
        a = -a;
    }
#if SMART_HELMET_SENS_EDGE_ABS_MV > 0
    if (sens_edge_armed && a >= (int32)SMART_HELMET_SENS_EDGE_ABS_MV)
    {
        SmartHelmet_VitalsPirEvent();
        sens_edge_armed = FALSE;
    }
    else if (!sens_edge_armed && a < (int32)(SMART_HELMET_SENS_EDGE_ABS_MV / 2))
    {
        sens_edge_armed = TRUE;
    }
#else
    UNUSED(a);
#endif
}

void SmartHelmet_VitalsPirEvent(void)
{
    if (pir_events < 0xffff)
    {
        pir_events++;
    }
}

void SmartHelmet_VitalsOnSensSample(void)
{
    if (sens_since_process < 0xff)
    {
        sens_since_process++;
    }
    if (sens_since_process >= MOTION_WIN)
    {
        sens_since_process = 0;
        SmartHelmet_VitalsProcess();
    }
}

void SmartHelmet_VitalsProcess(void)
{
    uint16 rms;
    uint16 e_accel;
    uint16 e_sens;
    uint16 sens_abs;
    uint16 energy;
    uint16 baseline;
    int32  delta_pct;
    bool   have_accel;
    bool   active;
    smart_helmet_trend_flag_t trend;
    uint8  n_ord;
    uint16 bpm_peak = 0;
    uint16 bpm_fft = 0;
    uint8  peak_n = 0;
    uint16 fft_mag = 0;

    have_accel = (motion_count >= (MOTION_WIN / 2));
    rms = have_accel ? shRmsU16(motion_mag, motion_count) : 0;
    sh_vitals.motion_rms_mg = rms;
    sh_vitals.pir_events_win = pir_events;

    e_sens = shEnergyI16(sens_hp, sens_count);
    sens_abs = shMeanAbsI16(sens_hp, sens_count);
    e_accel = have_accel ? shEnergyI16(band_hp, band_count) : 0;

    active = FALSE;
    if (have_accel && rms >= SMART_HELMET_MOTION_RMS_MG)
    {
        active = TRUE;
    }
    if (!have_accel && sens_count > (BAND_WIN / 4) &&
        sens_abs >= SMART_HELMET_SENS_MOTION_ABS_MV)
    {
        active = TRUE;
    }

    if (active)
    {
        sh_vitals.motion = smart_helmet_motion_active;
        sh_vitals.trend = smart_helmet_trend_unknown;
        sh_vitals.valid = FALSE;
        sh_vitals.hr_valid = FALSE;
        sh_vitals.hr_bpm = 0;
        sh_vitals.hr_bpm_peak = 0;
        sh_vitals.hr_bpm_fft = 0;
        sh_vitals.hr_reason = smart_helmet_hr_sa_high; /* or accel motion */
        if (have_accel && rms >= SMART_HELMET_MOTION_RMS_MG)
        {
            /* tag still sa_high-ish; log distinguishes via rms */
            sh_vitals.hr_reason = smart_helmet_hr_sa_high;
        }
        calm_windows = 0;
        pir_events = 0;
        shHrDisturb(SMART_HELMET_HR_DISTURB_HOLDOFF_WIN);
        sh_vitals.hr_holdoff = hr_holdoff;
        prev_sens_mv_valid = 0;
        CC_LOGN("Vitals: ACTIVE ok=0 why=motion rms=%u sa=%u ho=%u sens=%umV (pulse N/A)",
                rms, sens_abs, (unsigned)hr_holdoff, sh_vitals.sens_mv);
        return;
    }

    sh_vitals.motion = smart_helmet_motion_calm;

    if (sens_count > (BAND_WIN / 4))
    {
        energy = e_sens;
        if (have_accel && band_count > (BAND_WIN / 4))
        {
            energy = (uint16)(e_sens + (e_accel / 4));
        }
    }
    else if (have_accel)
    {
        energy = e_accel;
    }
    else
    {
        energy = 0;
    }
    energy = (uint16)(energy + pir_events * 8u);
    sh_vitals.band_energy = energy;

    baseline = sh_vitals.baseline_energy;
    if (baseline == 0)
    {
        baseline = energy ? energy : 1;
    }
    else
    {
        baseline = (uint16)(baseline +
            (((int32)energy - (int32)baseline) * SMART_HELMET_BASELINE_ALPHA_Q8) / 256);
    }
    sh_vitals.baseline_energy = baseline;

    if (calm_windows < 0xff)
    {
        calm_windows++;
    }

    {
        smart_helmet_hr_reason_t dwhy =
            shHrDisturbReason(energy, sens_abs, sh_vitals.sens_mv);
        /* Motion-like residual while gate still calm, or SENS DC spike */
        if (dwhy != smart_helmet_hr_ok)
        {
            shHrDisturb(SMART_HELMET_HR_DISTURB_HOLDOFF_WIN);
        }
    }

    if (calm_windows < SMART_HELMET_CALM_WINDOWS_MIN ||
        (sens_count < (BAND_WIN / 4) && !have_accel))
    {
        trend = smart_helmet_trend_unknown;
        sh_vitals.valid = FALSE;
        shHrDisturb(SMART_HELMET_HR_DISTURB_HOLDOFF_WIN);
        shFuseHr(0, 0, 0, 0, FALSE, smart_helmet_hr_warm);
    }
    else
    {
        bool allow_hr_valid;
        smart_helmet_hr_reason_t pre = smart_helmet_hr_ok;
        smart_helmet_hr_reason_t sig;

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

        allow_hr_valid = (hr_holdoff == 0) ? TRUE : FALSE;
        if (hr_holdoff > 0)
        {
            pre = smart_helmet_hr_holdoff;
            hr_holdoff--;
        }

#if (SMART_HELMET_ENABLE_HR_PEAK || SMART_HELMET_ENABLE_HR_FFT)
        sig = shHrSignalReason(energy, sens_abs);
        if (sig != smart_helmet_hr_ok)
        {
            /* Prefer concrete signal reason over generic holdoff */
            shFuseHr(0, 0, 0, 0, FALSE, sig);
        }
        else
        {
            n_ord = shCopySensOrdered(sens_count);
#if SMART_HELMET_ENABLE_HR_PEAK
            bpm_peak = shPeakBpm(sh_ordered, n_ord, &peak_n);
#endif
#if SMART_HELMET_ENABLE_HR_FFT
            bpm_fft = shFftBpm(sh_ordered, n_ord, &fft_mag);
#endif
            /* Refine empty detectors */
            if (!bpm_peak && !bpm_fft)
            {
                pre = (pre == smart_helmet_hr_holdoff) ? pre : smart_helmet_hr_none;
            }
            else if (!bpm_peak && bpm_fft && pre == smart_helmet_hr_ok)
            {
                /* fuse will mark fft_only */
            }
            else if (bpm_peak && !bpm_fft && pre == smart_helmet_hr_ok)
            {
                /* fuse will mark peak_only / no_fft */
            }
            shFuseHr(bpm_peak, bpm_fft, peak_n, fft_mag, allow_hr_valid, pre);
        }
#else
        UNUSED(n_ord);
        shFuseHr(0, 0, 0, 0, FALSE, smart_helmet_hr_disabled);
#endif
    }

    prev_sens_mv = sh_vitals.sens_mv;
    prev_sens_mv_valid = 1;

    sh_vitals.trend = trend;
    sh_vitals.hr_holdoff = hr_holdoff;
    pir_events = 0;

    /*
     * ok=1 => pulse proxy meaningful (hr_valid).
     * why= smart_helmet_hr_reason_t:
     *  0 OK  1 warm  2 E_low  3 E_high  4 sa_low  5 sa_high
     *  6 dc_spk  7 hold  8 no_pk  9 no_fft  10 disagr
     *  11 pk_only  12 fft_only  13 dBPM  14 none  15 off
     */
    CC_LOGN("Vitals: calm ok=%u why=%u e=%u base=%u tr=%u hr=%u pk=%u fft=%u pkn=%u ho=%u sa=%u sens=%umV",
            sh_vitals.hr_valid ? 1u : 0u,
            (unsigned)sh_vitals.hr_reason,
            energy, baseline, (unsigned)trend,
            sh_vitals.hr_bpm, sh_vitals.hr_bpm_peak, sh_vitals.hr_bpm_fft,
            (unsigned)sh_vitals.hr_peak_count,
            (unsigned)hr_holdoff,
            sens_abs, sh_vitals.sens_mv);
    shHrLogWhy(sh_vitals.hr_reason, energy, sens_abs, hr_holdoff,
               sh_vitals.hr_bpm_peak, sh_vitals.hr_bpm_fft, sh_vitals.hr_bpm);
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
void SmartHelmet_VitalsOnSensSample(void) {}
void SmartHelmet_VitalsProcess(void) {}
const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void)
{
    static smart_helmet_vitals_status_t empty;
    return &empty;
}

#endif /* SMART_HELMET_ENABLE_VITALS_PROXY */
