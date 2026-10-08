/*!
\file       smart_helmet_vitals.c
\brief      motion_gate + band_energy + peak BPM + fixed-point FFT BPM

SENS_IN primary. Peak detect gives beat intervals; FFT finds dominant
frequency in ~0.8–3 Hz. Both are proxies, not clinical HR.
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ONx
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
static bool   sh_fall_pending;

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
static uint8  hr_win_left;          /* windows left before hr_valid allowed */
static uint16 prev_sens_mv;        /* last process-window SENS DC */
static uint8  prev_sens_mv_valid;
static uint16 last_hr_valid_bpm;   /* last fused BPM with hr_valid */
static uint8  last_hr_valid_have;
static uint8  hr_delta_rej;        /* consecutive dBPM rejections */
static uint8  sens_dropped;        /* SENS samples missed this window */

#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
static smart_helmet_vitals_sink_t sh_sink;
static void  *sh_sink_ctx;
static char   sh_telem[SMART_HELMET_TELEMETRY_LINE_MAX];
static uint16 sh_telem_len;
#endif
#if SMART_HELMET_ENABLE_VITALS_CSV
static uint32 sh_win_seq;          /* processed-window counter */
#endif
#if SMART_HELMET_ENABLE_SENS_DUMP
static uint32 sh_dump_seq;         /* raw sample counter */
#endif

/*!
 * Effective sample rate in Q8 Hz. Either the nominal rate or, with
 * SMART_HELMET_ENABLE_VITALS_TIMESTAMP, the measured one. All BPM maths goes
 * through this so a drifting timer does not silently scale the result.
 */
static uint16 fs_q8 = (uint16)(FS_HZ * 256u);

#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
static uint32 ts_prev_us;
static uint8  ts_prev_valid;
static uint32 ts_sum_us;           /* sum of intervals this window */
static uint32 ts_jit_sum_us;       /* sum of |interval - nominal| */
static uint16 ts_n;                /* intervals accumulated */
static uint8  timebase_ok = 1;
#endif

#if SMART_HELMET_ENABLE_RESP
static int16  resp_buf[SMART_HELMET_RESP_WIN];
static uint8  resp_idx;
static uint8  resp_count;
static uint8  resp_decim_n;
static int32  resp_decim_acc;
static int32  resp_dc;             /* slow DC tracker (Q8) */
static uint8  resp_dc_valid;
static int16  resp_ordered[SMART_HELMET_RESP_WIN];
#endif

#if SMART_HELMET_ENABLE_HR_CUSUM
static uint16 hr_base_bpm;
static uint8  hr_base_n;
#if SMART_HELMET_HR_BASELINE_STALE_WIN
static uint16 hr_invalid_run;      /* consecutive windows without hr_valid */
#endif
static int32  hr_cusum_up;
static int32  hr_cusum_dn;
#endif

#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
/* NLMS motion canceller: accel residual history + adaptive weights (Q12). */
#define ADAPT_AXES SMART_HELMET_HR_ADAPT_REF_AXES
static int16  adapt_ref[ADAPT_AXES][SMART_HELMET_HR_ADAPT_TAPS];
static int32  adapt_w[ADAPT_AXES][SMART_HELMET_HR_ADAPT_TAPS];
static uint8  adapt_ref_n;
static int16  adapt_last_accel_hp[ADAPT_AXES];
/* Per-axis high-pass state, used only for the multi-channel reference. */
static int32  adapt_axis_px[ADAPT_AXES];
static int32  adapt_axis_py[ADAPT_AXES];
static uint8  adapt_have_accel;
static uint32 adapt_e_in;
static uint32 adapt_e_out;
#endif

/* Chronological scratch for peak/FFT/autocorrelation (max BAND_WIN) */
#if (SMART_HELMET_ENABLE_HR_PEAK || SMART_HELMET_ENABLE_HR_FFT || \
     SMART_HELMET_ENABLE_HR_AUTOCORR)
static int16  sh_ordered[BAND_WIN];
#endif

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

#if (SMART_HELMET_ENABLE_HR_PEAK || SMART_HELMET_ENABLE_HR_AUTOCORR)
/*!
 * BPM from an interval expressed in samples (Q8), using the effective rate.
 * BPM = 60 * fs / interval.
 */
static uint16 shIntervalQ8ToBpm(uint16 interval_q8)
{
    if (!interval_q8)
    {
        return 0;
    }
    return (uint16)((60u * (uint32)fs_q8) / (uint32)interval_q8);
}
#endif

/*! Saturating accumulate so a motion burst cannot wrap the sum to a small value. */
static void shAccSat(uint32 *sum, uint32 term)
{
    uint32 s = *sum + term;
    if (s < *sum)
    {
        s = 0xffffffffu;
    }
    *sum = s;
}

/*!
 * RMS of the accelerometer magnitude around its own mean.
 * The mean carries the ~1000 mg gravity vector, so it must be removed or the
 * gate would always report ACTIVE once the LIS3DH is populated.
 */
static uint16 shRmsDevU16(const uint16 *buf, uint8 n)
{
    uint32 sum = 0;
    uint32 mean;
    uint8 i;
    if (!n)
    {
        return 0;
    }
    for (i = 0; i < n; i++)
    {
        sum += buf[i];
    }
    mean = sum / n;
    sum = 0;
    for (i = 0; i < n; i++)
    {
        int32 d = (int32)buf[i] - (int32)mean;
        shAccSat(&sum, (uint32)(d * d));
    }
    return shSqrtU32(sum / n);
}

static uint16 shEnergyI16(const int16 *buf, uint8 n)
{
    uint32 sum = 0;
    uint32 mean;
    uint8 i;
    if (!n)
    {
        return 0;
    }
    for (i = 0; i < n; i++)
    {
        int32 v = buf[i];
        shAccSat(&sum, (uint32)(v * v));
    }
    mean = sum / n;
    return (mean > 0xffffu) ? 0xffffu : (uint16)mean;
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

#if (SMART_HELMET_ENABLE_HR_PEAK || SMART_HELMET_ENABLE_HR_FFT || \
     SMART_HELMET_ENABLE_HR_AUTOCORR)
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
#if SMART_HELMET_ENABLE_HR_BANDPASS
    /*
     * HP buffer + forward 1-pole LP = band-pass over the analysis chunk only.
     * band_energy / sens_abs keep using the untouched HP buffer so the
     * existing energy thresholds stay comparable.
     */
    {
        int32 y = sh_ordered[0];
        uint16 alpha = SMART_HELMET_HR_LP_ALPHA_Q8;
        if (alpha > 256)
        {
            alpha = 256;
        }
        for (i = 1; i < n; i++)
        {
            y += (((int32)sh_ordered[i] - y) * (int32)alpha) / 256;
            sh_ordered[i] = (int16)y;
        }
    }
#endif
    return n;
}
#endif /* any estimator */

#if (SMART_HELMET_ENABLE_HR_AUTOCORR || SMART_HELMET_ENABLE_RESP)
/*! Largest |sample| in a chunk (saturated to int16 range). */
static uint16 shMaxAbsI16(const int16 *x, uint8 n)
{
    uint16 m = 0;
    uint8 i;
    for (i = 0; i < n; i++)
    {
        int32 v = x[i];
        if (v < 0)
        {
            v = -v;
        }
        if ((uint32)v > m)
        {
            m = (uint16)v;
        }
    }
    return m;
}
#endif

#if (SMART_HELMET_ENABLE_HR_FFT && SMART_HELMET_HR_FFT_INTERP) || \
     SMART_HELMET_ENABLE_HR_AUTOCORR
/*!
 * Parabolic vertex offset of (a, b, c) around the centre sample, in Q8.
 * Result is clamped to +-0.5 bin. b must be the local maximum.
 */
static int16 shParabolicDeltaQ8(int32 a, int32 b, int32 c)
{
    int32 denom = (a - 2 * b + c);
    int32 num;
    int32 d;

    if (denom >= 0)
    {
        /* Not a strict local maximum: no meaningful vertex. */
        return 0;
    }
    num = (a - c) * 128;
    d = num / denom;
    if (d > 128)
    {
        d = 128;
    }
    if (d < -128)
    {
        d = -128;
    }
    return (int16)d;
}

/*! Scale (a, b, c) down so the parabolic maths stays inside int32. */
static void shFitScale3(uint32 *a, uint32 *b, uint32 *c)
{
    uint32 m = *a;
    if (*b > m)
    {
        m = *b;
    }
    if (*c > m)
    {
        m = *c;
    }
    while (m > 0x000fffffu)
    {
        *a >>= 1;
        *b >>= 1;
        *c >>= 1;
        m >>= 1;
    }
}
#endif

