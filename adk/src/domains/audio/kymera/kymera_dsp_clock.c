/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module for configuring the DSP clock/power settings
*/

#include "kymera_dsp_clock.h"
#include "kymera_data.h"
#include "kymera_state.h"
#include "kymera_output_if.h"
#include "kymera_latency_manager.h"
#include "kymera_tones_prompts.h"
#include "kymera_va.h"
#include "kymera_a2dp.h"
#include "kymera_sco_private.h"
#include "latency_config.h"
#include "av_seids.h"
#include "anc_state_manager.h"
#include "fit_test.h"
#include "kymera_anc_common.h"
#include "kymera_anc.h"
#include <audio_power.h>


#define MHZ_TO_HZ (1000000)

#if defined(__QCC307X__) || defined(__QCC517X__)
#define MAX_DSP_CLOCK AUDIO_DSP_TURBO_PLUS_CLOCK
#else
#define MAX_DSP_CLOCK  AUDIO_DSP_TURBO_CLOCK
#endif

static audio_dsp_clock_type appKymeraGetNbWbScoDspClockType(void)
{
#if defined(ENABLE_ADAPTIVE_ANC) || defined(KYMERA_SCO_USE_3MIC)
    return AUDIO_DSP_TURBO_CLOCK;
#else
    return AUDIO_DSP_BASE_CLOCK;
#endif
}

static audio_dsp_clock_type appKymeraGetSwbScoDspClockType(void)
{
    return MAX_DSP_CLOCK;
}

static audio_dsp_clock_type appKymeraGetMinClockForPrompts(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if ((theKymera->output_rate == 96000) || (Kymera_OutputGetKickPeriod() < KICK_PERIOD_TONES))
        return AUDIO_DSP_BASE_CLOCK;

    return AUDIO_DSP_SLOW_CLOCK;
}

static audio_dsp_clock_type kymera_GetDSPClockDuringAptxAdaptive(void)
{
    audio_dsp_clock_type mode; 

#ifdef INCLUDE_DECODERS_ON_P1

#ifdef INCLUDE_STEREO
    /* Not enough MIPs to run aptX adaptive on base clock */ 
    mode = AUDIO_DSP_TURBO_CLOCK; 
#else
    mode = AUDIO_DSP_BASE_CLOCK;
#endif

#else /* !INCLUDE_DECODERS_ON_P1 */

    /* Not enough MIPs to run aptX adaptive (TWS standard and TWS+) on base clock in a single audio core */
    mode = MAX_DSP_CLOCK; 

#endif /* INCLUDE_DECODERS_ON_P1 */

#if !defined(__QCC307X__) && !defined(__QCC517X__)
    mode = AUDIO_DSP_BASE_CLOCK;
#ifdef INCLUDE_STEREO
    /* Not enough MIPS in QCC514x/QCC304x to run aptX AD 96KHz on base clock */
    if(KymeraGetTaskData()->output_rate == 96000)
        mode = AUDIO_DSP_TURBO_CLOCK;
#endif /*INCLUDE_STEREO*/
#endif

    return mode;
}

bool appKymeraBoostDspClockToMax(void)
{
    audio_dsp_clock_configuration cconfig =
    {
        .active_mode = MAX_DSP_CLOCK,
        .low_power_mode =  AUDIO_DSP_CLOCK_NO_CHANGE,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };

    DEBUG_LOG("appKymeraBoostDspClockToMax: enum:audio_dsp_clock_type:%d", MAX_DSP_CLOCK);
    return AudioDspClockConfigure(&cconfig);
}