/*! BPM from a fractional FFT bin index (Q8). */
#if SMART_HELMET_ENABLE_HR_FFT
static uint16 shBinQ8ToBpm(uint16 k_q8)
{
    /* BPM = k * fs * 60 / N, with k in Q8 and fs taken as Q4 for headroom. */
    return (uint16)(((uint32)k_q8 * 60u * (uint32)(fs_q8 >> 4)) /
                    ((uint32)FFT_N * 4096u));
}
#endif

#if SMART_HELMET_ENABLE_HR_PEAK
#define SH_IBI_MAX 16

/*!
 * Local-maxima peak detect on ordered residual.
 * Returns peak count; writes a BPM estimate from the inter-beat intervals
 * (median or mean, see SMART_HELMET_HR_PEAK_IBI_MEDIAN) when >=2 peaks.
 */
static uint16 shPeakBpm(const int16 *x, uint8 n, uint8 *peak_count_out)
{
    uint8 i;
    uint8 peaks = 0;
    uint8 last_peak = 0xff;
    uint16 mean_abs;
    int32 thr;
    uint8 ibi_list[SH_IBI_MAX];
    uint32 ibi_sum = 0;
    uint8 ibi_n = 0;
    uint16 ibi_q8;
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
            if (ibi >= min_ibi && ibi <= max_ibi && ibi_n < SH_IBI_MAX)
            {
                ibi_list[ibi_n] = (uint8)ibi;
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

#if SMART_HELMET_HR_PEAK_IBI_MEDIAN
    UNUSED(ibi_sum);
    /* Median IBI: one missed or doubled beat no longer drags the estimate. */
    {
        uint8 a, b;
        for (a = 1; a < ibi_n; a++)
        {
            uint8 key = ibi_list[a];
            b = a;
            while (b > 0 && ibi_list[b - 1] > key)
            {
                ibi_list[b] = ibi_list[b - 1];
                b--;
            }
            ibi_list[b] = key;
        }
        if (ibi_n & 1u)
        {
            ibi_q8 = (uint16)((uint16)ibi_list[ibi_n / 2u] << 8);
        }
        else
        {
            ibi_q8 = (uint16)((((uint16)ibi_list[ibi_n / 2u - 1u] +
                                (uint16)ibi_list[ibi_n / 2u]) << 8) / 2u);
        }
    }
#else
    ibi_q8 = (uint16)((ibi_sum << 8) / ibi_n);
    UNUSED(ibi_list);
#endif

    if (!ibi_q8)
    {
        return 0;
    }

#if SMART_HELMET_HR_IBI_SPREAD_PCT > 0
    /* Irregular spacing => noise bursts rather than a rhythm. */
    if (ibi_n >= 3)
    {
        uint32 dev_max = 0;
        for (i = 0; i < ibi_n; i++)
        {
            int32 d = ((int32)ibi_list[i] << 8) - (int32)ibi_q8;
            if (d < 0)
            {
                d = -d;
            }
            if ((uint32)d > dev_max)
            {
                dev_max = (uint32)d;
            }
        }
        if ((dev_max * 100u) >
            ((uint32)ibi_q8 * SMART_HELMET_HR_IBI_SPREAD_PCT))
        {
            return 0;
        }
    }
#endif

    /* BPM = 60 * fs / ibi (ibi carried in Q8 for sub-sample resolution) */
    bpm = shIntervalQ8ToBpm(ibi_q8);
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

#if SMART_HELMET_HR_FFT_WINDOW_HANN
/*! cos(2*pi*i/FFT_N) in Q15 from the twiddle LUT. */
static int16 shCos64Q15(uint16 i)
{
    uint16 kk = (uint16)(i % FFT_N);
    if (kk <= 32)
    {
        return sh_fft_cos64_q15[kk];
    }
    return sh_fft_cos64_q15[FFT_N - kk];
}
#endif

/*! Fetch the i-th sample of the analysis frame (last FFT_N samples). */
static int16 shFftFrameSample(const int16 *x, uint8 n, uint16 i)
{
    if (n >= FFT_N)
    {
        return x[n - FFT_N + i];
    }
    return (i < n) ? x[i] : 0;
}

static uint16 shFftBpm(const int16 *x, uint8 n, uint16 *mag_out)
{
    uint16 i;
    uint16 k;
    uint16 k_lo;
    uint16 k_hi;
    uint16 best_k = 0;
    uint16 best_k_q8;
    uint32 best_p = 0;
    uint32 best_score = 0;
    uint32 sum_p = 0;
    uint16 band_bins = 0;
    uint16 bpm;
    int32 mean = 0;
    uint8 prescale = 0;

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
        mean += shFftFrameSample(x, n, i);
    }
    mean /= (int32)FFT_N;

#if SMART_HELMET_HR_FFT_PRESCALE
    /*
     * The butterfly divides by 2 per stage, so a residual of a few tens of mV
     * would be truncated away after 6 stages. Scale the frame up first and
     * keep max|x| <= 2^14 so the Q15 twiddle products stay inside int32.
     */
    {
        uint16 peak = 0;
        for (i = 0; i < FFT_N; i++)
        {
            int32 v = (int32)shFftFrameSample(x, n, i) - mean;
            if (v < 0)
            {
                v = -v;
            }
            if ((uint32)v > peak)
            {
                peak = (uint16)v;
            }
        }
        if (!peak)
        {
            return 0;
        }
        while (prescale < 12 && ((uint32)peak << (prescale + 1)) <= 16384u)
        {
            prescale++;
        }
    }
#endif

    for (i = 0; i < FFT_N; i++)
    {
        int32 v = (int32)shFftFrameSample(x, n, i) - mean;
#if SMART_HELMET_HR_FFT_WINDOW_HANN
        /* Hann: w = (1 - cos(2*pi*i/N)) / 2, applied in Q15. */
        v = (v * (int32)((32768 - shCos64Q15(i)) / 2)) / 32768;
#endif
        fft_re[i] = v << prescale;
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
        uint32 score = p;
#if SMART_HELMET_HR_FFT_HPS
        /*
         * Harmonic product spectrum (2 harmonics): a real pulse has energy at
         * the rate and its first harmonic, whereas a motion/interference line
         * usually does not. This strongly suppresses the half-rate pick.
         */
        if ((uint32)(2u * k) < (FFT_N / 2u))
        {
            int32 hr_re = fft_re[2u * k] / 8;
            int32 hr_im = fft_im[2u * k] / 8;
            uint32 ph = (uint32)(hr_re * hr_re + hr_im * hr_im);
            /*
             * Geometric mean of the two, kept in 32-bit range. Note this
             * deliberately penalises harmonic-free lines, so enable it only
             * when the pulse waveform is expected to be harmonic-rich.
             */
            score = shSqrtU32(((p >> 8) + 1u) * ((ph >> 8) + 1u));
        }
        else
        {
            /* Harmonic out of range: score on the fundamental alone, same scale. */
            score = (p >> 8) + 1u;
        }
#endif
#if SMART_HELMET_HR_FFT_HPS
        sum_p += score;
#else
        sum_p += p;
#endif
        band_bins++;
        if (score > best_score)
        {
            best_score = score;
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
#if SMART_HELMET_HR_FFT_HPS
        /* With HPS the gate must use the same (harmonic-weighted) score. */
        if (best_score < (mean_p * SMART_HELMET_HR_FFT_SNR_Q8) / 256u)
#else
        if (best_p < (mean_p * SMART_HELMET_HR_FFT_SNR_Q8) / 256u)
#endif
        {
            return 0;
        }
    }

    if (mag_out)
    {
        *mag_out = (best_p > 0xffffu) ? 0xffffu : (uint16)best_p;
    }

    best_k_q8 = (uint16)(best_k << 8);
#if SMART_HELMET_HR_FFT_INTERP
    /*
     * Bin spacing is fs*60/N (23.4 BPM at 25 Hz / N=64). Interpolate the
     * spectral peak so the estimate is not quantised to 47/70/94 BPM.
     */
    if (best_k > k_lo && best_k < k_hi && best_k + 1 < (FFT_N / 2))
    {
        uint32 pa = (uint32)((fft_re[best_k - 1] / 8) * (fft_re[best_k - 1] / 8) +
                             (fft_im[best_k - 1] / 8) * (fft_im[best_k - 1] / 8));
        uint32 pb = best_p;
        uint32 pc = (uint32)((fft_re[best_k + 1] / 8) * (fft_re[best_k + 1] / 8) +
                             (fft_im[best_k + 1] / 8) * (fft_im[best_k + 1] / 8));
        int16 d_q8;
        shFitScale3(&pa, &pb, &pc);
        d_q8 = shParabolicDeltaQ8((int32)pa, (int32)pb, (int32)pc);
        best_k_q8 = (uint16)((int32)best_k_q8 + d_q8);
    }
#endif

    /* Temporal smooth on the (fractional) bin index, then map to BPM */
    {
        uint16 k_use;
        uint16 alpha = SMART_HELMET_HR_FFT_SMOOTH_ALPHA_Q8;
        if (alpha > 256)
        {
            alpha = 256;
        }
        if (!fft_k_smooth_valid)
        {
            fft_k_smooth_q8 = best_k_q8;
            fft_k_smooth_valid = 1;
        }
        else
        {
            uint16 jump = (best_k_q8 > fft_k_smooth_q8)
                              ? (uint16)(best_k_q8 - fft_k_smooth_q8)
                              : (uint16)(fft_k_smooth_q8 - best_k_q8);
            /* Large hop: pull gently (half alpha) so one noisy frame cannot flip BPM */
            if (jump > (uint16)(SMART_HELMET_HR_FFT_MAX_BIN_JUMP << 8))
            {
                alpha = (uint16)(alpha / 2u);
                if (alpha < 16)
                {
                    alpha = 16;
                }
            }
            fft_k_smooth_q8 = (uint16)(
                ((uint32)fft_k_smooth_q8 * (256u - alpha) +
                 (uint32)best_k_q8 * alpha) / 256u);
        }
        k_use = fft_k_smooth_q8;
        if (k_use < 256u)
        {
            k_use = 256u;
        }
        bpm = shBinQ8ToBpm(k_use);
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

#if SMART_HELMET_ENABLE_HR_AUTOCORR

#define SH_AC_MAX_LAG   ((FS_HZ * 60u) / SMART_HELMET_HR_BPM_MIN)
#define SH_AC_MIN_LAG   ((FS_HZ * 60u) / SMART_HELMET_HR_BPM_MAX)

/*! Normalised correlation per lag, indexed by (lag - SH_AC_MIN_LAG). */
static int32 sh_ac_r[SH_AC_MAX_LAG - SH_AC_MIN_LAG + 1];

/*!
 * Autocorrelation BPM: the lag inside the HR band with the strongest
 * normalised correlation. Periodicity based, so it survives weak or partly
 * clipped individual beats better than the peak detector.
 * \param quality_q8 r(lag)/r(0) of the winning lag, Q8. May be NULL.
 */
static uint16 shAutoCorrBpm(const int16 *x, uint8 n, uint16 *quality_q8)
{
    uint16 min_lag = SH_AC_MIN_LAG;
    uint16 max_lag = SH_AC_MAX_LAG;
    uint16 lag;
    uint16 best_lag = 0;
    int32  best_r = 0;
    int32  r0;
    uint16 lag_q8;
    uint16 bpm;
    uint8  shift = 0;
    uint16 peak;
    uint8  i;

    if (quality_q8)
    {
        *quality_q8 = 0;
    }
    if (!min_lag)
    {
        min_lag = 1;
    }
    /* Need enough overlap for the slowest rate under test. */
    if (n <= (uint8)(min_lag + 1u))
    {
        return 0;
    }
    if ((uint16)n <= max_lag + min_lag)
    {
        max_lag = (uint16)(n - min_lag);
    }
    if (max_lag < min_lag)
    {
        return 0;
    }

    /* Scale so the products stay inside int32 for the whole window. */
    peak = shMaxAbsI16(x, n);
    if (!peak)
    {
        return 0;
    }
    while (shift < 15 &&
           ((((uint32)peak >> shift) * ((uint32)peak >> shift)) * n) > 0x3fffffffu)
    {
        shift++;
    }

    r0 = 0;
    for (i = 0; i < n; i++)
    {
        int32 v = (int32)x[i] >> shift;
        r0 += v * v;
    }
    if (r0 <= 0)
    {
        return 0;
    }

    for (lag = min_lag; lag <= max_lag; lag++)
    {
        int32 acc = 0;
        uint8 cnt = (uint8)(n - lag);
        int32 r;
        for (i = 0; i < cnt; i++)
        {
            acc += ((int32)x[i] >> shift) * ((int32)x[i + lag] >> shift);
        }
        /*
         * Biased estimator (common divisor n, i.e. the raw sum): long lags
         * keep the taper from the shrinking overlap, which is what stops the
         * search from locking onto the 1/2-rate sub-harmonic.
         */
        r = acc;
        sh_ac_r[lag - min_lag] = r;
        if (!best_lag || r > best_r)
        {
            best_r = r;
            best_lag = lag;
        }
    }

    if (!best_lag || best_r <= 0)
    {
        return 0;
    }
    {
        uint32 denom = ((uint32)r0) >> 8;
        uint32 q;
        if (!denom)
        {
            denom = 1;
        }
        q = (uint32)best_r / denom;
        if (quality_q8)
        {
            *quality_q8 = (q > 0xffffu) ? 0xffffu : (uint16)q;
        }
        if (q < SMART_HELMET_HR_AC_MIN_Q8)
        {
            return 0;
        }
    }

    lag_q8 = (uint16)(best_lag << 8);
    if (best_lag > min_lag && best_lag < max_lag)
    {
        int32 rp = sh_ac_r[best_lag - min_lag - 1];
        int32 rn = sh_ac_r[best_lag - min_lag + 1];
        if (rp > 0 && rn > 0)
        {
            uint32 a = (uint32)rp;
            uint32 b = (uint32)best_r;
            uint32 c = (uint32)rn;
            int16 d_q8;
            shFitScale3(&a, &b, &c);
            d_q8 = shParabolicDeltaQ8((int32)a, (int32)b, (int32)c);
            lag_q8 = (uint16)((int32)lag_q8 + d_q8);
        }
    }
    if (!lag_q8)
    {
        return 0;
    }

    bpm = shIntervalQ8ToBpm(lag_q8);
    if (bpm < SMART_HELMET_HR_BPM_MIN || bpm > SMART_HELMET_HR_BPM_MAX)
    {
        return 0;
    }
    return bpm;
}
#endif /* AUTOCORR */

#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
/*!
 * NLMS motion canceller.
 *
 * Uses the accelerometer residual as a reference for the motion-correlated
 * part of the SENS residual and subtracts the adaptively filtered estimate,
 * instead of discarding the whole window. Falls through unchanged when no
 * accelerometer sample has ever arrived (LIS3DH disabled or not detected),
 * which keeps the SENS-only configuration working exactly as before.
 */
static int16 shMotionCancel(int16 hp)
{
    int32 y = 0;
    int32 e;
    int32 norm = 0;
    uint8 i;
    uint8 ax;

    if (!adapt_have_accel)
    {
        return hp;
    }

    /* Newest reference sample first, one delay line per reference channel. */
    for (ax = 0; ax < ADAPT_AXES; ax++)
    {
        for (i = SMART_HELMET_HR_ADAPT_TAPS - 1u; i > 0; i--)
        {
            adapt_ref[ax][i] = adapt_ref[ax][i - 1];
        }
        adapt_ref[ax][0] = adapt_last_accel_hp[ax];
    }
    if (adapt_ref_n < SMART_HELMET_HR_ADAPT_TAPS)
    {
        adapt_ref_n++;
        return hp;   /* delay line not full yet */
    }

    for (ax = 0; ax < ADAPT_AXES; ax++)
    {
        for (i = 0; i < SMART_HELMET_HR_ADAPT_TAPS; i++)
        {
            int32 r = adapt_ref[ax][i];
            y += (adapt_w[ax][i] * r) / 4096;
            norm += r * r;
        }
    }

    if (y > 32767)
    {
        y = 32767;
    }
    if (y < -32768)
    {
        y = -32768;
    }
    e = (int32)hp - y;
    if (e > 32767)
    {
        e = 32767;
    }
    if (e < -32768)
    {
        e = -32768;
    }

    /* Track how much energy the filter removes, for the log / status. */
    {
        uint32 in2 = (uint32)((int32)hp * (int32)hp);
        uint32 out2 = (uint32)(e * e);
        adapt_e_in = adapt_e_in - (adapt_e_in >> 5) + (in2 >> 5);
        adapt_e_out = adapt_e_out - (adapt_e_out >> 5) + (out2 >> 5);
    }

    /* w += mu * e * ref / (norm + eps) */
    if (norm > 0)
    {
        int32 den = norm + 64;
        int32 mu_e = ((int32)SMART_HELMET_HR_ADAPT_MU_Q8 * e) / 256;
        for (ax = 0; ax < ADAPT_AXES; ax++)
        {
            for (i = 0; i < SMART_HELMET_HR_ADAPT_TAPS; i++)
            {
                int32 step = mu_e * adapt_ref[ax][i];
                int32 d = den;
                /*
                 * step * 4096 must stay inside int32, so scale numerator and
                 * denominator down together (the ratio is preserved).
                 */
                while (step > 524287 || step < -524288)
                {
                    step >>= 1;
                    d = (d > 1) ? (d >> 1) : 1;
                }
                adapt_w[ax][i] += (step * 4096) / d;
                /* Keep weights bounded so a glitch cannot run away. */
                if (adapt_w[ax][i] > (int32)(8 * 4096))
                {
                    adapt_w[ax][i] = (int32)(8 * 4096);
                }
                if (adapt_w[ax][i] < -(int32)(8 * 4096))
                {
                    adapt_w[ax][i] = -(int32)(8 * 4096);
                }
            }
        }
    }

    return (int16)e;
}

static void shMotionAdaptReset(void)
{
    memset(adapt_ref, 0, sizeof(adapt_ref));
    memset(adapt_w, 0, sizeof(adapt_w));
    adapt_ref_n = 0;
    memset(adapt_last_accel_hp, 0, sizeof(adapt_last_accel_hp));
    memset(adapt_axis_px, 0, sizeof(adapt_axis_px));
    memset(adapt_axis_py, 0, sizeof(adapt_axis_py));
    adapt_have_accel = 0;
    adapt_e_in = 0;
    adapt_e_out = 0;
}
#endif /* MOTION_ADAPT */

#if SMART_HELMET_ENABLE_RESP
/*!
 * Feed the respiration path: decimate by averaging, then remove the slow DC
 * so only the breathing excursion is left.
 */
static void shRespPush(uint16 mv)
{
    int32 v;

    resp_decim_acc += mv;
    resp_decim_n++;
    if (resp_decim_n < SMART_HELMET_RESP_DECIM)
    {
        return;
    }
    v = resp_decim_acc / SMART_HELMET_RESP_DECIM;
    resp_decim_acc = 0;
    resp_decim_n = 0;

    if (!resp_dc_valid)
    {
        resp_dc = v << 8;
        resp_dc_valid = 1;
    }
    else
    {
        /* Slow EMA: below the respiration band, so it only tracks drift. */
        resp_dc += (((v << 8) - resp_dc) * 12) / 256;
    }

    v -= (resp_dc >> 8);
    if (v > 32767)
    {
        v = 32767;
    }
    if (v < -32768)
    {
        v = -32768;
    }
    resp_buf[resp_idx] = (int16)v;
    resp_idx = (uint8)((resp_idx + 1) % SMART_HELMET_RESP_WIN);
    if (resp_count < SMART_HELMET_RESP_WIN)
    {
        resp_count++;
    }
}

/*!
 * Respiration rate by autocorrelation of the decimated envelope.
 * \param quality_q8 r(lag)/r(0) at the winning lag, Q8. May be NULL.
 */
static uint16 shRespBpm(uint16 *quality_q8)
{
    uint16 fs_resp_q8 = (uint16)(fs_q8 / SMART_HELMET_RESP_DECIM);
    uint16 min_lag;
    uint16 max_lag;
    uint16 lag;
    uint16 best_lag = 0;
    int32  best_r = 0;
    int32  r0 = 0;
    uint8  n;
    uint8  i;
    uint8  start;
    uint8  shift = 0;
    uint16 peak = 0;
    uint16 bpm;

    if (quality_q8)
    {
        *quality_q8 = 0;
    }
    if (!fs_resp_q8)
    {
        return 0;
    }

    /* lag = fs_resp * 60 / bpm */
    min_lag = (uint16)(((uint32)fs_resp_q8 * 60u) /
                       ((uint32)SMART_HELMET_RESP_BPM_MAX * 256u));
    max_lag = (uint16)(((uint32)fs_resp_q8 * 60u) /
                       ((uint32)SMART_HELMET_RESP_BPM_MIN * 256u));
    if (min_lag < 2)
    {
        min_lag = 2;
    }

    n = resp_count;
    /* Need at least two periods of the slowest rate for a stable estimate. */
    if (n < (uint8)(max_lag * 2u))
    {
        return 0;
    }
    if (max_lag >= n)
    {
        max_lag = (uint16)(n - 1u);
    }
    if (max_lag < min_lag)
    {
        return 0;
    }

    start = (uint8)((resp_idx + SMART_HELMET_RESP_WIN - n) % SMART_HELMET_RESP_WIN);
    for (i = 0; i < n; i++)
    {
        resp_ordered[i] = resp_buf[(start + i) % SMART_HELMET_RESP_WIN];
    }

    peak = shMaxAbsI16(resp_ordered, n);
    if (!peak)
    {
        return 0;
    }
    while (shift < 15 &&
           ((((uint32)peak >> shift) * ((uint32)peak >> shift)) * n) > 0x3fffffffu)
    {
        shift++;
    }

    for (i = 0; i < n; i++)
    {
        int32 v = (int32)resp_ordered[i] >> shift;
        r0 += v * v;
    }
    if (r0 <= 0)
    {
        return 0;
    }

    for (lag = min_lag; lag <= max_lag; lag++)
    {
        int32 acc = 0;
        uint8 cnt = (uint8)(n - lag);
        for (i = 0; i < cnt; i++)
        {
            acc += ((int32)resp_ordered[i] >> shift) *
                   ((int32)resp_ordered[i + lag] >> shift);
        }
        if (!best_lag || acc > best_r)
        {
            best_r = acc;
            best_lag = lag;
        }
    }

    if (!best_lag || best_r <= 0)
    {
        return 0;
    }
    {
        uint32 den = ((uint32)r0) >> 8;
        uint32 q;
        if (!den)
        {
            den = 1;
        }
        q = (uint32)best_r / den;
        if (quality_q8)
        {
            *quality_q8 = (q > 0xffffu) ? 0xffffu : (uint16)q;
        }
        if (q < SMART_HELMET_RESP_MIN_Q8)
        {
            return 0;
        }
    }

    bpm = (uint16)(((uint32)fs_resp_q8 * 60u) /
                   ((uint32)best_lag * 256u));
    if (bpm < SMART_HELMET_RESP_BPM_MIN || bpm > SMART_HELMET_RESP_BPM_MAX)
    {
        return 0;
    }
    return bpm;
}
#endif /* RESP */

#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
/*
 * Minimal CSV builder. The firmware has no printf, and the QCC log macro
 * cannot carry a formatted string, so records are assembled by hand.
 */
static void shTelemReset(void)
{
    sh_telem_len = 0;
    sh_telem[0] = '\0';
}

static void shTelemChar(char c)
{
    /* Leave room for the NUL terminator. */
    if (sh_telem_len + 1u < (uint16)SMART_HELMET_TELEMETRY_LINE_MAX)
    {
        sh_telem[sh_telem_len++] = c;
        sh_telem[sh_telem_len] = '\0';
    }
}

static void shTelemU32(uint32 v)
{
    char tmp[10];
    uint8 n = 0;

    if (!v)
    {
        shTelemChar('0');
        return;
    }
    while (v && n < sizeof(tmp))
    {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n)
    {
        shTelemChar(tmp[--n]);
    }
}

/*! Append a field, preceded by a comma unless it is the first one. */
static void shTelemFieldI32(int32 v)
{
    if (sh_telem_len)
    {
        shTelemChar(',');
    }
    if (v < 0)
    {
        shTelemChar('-');
        /* -(-2147483648) overflows int32, so negate in uint32 space. */
        shTelemU32(0u - (uint32)v);
    }
    else
    {
        shTelemU32((uint32)v);
    }
}

static void shTelemFlush(void)
{
    if (sh_sink && sh_telem_len)
    {
        sh_sink(sh_telem, sh_telem_len, sh_sink_ctx);
    }
    shTelemReset();
}
#endif /* CSV || DUMP */

#if SMART_HELMET_ENABLE_VITALS_CSV
/*!
 * One CSV record per processed window.
 *
 * Column order is fixed and documented in headset/test/smart_helmet/README.md;
 * the offline analysis scripts depend on it, so append new columns at the end
 * rather than inserting them.
 */
static void shTelemCsvWindow(uint16 energy, uint16 baseline, uint16 sens_abs,
                             uint16 rms, uint8 active)
{
    if (!sh_sink)
    {
        return;
    }
    shTelemReset();
    shTelemFieldI32((int32)sh_win_seq);
    shTelemFieldI32((int32)active);
    shTelemFieldI32(sh_vitals.hr_valid ? 1 : 0);
    shTelemFieldI32((int32)sh_vitals.hr_reason);
    shTelemFieldI32((int32)sh_vitals.hr_bpm);
    shTelemFieldI32((int32)sh_vitals.hr_bpm_peak);
    shTelemFieldI32((int32)sh_vitals.hr_bpm_fft);
    shTelemFieldI32((int32)sh_vitals.hr_bpm_ac);
    shTelemFieldI32((int32)sh_vitals.hr_baseline_bpm);
    shTelemFieldI32((int32)sh_vitals.hr_delta_bpm);
    shTelemFieldI32((int32)sh_vitals.hr_cusum_up);
    shTelemFieldI32((int32)sh_vitals.hr_cusum_dn);
    shTelemFieldI32((int32)sh_vitals.hr_change);
    shTelemFieldI32((int32)sh_vitals.resp_bpm);
    shTelemFieldI32((int32)sh_vitals.resp_q8);
    shTelemFieldI32((int32)sh_vitals.fs_x100);
    shTelemFieldI32((int32)sh_vitals.fs_jitter_pct);
    shTelemFieldI32((int32)sh_vitals.sens_dropped);
    shTelemFieldI32((int32)energy);
    shTelemFieldI32((int32)baseline);
    shTelemFieldI32((int32)sens_abs);
    shTelemFieldI32((int32)rms);
    shTelemFieldI32((int32)sh_vitals.adapt_removed_pct);
    shTelemFieldI32((int32)sh_vitals.sens_mv);
    shTelemFlush();
    sh_win_seq++;
}
#endif /* CSV */

/*!
 * Change-detection state, emitted as a second log line and (optionally) as a
 * CSV record. Split from the main calm line because the QCC log macro takes a
 * limited number of arguments.
 */
static void shVitalsReport(uint16 energy, uint16 baseline, uint16 sens_abs,
                           uint16 rms, uint8 active)
{
#if SMART_HELMET_ENABLE_VITALS_LOG2
    CC_LOGN("Vitals: chg=%u base=%u d=%d cup=%u cdn=%u resp=%u rq=%u fs=%u jit=%u drop=%u rem=%u act=%u",
            (unsigned)sh_vitals.hr_change,
            (unsigned)sh_vitals.hr_baseline_bpm,
            (int)sh_vitals.hr_delta_bpm,
            (unsigned)sh_vitals.hr_cusum_up,
            (unsigned)sh_vitals.hr_cusum_dn,
            (unsigned)sh_vitals.resp_bpm,
            (unsigned)sh_vitals.resp_q8,
            (unsigned)sh_vitals.fs_x100,
            (unsigned)sh_vitals.fs_jitter_pct,
            (unsigned)sh_vitals.sens_dropped,
            (unsigned)sh_vitals.adapt_removed_pct,
            (unsigned)active);
#endif
#if SMART_HELMET_ENABLE_VITALS_CSV
    shTelemCsvWindow(energy, baseline, sens_abs, rms, active);
#else
    UNUSED(energy);
    UNUSED(baseline);
    UNUSED(sens_abs);
    UNUSED(rms);
#endif
#if !SMART_HELMET_ENABLE_VITALS_LOG2
    UNUSED(active);
#endif
}

static void shHrDisturb(uint8 holdoff_win)
{
	if (holdoff_win > hr_win_left)
    {
		hr_win_left = holdoff_win;
    }
    last_hr_valid_have = 0;
    last_hr_valid_bpm = 0;
    hr_delta_rej = 0;
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
		case hr_ok:
			CC_LOGN("Vitals: pulse MEANINGFUL hr=%u pk=%u fft=%u e=%u sa=%u", hr, pk, fft, energy, sa);
            break;
		case hr_warm:
			CC_LOGN("Vitals: pulse N/A warm-up (need more calm windows) ho=%u", (unsigned)ho);
            break;
		case hr_e_low:
            CC_LOGN("Vitals: pulse N/A E_low e=%u < min (SNR too weak)", energy);
            break;
		case hr_e_high:
            CC_LOGN("Vitals: pulse N/A E_high e=%u > max (motion-like energy)", energy);
            break;
		case hr_sa_low:
            CC_LOGN("Vitals: pulse N/A sa_low sa=%u (residual too weak)", sa);
            break;
		case hr_sa_high:
            CC_LOGN("Vitals: pulse N/A sa_high sa=%u (motion-like residual)", sa);
            break;
		case hr_dc_spike:
			CC_LOGN("Vitals: pulse N/A dc_spike sens jump (contact/motion) ho=%u", (unsigned)ho);
            break;
		case hr_holdoff:
			CC_LOGN("Vitals: pulse N/A holdoff ho=%u after disturb (pk=%u fft=%u)", (unsigned)ho, pk, fft);
            break;
		case hr_no_peak:
            CC_LOGN("Vitals: pulse N/A no_peak (fft=%u)", fft);
            break;
		case hr_no_fft:
            CC_LOGN("Vitals: pulse N/A no_fft (pk=%u)", pk);
            break;
		case hr_disagree:
            CC_LOGN("Vitals: pulse N/A disagree pk=%u vs fft=%u", pk, fft);
            break;
		case hr_peak_only:
            CC_LOGN("Vitals: pulse N/A peak_only pk=%u (need FFT agree)", pk);
            break;
		case hr_fft_only:
            CC_LOGN("Vitals: pulse N/A fft_only fft=%u (need peaks)", fft);
            break;
		case hr_delta:
            CC_LOGN("Vitals: pulse N/A dBPM jump hr=%u vs last valid", hr);
            break;
		case hr_none:
            CC_LOGN("Vitals: pulse N/A none (no peak/FFT estimate)");
            break;
		case hr_disabled:
            CC_LOGN("Vitals: pulse N/A HR compile-disabled");
            break;
		case hr_motion:
            CC_LOGN("Vitals: pulse N/A motion gate active e=%u sa=%u", energy, sa);
            break;
        default:
            CC_LOGN("Vitals: pulse N/A why=%u e=%u sa=%u", (unsigned)r, energy, sa);
            break;
    }
}

#if (SMART_HELMET_ENABLE_HR_PEAK || SMART_HELMET_ENABLE_HR_FFT || \
     SMART_HELMET_ENABLE_HR_AUTOCORR)
/*! Classify energy/sa band (0 = in band). */
static smart_helmet_hr_reason_t shHrSignalReason(uint16 energy, uint16 sens_abs)
{
    if (energy < SMART_HELMET_HR_MIN_ENERGY)
    {
		return hr_e_low;
    }
    if (energy > SMART_HELMET_HR_MAX_ENERGY)
    {
		return hr_e_high;
    }
#if SMART_HELMET_HR_MIN_SENS_ABS > 0
    if (sens_abs < SMART_HELMET_HR_MIN_SENS_ABS)
    {
		return hr_sa_low;
    }
#endif
#if SMART_HELMET_HR_MAX_SENS_ABS > 0
    if (sens_abs > SMART_HELMET_HR_MAX_SENS_ABS)
    {
		return hr_sa_high;
    }
#else
    UNUSED(sens_abs);
#endif
	return hr_ok;
}
#endif /* any estimator */

/*! Disturbance class for holdoff arming; ok means no disturb. */
static smart_helmet_hr_reason_t shHrDisturbReason(uint16 energy, uint16 sens_abs,
                                                  uint16 sens_mv)
{
    int32 ddc;

    if (energy > SMART_HELMET_HR_MAX_ENERGY)
    {
		return hr_e_high;
    }
#if SMART_HELMET_HR_MAX_SENS_ABS > 0
    if (sens_abs > SMART_HELMET_HR_MAX_SENS_ABS)
    {
		return hr_sa_high;
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
			return hr_dc_spike;
        }
    }
	return hr_ok;
}

/*!
 * Fuse peak/FFT. pre_reason is already known blocker (holdoff/signal), or ok.
 * Sets hr_* and hr_reason (most specific failure wins if not valid).
 */
static void shFuseHr(uint16 bpm_peak, uint16 bpm_fft, uint16 bpm_ac, uint8 peaks,
                     uint16 fft_mag, bool allow_valid,
                     smart_helmet_hr_reason_t pre_reason)
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
    sh_vitals.hr_bpm_ac = bpm_ac;
    sh_vitals.hr_peak_count = peaks;
    sh_vitals.hr_fft_mag = fft_mag;

#if SMART_HELMET_HR_FUSE_MODE
    UNUSED(w_peak);
    /*
     * Consensus mode: median of every enabled estimator, then require
     * HR_FUSE_MIN_AGREE of them within HR_AGREE_PCT of that median.
     */
    {
        uint16 cand[3];
        uint8 ncand = 0;
        uint8 a, b;

        if (bpm_peak && peaks >= min_peaks)
        {
            cand[ncand++] = bpm_peak;
        }
        if (bpm_fft)
        {
            cand[ncand++] = bpm_fft;
        }
        if (bpm_ac)
        {
            cand[ncand++] = bpm_ac;
        }

        for (a = 1; a < ncand; a++)
        {
            uint16 key = cand[a];
            b = a;
            while (b > 0 && cand[b - 1] > key)
            {
                cand[b] = cand[b - 1];
                b--;
            }
            cand[b] = key;
        }

        if (ncand == 0)
        {
            if (why == hr_ok)
            {
                why = (bpm_peak || bpm_fft || bpm_ac) ? hr_no_peak : hr_none;
            }
        }
        else
        {
            uint16 med = cand[ncand / 2u];
            uint32 sum = 0;
            uint8 agree = 0;
            for (a = 0; a < ncand; a++)
            {
                int32 d = (int32)cand[a] - (int32)med;
                if (d < 0)
                {
                    d = -d;
                }
                if ((d * 100) <= ((int32)med * (int32)SMART_HELMET_HR_AGREE_PCT))
                {
                    sum += cand[a];
                    agree++;
                }
            }
            fused = agree ? (uint16)(sum / agree) : med;
            if (agree >= SMART_HELMET_HR_FUSE_MIN_AGREE)
            {
                ok = TRUE;
            }
            else if (why == hr_ok)
            {
                why = (ncand > 1u) ? hr_disagree
                                   : (bpm_fft ? hr_fft_only : hr_peak_only);
            }
        }
    }
#else
    UNUSED(bpm_ac);
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
			if (why == hr_ok)
            {
				why = hr_ok;
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
				why = (why == hr_ok) ? hr_disagree : why;
            }
#else
            UNUSED(min_peaks);
            ok = FALSE;
			if (why == hr_ok)
            {
				why = hr_disagree;
            }
#endif
        }
    }
    else if (bpm_peak && peaks >= min_peaks)
    {
        fused = bpm_peak;
#if SMART_HELMET_HR_VALID_REQUIRE_AGREE
        ok = FALSE;
		if (why == hr_ok)
        {
			why = hr_peak_only;
        }
#else
        ok = TRUE;
#endif
    }
    else if (bpm_fft)
    {
        fused = bpm_fft;
        ok = FALSE;
		if (why == hr_ok)
        {
			why = hr_fft_only;
        }
    }
    else if (bpm_peak)
    {
        fused = bpm_peak;
        ok = FALSE;
		if (why == hr_ok)
        {
			why = hr_no_fft; /* weak peaks */
        }
    }
    else
    {
		if (why == hr_ok)
        {
			why = hr_none;
        }
    }