void appKymeraConfigureDspPowerMode(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    bool tone_playing = appKymeraIsPlayingPrompt();

    DEBUG_LOG("appKymeraConfigureDspPowerMode, tone %u, state %u, a2dp seid %u", tone_playing, appKymeraGetState(), theKymera->a2dp_seid);

    /* Assume we are switching to the low power slow clock unless one of the
     * special cases below applies */
    audio_dsp_clock_configuration cconfig =
    {
        .active_mode = AUDIO_DSP_SLOW_CLOCK,
        .low_power_mode =  AUDIO_DSP_SLOW_CLOCK,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };

    audio_dsp_clock kclocks;
    audio_power_save_mode mode = AUDIO_POWER_SAVE_MODE_3;

    if (Kymera_A2dpIsActive() || (appKymeraGetState() == KYMERA_STATE_STANDALONE_LEAKTHROUGH))
    {
        if(KymeraAnc_IsDspClockUpdateRequired())
        {
            cconfig.active_mode = KymeraAnc_GetOptimalDspClockForMusicConcurrency(theKymera->a2dp_seid);
            mode = AUDIO_POWER_SAVE_MODE_1;
        }
        else if(Kymera_IsVaActive())
        {
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
        }
        else if(tone_playing)
        {
            mode = AUDIO_POWER_SAVE_MODE_1;
            switch(theKymera->a2dp_seid)
            {
                case AV_SEID_APTX_SNK:
                case AV_SEID_APTXHD_SNK:
                    cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                    break;

                case AV_SEID_APTX_ADAPTIVE_SNK:
                case AV_SEID_APTX_ADAPTIVE_TWS_SNK:
                    cconfig.active_mode = kymera_GetDSPClockDuringAptxAdaptive();
                    break;

                default:
                    /* For most codecs there is not enough MIPs when running on a slow clock to also play a tone */
                    cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                    break;
            }
        }
        else
        {
            switch(theKymera->a2dp_seid)
            {
                case AV_SEID_APTX_SNK:
                case AV_SEID_APTXHD_SNK:
                {
                    cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
                break;

                case AV_SEID_APTX_ADAPTIVE_SNK:
                case AV_SEID_APTX_ADAPTIVE_TWS_SNK:
                    cconfig.active_mode = kymera_GetDSPClockDuringAptxAdaptive();
                    mode = AUDIO_POWER_SAVE_MODE_1;
                    break;

                case AV_SEID_SBC_SNK:
#if defined(INCLUDE_MIRRORING) && !defined(INCLUDE_DECODERS_ON_P1)
                {
                    if (Kymera_OutputIsAecAlwaysUsed())
                    {
                        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                }
#endif
                break;

                case AV_SEID_AAC_SNK:
#if !defined(INCLUDE_DECODERS_ON_P1) && defined(INCLUDE_MUSIC_PROCESSING)
                {
                    cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
#endif
                break;

                default:
                break;
            }
        }
        if (Kymera_BoostClockInGamingMode() && Kymera_LatencyManagerIsGamingModeEnabled())
        {
            cconfig.active_mode += 1;
            cconfig.active_mode = MIN(cconfig.active_mode, AUDIO_DSP_TURBO_CLOCK);
        }
    }
    // SCO assumed mutually exclusive to A2DP
    else if (Kymera_ScoIsActive())
    {
        DEBUG_LOG("appKymeraConfigureDspPowerMode, sco_mode enum:appKymeraScoMode:%d", theKymera->sco_info->mode);

        if(KymeraAnc_IsDspClockUpdateRequired())
        {
            cconfig.active_mode = KymeraAnc_GetOptimalDspClockForScoConcurrency(theKymera->sco_info->mode);
            mode = AUDIO_POWER_SAVE_MODE_1;
        }
        else
        {
            switch (theKymera->sco_info->mode)
            {
                case SCO_NB:
                case SCO_WB:
                {
                    cconfig.active_mode = appKymeraGetNbWbScoDspClockType();
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
                break;

                case SCO_SWB:
                case SCO_UWB:
                {
                    cconfig.active_mode = appKymeraGetSwbScoDspClockType();
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
                break;

                default:
                break;
            }
        }
    }
    else
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_ANC_TUNING:
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            break;

            case KYMERA_STATE_MIC_LOOPBACK:
            {
                if(AncStateManager_CheckIfDspClockBoostUpRequired())
                {
                    cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
                else if(Kymera_IsVaActive())
                {
                    cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
                else if ((appKymeraInConcurrency()) || (FitTest_IsRunning()))
                {
                    cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
            }
            break;


            case KYMERA_STATE_WIRED_AUDIO_PLAYING:
            case KYMERA_STATE_IDLE:
            case KYMERA_STATE_ADAPTIVE_ANC_STARTED:
                if(AncStateManager_CheckIfDspClockBoostUpRequired())
                {
                    cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                    mode = AUDIO_POWER_SAVE_MODE_1;
                }
                else if(AncConfig_IsAdvancedAnc())
                {
                    if(appKymeraInConcurrency())
                    {
                        cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    else if(Kymera_IsVaActive())
                    {
                        cconfig.active_mode = Kymera_VaGetMinDspClock();
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    else if(KymeraAncCommon_IsAancActive())
                    {
                        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                }
                else
                {
                    if(Kymera_IsVaActive())
                    {
                        cconfig.active_mode = Kymera_VaGetMinDspClock();
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    else if ((appKymeraInConcurrency()) || (FitTest_IsRunning()))
                    {
                        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                }
                break; 
            case KYMERA_STATE_USB_AUDIO_ACTIVE:
            case KYMERA_STATE_USB_VOICE_ACTIVE:
            case KYMERA_STATE_USB_SCO_VOICE_ACTIVE:
            case KYMERA_STATE_WIRED_A2DP_STREAMING:
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
                break;

            case KYMERA_STATE_USB_LE_AUDIO_ACTIVE:
                cconfig.active_mode = MAX_DSP_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
                break;

            default:
                break;
        }
    }

    if (tone_playing)
    {
        cconfig.active_mode = MAX(cconfig.active_mode, appKymeraGetMinClockForPrompts());
    }

#ifdef AUDIO_IN_SQIF
    /* Make clock faster when running from SQIF */
    cconfig.active_mode += 1;
#endif

    PanicFalse(AudioDspClockConfigure(&cconfig));
    PanicFalse(AudioPowerSaveModeSet(mode));

    PanicFalse(AudioDspGetClock(&kclocks));
    mode = AudioPowerSaveModeGet();
    DEBUG_LOG_INFO("appKymeraConfigureDspPowerMode, kymera clocks %d %d %d, mode %d", kclocks.active_mode, kclocks.low_power_mode, kclocks.trigger_mode, mode);
}

void appKymeraConfigureDspClockSpeed(void)
{
    if (Kymera_IsVaActive())
    {
        PanicFalse(AudioMapCpuSpeed(AUDIO_DSP_SLOW_CLOCK, Kymera_VaGetMinLpClockSpeedMhz() * MHZ_TO_HZ));
    }
    else
    {
        PanicFalse(AudioMapCpuSpeed(AUDIO_DSP_SLOW_CLOCK, DEFAULT_LOW_POWER_CLK_SPEED_MHZ * MHZ_TO_HZ));
    }
}