#endif /* SMART_HELMET_HR_FUSE_MODE */

    if (!allow_valid)
    {
        ok = FALSE;
		if (pre_reason != hr_ok)
        {
            why = pre_reason;
        }
		else if (hr_win_left > 0 || pre_reason == hr_holdoff)
        {
			why = hr_holdoff;
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
			why = hr_delta;
#if SMART_HELMET_HR_DELTA_RESYNC_WIN > 0
            /*
             * A real, fast change must not lock the guard out forever:
             * after a few consecutive rejections drop the stale reference.
             */
            if (hr_delta_rej < 0xff)
            {
                hr_delta_rej++;
            }
            if (hr_delta_rej >= SMART_HELMET_HR_DELTA_RESYNC_WIN)
            {
                last_hr_valid_have = 0;
                last_hr_valid_bpm = 0;
                hr_delta_rej = 0;
            }
#endif
        }
    }

    if (ok)
    {
		why = hr_ok;
        last_hr_valid_bpm = fused;
        last_hr_valid_have = 1;
        hr_delta_rej = 0;
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
    sh_fall_pending = FALSE;
    band_idx = band_count = 0;
    sens_idx = sens_count = 0;
    hp_prev_x = hp_prev_y = 0;
    sens_prev_x = sens_prev_y = 0;
    pir_events = 0;
    sens_edge_armed = TRUE;
    sens_since_process = 0;
    calm_windows = 0;
	hr_win_left = SMART_HELMET_HR_DISTURB_HOLDOFF_WIN;
    prev_sens_mv = 0;
    prev_sens_mv_valid = 0;
    last_hr_valid_bpm = 0;
    last_hr_valid_have = 0;
    hr_delta_rej = 0;
    sens_dropped = 0;
    fs_q8 = (uint16)(FS_HZ * 256u);
#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
    shTelemReset();
#endif
#if SMART_HELMET_ENABLE_VITALS_CSV
    sh_win_seq = 0;
#endif
#if SMART_HELMET_ENABLE_SENS_DUMP
    sh_dump_seq = 0;
#endif
#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
    ts_prev_us = 0;
    ts_prev_valid = 0;
    ts_sum_us = 0;
    ts_jit_sum_us = 0;
    ts_n = 0;
    timebase_ok = 1;
#endif
#if SMART_HELMET_ENABLE_RESP
    memset(resp_buf, 0, sizeof(resp_buf));
    resp_idx = resp_count = 0;
    resp_decim_n = 0;
    resp_decim_acc = 0;
    resp_dc = 0;
    resp_dc_valid = 0;
#endif
#if SMART_HELMET_ENABLE_HR_CUSUM
    hr_base_bpm = 0;
    hr_base_n = 0;
#if SMART_HELMET_HR_BASELINE_STALE_WIN
    hr_invalid_run = 0;
#endif
    hr_cusum_up = 0;
    hr_cusum_dn = 0;
#endif
#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
    shMotionAdaptReset();
#endif
#if SMART_HELMET_ENABLE_HR_FFT
    shFftSmoothReset();
#endif
    sh_vitals.trend = smart_helmet_trend_unknown;
    sh_vitals.motion = smart_helmet_motion_active;
    CC_LOGN("SmartHelmet Vitals: SENS peak=%u FFT=%u ac=%u bp=%u win=%u interp=%u fuse=%u",
            (unsigned)SMART_HELMET_ENABLE_HR_PEAK,
            (unsigned)SMART_HELMET_ENABLE_HR_FFT,
            (unsigned)SMART_HELMET_ENABLE_HR_AUTOCORR,
            (unsigned)SMART_HELMET_ENABLE_HR_BANDPASS,
            (unsigned)SMART_HELMET_HR_FFT_WINDOW_HANN,
            (unsigned)SMART_HELMET_HR_FFT_INTERP,
            (unsigned)SMART_HELMET_HR_FUSE_MODE);
    CC_LOGN("SmartHelmet Vitals: ts=%u resp=%u cusum=%u hps=%u ovs=%u fifo=%u adapt=%u",
            (unsigned)SMART_HELMET_ENABLE_VITALS_TIMESTAMP,
            (unsigned)SMART_HELMET_ENABLE_RESP,
            (unsigned)SMART_HELMET_ENABLE_HR_CUSUM,
            (unsigned)SMART_HELMET_HR_FFT_HPS,
            (unsigned)SMART_HELMET_ENABLE_SENS_OVERSAMPLE,
            (unsigned)SMART_HELMET_LIS3DH_USE_FIFO,
            (unsigned)SMART_HELMET_ENABLE_HR_MOTION_ADAPT);
}

static void shNoteFallMag(uint16 mag)
{
    if (mag == 0)
    {
        return;
    }
    if (mag >= SMART_HELMET_FALL_IMPACT_MG ||
        mag <= SMART_HELMET_FALL_FREEFALL_MG)
    {
        sh_fall_pending = TRUE;
    }
}

static void shNoteFallWindow(uint16 rms, uint16 mn, uint16 pk)
{
    uint16 swing = (pk > mn) ? (uint16)(pk - mn) : 0;

    if (rms >= SMART_HELMET_FALL_RMS_MG ||
        pk >= SMART_HELMET_FALL_IMPACT_MG ||
        (mn > 0 && mn <= SMART_HELMET_FALL_FREEFALL_MG) ||
        swing >= SMART_HELMET_FALL_SWING_MG)
    {
        sh_fall_pending = TRUE;
    }
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
    shNoteFallMag(mag);

    hp = shHighPass((int32)mag, &hp_prev_x, &hp_prev_y);
#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
#if (SMART_HELMET_HR_ADAPT_REF_AXES == 3)
    /*
     * Per-axis reference. The high pass removes gravity, leaving a signed
     * disturbance the filter can actually subtract; the vector magnitude
     * cannot serve that purpose because gravity rectifies it.
     */
    adapt_last_accel_hp[0] = shHighPass(ax, &adapt_axis_px[0],
                                        &adapt_axis_py[0]);
    adapt_last_accel_hp[1] = shHighPass(ay, &adapt_axis_px[1],
                                        &adapt_axis_py[1]);
    adapt_last_accel_hp[2] = shHighPass(az, &adapt_axis_px[2],
                                        &adapt_axis_py[2]);
#else
    adapt_last_accel_hp[0] = hp;
#endif
    adapt_have_accel = 1;
#endif
    band_hp[band_idx] = hp;
    band_idx = (uint8)((band_idx + 1) % BAND_WIN);
    if (band_count < BAND_WIN)
    {
        band_count++;
    }
}

void SmartHelmet_VitalsPushSensInMv(uint16 mv)
{
    SmartHelmet_VitalsPushSensInMvAt(mv, 0);
}

void SmartHelmet_VitalsNoteSensDropped(void)
{
    if (sens_dropped < 0xff)
    {
        sens_dropped++;
    }
#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
    /* The next interval spans a gap and must not pollute the rate estimate. */
    ts_prev_valid = 0;
#endif
}

void SmartHelmet_VitalsSetSink(smart_helmet_vitals_sink_t sink, void *ctx)
{
#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
    sh_sink = sink;
    sh_sink_ctx = ctx;
    shTelemReset();
#else
    UNUSED(sink);
    UNUSED(ctx);
#endif
}

void SmartHelmet_VitalsPushSensInMvAt(uint16 mv, uint32 time_us)
{
    int16 hp;
    int32 a;

#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
    if (time_us)
    {
        if (ts_prev_valid)
        {
            /* Unsigned difference is wrap-safe for a monotonic microsecond clock. */
            uint32 dt = time_us - ts_prev_us;
            const uint32 nominal = 1000000u / FS_HZ;
            /* Ignore absurd gaps (scheduler stall, first sample after a drop). */
            if (dt >= (nominal / 4u) && dt <= (nominal * 4u))
            {
                uint32 jit = (dt > nominal) ? (dt - nominal) : (nominal - dt);
                ts_sum_us += dt;
                ts_jit_sum_us += jit;
                if (ts_n < 0xffff)
                {
                    ts_n++;
                }
            }
        }
        ts_prev_us = time_us;
        ts_prev_valid = 1;
    }
#else
    UNUSED(time_us);
#endif

    sh_vitals.sens_mv = mv;

#if SMART_HELMET_ENABLE_SENS_DUMP
    /* seq,mv,time_us -- replayable by the host harness. */
    if (sh_sink)
    {
        shTelemReset();
        shTelemFieldI32((int32)sh_dump_seq);
        shTelemFieldI32((int32)mv);
        shTelemFieldI32((int32)time_us);
        shTelemFlush();
        sh_dump_seq++;
    }
#endif

    hp = shHighPass((int32)mv, &sens_prev_x, &sens_prev_y);

#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
    hp = shMotionCancel(hp);
#endif

#if SMART_HELMET_ENABLE_RESP
    shRespPush(mv);
#endif

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

/*!
 * Refresh the effective sample rate from the measured intervals and decide
 * whether the window's time base is trustworthy.
 */
static void shUpdateTimebase(void)
{
#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
    const uint32 nominal = 1000000u / FS_HZ;

    if (ts_n >= (MOTION_WIN / 2u))
    {
        uint32 mean = ts_sum_us / ts_n;
        uint32 jit = ts_jit_sum_us / ts_n;
        uint32 err_pct;

        if (mean)
        {
            uint32 hz_x100 = (100u * 1000000u) / mean;
            uint32 q8 = (1000000u * 256u) / mean;
            sh_vitals.fs_x100 = (hz_x100 > 0xffffu) ? 0xffffu : (uint16)hz_x100;
            fs_q8 = (q8 > 0xffffu) ? 0xffffu : (uint16)q8;

            sh_vitals.fs_jitter_pct = (uint8)((jit * 100u) / mean > 255u ?
                                              255u : (jit * 100u) / mean);
            err_pct = (mean > nominal) ? (((mean - nominal) * 100u) / nominal)
                                       : (((nominal - mean) * 100u) / nominal);
            timebase_ok = (err_pct <= SMART_HELMET_VITALS_FS_TOL_PCT &&
                           sh_vitals.fs_jitter_pct <= SMART_HELMET_VITALS_JITTER_PCT)
                          ? 1u : 0u;
        }
    }
    else
    {
        /* Not enough clean intervals: fall back to the nominal rate. */
        fs_q8 = (uint16)(FS_HZ * 256u);
        sh_vitals.fs_x100 = 0;
        sh_vitals.fs_jitter_pct = 0;
        timebase_ok = 1;
    }
    ts_sum_us = 0;
    ts_jit_sum_us = 0;
    ts_n = 0;
#else
    fs_q8 = (uint16)(FS_HZ * 256u);
    sh_vitals.fs_x100 = 0;
    sh_vitals.fs_jitter_pct = 0;
#endif
}

/*! Refresh the respiration estimate (no-op when the option is off). */
static void shUpdateResp(void)
{
#if SMART_HELMET_ENABLE_RESP
    uint16 q8 = 0;
    uint16 bpm = shRespBpm(&q8);

    sh_vitals.resp_bpm = bpm;
    sh_vitals.resp_q8 = q8;
    sh_vitals.resp_valid = bpm ? TRUE : FALSE;
#else
    sh_vitals.resp_bpm = 0;
    sh_vitals.resp_q8 = 0;
    sh_vitals.resp_valid = FALSE;
#endif
}

/*!
 * Track a slow personal BPM baseline and run a two-sided CUSUM on the
 * deviation, so a small but sustained rate shift is flagged even though the
 * absolute BPM is only a proxy.
 */
static void shUpdateHrChange(void)
{
#if SMART_HELMET_ENABLE_HR_CUSUM
    int32 dev;
    int32 slack = SMART_HELMET_HR_CUSUM_SLACK_BPM;

#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
    if (adapt_e_in)
    {
        uint32 pct = (adapt_e_out >= adapt_e_in) ? 0u :
                     (((adapt_e_in - adapt_e_out) * 100u) / adapt_e_in);
        sh_vitals.adapt_removed_pct = (pct > 100u) ? 100u : (uint8)pct;
    }
#endif

    if (!sh_vitals.hr_valid || !sh_vitals.hr_bpm)
    {
#if SMART_HELMET_HR_BASELINE_STALE_WIN
        /*
         * A long gap means we can no longer assume the same subject at the
         * same rate. Drop the baseline so it is re-learned instead of
         * reporting the re-acquisition as a real change.
         */
        if (hr_invalid_run < 0xffffu)
        {
            hr_invalid_run++;
        }
        if (hr_invalid_run >= SMART_HELMET_HR_BASELINE_STALE_WIN)
        {
            hr_base_bpm = 0;
            hr_base_n = 0;
            hr_cusum_up = 0;
            hr_cusum_dn = 0;
            sh_vitals.hr_change = smart_helmet_trend_unknown;
        }
#endif
        sh_vitals.hr_baseline_bpm = hr_base_bpm;
        sh_vitals.hr_delta_bpm = 0;
        sh_vitals.hr_cusum_up = (uint16)hr_cusum_up;
        sh_vitals.hr_cusum_dn = (uint16)hr_cusum_dn;
        return;
    }

#if SMART_HELMET_HR_BASELINE_STALE_WIN
    hr_invalid_run = 0;
#endif

    if (!hr_base_bpm)
    {
        hr_base_bpm = sh_vitals.hr_bpm;
    }

#if SMART_HELMET_HR_CUSUM_SLACK_PCT
    {
        int32 prop = ((int32)hr_base_bpm * SMART_HELMET_HR_CUSUM_SLACK_PCT)
                     / 100;
        if (prop > slack)
        {
            slack = prop;
        }
    }
#endif

    dev = (int32)sh_vitals.hr_bpm - (int32)hr_base_bpm;

    if (hr_base_n < SMART_HELMET_HR_BASELINE_MIN_WIN)
    {
        /* Learning phase: track fast, do not raise change events yet. */
        hr_base_n++;
        hr_base_bpm = (uint16)((int32)hr_base_bpm +
                               (dev * 64) / 256);
        hr_cusum_up = 0;
        hr_cusum_dn = 0;
    }
    else
    {
        hr_cusum_up += dev - slack;
        if (hr_cusum_up < 0)
        {
            hr_cusum_up = 0;
        }
        hr_cusum_dn += (-dev) - slack;
        if (hr_cusum_dn < 0)
        {
            hr_cusum_dn = 0;
        }
        if (hr_cusum_up > 0xffff)
        {
            hr_cusum_up = 0xffff;
        }
        if (hr_cusum_dn > 0xffff)
        {
            hr_cusum_dn = 0xffff;
        }

        if (hr_cusum_up >= (int32)SMART_HELMET_HR_CUSUM_LIMIT)
        {
            sh_vitals.hr_change = smart_helmet_trend_rising;
            /* Accept the new level as the baseline and re-arm. */
            hr_base_bpm = sh_vitals.hr_bpm;
            hr_cusum_up = 0;
            hr_cusum_dn = 0;
        }
        else if (hr_cusum_dn >= (int32)SMART_HELMET_HR_CUSUM_LIMIT)
        {
            sh_vitals.hr_change = smart_helmet_trend_falling;
            hr_base_bpm = sh_vitals.hr_bpm;
            hr_cusum_up = 0;
            hr_cusum_dn = 0;
        }
        else
        {
            sh_vitals.hr_change = smart_helmet_trend_stable;
            hr_base_bpm = (uint16)((int32)hr_base_bpm +
                (dev * SMART_HELMET_HR_BASELINE_ALPHA_Q8) / 256);
        }
    }

    sh_vitals.hr_baseline_bpm = hr_base_bpm;
    sh_vitals.hr_delta_bpm = (int16)dev;
    sh_vitals.hr_cusum_up = (uint16)hr_cusum_up;
    sh_vitals.hr_cusum_dn = (uint16)hr_cusum_dn;
#else
    sh_vitals.hr_change = smart_helmet_trend_unknown;
#endif
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
    uint16 bpm_ac = 0;
#if SMART_HELMET_ENABLE_HR_AUTOCORR
    uint16 ac_q8 = 0;
#endif
    uint16 motion_thresh = SMART_HELMET_MOTION_RMS_MG;

    shUpdateTimebase();
    shUpdateResp();

#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
    /*
     * With the adaptive canceller running we can tolerate more movement
     * before giving up on the window.
     */
    if (adapt_have_accel && adapt_ref_n >= SMART_HELMET_HR_ADAPT_TAPS)
    {
        motion_thresh = SMART_HELMET_MOTION_RMS_MG_ADAPT;
    }
#endif

    have_accel = (motion_count >= (MOTION_WIN / 2));
    rms = have_accel ? shRmsDevU16(motion_mag, motion_count) : 0;
    sh_vitals.motion_rms_mg = rms;
    if (have_accel && motion_count)
    {
        uint8 i;
        uint16 mn = motion_mag[0];
        uint16 pk = motion_mag[0];
        for (i = 1; i < motion_count; i++)
        {
            if (motion_mag[i] < mn)
            {
                mn = motion_mag[i];
            }
            if (motion_mag[i] > pk)
            {
                pk = motion_mag[i];
            }
        }
        sh_vitals.motion_min_mg = mn;
        sh_vitals.motion_peak_mg = pk;
        shNoteFallWindow(rms, mn, pk);
    }
    else
    {
        sh_vitals.motion_min_mg = 0;
        sh_vitals.motion_peak_mg = 0;
    }
    sh_vitals.fall_pending = sh_fall_pending;
    sh_vitals.pir_events_win = pir_events;
    sh_vitals.sens_dropped = sens_dropped;
    sens_dropped = 0;

    e_sens = shEnergyI16(sens_hp, sens_count);
    sens_abs = shMeanAbsI16(sens_hp, sens_count);
    e_accel = have_accel ? shEnergyI16(band_hp, band_count) : 0;

    active = FALSE;
    if (have_accel && rms >= motion_thresh)
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
        sh_vitals.hr_bpm_ac = 0;
        sh_vitals.hr_ac_q8 = 0;
		sh_vitals.hr_reason = hr_motion;
        calm_windows = 0;
        pir_events = 0;
        shHrDisturb(SMART_HELMET_HR_DISTURB_HOLDOFF_WIN);
		sh_vitals.hr_win_left = hr_win_left;
        prev_sens_mv_valid = 0;
        CC_LOGN("Vitals: ACTIVE ok=0 why=motion rms=%u sa=%u ho=%u sens=%umV (pulse N/A)",
				rms, sens_abs, (unsigned)hr_win_left, sh_vitals.sens_mv);
        shVitalsReport(sh_vitals.band_energy, sh_vitals.baseline_energy,
                       sens_abs, rms, 1u);
        return;
    }

    sh_vitals.motion = smart_helmet_motion_calm;

    if (sens_count > (BAND_WIN / 4))
    {
        energy = e_sens;
        if (have_accel && band_count > (BAND_WIN / 4))
        {
#if SMART_HELMET_ENABLE_HR_MOTION_ADAPT
            /*
             * The SENS residual has already had the accel-correlated part
             * removed, so folding raw accel energy back in would re-create
             * the disturbance the canceller just took out.
             */
            if (!(adapt_have_accel && adapt_ref_n >= SMART_HELMET_HR_ADAPT_TAPS))
#endif
            {
                energy = (uint16)(e_sens + (e_accel / 4));
            }
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
		if (dwhy != hr_ok)
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
		shFuseHr(0, 0, 0, 0, 0, FALSE, hr_warm);
    }
    else
    {
        bool allow_hr_valid;
		smart_helmet_hr_reason_t pre = hr_ok;
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

		allow_hr_valid = (hr_win_left == 0) ? TRUE : FALSE;
		if (hr_win_left > 0)
        {
			pre = hr_holdoff;
			hr_win_left--;
        }

#if (SMART_HELMET_ENABLE_HR_PEAK || SMART_HELMET_ENABLE_HR_FFT || \
     SMART_HELMET_ENABLE_HR_AUTOCORR)
        sig = shHrSignalReason(energy, sens_abs);
#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
        if (sig == hr_ok && !timebase_ok)
        {
            sig = hr_timebase;
        }
#endif
#if (SMART_HELMET_ENABLE_RESP && SMART_HELMET_HR_REQUIRE_RESP)
        if (sig == hr_ok && !sh_vitals.resp_valid)
        {
            sig = hr_no_resp;
        }
#endif
		if (sig != hr_ok)
        {
            /* Prefer concrete signal reason over generic holdoff */
            shFuseHr(0, 0, 0, 0, 0, FALSE, sig);
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
#if SMART_HELMET_ENABLE_HR_AUTOCORR
            bpm_ac = shAutoCorrBpm(sh_ordered, n_ord, &ac_q8);
            sh_vitals.hr_ac_q8 = ac_q8;
#endif
            /* Refine empty detectors */
            if (!bpm_peak && !bpm_fft && !bpm_ac)
            {
				pre = (pre == hr_holdoff) ? pre : hr_none;
            }
			else if (!bpm_peak && bpm_fft && pre == hr_ok)
            {
                /* fuse will mark fft_only */
            }
			else if (bpm_peak && !bpm_fft && pre == hr_ok)
            {
                /* fuse will mark peak_only / no_fft */
            }
            shFuseHr(bpm_peak, bpm_fft, bpm_ac, peak_n, fft_mag, allow_hr_valid, pre);
        }
#else
        UNUSED(n_ord);
        UNUSED(sig);
        UNUSED(pre);
        UNUSED(allow_hr_valid);
        UNUSED(bpm_peak);
        UNUSED(bpm_fft);
        UNUSED(bpm_ac);
        UNUSED(peak_n);
        UNUSED(fft_mag);
		shFuseHr(0, 0, 0, 0, 0, FALSE, hr_disabled);
#endif
    }

    shUpdateHrChange();

    prev_sens_mv = sh_vitals.sens_mv;
    prev_sens_mv_valid = 1;

    sh_vitals.trend = trend;
	sh_vitals.hr_win_left = hr_win_left;
    pir_events = 0;

    /*
     * ok=1 => pulse proxy meaningful (hr_valid).
     * why= smart_helmet_hr_reason_t:
     *  0 OK  1 warm  2 E_low  3 E_high  4 sa_low  5 sa_high
     *  6 dc_spk  7 hold  8 no_pk  9 no_fft  10 disagr
     *  11 pk_only  12 fft_only  13 dBPM  14 none  15 off  16 motion
     */
	CC_LOGN("Vitals: calm ok=%u why=enum:smart_helmet_hr_reason_t:%u e=%u base=%u tr=%u hr=%u pk=%u fft=%u ac=%u pkn=%u ho=%u sa=%u sens=%umV",
            sh_vitals.hr_valid ? 1u : 0u,
            (unsigned)sh_vitals.hr_reason,
            energy, baseline, (unsigned)trend,
            sh_vitals.hr_bpm, sh_vitals.hr_bpm_peak, sh_vitals.hr_bpm_fft,
            sh_vitals.hr_bpm_ac,
            (unsigned)sh_vitals.hr_peak_count,
			(unsigned)hr_win_left,
            sens_abs, sh_vitals.sens_mv);
	UNUSED(shHrLogWhy);
	shHrLogWhy(sh_vitals.hr_reason, energy, sens_abs, hr_win_left, sh_vitals.hr_bpm_peak, sh_vitals.hr_bpm_fft, sh_vitals.hr_bpm);
    shVitalsReport(energy, baseline, sens_abs, rms, 0u);
}

const smart_helmet_vitals_status_t *SmartHelmet_VitalsGetStatus(void)
{
    return &sh_vitals;
}

bool SmartHelmet_VitalsTakeFall(void)
{
    bool hit = sh_fall_pending;

    sh_fall_pending = FALSE;
    sh_vitals.fall_pending = FALSE;
    return hit;
}

#else /* !SMART_HELMET_ENABLE_VITALS_PROXY */

void SmartHelmet_VitalsInit(void) {}
void SmartHelmet_VitalsPushSensInMvAt(uint16 mv, uint32 time_us)
{
    UNUSED(mv); UNUSED(time_us);
}
void SmartHelmet_VitalsNoteSensDropped(void) {}
void SmartHelmet_VitalsSetSink(smart_helmet_vitals_sink_t sink, void *ctx)
{
    UNUSED(sink); UNUSED(ctx);
}
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
bool SmartHelmet_VitalsTakeFall(void)
{
    return FALSE;
}

#endif /* SMART_HELMET_ENABLE_VITALS_PROXY */
