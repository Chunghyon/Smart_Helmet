/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_anc_common.c
\brief      Implementation of kymera anc common related functionality, controls AANCv2, Adaptive Ambient,
            Static ANC and Static leakthrough audio graphs for both standalone and concurrency use cases.
*/
#include <app/audio/audio_if.h>
#include <operators.h>
#include "kymera_anc_common.h"
#include "kymera.h"
#include "macros.h"
#include "kymera_aah.h"
#include "kymera_ahm.h"
#include "kymera_hcgr.h"
#include "kymera_data.h"
#include "kymera_dsp_clock.h"
#include "kymera_state.h"
#include "kymera_mic_if.h"
#include "kymera_output_if.h"
#include "kymera_setup.h"
#include "kymera_internal_msg_ids.h"
#include "panic.h"
#include "anc_state_manager.h"
#include "kymera_wind_detect.h"
#include "kymera_echo_canceller.h"
#include "kymera_adaptive_anc.h"
#include "kymera_adaptive_ambient.h"
#include "kymera_output_ultra_quiet_dac.h"
#include "kymera_anc.h"
#include "stream.h"
#include "wind_detect.h"
#include "kymera_self_speech_detect.h"
#include "kymera_va.h"

#ifdef ENABLE_ADAPTIVE_ANC
#define KYMERA_ANC_COMMON_SAMPLE_RATE (16000U)
#define MAX_ANC_COMMON_MICS (2U)
#define TASK_PERIOD_FAST (1000U) /* 1msec */

/*! \brief AANC config type. */
typedef enum
{
    aanc_config_none,
    aanc_config_adaptive_anc,
    aanc_config_adaptive_ambient,
    aanc_config_static_anc,
    aanc_config_static_leakthrough,
    aanc_config_fit_test
} aanc_config_t;

typedef enum
{
    aanc_state_idle,
    aanc_state_enable_initiated,
    aanc_state_enabled
}aanc_state_t;

typedef struct
{
   uint8 lpf_shift_1;
   uint8 lpf_shift_2;
} kymera_anc_common_lpf_config_t;

typedef struct
{
   uint8 filter_shift;
   uint8 filter_enable;
} kymera_anc_common_dc_config_t;

typedef kymera_anc_common_dc_config_t kymera_anc_common_small_lpf_config_t;

typedef struct
{
   kymera_anc_common_lpf_config_t lpf_config;
   kymera_anc_common_dc_config_t dc_filter_config;
} kymera_anc_common_feed_forward_config_t;

typedef struct
{
   kymera_anc_common_lpf_config_t lpf_config;
} kymera_anc_common_feed_back_config_t;

typedef struct
{
   kymera_anc_common_feed_forward_config_t ffa_config;
   kymera_anc_common_feed_forward_config_t ffb_config;
   kymera_anc_common_feed_back_config_t fb_config;
   kymera_anc_common_small_lpf_config_t small_lpf_config;
} kymera_anc_common_anc_instance_config_t;

typedef struct
{
   anc_mode_t mode;
   kymera_anc_common_anc_instance_config_t anc_instance_0_config;
   kymera_anc_common_anc_instance_config_t anc_instance_1_config;
} kymera_anc_common_anc_filter_config_t;

typedef enum
{
    KYMERA_ANC_COMMON_INTERNAL_AHM_TRANSITION_TIMEOUT = INTERNAL_MESSAGE_BASE,
    KYMERA_ANC_COMMON_INTERNAL_AHM_RAMP_TIMEOUT,
} kymera_anc_common_internal_message_ids;

static void kymeraAncCommon_HandleMessage(Task task, MessageId id, Message message);
static const TaskData kymera_anc_common_task = { .handler=kymeraAncCommon_HandleMessage };

#define kymeraAncCommon_GetTask() (Task)&kymera_anc_common_task

typedef struct
{
   aanc_config_t aanc_config;
   KYMERA_INTERNAL_AANC_ENABLE_T mode_settings;
   bool concurrency_req; /* Flag indicating standalone/concurrency anc chain has to be bought up during anc enable user-event*/
   bool transition_in_progress;/* Flag indicating standalone to concurrency or concurrency to standalone chain setup is in progress*/
   bool wind_detect_in_stage2;/* Can be in Stage 2 if there is Stage 1 Attack or in SCO concurrency*/
   bool wind_confirmed;/* Wind is detected and AHM sysmode is set to Wind */
   bool mode_change_in_progress; /*Flag indicating mode change progress to disallow self speech mic disconnection and connection */
   kymera_anc_common_anc_filter_config_t* prev_filter_config;
   Operator spc_op;
   output_users_t connecting_user;/*Used to check if self speech can be enabled or not based on the connecting concurrency user*/
   aanc_state_t aanc_state;
} kymera_anc_common_data_t;

static kymera_anc_common_data_t data;
#define getData() (&data)

typedef void (*kymeraAncCommonCallback)(void);
kymeraAncCommonCallback ahmRampingCompleteCallback = {NULL};

/***************************************
 ********* Static Functions ************
 ***************************************/
static void kymeraAncCommon_RampGainsToMute(kymeraAncCommonCallback func);
static void kymeraAncCommn_RampGainsToQuiet(kymeraAncCommonCallback func);
static void kymeraAncCommon_SetAancCurrentConfig(aanc_config_t aanc_config);
static aanc_config_t kymeraAncCommon_GetAancCurrentConfig(void);
static aanc_config_t kymeraAncCommon_IsAancCurrentConfigValid(void);
static bool kymeraAncCommon_IsCurrentConfigStaticLeakthrough(void);
static void kymeraAncCommon_UpdateConcurrencyRequest(bool enable);
static void kymeraAncCommon_UpdateTransitionInProgress(bool enable);
static bool kymeraAncCommon_isTransitionInProgress(void);
static void kymeraAncCommon_AhmDestroy(void);
static void kymeraAncCommon_AhmCreate(void);
static bool kymeraAncCommon_IsConcurrencyRequested(void);
static void kymeraAncCommon_StoreModeSettings(const KYMERA_INTERNAL_AANC_ENABLE_T *mode_params);
static void kymeraAncCommon_SetKymeraState(void);
static void kymeraAncCommon_ResetKymeraState(void);
static uint8_t kymeraAncCommon_GetRequiredAncMics(void);
static uint16 kymeraAncCommon_GetAncFeedforwardMicConfig(void);
static uint16 kymeraAncCommon_GetAncFeedbackMicConfig(void);
static Sink kymeraAncCommon_GetFeedforwardPathSink(void);
static Sink kymeraAncCommon_GetFeedbackPathSink(void);
static Sink kymeraAncCommon_GetPlaybackPathSink(void);
static bool kymeraAncCommon_IsAncModeFitTest(anc_mode_t mode);
static aanc_config_t kymeraAncCommon_GetAancConfigFromMode(anc_mode_t mode);
static void kymeraAncCommon_FreezeAdaptivity(void);
static void kymeraAncCommon_StartAdaptivity(void);
static void kymeraAncCommon_Create(void);
static void kymeraAncCommon_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T *msg);
static void kymeraAncCommon_WindDetectConnect(void);
static void kymeraAncCommon_Connect(void);
static void kymeraAncCommon_Start(void);
static void kymeraAncCommon_Stop(void);
static void kymeraAncCommon_Destroy(void);
static void kymeraAncCommon_Disconnect(void);
static void kymeraAncCommon_Disable(void);
static void kymeraAncCommon_DisableOnUserRequest(void);
static void kymeraAncCommon_StandaloneToConcurrencyChain(void);
static void kymeraAncCommon_ConcurrencyToStandaloneChain(void);
static void kymeraAncCommon_ApplyModeChangeSettings(KYMERA_INTERNAL_AANC_ENABLE_T *mode_params);
static bool kymeraAncCommon_MicGetConnectionParameters(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymeraAncCommon_MicDisconnectIndication(const mic_change_info_t *info);
static void kymeraAncCommon_MicReconnectedIndication(void);
static void kymeraAncCommon_AdaptiveAncOutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type);
static void kymeraAncCommon_AdaptiveAncOutputIdleIndication(void);
static void KymeraAncCommon_StandaloneToConcurrencyReq(void);
static void KymeraAncCommon_ConcurrencyToStandaloneReq(void);
static uint16 kymeraAncCommon_GetTargetGain(void);
static void kymeraAncCommon_SetAancState(aanc_state_t state);
static aanc_state_t kymeraAncCommon_GetAancState(void);

static void kymeraAncCommon_AncStopOperators(void);
static void kymeraAncCommon_AncMicDisconnect(void);

#if defined(ENABLE_WIND_DETECT)
static void kymeraAncCommon_SetWindConfirmed(void);
static void kymeraAncCommon_ResetWindConfirmed(void);
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
static bool kymeraAncCommon_IsWindConfirmed(void);
#endif
#endif

#ifndef ENABLE_ANC_FAST_MODE_SWITCH
static void kymeraAncCommon_RampGainsToStatic(kymeraAncCommonCallback func);
static void kymeraAncCommon_ModeChangeActionPostGainRamp(void);
#endif

#ifdef ENABLE_ANC_FAST_MODE_SWITCH
static void kymeraAncCommon_ResetFilterConfig(kymera_anc_common_anc_filter_config_t* filter_config);
static kymera_anc_common_anc_filter_config_t* kymeraAncCommon_GetPrevFilterConfig(void);
static void kymeraAncCommon_SetPrevFilterConfig(kymera_anc_common_anc_filter_config_t* filter_config);

static void KymeraAncCommon_ReadLpfCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8* lpf_shift_1, uint8* lpf_shift_2);
static void KymeraAncCommon_ReadDcFilterCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8* filter_shift, uint8* filter_enable);
static void KymeraAncCommon_ReadSmallLpfCoefficients(audio_anc_instance instance, uint8* filter_shift, uint8* filter_enable);

static kymera_anc_common_anc_instance_config_t* KymeraAncCommon_GetInstanceConfig(kymera_anc_common_anc_filter_config_t* filter_config, audio_anc_instance instance);

static void kymeraAncCommon_EnableRxMix(void);
#endif

#if defined(ENABLE_SELF_SPEECH) && defined (ENABLE_AUTO_AMBIENT)
static bool kymeraAncCommon_IsModeChangeInProgress(void);
static void kymeraAncCommon_SelfSpeechDetect_MicReConnect(void);
static void kymeraAncCommon_SelfSpeechDetect_MicDisconnect(void);
#elif defined ENABLE_ANC_FAST_MODE_SWITCH
static bool kymeraAncCommon_IsModeChangeInProgress(void);
#endif

#ifdef ENABLE_WIND_DETECT
static bool kymeraAncCommon_IsWindDetectInStage2(void);
static void kymeraAncCommon_SetWindDetectInStage2(void);
static void kymeraAncCommon_ResetWindDetectInStage2(void);
static void kymeraAncCommon_RampGainsToTarget(kymeraAncCommonCallback func);
static void kymeraAncCommon_WindConfirmed(void);
static void kymeraAncCommon_StartOnWindRelease(void);
static void kymeraAncCommon_MoveToStage1(void);
static void kymeraAncCommon_MoveToStage2(void);
#endif

static void kymeraAncCommon_RampGainsToMute(kymeraAncCommonCallback func)
{
    DEBUG_LOG("kymeraAncCommon_RampGainsToMute");
    ahmRampingCompleteCallback = func;
    KymeraAhm_SetSysMode(ahm_sysmode_mute_anc);
}

#ifndef ENABLE_ANC_FAST_MODE_SWITCH
static void kymeraAncCommon_RampGainsToStatic(kymeraAncCommonCallback func)
{
    ahmRampingCompleteCallback = func;
    KymeraAhm_SetSysMode(ahm_sysmode_full);
}
#endif

static void kymeraAncCommn_RampGainsToQuiet(kymeraAncCommonCallback func)
{
    DEBUG_LOG("kymeraAncCommn_RampGainsToQuiet");
    ahmRampingCompleteCallback = func;
}

static void kymeraAncCommon_SetAancCurrentConfig(aanc_config_t aanc_config)
{
    data.aanc_config = aanc_config;
}

static aanc_config_t kymeraAncCommon_GetAancCurrentConfig(void)
{
    return (data.aanc_config);
}

static aanc_config_t kymeraAncCommon_IsAancCurrentConfigValid(void)
{
    return (data.aanc_config != aanc_config_none);
}

static bool kymeraAncCommon_IsCurrentConfigStaticLeakthrough(void)
{
    return (data.aanc_config == aanc_config_static_leakthrough);
}

static void kymeraAncCommon_UpdateConcurrencyRequest(bool enable)
{
    data.concurrency_req = enable;
}

static void kymeraAncCommon_UpdateTransitionInProgress(bool enable)
{
   data.transition_in_progress=enable;
}

static bool kymeraAncCommon_isTransitionInProgress(void)
{
    return data.transition_in_progress;
}

static void kymeraAncCommon_SetModeChangeInProgress(bool enable)
{
   data.mode_change_in_progress=enable;
}

static void kymeraAncCommon_AhmDestroy(void)
{
    if(!kymeraAncCommon_isTransitionInProgress() && KymeraAhm_IsActive())
    {
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
        if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
        {
            KymeraAhm_Destroy();
        }
    }
}

static void kymeraAncCommon_HcgrDestroy(void)
{
    if(!kymeraAncCommon_isTransitionInProgress() && KymeraHcgr_IsActive())
    {
        KymeraHcgr_Destroy();
    }
}

static void kymeraAncCommon_WindDetectDestroy(void)
{
    if(!kymeraAncCommon_isTransitionInProgress() && KymeraWindDetect_IsActive())
    {
        KymeraWindDetect_Destroy();
    }
}
static void kymeraAncCommon_AhmCreate(void)
{
    if(!kymeraAncCommon_isTransitionInProgress() && !KymeraAhm_IsActive())
    {
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
        if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
        {
            KymeraAhm_Create();
        }
    }
}

static void kymeraAncCommon_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG_INFO("kymeraAncCommon_HandleMessage, id: enum:kymera_anc_common_internal_message_ids:%d", id);

    switch(id)
    {
    case KYMERA_ANC_COMMON_INTERNAL_AHM_TRANSITION_TIMEOUT:
        DEBUG_LOG_INFO("kymeraAncCommon_HandleMessage, Transition timer expiry");
        KymeraAncCommon_TransitionCompleteAction();
        break;

    case KYMERA_ANC_COMMON_INTERNAL_AHM_RAMP_TIMEOUT:
         DEBUG_LOG_INFO("kymeraAncCommon_HandleMessage, KYMERA_ANC_COMMON_INTERNAL_AHM_RAMP_TIMEOUT");
         KymeraAncCommon_AhmRampExpiryAction();
    break;
    }
}

static void kymeraAncCommon_HcgrCreate(void)
{
    if(!kymeraAncCommon_isTransitionInProgress() &&!KymeraHcgr_IsActive())
    {
        KymeraHcgr_Create();
    }
}

static void kymeraAncCommon_WindDetectCreate(void)
{
    if(!kymeraAncCommon_isTransitionInProgress() && !KymeraWindDetect_IsActive())
    {
        KymeraWindDetect_Create();
    }
}

static bool kymeraAncCommon_IsConcurrencyRequested(void)
{
    return data.concurrency_req;
}

/*Mode settings are used during concurrency/standalone transitions*/
static void kymeraAncCommon_StoreModeSettings(const KYMERA_INTERNAL_AANC_ENABLE_T *mode_params)
{
    if(mode_params)
    {
        getData()->mode_settings = *mode_params;
    }
}

static void kymeraAncCommon_SetKymeraState(void)
{
    if(!appKymeraIsBusy())
        appKymeraSetState(KYMERA_STATE_ADAPTIVE_ANC_STARTED);
}

static void kymeraAncCommon_ResetKymeraState(void)
{
    if((appKymeraGetState() == KYMERA_STATE_ADAPTIVE_ANC_STARTED))
        appKymeraSetState(KYMERA_STATE_IDLE);
}

static uint8_t kymeraAncCommon_GetRequiredAncMics(void)
{
    uint8_t num_mics;
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            num_mics = 2U;
        break;

        case aanc_config_static_anc:
#if defined(ENABLE_WIND_DETECT) || defined(ENABLE_ANC_AAH)
            num_mics = 2U;
#else
            num_mics = 1U;
#endif
        break;

        case aanc_config_adaptive_ambient:
#if defined(ENABLE_ANC_AAH)
            num_mics = 2U;
#else
            num_mics = 1U;
#endif
        break;

        case aanc_config_static_leakthrough:
#if defined(ENABLE_ANC_AAH)
            num_mics = 2U;
#elif defined(ENABLE_WIND_DETECT)
            num_mics = 1U;
#else
            num_mics = 0U;
#endif
        break;

        case aanc_config_fit_test:
            num_mics = 2U;
        break;

        default:
            num_mics = 2U;
            break;
    }

    return num_mics;
}

static uint16 kymeraAncCommon_GetAncFeedforwardMicConfig(void)
{
    return appConfigAncFeedForwardMic();
}

static uint16 kymeraAncCommon_GetAncFeedbackMicConfig(void)
{
    uint16 mic = MICROPHONE_NONE;
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            mic = appConfigAncFeedBackMic();
        break;

       case aanc_config_static_anc:
        mic = appConfigAncFeedBackMic();
       break;

       case aanc_config_adaptive_ambient:
       case aanc_config_static_leakthrough:
#if defined(ENABLE_ANC_AAH)
            mic = appConfigAncFeedBackMic();
#else
            mic = MICROPHONE_NONE;
#endif
            break;

        case aanc_config_fit_test:
            mic = appConfigAncFeedBackMic();
        break;

        default:
            mic = MICROPHONE_NONE;
            break;
    }
    return mic;
}

/*! \brief Get the Adaptive ANC Feed Forward mic Sink
*/
static Sink kymeraAncCommon_GetAancFFMicPathSink(void)
{
    if(KymeraEchoCanceller_IsActive() && ancConfigIncludeFFPathFbcInConcurrency())
    {
        return KymeraEchoCanceller_GetFFMicPathSink();
    }
    else
    {
        return ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_FF_MIC_IN);
    }
}

static Sink kymeraAncCommon_GetFeedforwardPathSink(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
#if defined(ENABLE_ANC_AAH)
        return KymeraAah_GetFFMicPathSink();
#else
        return kymeraAncCommon_GetAancFFMicPathSink();
#endif

        case aanc_config_static_anc:
#if defined(ENABLE_ANC_AAH)
        return KymeraAah_GetFFMicPathSink();
#else
#ifdef ENABLE_WIND_DETECT
    return KymeraWindDetect_GetFFMicPathSink();
#else
    return NULL;
#endif
#endif
        case aanc_config_adaptive_ambient:
#if defined(ENABLE_ANC_AAH)
        return KymeraAah_GetFFMicPathSink();
#else
        return KymeraAdaptiveAmbient_GetFFMicPathSink();
#endif

        case aanc_config_static_leakthrough:
#if defined(ENABLE_ANC_AAH)
            return KymeraAah_GetFFMicPathSink();

#elif defined(ENABLE_WIND_DETECT)
            return KymeraWindDetect_GetFFMicPathSink();
#else
            return NULL;
#endif

        case aanc_config_fit_test:
            return kymeraAncCommon_GetAancFFMicPathSink();

        default:
            return NULL;
    }
}

static Sink kymeraAncCommon_GetFeedbackPathSink(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
#if defined(ENABLE_ANC_AAH)
        return KymeraAah_GetFBMicPathSink();
#else
        if(KymeraEchoCanceller_IsActive())
        {
            return KymeraEchoCanceller_GetFBMicPathSink();
        }
        else
        {
            return ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_ERR_MIC_IN);
        }
#endif

        case aanc_config_static_anc:
#if defined(ENABLE_ANC_AAH)
        return KymeraAah_GetFBMicPathSink();
#else
        if(KymeraEchoCanceller_IsActive())
        {
            return KymeraEchoCanceller_GetFBMicPathSink();
        }
        else
        {
            return KymeraHcgr_GetFbMicPathSink();
        }

#endif

        case aanc_config_adaptive_ambient:
        case aanc_config_static_leakthrough:
#if defined(ENABLE_ANC_AAH)
        return KymeraAah_GetFBMicPathSink();
#else
        return NULL;
#endif

        case aanc_config_fit_test:
            return ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_ERR_MIC_IN);

        default:
            return NULL;
    }
}

static Sink kymeraAncCommon_GetPlaybackPathSink(void)
{
    if(KymeraEchoCanceller_IsActive())
    {
#if defined(ENABLE_ANC_AAH)
        return KymeraAah_GetRefPathSink();
#else
        return KymeraEchoCanceller_GetSpkRefPathSink();
#endif
    }
    return NULL;
}

static bool kymeraAncCommon_IsAncModeFitTest(anc_mode_t mode)
{
    return (mode == anc_mode_fit_test);
}

static aanc_config_t kymeraAncCommon_GetAancConfigFromMode(anc_mode_t mode)
{
    if(kymeraAncCommon_IsAncModeFitTest(mode))
    {
        return aanc_config_fit_test;
    }
    else if(AncConfig_IsAncModeAdaptive(mode))
    {
        if(!AncConfig_IsAncModeLeakThrough(mode))
            return aanc_config_adaptive_anc;
        else
            return aanc_config_adaptive_ambient;
    }
    else
    {
        if(!AncConfig_IsAncModeLeakThrough(mode))
            return aanc_config_static_anc;
        else
            return aanc_config_static_leakthrough;
    }
}

static void kymeraAncCommon_AdaptiveAncFreezeAdaptivity(void)
{
    if (KymeraAdaptiveAnc_IsActive())
    {
        KymeraAdaptiveAnc_SetSysMode(adaptive_anc_sysmode_freeze);
    }
}

static void kymeraAncCommon_AdaptiveAncStartAdaptivity(void)
{
    adaptive_ancv2_sysmode_t sys_mode = adaptive_anc_sysmode_full;

    if (kymeraAncCommon_IsConcurrencyRequested())
    {
        sys_mode = adaptive_anc_sysmode_concurrency;
    }

    if (KymeraAdaptiveAnc_IsActive())
    {
        KymeraAdaptiveAnc_SetSysMode(sys_mode);
    }
}

static void kymeraAncCommon_FreezeAdaptivity(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            kymeraAncCommon_AdaptiveAncFreezeAdaptivity();
            KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_standby);
            KymeraAah_SetSysMode(aah_sysmode_standby);
        break;

        case aanc_config_adaptive_ambient:
            KymeraAdaptiveAmbient_SetAncCompanderSysMode(anc_compander_sysmode_passthrough);
            KymeraAdaptiveAmbient_SetHowlingControlSysMode(hc_sysmode_standby);
            KymeraAah_SetSysMode(aah_sysmode_standby);
        break;

        case aanc_config_static_anc:
            KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_standby);
            KymeraAah_SetSysMode(aah_sysmode_standby);
        break;

        case aanc_config_static_leakthrough:
           KymeraAah_SetSysMode(aah_sysmode_standby);
        break;

        default:
        /* do nothing */
            break;
    }
}

static void kymeraAncCommon_StartAdaptivity(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            kymeraAncCommon_AdaptiveAncStartAdaptivity();
            KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_full);
            KymeraAah_SetSysMode(aah_sysmode_full);
        break;

        case aanc_config_adaptive_ambient:
            KymeraAdaptiveAmbient_SetAncCompanderSysMode(anc_compander_sysmode_full);
            KymeraAdaptiveAmbient_SetHowlingControlSysMode(hc_sysmode_full);
            KymeraAah_SetSysMode(aah_sysmode_full);
        break;

        case aanc_config_static_anc:
            KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_full);
            KymeraAah_SetSysMode(aah_sysmode_full);
        break;

        case aanc_config_static_leakthrough:
           KymeraAah_SetSysMode(aah_sysmode_full);
        break;

        default:
        /* do nothing */
            break;
    }
}

#ifdef ENABLE_ANC_FAST_MODE_SWITCH

static void kymeraAncCommon_AhmConfigure(const KYMERA_INTERNAL_AANC_ENABLE_T *msg)
{
    if (!kymeraAncCommon_IsModeChangeInProgress())
    {
        KymeraAhm_Configure(msg);

        /* Cancel any pending AHM Transition timeout messages */
        MessageCancelAll(kymeraAncCommon_GetTask(), KYMERA_ANC_COMMON_INTERNAL_AHM_TRANSITION_TIMEOUT);

        /*Inform Kymera module to put AHM in full proc after timeout */
        MessageSendLater(kymeraAncCommon_GetTask(), KYMERA_ANC_COMMON_INTERNAL_AHM_TRANSITION_TIMEOUT, NULL, AHM_TRANSITION_TIMEOUT_MS);
    }
    else
    {
        KymeraAhm_ApplyModeChange(msg);
    }
}

#else

static void kymeraAncCommon_AhmConfigure(const KYMERA_INTERNAL_AANC_ENABLE_T *msg)
{
    KymeraAhm_Configure(msg);
}

#endif

static void kymeraAncCommon_AhmConnect(void)
{
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
    if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
    {
        KymeraAhm_Connect();
    }
}

static void kymeraAncCommon_AhmStart(void)
{
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
    if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
    {
        KymeraAhm_Start();
    }
}

static void kymeraAncCommon_AhmStop(void)
{
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
    if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
    {
        KymeraAhm_Stop();
    }
}

#if defined(ENABLE_ANC_AAH) && !defined(ENABLE_WIND_DETECT)
/* When wind not enabled in the build then AAH has to be connected to SPC in fb path */
static void kymeraAncCommon_EnableSpc(void)
{
    data.spc_op = CustomOperatorCreate(capability_id_switched_passthrough_consumer, OPERATOR_PROCESSOR_ID_0,
                                  operator_priority_lowest, NULL);
    if(data.spc_op)
    {
        OperatorsSetSwitchedPassthruEncoding(data.spc_op, spc_op_format_pcm);
    }
    /* AAH FF -> SPC */
    PanicNull(StreamConnect(KymeraAah_GetFFMicPathSource(), StreamSinkFromOperatorTerminal(data.spc_op, 0)));
}

static void kymeraAncCommon_DisableSpc(void)
{
    if(data.spc_op)
    {
        OperatorStop(data.spc_op);
        StreamDisconnect(KymeraAah_GetFFMicPathSource(), NULL);
        CustomOperatorDestroy(&data.spc_op, 1);
    }
}
#endif

static void kymeraAncCommon_Create(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            KymeraAah_Create();
            kymeraAncCommon_HcgrCreate();
            kymeraAncCommon_AhmCreate();
            kymeraAncCommon_WindDetectCreate();
            KymeraAdaptiveAnc_CreateChain();
            if(kymeraAncCommon_IsConcurrencyRequested())
            {
                KymeraEchoCanceller_Create(ancConfigIncludeFFPathFbcInConcurrency());
            }
        break;

        case aanc_config_adaptive_ambient:
            KymeraAah_Create();
            kymeraAncCommon_AhmCreate();
            kymeraAncCommon_WindDetectCreate();
            KymeraAdaptiveAmbient_CreateChain();
        break;

        case aanc_config_static_anc:
            KymeraAah_Create();
            kymeraAncCommon_HcgrCreate();
            kymeraAncCommon_AhmCreate();
            kymeraAncCommon_WindDetectCreate();
            if(kymeraAncCommon_IsConcurrencyRequested())
            {
                KymeraEchoCanceller_Create(fb_path_only);
            }
        break;

        case aanc_config_static_leakthrough:
            KymeraAah_Create();
            kymeraAncCommon_AhmCreate();
            kymeraAncCommon_WindDetectCreate();
        break;

        case aanc_config_fit_test:
            KymeraAdaptiveAnc_CreateChain();
        break;

        default:
            break;
    }

    DEBUG_LOG("kymeraAncCommon_Create, eumn:aanc_config_t:%d chain created", kymeraAncCommon_GetAancCurrentConfig());
}

static void kymeraAncCommon_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T *msg)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            KymeraAah_Configure(msg);
            if(!kymeraAncCommon_isTransitionInProgress())
            {
                kymeraAncCommon_AhmConfigure(msg);
                KymeraHcgr_Configure(msg);
                KymeraWindDetect_Configure(msg);
            }
            KymeraAdaptiveAnc_ConfigureChain(msg);
            KymeraEchoCanceller_Configure();
        break;

        case aanc_config_adaptive_ambient:
            KymeraAah_Configure(msg);
            kymeraAncCommon_AhmConfigure(msg);
            KymeraWindDetect_Configure(msg);            
            KymeraAdaptiveAmbient_ConfigureChain(msg);
        break;

        case aanc_config_static_anc:
            KymeraAah_Configure(msg);
            if(!kymeraAncCommon_isTransitionInProgress())
            {
                kymeraAncCommon_AhmConfigure(msg);
                KymeraHcgr_Configure(msg);
                KymeraWindDetect_Configure(msg);
            }
            KymeraEchoCanceller_Configure();
        break;

        case aanc_config_static_leakthrough:
            KymeraAah_Configure(msg);
            kymeraAncCommon_AhmConfigure(msg);
#ifdef ENABLE_WIND_DETECT
            KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain());
#endif
            KymeraWindDetect_Configure(msg);
        break;

        case aanc_config_fit_test:
            KymeraAdaptiveAnc_ConfigureChain(msg);
        break;

        default:
            break;
    }
    DEBUG_LOG("kymeraAncCommon_Configure, eumn:aanc_config_t:%d chain configured", kymeraAncCommon_GetAancCurrentConfig());
}

static void kymeraAncCommon_EchoCancellerConnect(void)
{

    if(KymeraEchoCanceller_IsActive())
    {
#ifdef ENABLE_ANC_AAH
        PanicNull(StreamConnect(KymeraAah_GetRefPathSource(),KymeraEchoCanceller_GetSpkRefPathSink()));
#endif
        KymeraEchoCanceller_Connect();
    }
}

static void kymeraAncCommon_HcgrConnect(void)
{
    DEBUG_LOG("kymeraAncCommon_HcgrConnect");
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
        PanicNull(StreamConnect(ChainGetOutput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_ERR_MIC_OUT),
                        KymeraHcgr_GetFbMicPathSink()));
        break;

        case aanc_config_static_anc:
        if((KymeraEchoCanceller_IsActive()))
        {
#if defined(ENABLE_ANC_AAH)
            /* Connect AAH FB output to HCGR input through FBC */
            PanicNull(StreamConnect(KymeraAah_GetFBMicPathSource(),
                                    KymeraEchoCanceller_GetFBMicPathSink()));
#endif
            PanicNull(StreamConnect(KymeraEchoCanceller_GetFBMicPathSource(),
                        KymeraHcgr_GetFbMicPathSink()));
        }
        else
        {
#if defined(ENABLE_ANC_AAH)
            /* Connect AAH FB output to HCGR input directly*/
            PanicNull(StreamConnect(KymeraAah_GetFBMicPathSource(),
                                    KymeraHcgr_GetFbMicPathSink()));
#endif
        }
        break;

        default:
            break;
    }
    KymeraHcgr_Connect();
}

static void kymeraAncCommon_WindDetectConnect(void)
{
    DEBUG_LOG("kymeraAncCommon_WindDetectConnect");

#if defined(ENABLE_WIND_DETECT)
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            PanicNull(StreamConnect(ChainGetOutput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_FF_MIC_OUT),
                        ChainGetInput(KymeraWindDetect_GetChain(), EPR_WIND_DETECT_FF_MIC_IN)));
            KymeraWindDetect_Connect();
        break;

        case aanc_config_adaptive_ambient:
            PanicNull(StreamConnect(ChainGetOutput(KymeraAdaptiveAmbient_GetChain(),EPR_ADV_ANC_COMPANDER_OUT),
                                ChainGetInput(KymeraWindDetect_GetChain(), EPR_WIND_DETECT_FF_MIC_IN)));
            KymeraWindDetect_Connect();
        break;

        case aanc_config_static_anc:
#if defined(ENABLE_ANC_AAH)
            /* AAH FF -> WND */
            PanicNull(StreamConnect(KymeraAah_GetFFMicPathSource(),
                                    ChainGetInput(KymeraWindDetect_GetChain(), EPR_WIND_DETECT_FF_MIC_IN)));
#endif
            KymeraWindDetect_Connect();
        break;

        case aanc_config_static_leakthrough:
#if defined(ENABLE_ANC_AAH)
        /* AAH FF -> WND */
        PanicNull(StreamConnect(KymeraAah_GetFFMicPathSource(),
                                ChainGetInput(KymeraWindDetect_GetChain(), EPR_WIND_DETECT_FF_MIC_IN)));
#endif
            KymeraWindDetect_Connect();
        break;

        default:
            break;
    }
#endif
}

static void kymeraAncCommon_AancConnect(fbc_config_t fbc_config)
{
    if(KymeraEchoCanceller_IsActive())
    {
        switch(fbc_config)
        {
        case fb_path_only:
#if defined(ENABLE_ANC_AAH)
            /* AAH (FF) -> AANCv2 */
            PanicNull(StreamConnect(KymeraAah_GetFFMicPathSource(),
                                    ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_FF_MIC_IN)));
            /* AAH (FB) -> FBC -> AANCv2 */
            PanicNull(StreamConnect(KymeraAah_GetFBMicPathSource(),
                                    KymeraEchoCanceller_GetFBMicPathSink()));
#endif
            PanicNull(StreamConnect(KymeraEchoCanceller_GetFBMicPathSource(),
                                    ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_ERR_MIC_IN)));
            break;
        case ff_fb_paths:
#if defined(ENABLE_ANC_AAH)
            /* AAH (FF) -> FBC -> AANCv2 */
            PanicNull(StreamConnect(KymeraAah_GetFFMicPathSource(),
                                    KymeraEchoCanceller_GetFFMicPathSink()));
#endif
            PanicNull(StreamConnect(KymeraEchoCanceller_GetFFMicPathSource(),
                                    ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_FF_MIC_IN)));

#if defined(ENABLE_ANC_AAH)
            /* AAH (FB) -> FBC -> AANCv2 */
            PanicNull(StreamConnect(KymeraAah_GetFBMicPathSource(),
                                    KymeraEchoCanceller_GetFBMicPathSink()));
#endif
            PanicNull(StreamConnect(KymeraEchoCanceller_GetFBMicPathSource(),
                                    ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_ERR_MIC_IN)));
            break;
        default:
            break;
        }
    }
#if defined(ENABLE_ANC_AAH)
    else
    {
        /* AAH (FF) -> AANCv2 */
        PanicNull(StreamConnect(KymeraAah_GetFFMicPathSource(),
                                ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_FF_MIC_IN)));
        /* AAH (FB) -> AANCv2 */
        PanicNull(StreamConnect(KymeraAah_GetFBMicPathSource(),
                                ChainGetInput(KymeraAdaptiveAnc_GetChain(), EPR_AANCV2_ERR_MIC_IN)));
    }
    KymeraAah_Connect();
#endif
    KymeraAdaptiveAnc_ConnectChain();
}

static void kymeraAncCommon_AdaptiveAmbientConnect(void)
{
#if defined(ENABLE_ANC_AAH)
    PanicNull(StreamConnect(KymeraAah_GetFFMicPathSource(),
                            KymeraAdaptiveAmbient_GetFFMicPathSink()));
#endif
    KymeraAdaptiveAmbient_ConnectChain();
}

static void kymeraAncCommon_Connect(void)
{
    DEBUG_LOG("kymeraAncCommon_Connect");

    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
        /* Standalone graph:
         * Mic Framework (FF Mic)  -> AAH -> AANCV2 -> WIND
         * Mic Framework (FB Mic)  -> AAH -> AANCv2 -> HCGR
         * Concurrency graph:
         * Mic Framework (FF Mic)  -> AAH -> FBC1 -> AANCv2 -> WIND
         * Mic Framework (FB Mic)  -> AAH -> FBC2 -> AANCv2 -> HCGR
         */
          kymeraAncCommon_HcgrConnect();
          kymeraAncCommon_WindDetectConnect();
          kymeraAncCommon_AancConnect(ancConfigIncludeFFPathFbcInConcurrency());
          kymeraAncCommon_EchoCancellerConnect();
          kymeraAncCommon_AhmConnect();
          KymeraSelfSpeechDetect_Connect();
        break;

        case aanc_config_adaptive_ambient:
            kymeraAncCommon_AhmConnect();
            kymeraAncCommon_WindDetectConnect();
            kymeraAncCommon_AdaptiveAmbientConnect();
            KymeraSelfSpeechDetect_Connect();
        break;

        case aanc_config_static_anc:
            kymeraAncCommon_HcgrConnect();
            kymeraAncCommon_WindDetectConnect();
            kymeraAncCommon_EchoCancellerConnect();
            kymeraAncCommon_AhmConnect();
            KymeraSelfSpeechDetect_Connect();
        break;

        case aanc_config_static_leakthrough:
            kymeraAncCommon_AhmConnect();
            kymeraAncCommon_WindDetectConnect();
            KymeraSelfSpeechDetect_Connect();
        break;

        case aanc_config_fit_test:
            /* Do nothing */
        break;

        default:            
            KymeraSelfSpeechDetect_Connect();
        break;
    }
}

static void kymeraAncCommon_Start(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
#ifdef ENABLE_WIND_DETECT
            if (!kymeraAncCommon_IsWindDetectInStage2())
#endif
            {
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
                kymeraAncCommon_StartAdaptivity();
#else
                kymeraAncCommon_RampGainsToStatic(kymeraAncCommon_StartAdaptivity);
#endif
            }
            kymeraAncCommon_AhmStart();
            KymeraHcgr_Start();
            KymeraWindDetect_Start();
            KymeraAdaptiveAnc_StartChain();
            KymeraEchoCanceller_Start();
            KymeraAah_Start();
            KymeraSelfSpeechDetect_Start();
        break;

        case aanc_config_adaptive_ambient:
#ifdef ENABLE_WIND_DETECT
            if (!kymeraAncCommon_IsWindDetectInStage2())
#endif
            {
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
                kymeraAncCommon_StartAdaptivity();
#else
                kymeraAncCommon_RampGainsToStatic(kymeraAncCommon_StartAdaptivity);
#endif
            }
            kymeraAncCommon_AhmStart();
            KymeraWindDetect_Start();
            KymeraAdaptiveAmbient_StartChain();
            KymeraAah_Start();
            KymeraSelfSpeechDetect_Start();
        break;

        case aanc_config_static_anc:
#if defined(ENABLE_ANC_AAH) && !defined(ENABLE_WIND_DETECT)
            /* When wind flag not enabled in the build then AAH has to be connected to SPC in fb path */
            kymeraAncCommon_EnableSpc();
#endif

#ifdef ENABLE_WIND_DETECT
            if (!kymeraAncCommon_IsWindDetectInStage2())
#endif
            {
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
                kymeraAncCommon_StartAdaptivity();
#else
                kymeraAncCommon_RampGainsToStatic(kymeraAncCommon_StartAdaptivity);
#endif
            }
            kymeraAncCommon_AhmStart();
            KymeraHcgr_Start();
            KymeraWindDetect_Start();
            KymeraEchoCanceller_Start();
            KymeraAah_Start();
            KymeraSelfSpeechDetect_Start();
        break;

        case aanc_config_static_leakthrough:
#ifdef ENABLE_WIND_DETECT
            if (!kymeraAncCommon_IsWindDetectInStage2())
#endif
            {
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
                kymeraAncCommon_RampGainsToStatic(NULL);
#endif
            }
#if defined(ENABLE_ANC_AAH) && !defined(ENABLE_WIND_DETECT)
            /* When wind flag not enabled in the build then AAH has to be connected to SPC in ff path */
            kymeraAncCommon_EnableSpc();
#endif
            kymeraAncCommon_AhmStart();
            KymeraWindDetect_Start();
            KymeraAah_Start();
            KymeraSelfSpeechDetect_Start();
        break;

        case aanc_config_fit_test:
            KymeraAdaptiveAnc_StartChain();
        break;

        default:            
            /*For concurrency use cases when VAD only is enabled*/
            KymeraSelfSpeechDetect_Start();
            break;
    }
    DEBUG_LOG("kymeraAncCommon_Start, eumn:aanc_config_t:%d chain active now", kymeraAncCommon_GetAancCurrentConfig());
}

static void kymeraAncCommon_Stop(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            KymeraAah_Stop();
            KymeraEchoCanceller_Stop();
            KymeraAdaptiveAnc_StopChain();
            kymeraAncCommon_AhmStop();
            KymeraWindDetect_Stop();
            KymeraHcgr_Stop();
            KymeraSelfSpeechDetect_Stop();
        break;

        case aanc_config_adaptive_ambient:
            KymeraAah_Stop();
            KymeraAdaptiveAmbient_StopChain();
            kymeraAncCommon_AhmStop();
            KymeraWindDetect_Stop();
            KymeraSelfSpeechDetect_Stop();
        break;

        case aanc_config_static_anc:
            KymeraAah_Stop();
            KymeraEchoCanceller_Stop();
            kymeraAncCommon_AhmStop();
            KymeraWindDetect_Stop();
            KymeraHcgr_Stop();
            KymeraSelfSpeechDetect_Stop();
#if defined(ENABLE_ANC_AAH) && !defined(ENABLE_WIND_DETECT)
            kymeraAncCommon_DisableSpc();
#endif

        break;

        case aanc_config_static_leakthrough:
            KymeraAah_Stop();
            kymeraAncCommon_AhmStop();
            KymeraWindDetect_Stop();
            KymeraSelfSpeechDetect_Stop();
#if defined(ENABLE_ANC_AAH) && !defined(ENABLE_WIND_DETECT)
            kymeraAncCommon_DisableSpc();
#endif
        break;

        case aanc_config_fit_test:
            KymeraAdaptiveAnc_StopChain();
        break;

        default:
            /*For concurrency use cases when VAD only is enabled*/
            KymeraSelfSpeechDetect_Stop();
            break;
    }

    DEBUG_LOG("kymeraAncCommon_Stop, eumn:aanc_config_t:%d chain stopped", kymeraAncCommon_GetAancCurrentConfig());
}

static void kymeraAncCommon_Destroy(void)
{
    DEBUG_LOG("kymeraAncCommon_Destroy");

    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            KymeraAah_Destroy();
            KymeraEchoCanceller_Destroy();
            KymeraAdaptiveAnc_DestroyChain();
            kymeraAncCommon_AhmDestroy();
            kymeraAncCommon_WindDetectDestroy();
            kymeraAncCommon_HcgrDestroy();
        break;

        case aanc_config_adaptive_ambient:
            KymeraAah_Destroy();
            KymeraAdaptiveAmbient_DestroyChain();
            kymeraAncCommon_AhmDestroy();
            kymeraAncCommon_WindDetectDestroy();
        break;

        case aanc_config_static_anc:
            KymeraAah_Destroy();
            KymeraEchoCanceller_Destroy();
            kymeraAncCommon_AhmDestroy();
            kymeraAncCommon_WindDetectDestroy();
            kymeraAncCommon_HcgrDestroy();
        break;

        case aanc_config_static_leakthrough:
            KymeraAah_Destroy();
            kymeraAncCommon_AhmDestroy();
            kymeraAncCommon_WindDetectDestroy();
        break;

        case aanc_config_fit_test:
            KymeraAdaptiveAnc_DestroyChain();
        break;

        default:
            break;
    }

    DEBUG_LOG("kymeraAncCommon_Destroy, eumn:aanc_config_t:%d chain destroyed", kymeraAncCommon_GetAancCurrentConfig());
}

static void kymeraAncCommon_Disconnect(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            KymeraAah_Disconnect();
            KymeraEchoCanceller_Disconnect();
            /*Stream Disconnect AANC with AHM*/
            KymeraAdaptiveAnc_DisconnectChain();
            KymeraHcgr_Disconnect();         
            KymeraSelfSpeechDetect_Disconnect();         
        break;

        case aanc_config_adaptive_ambient:
            KymeraAah_Disconnect();
            StreamDisconnect(ChainGetOutput(KymeraAdaptiveAmbient_GetChain(),EPR_ADV_ANC_COMPANDER_OUT), NULL);
            KymeraAdaptiveAmbient_DisconnectChain();
            KymeraSelfSpeechDetect_Disconnect();
        break;

        case aanc_config_static_anc:
            KymeraAah_Disconnect();
            KymeraEchoCanceller_Disconnect();
            KymeraHcgr_Disconnect();
            KymeraSelfSpeechDetect_Disconnect();
        break;

        case aanc_config_static_leakthrough:
            KymeraAah_Disconnect();
            KymeraSelfSpeechDetect_Disconnect();
        break;

        case aanc_config_fit_test:
        /* Do nothing */
        break;

        default:            
            /*Stream Disconnect between PEQ and VAD to avoid false triggers from VAD*/
            KymeraSelfSpeechDetect_Disconnect();
            break;
    }
}

static void kymeraAncCommon_AncStopOperators(void)
{
    if (KymeraAncCommon_AdaptiveAncIsEnabled())/*if any ANC mode is active*/
    {
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
        if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
        {
            kymeraAncCommon_FreezeAdaptivity();
        }
        kymeraAncCommon_Stop();
        kymeraAncCommon_Disconnect();
    }
}

static void kymeraAncCommon_AncMicDisconnect(void)
{
    if (KymeraAncCommon_AdaptiveAncIsEnabled())/*if any ANC mode is active*/
    {
        kymeraAncCommon_AncStopOperators();
        Kymera_MicDisconnect(mic_user_aanc);
    }
}

static void kymeraAncCommon_Disable(void)
{
    DEBUG_LOG("kymeraAncCommon_Disable, tearing down AANC graph");

    /* Disable ultra quiet mode with ongoing adaptive anc capability being on mute */
    Kymera_CancelUltraQuietDac();
    kymeraAncCommon_AncMicDisconnect();
    kymeraAncCommon_Destroy();
    kymeraAncCommon_SetAancCurrentConfig(aanc_config_none);
#ifdef ENABLE_WIND_DETECT
    kymeraAncCommon_ResetWindDetectInStage2();
#endif

#if defined(ENABLE_SELF_SPEECH) && defined (ENABLE_AUTO_AMBIENT)
    /*Reconnect Self Speech if active*/
    if (!kymeraAncCommon_IsModeChangeInProgress())
    {
        kymeraAncCommon_SelfSpeechDetect_MicReConnect();
    }
#endif

    kymeraAncCommon_SetAancState(aanc_state_idle);
    kymeraAncCommon_ResetKymeraState();

#ifdef ENABLE_ANC_FAST_MODE_SWITCH
    if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
    {
        /* Update default DSP clock */
        appKymeraConfigureDspPowerMode();
    }
}

static void kymeraAncCommon_DisableOnUserRequest(void)
{
    DEBUG_LOG("kymeraAncCommon_DisableOnUserRequest");

    /* Cancel any pending AHM Ramp-up timeout messages */
    MessageCancelAll(kymeraAncCommon_GetTask(), KYMERA_ANC_COMMON_INTERNAL_AHM_RAMP_TIMEOUT);

    /*Tear down the chains*/
    kymeraAncCommon_Disable();

    /*Inform ANC SM to disable HW*/
    MessageSend(AncStateManager_GetTask(), KYMERA_ANC_COMMON_CAPABILITY_DISABLE_COMPLETE, NULL);
}

static void kymeraAncCommon_StandaloneToConcurrencyChain(void)
{
    DEBUG_LOG_INFO("kymeraAncCommon_StandaloneToConcurrencyChain");
    kymeraAncCommon_UpdateTransitionInProgress(TRUE); /* Standalone to concurrency transition is in progress- ahm should not be destroyed */
    kymeraAncCommon_FreezeAdaptivity();
    KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain()); /* Set target gain to AHM in order to avoid muting ANC during transition */
    kymeraAncCommon_Disable(); /* Disable Standalone Chain with no ramp */
    DEBUG_LOG_INFO("kymeraAncCommon_StandaloneToConcurrencyChain, disabled Standalone chain");

    kymeraAncCommon_UpdateConcurrencyRequest(TRUE); /* Update concurrency flag TRUE */
    KYMERA_INTERNAL_AANC_ENABLE_T mode_settings = getData()->mode_settings;
    KymeraAncCommon_AncEnable(&mode_settings); /* Enable Concurrency chain */
    kymeraAncCommon_UpdateTransitionInProgress(FALSE);/* Standalone to concurrency transition is over- user can destroy ahm using anc disable event*/
}

static void kymeraAncCommon_ConcurrencyToStandaloneChain(void)
{
    DEBUG_LOG_INFO("kymeraAncCommon_ConcurrencyToStandaloneChain");
    kymeraAncCommon_UpdateTransitionInProgress(TRUE);/* Concurrency to standalone transition is in progress- ahm should not be destroyed */
    kymeraAncCommon_FreezeAdaptivity();
    KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain()); /* Set target gain to AHM in order to avoid muting ANC during transition */
    kymeraAncCommon_Disable(); /* Disable Concurrency chain with no ramp */
    DEBUG_LOG_INFO("kymeraAncCommon_ConcurrencyToStandaloneChain, disabled Concurrency chain");

    kymeraAncCommon_UpdateConcurrencyRequest(FALSE); /* Update concurrency flag to FALSE */
    KYMERA_INTERNAL_AANC_ENABLE_T mode_settings = getData()->mode_settings;
    KymeraAncCommon_AncEnable(&mode_settings); /* Enable Standalone chain */
    kymeraAncCommon_UpdateTransitionInProgress(FALSE);/* Concurrency to standalone transition is over- user can destroy ahm using anc disable event*/
}

static void kymeraAncCommon_ApplyModeChangeSettings(KYMERA_INTERNAL_AANC_ENABLE_T *mode_params)
{
    UNUSED(mode_params);
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            KymeraAhm_ApplyModeChange(mode_params);
            KymeraWindDetect_ApplyModeChange(mode_params);
            KymeraAdaptiveAnc_ApplyModeChange(mode_params);
            KymeraAah_ApplyModeChange(mode_params);
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
            kymeraAncCommon_RampGainsToStatic(kymeraAncCommon_StartAdaptivity);
#endif
        break;

        case aanc_config_adaptive_ambient:
            KymeraAhm_ApplyModeChange(mode_params);
            KymeraWindDetect_ApplyModeChange(mode_params);
            KymeraAdaptiveAmbient_ApplyModeChange(mode_params);
            KymeraAah_ApplyModeChange(mode_params);
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
            kymeraAncCommon_RampGainsToStatic(kymeraAncCommon_StartAdaptivity);
#endif
        break;

        case aanc_config_static_anc:
            KymeraAhm_ApplyModeChange(mode_params);
            KymeraWindDetect_ApplyModeChange(mode_params);
            KymeraAah_ApplyModeChange(mode_params);
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
            kymeraAncCommon_RampGainsToStatic(kymeraAncCommon_StartAdaptivity);
#endif
        break;
		
        case aanc_config_static_leakthrough:
            KymeraAhm_ApplyModeChange(mode_params);
            KymeraWindDetect_ApplyModeChange(mode_params);
            KymeraAah_ApplyModeChange(mode_params);
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
            kymeraAncCommon_RampGainsToStatic(NULL);
#endif

        default:
        /* Do nothing */
            break;
    }
}

#ifndef ENABLE_ANC_FAST_MODE_SWITCH

static void kymeraAncCommon_ModeChangeActionPostGainRamp(void)
{
    DEBUG_LOG("kymeraAncCommon_ModeChangeActionPostGainRamp");

    /* Cancel any pending AHM Ramp-up timeout messages */
    MessageCancelAll(AncStateManager_GetTask(), KYMERA_ANC_COMMON_CAPABILITY_MODE_CHANGE_TRIGGER);

    /*Inform ANC SM to Change mode in HW*/
    MessageSend(AncStateManager_GetTask(), KYMERA_ANC_COMMON_CAPABILITY_MODE_CHANGE_TRIGGER, NULL);
}

#endif /* ENABLE_ANC_FAST_MODE_SWITCH */

static bool kymeraAncCommon_IsConcurrencyChainActive(void)
{
    return (KymeraEchoCanceller_IsActive());
}

/***************************************
 *** Mic framework callbacks ***
 ***************************************/
static mic_user_state_t kymera_AncCommonMicState = mic_user_state_interruptible;
static uint32 kymera_AncCommonMicTaskPeriod = TASK_PERIOD_FAST;

static uint16 kymera_AncCommonMics[MAX_ANC_COMMON_MICS] =
{
    appConfigAncFeedForwardMic(),
    appConfigAncFeedBackMic()
};

/*! For a reconnection the mic parameters are sent to the mic interface.
 *  return TRUE to reconnect with the given parameters
 */
static bool kymeraAncCommon_MicGetConnectionParameters(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    DEBUG_LOG("kymeraAncCommon_MicGetConnectionParameters");

    *sample_rate = KYMERA_ANC_COMMON_SAMPLE_RATE;
    *num_of_mics = 0;    
    aec_ref_sink[0]=NULL;

    if (KymeraAncCommon_AdaptiveAncIsEnabled())
    {
        *num_of_mics = kymeraAncCommon_GetRequiredAncMics();
      if (*num_of_mics > 0)
      {
        mic_ids[0] = kymeraAncCommon_GetAncFeedforwardMicConfig();
        mic_ids[1] = kymeraAncCommon_GetAncFeedbackMicConfig();

        mic_sinks[0] = kymeraAncCommon_GetFeedforwardPathSink();
        mic_sinks[1] = kymeraAncCommon_GetFeedbackPathSink();

        if(kymeraAncCommon_GetAancCurrentConfig()== aanc_config_static_anc)
        {
#if !defined(ENABLE_ANC_AAH) && !defined(ENABLE_WIND_DETECT)
            mic_ids[0] = kymeraAncCommon_GetAncFeedbackMicConfig();
            mic_ids[1] = MICROPHONE_NONE;
            mic_sinks[0] = kymeraAncCommon_GetFeedbackPathSink();
            mic_sinks[1] = NULL;
#endif
        }
      }
        aec_ref_sink[0] = kymeraAncCommon_GetPlaybackPathSink();

        DEBUG_LOG("kymeraAncCommon_MicGetConnectionParameters for ANC mics");
#if defined(ENABLE_WIND_DETECT)
        if (kymeraAncCommon_IsWindDetectInStage2())
        {   
            uint16 mic_index = *num_of_mics;
            *num_of_mics = mic_index+1;/*Include Diversity mic for wind detect*/
            DEBUG_LOG("kymeraAncCommon_MicGetConnectionParameters WNR *num_of_mics %d, mic_index %d", *num_of_mics, mic_index);
            mic_ids[mic_index] = appConfigWindDetectDiversityMic();
            mic_sinks[mic_index] = KymeraWindDetect_GetDiversityMicPathSink();
        }
#endif
    }     
    
#if defined(ENABLE_AUTO_AMBIENT)
    if (KymeraSelfSpeechDetect_IsActive())
    {
        uint16 mic_index = *num_of_mics;
        *num_of_mics = mic_index+1;/*Include Self Speech mic for Ambient mode auto enable*/
        DEBUG_LOG("kymeraAncCommon_MicGetConnectionParameters Auto ambient *num_of_mics %d, mic_index %d ", *num_of_mics, mic_index);
        mic_ids[mic_index] = appConfigSelfSpeechDetectMic();
        mic_sinks[mic_index] =  KymeraSelfSpeechDetect_GetMicPathSink();
    }
#endif
    return TRUE;
}


static bool kymeraAncCommon_MicDisconnectIndication(const mic_change_info_t *info)
{
    UNUSED(info);
    /* Stop & disconnect Adaptive ANC audio graph - due to client disconnection request */
    kymeraAncCommon_FreezeAdaptivity();
    kymeraAncCommon_Stop();
    kymeraAncCommon_Disconnect();
    return TRUE;
}

static void kymeraAncCommon_MicReconnectedIndication(void)
{
    kymeraAncCommon_Connect();
    kymeraAncCommon_Start();
}

static const mic_callbacks_t kymera_AncCommonCallbacks =
{
    .MicGetConnectionParameters = kymeraAncCommon_MicGetConnectionParameters,
    .MicDisconnectIndication = kymeraAncCommon_MicDisconnectIndication,
    .MicReconnectedIndication = kymeraAncCommon_MicReconnectedIndication,
};

static const mic_registry_per_user_t kymera_AncCommonRegistry =
{
    .user = mic_user_aanc,
    .callbacks = &kymera_AncCommonCallbacks,
    .mandatory_mic_ids = &kymera_AncCommonMics[0],
    .num_of_mandatory_mics = MAX_ANC_COMMON_MICS,
    .mic_user_state = &kymera_AncCommonMicState,
    .mandatory_task_period_us = &kymera_AncCommonMicTaskPeriod,
    .use_aec_ref = &data.concurrency_req,
};

/***************************************
 *** Kymera Output Manager callbacks ***
 ***************************************/
/*! Notifies registered user that another user is about to connect to the output chain. */
static void kymeraAncCommon_AdaptiveAncOutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    DEBUG_LOG_INFO("kymeraAncCommon_AdaptiveAncOutputConnectingIndication connecting user: enum:output_users_t:%d",connecting_user);

    data.connecting_user = connecting_user;

    if(kymeraAncCommon_IsAancCurrentConfigValid())
    {
        kymeraAncCommon_UpdateConcurrencyRequest(TRUE);
        /* Update optimum DSP clock for concurrency usecase */
        appKymeraConfigureDspPowerMode();

        if(((kymeraAncCommon_GetAancCurrentConfig() == aanc_config_adaptive_anc) || (kymeraAncCommon_GetAancCurrentConfig() == aanc_config_static_anc))
                && !kymeraAncCommon_IsConcurrencyChainActive())
        {
                KymeraAncCommon_StandaloneToConcurrencyReq();
        }
    }
    else if (KymeraSelfSpeechDetect_IsActive())
	{
		kymeraAncCommon_UpdateConcurrencyRequest(TRUE);
	}
}

/*! Notifies the user the output chain is idle (no active users/the chain is destroyed). */
static void kymeraAncCommon_AdaptiveAncOutputIdleIndication(void)
{
    DEBUG_LOG_INFO("kymeraAncCommon_AdaptiveAncOutputIdleIndication Kymera_OutputIsChainInUse():%d",Kymera_OutputIsChainInUse());

    if(!Kymera_OutputIsChainInUse())
    {
        data.connecting_user = output_user_none;
        kymeraAncCommon_UpdateConcurrencyRequest(FALSE);

        if(((kymeraAncCommon_GetAancCurrentConfig() == aanc_config_adaptive_anc) || (kymeraAncCommon_GetAancCurrentConfig() == aanc_config_static_anc))
                && kymeraAncCommon_IsConcurrencyChainActive())
        {
            KymeraAncCommon_ConcurrencyToStandaloneReq();
        }
        /* Update optimum DSP clock for standalone usecase */
        appKymeraConfigureDspPowerMode();
    }
}

static void kymeraAncCommon_SetAancState(aanc_state_t state)
{
    data.aanc_state = state;
}

static aanc_state_t kymeraAncCommon_GetAancState(void)
{
    return (data.aanc_state);
}

static const output_indications_registry_entry_t aanc_user_info =
{
    .OutputConnectingIndication = kymeraAncCommon_AdaptiveAncOutputConnectingIndication,
    .OutputIdleIndication = kymeraAncCommon_AdaptiveAncOutputIdleIndication,
};



/***************************************
 ********* Global functions ************
 ***************************************/
void KymeraAncCommon_Init(void)
{
    Kymera_OutputRegisterForIndications(&aanc_user_info);
    kymera_AncCommonMicTaskPeriod = TASK_PERIOD_FAST;
    Kymera_MicRegisterUser(&kymera_AncCommonRegistry);
    ahmRampingCompleteCallback = NULL;
    kymeraAncCommon_SetAancCurrentConfig(aanc_config_none);
#ifdef ENABLE_WIND_DETECT
    kymeraAncCommon_ResetWindDetectInStage2();
#endif
    KymeraHcgr_UpdateUserState(howling_detection_enabled);
    kymeraAncCommon_SetAancState(aanc_state_idle);
    kymeraAncCommon_SetModeChangeInProgress(FALSE);

    getData()->prev_filter_config = NULL;
}

void KymeraAncCommon_AncEnable(const KYMERA_INTERNAL_AANC_ENABLE_T *msg)
{
    DEBUG_LOG("KymeraAncCommon_AncEnable");

    bool mic_Connect_Request= TRUE;
    kymeraAncCommon_SetAancState(aanc_state_enable_initiated);
    /* Store mode settings to apply again in transitional case */
    kymeraAncCommon_StoreModeSettings(msg);
    kymeraAncCommon_SetAancCurrentConfig(kymeraAncCommon_GetAancConfigFromMode(msg->current_mode));

    if(Kymera_OutputIsChainInUse())
    {
        kymeraAncCommon_UpdateConcurrencyRequest(TRUE);
    }

    appKymeraConfigureDspPowerMode();

#if defined(ENABLE_SELF_SPEECH) && defined (ENABLE_AUTO_AMBIENT)
    /*If Self speech is active, Disconnect Mic*/
    if (!kymeraAncCommon_IsModeChangeInProgress())
    {
        kymeraAncCommon_SelfSpeechDetect_MicDisconnect();
    }
#endif

    /*Create and Enable ANC chain*/
    kymeraAncCommon_Create();
    kymeraAncCommon_Configure(msg);


    /*
     * If the requested mode/config is Static LT and WIND DETECT Flag and AAH Flag is not included in the
     * build then mic connect with the AEC_REF chain is not required as  AHM is moved out of
     * audio graph and no operator available for connection after the AEC_REF chain.
    */
    if(kymeraAncCommon_IsCurrentConfigStaticLeakthrough())
    {
#if !defined(ENABLE_ANC_AAH) && !defined(ENABLE_WIND_DETECT)
        if (!KymeraSelfSpeechDetect_IsActive())
        {
            mic_Connect_Request = FALSE;
        }
#endif
    }

    if(!mic_Connect_Request)
    {
        kymeraAncCommon_Connect();
        kymeraAncCommon_Start();
        kymeraAncCommon_SetAancState(aanc_state_enabled);
        kymeraAncCommon_SetKymeraState();
    }
    else
    {
        /*kymeraAncCommon_MicGetConnectionParameters() takes care of ANC and VAD connection to mic framework*/
        if (!Kymera_MicConnect(mic_user_aanc))
        {
            kymeraAncCommon_Destroy();
            kymeraAncCommon_SetAancState(aanc_state_idle);
            kymeraAncCommon_SetAancCurrentConfig(aanc_config_none);
            kymeraAncCommon_UpdateConcurrencyRequest(FALSE);
            MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_MIC_CONNECTION_TIMEOUT_ANC,
                             NULL, MIC_CONNECT_RETRY_MS);
        }
        else
        {
            kymeraAncCommon_Connect();
            kymeraAncCommon_Start();
            kymeraAncCommon_SetAancState(aanc_state_enabled);
            kymeraAncCommon_SetKymeraState();
        }
    }
}

void KymeraAncCommon_AdaptiveAncDisable(void)
{
    if(kymeraAncCommon_GetAancCurrentConfig() == aanc_config_fit_test)
    {
        DEBUG_LOG("KymeraAncCommon_AdaptiveAncDisable");
        kymeraAncCommon_Disable();
    }
}
bool KymeraAncCommon_AdaptiveAncIsEnabled(void)
{
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
        case aanc_config_adaptive_ambient:
        case aanc_config_static_anc:
        case aanc_config_static_leakthrough:
            return KymeraAhm_IsActive();

        case aanc_config_fit_test:
            return KymeraAdaptiveAnc_IsActive();

        default:
        /* Do nothing */
            break;
    }
    return FALSE;
}

void KymeraAncCommon_EnableQuietMode(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraAncCommon_EnableQuietMode");
    kymeraAncCommon_AdaptiveAncFreezeAdaptivity();
    KymeraAhm_SetSysMode(ahm_sysmode_quiet);
#ifdef INCLUDE_ULTRA_QUIET_DAC_MODE
    kymeraAncCommn_RampGainsToQuiet(Kymera_RequestUltraQuietDac);
#else
    kymeraAncCommn_RampGainsToQuiet(NULL);
#endif
}

void KymeraAncCommon_DisableQuietMode(void)
{
    DEBUG_LOG_FN_ENTRY("KymeraAncCommon_DisableQuietMode");
    Kymera_CancelUltraQuietDac();
    KymeraAhm_SetSysMode(ahm_sysmode_full);
    kymeraAncCommon_AdaptiveAncStartAdaptivity();
}

void KymeraAncCommon_GetFFGain(uint8 *gain)
{
    uint16 read_gain = 0;
    KymeraAncCommon_AhmGetFFGain(&read_gain);
    *gain = (read_gain & 0xFF);
}

void KymeraAncCommon_GetFBGain(uint8 *gain)
{
    uint16 read_gain = 0;
    KymeraAhm_GetFBGain(&read_gain);
    *gain = (read_gain & 0xFF);
}

audio_dsp_clock_type KymeraAncCommon_GetOptimalAudioClockAancScoConcurrency(appKymeraScoMode mode)
{
    audio_dsp_clock_type dsp_clock = AUDIO_DSP_TURBO_CLOCK;
    DEBUG_LOG("KymeraAncCommon_GetOptimalAudioClockAancScoConcurrency");

    if(appKymeraInConcurrency())
    {
        switch(mode)
        {
        case SCO_WB:
#ifdef KYMERA_SCO_USE_3MIC
            dsp_clock = AUDIO_DSP_TURBO_PLUS_CLOCK;
#endif
        break;
        case SCO_SWB:
        case SCO_UWB:
            dsp_clock = AUDIO_DSP_TURBO_PLUS_CLOCK;
        break;
        default:
        break;
        }
    }
    return dsp_clock;
}

audio_dsp_clock_type KymeraAncCommon_GetOptimalAudioClockAancMusicConcurrency(int seid)
{
    DEBUG_LOG("KymeraAncCommon_GetOptimalAudioClockAancMusicConcurrency");
#ifndef INCLUDE_DECODERS_ON_P1
    if(seid == AV_SEID_APTX_ADAPTIVE_SNK)
    {
        return AUDIO_DSP_TURBO_PLUS_CLOCK;
    }
#else
    UNUSED(seid);
#endif
    return AUDIO_DSP_TURBO_CLOCK;
}

void KymeraAncCommon_AhmGetFFGain(uint16 *gain)
{
    KymeraAhm_GetFFGain(gain);
}

void KymeraAncCommon_AdaptiveAncSetGainValues(uint32 mantissa, uint32 exponent)
{
    UNUSED(mantissa);
    UNUSED(exponent);
}

void KymeraAncCommon_UpdateInEarStatus(void)
{
    KymeraAhm_UpdateInEarStatus(TRUE);
}

void KymeraAncCommon_UpdateOutOfEarStatus(void)
{
    KymeraAhm_UpdateInEarStatus(FALSE);
}


void KymeraAncCommon_ExitAdaptiveAncTuning(const adaptive_anc_tuning_disconnect_parameters_t *param)
{
    UNUSED(param);

}

#ifdef ENABLE_ANC_FAST_MODE_SWITCH

static void kymeraAncCommon_ResetFilterConfig(kymera_anc_common_anc_filter_config_t* filter_config)
{
    if (filter_config != NULL)
    {
        free(filter_config);
    }
}

static void kymeraAncCommon_ResetPrevFilterConfig(void)
{
    kymeraAncCommon_ResetFilterConfig(data.prev_filter_config);
    data.prev_filter_config = NULL;
}

static kymera_anc_common_anc_filter_config_t* kymeraAncCommon_GetPrevFilterConfig(void)
{
    return data.prev_filter_config;
}

static void kymeraAncCommon_SetPrevFilterConfig(kymera_anc_common_anc_filter_config_t* filter_config)
{
    kymeraAncCommon_ResetFilterConfig(kymeraAncCommon_GetPrevFilterConfig());
    data.prev_filter_config = filter_config;
}

static void KymeraAncCommon_SetLpfCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8 lpf_shift_1, uint8 lpf_shift_2)
{
    DEBUG_LOG("KymeraAncCommon_SetLpfCoefficients, instance: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d, shift1: %d, shift2: %d", instance, path, lpf_shift_1, lpf_shift_2);

    PanicFalse(AncConfigureLPFCoefficients(instance, path, lpf_shift_1, lpf_shift_2));

}

static void KymeraAncCommon_SetDcFilterCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8 filter_shift, uint8 filter_enable)
{
    DEBUG_LOG("KymeraAncCommon_SetDcFilterCoefficients, instance: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d, shift: %d, shift: %d", instance, path, filter_shift, filter_enable);

    PanicFalse(AncConfigureDcFilterCoefficients(instance, path, filter_shift, filter_enable));

}

static void KymeraAncCommon_SetSmallLpfCoefficients(audio_anc_instance instance, uint8 filter_shift, uint8 filter_enable)
{
    DEBUG_LOG("KymeraAncCommon_SetSmallLpfCoefficients, instance: enum:audio_anc_instance:%d, shift: %d, shift: %d", instance, filter_shift, filter_enable);

    PanicFalse(AncConfigureSmLPFCoefficients(instance, filter_shift, filter_enable));

}

static kymera_anc_common_lpf_config_t* KymeraAncCommon_GetLpfCoefficientsForPath(audio_anc_instance instance, audio_anc_path_id path, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_GetLpfCoefficientsForPath, instance: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    kymera_anc_common_anc_instance_config_t* instance_config = KymeraAncCommon_GetInstanceConfig(filter_config, instance);

    kymera_anc_common_lpf_config_t *lpf_config = NULL;

    if (instance_config)
    {
        switch (path)
        {
        case AUDIO_ANC_PATH_ID_FFA:
        {
            lpf_config = &instance_config->ffa_config.lpf_config;
        }
        break;

        case AUDIO_ANC_PATH_ID_FFB:
        {
            lpf_config = &instance_config->ffb_config.lpf_config;
        }
        break;

        case AUDIO_ANC_PATH_ID_FB:
        {
            lpf_config = &instance_config->fb_config.lpf_config;
        }
        break;

        default:
        {
            lpf_config = NULL;
        }
        break;
        }
    }

    return lpf_config;
}

static kymera_anc_common_dc_config_t* KymeraAncCommon_GetDcFilterCoefficientsForPath(audio_anc_instance instance, audio_anc_path_id path, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_GetDcFilterCoefficientsForPath, instance: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    kymera_anc_common_anc_instance_config_t* instance_config = KymeraAncCommon_GetInstanceConfig(filter_config, instance);

    kymera_anc_common_dc_config_t *dc_config = NULL;

    if (instance_config)
    {
        switch (path)
        {
        case AUDIO_ANC_PATH_ID_FFA:
        {
            dc_config = &instance_config->ffa_config.dc_filter_config;
        }
        break;

        case AUDIO_ANC_PATH_ID_FFB:
        {
            dc_config = &instance_config->ffb_config.dc_filter_config;
        }
        break;

        default:
        {
            dc_config = NULL;
        }
        break;
        }
    }

    return dc_config;
}

static void KymeraAncCommon_SetLpfCoefficientsForPath(audio_anc_instance instance, audio_anc_path_id path)
{
    DEBUG_LOG("KymeraAncCommon_SetLpfCoefficientsForPath, instance: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    kymera_anc_common_lpf_config_t* curr_lpf_config = PanicUnlessMalloc(sizeof(kymera_anc_common_lpf_config_t));
    bool compare = kymeraAncCommon_IsModeChangeInProgress();
    bool update = TRUE;

    memset(curr_lpf_config, 0, sizeof(kymera_anc_common_lpf_config_t));

    KymeraAncCommon_ReadLpfCoefficients(instance, path, &curr_lpf_config->lpf_shift_1, &curr_lpf_config->lpf_shift_2);

    if (compare)
    {
        kymera_anc_common_anc_filter_config_t* prev_filter_config = kymeraAncCommon_GetPrevFilterConfig();
        kymera_anc_common_lpf_config_t* prev_lpf_config = KymeraAncCommon_GetLpfCoefficientsForPath(instance, path, prev_filter_config);

        if(prev_lpf_config && prev_lpf_config->lpf_shift_1 == curr_lpf_config->lpf_shift_1 &&
                prev_lpf_config->lpf_shift_2 == curr_lpf_config->lpf_shift_2)
        {
            update = FALSE;
        }
    }

    if (update)
    {
        KymeraAncCommon_SetLpfCoefficients(instance, path, curr_lpf_config->lpf_shift_1, curr_lpf_config->lpf_shift_2);
    }

    free(curr_lpf_config);
}

static void KymeraAncCommon_SetDcFilterCoefficientsForPath(audio_anc_instance instance, audio_anc_path_id path)
{
    DEBUG_LOG("KymeraAncCommon_SetDcFilterCoefficientsForPath, instance: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    kymera_anc_common_dc_config_t* curr_dc_config = PanicUnlessMalloc(sizeof(kymera_anc_common_dc_config_t));
    bool compare = kymeraAncCommon_IsModeChangeInProgress();
    bool update = TRUE;

    memset(curr_dc_config, 0, sizeof(kymera_anc_common_lpf_config_t));

    KymeraAncCommon_ReadDcFilterCoefficients(instance, path, &curr_dc_config->filter_shift, &curr_dc_config->filter_enable);

    if (compare)
    {
        kymera_anc_common_anc_filter_config_t* prev_filter_config = kymeraAncCommon_GetPrevFilterConfig();
        kymera_anc_common_dc_config_t* prev_dc_config = KymeraAncCommon_GetDcFilterCoefficientsForPath(instance, path, prev_filter_config);

        if(prev_dc_config && prev_dc_config->filter_shift == curr_dc_config->filter_shift &&
                prev_dc_config->filter_enable == curr_dc_config->filter_enable)
        {
            update = FALSE;
        }
    }

    if (update)
    {
        KymeraAncCommon_SetDcFilterCoefficients(instance, path, curr_dc_config->filter_shift, curr_dc_config->filter_enable);
    }

    free(curr_dc_config);
}

static void KymeraAncCommon_SetLpfCoefficientsForInstance(audio_anc_instance instance)
{
    DEBUG_LOG("KymeraAncCommon_SetLpfCoefficientsForInstance, instance: enum:audio_anc_instance:%d", instance);

    KymeraAncCommon_SetLpfCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFA);
    KymeraAncCommon_SetLpfCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFB);
    KymeraAncCommon_SetLpfCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FB);
}

static void KymeraAncCommon_SetDcFilterCoefficientsForInstance(audio_anc_instance instance)
{
    DEBUG_LOG("KymeraAncCommon_SetDcFilterCoefficientsForInstance, instance: enum:audio_anc_instance:%d", instance);

    KymeraAncCommon_SetDcFilterCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFA);
    KymeraAncCommon_SetDcFilterCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFB);
}

static void KymeraAncCommon_SetSmallLpfCoefficientsForInstance(audio_anc_instance instance)
{

    DEBUG_LOG("KymeraAncCommon_SetSmallLpfCoefficientsForInstance, instance: enum:audio_anc_instance:%d", instance);

    kymera_anc_common_small_lpf_config_t* curr_small_lpf_config = PanicUnlessMalloc(sizeof(kymera_anc_common_small_lpf_config_t));
    bool compare = kymeraAncCommon_IsModeChangeInProgress();
    bool update = TRUE;

    memset(curr_small_lpf_config, 0, sizeof(kymera_anc_common_lpf_config_t));

    KymeraAncCommon_ReadSmallLpfCoefficients(instance, &curr_small_lpf_config->filter_shift, &curr_small_lpf_config->filter_enable);

    if (compare)
    {
        kymera_anc_common_anc_filter_config_t* prev_filter_config = kymeraAncCommon_GetPrevFilterConfig();
        kymera_anc_common_anc_instance_config_t* instance_config = KymeraAncCommon_GetInstanceConfig(prev_filter_config, instance);
        kymera_anc_common_small_lpf_config_t* prev_small_lpf_config = instance_config != NULL ? &instance_config->small_lpf_config : NULL;

        if(prev_small_lpf_config && prev_small_lpf_config->filter_shift == curr_small_lpf_config->filter_shift &&
                prev_small_lpf_config->filter_enable == curr_small_lpf_config->filter_enable)
        {
            update = FALSE;
        }
    }

    if (update)
    {
        KymeraAncCommon_SetSmallLpfCoefficients(instance, curr_small_lpf_config->filter_shift, curr_small_lpf_config->filter_enable);
    }

    free(curr_small_lpf_config);
}

static void KymeraAncCommon_SetFilterCoefficientsForInstance(audio_anc_instance instance)
{
    DEBUG_LOG("KymeraAncCommon_SetFilterCoefficientsForInstance");

    KymeraAncCommon_SetLpfCoefficientsForInstance(instance);
    KymeraAncCommon_SetDcFilterCoefficientsForInstance(instance);
    KymeraAncCommon_SetSmallLpfCoefficientsForInstance(instance);
}

static void KymeraAncCommon_ReadLpfCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8* lpf_shift_1, uint8* lpf_shift_2)
{
    DEBUG_LOG("KymeraAncCommon_ReadLpfCoefficients, inst: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    PanicFalse(AncReadLpfCoefficients(instance, path, lpf_shift_1, lpf_shift_2));

    DEBUG_LOG("KymeraAncCommon_ReadLpfCoefficients, shift1: %d, shift2: %d", *lpf_shift_1, *lpf_shift_2);
}

static void KymeraAncCommon_ReadDcFilterCoefficients(audio_anc_instance instance, audio_anc_path_id path, uint8* filter_shift, uint8* filter_enable)
{
    DEBUG_LOG("KymeraAncCommon_ReadDcFilterCoefficients, inst: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    PanicFalse(AncReadDcFilterCoefficients(instance, path, filter_shift, filter_enable));

    DEBUG_LOG("KymeraAncCommon_ReadDcFilterCoefficients, filter_shift: %d, filter_enable: %d", *filter_shift, *filter_enable);

}

static void KymeraAncCommon_ReadSmallLpfCoefficients(audio_anc_instance instance, uint8* filter_shift, uint8* filter_enable)
{
    DEBUG_LOG("KymeraAncCommon_ReadSmallLpfCoefficients, inst: enum:audio_anc_instance:%d", instance);

    PanicFalse(AncReadSmallLpfCoefficients(instance, filter_shift, filter_enable));

    DEBUG_LOG("KymeraAncCommon_ReadSmallLpfCoefficients, filter_shift: %d, filter_enable: %d", *filter_shift, *filter_enable);
}

static void KymeraAncCommon_ReadLpfCoefficientsForPath(audio_anc_instance instance, audio_anc_path_id path, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_ReadLpfCoefficientsForPath, inst: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    kymera_anc_common_lpf_config_t* lpf_config = KymeraAncCommon_GetLpfCoefficientsForPath(instance, path, filter_config);

    if (lpf_config)
    {
        KymeraAncCommon_ReadLpfCoefficients(instance, path, &lpf_config->lpf_shift_1, &lpf_config->lpf_shift_2);
    }
}

static void KymeraAncCommon_ReadDcFilterCoefficientsForPath(audio_anc_instance instance, audio_anc_path_id path, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_ReadLpfCoefficientsForPath, inst: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    kymera_anc_common_dc_config_t* dc_config = KymeraAncCommon_GetDcFilterCoefficientsForPath(instance, path, filter_config);

    if (dc_config)
    {
        KymeraAncCommon_ReadDcFilterCoefficients(instance, path, &dc_config->filter_shift, &dc_config->filter_enable);
    }
}



static void KymeraAncCommon_ReadLpfCoefficientsForInstance(audio_anc_instance instance, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_ReadFilterCoefficientsForInstance, instance: enum:audio_anc_instance:%d", instance);

    KymeraAncCommon_ReadLpfCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFA, filter_config);
    KymeraAncCommon_ReadLpfCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFB, filter_config);
    KymeraAncCommon_ReadLpfCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FB, filter_config);
}

static void KymeraAncCommon_ReadDcFilterCoefficientsForInstance(audio_anc_instance instance, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_ReadDcFilterCoefficientsForInstance, instance: enum:audio_anc_instance:%d", instance);

    KymeraAncCommon_ReadDcFilterCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFA, filter_config);
    KymeraAncCommon_ReadDcFilterCoefficientsForPath(instance, AUDIO_ANC_PATH_ID_FFB, filter_config);
}

static void KymeraAncCommon_ReadSmallLpfCoefficientsForInstance(audio_anc_instance instance, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_ReadSmallLpfCoefficients");

    kymera_anc_common_anc_instance_config_t* instance_config = KymeraAncCommon_GetInstanceConfig(filter_config, instance);
    kymera_anc_common_small_lpf_config_t* small_lpf_config = instance_config != NULL ? &instance_config->small_lpf_config : NULL;

    if (small_lpf_config)
    {
        KymeraAncCommon_ReadSmallLpfCoefficients(instance, &small_lpf_config->filter_shift, &small_lpf_config->filter_enable);
    }
}

static void KymeraAncCommon_ReadFilterCoefficientsForInstance(audio_anc_instance instance, kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_ReadFilterCoefficientsForInstance, instance: enum:audio_anc_instance:%d", instance);

    KymeraAncCommon_ReadLpfCoefficientsForInstance(instance, filter_config);
    KymeraAncCommon_ReadDcFilterCoefficientsForInstance(instance, filter_config);
    KymeraAncCommon_ReadSmallLpfCoefficientsForInstance(instance, filter_config);
}

static void KymeraAncCommon_GetFilterConfig(kymera_anc_common_anc_filter_config_t* filter_config)
{
    DEBUG_LOG("KymeraAncCommon_GetFilterConfig");

    filter_config->mode = AncStateManager_GetCurrentMode();

    KymeraAncCommon_ReadFilterCoefficientsForInstance(AUDIO_ANC_INSTANCE_0, filter_config);
    if (appKymeraIsParallelAncFilterEnabled())
    {
        KymeraAncCommon_ReadFilterCoefficientsForInstance(AUDIO_ANC_INSTANCE_1, filter_config);
    }
}

static bool KymeraAncCommon_IsAncModeLeakthrough(anc_mode_t mode)
{
    DEBUG_LOG("KymeraAncCommon_IsAncModeLeakthrough");

    return AncConfig_IsAncModeLeakThrough(mode);
}

static bool KymeraAncCommon_IsAncModeNoiseCancellation(anc_mode_t mode)
{
    DEBUG_LOG("KymeraAncCommon_IsAncModeNoiseCancellation");

    return !AncConfig_IsAncModeLeakThrough(mode);
}

static ahm_trigger_transition_ctrl_t KymeraAncCommon_GetTransition(anc_mode_t prev_mode, anc_mode_t curr_mode)
{
    DEBUG_LOG("KymeraAncCommon_GetTransition");

    ahm_trigger_transition_ctrl_t transition = ahm_trigger_transition_ctrl_dissimilar_filters;

    if (KymeraAncCommon_IsAncModeLeakthrough(prev_mode) == KymeraAncCommon_IsAncModeLeakthrough(curr_mode) ||
            KymeraAncCommon_IsAncModeNoiseCancellation(prev_mode) == KymeraAncCommon_IsAncModeNoiseCancellation(curr_mode))
    {
        transition = ahm_trigger_transition_ctrl_similar_filters;
    }

    return transition;
}

static kymera_anc_common_anc_instance_config_t* KymeraAncCommon_GetInstanceConfig(kymera_anc_common_anc_filter_config_t* filter_config, audio_anc_instance instance)
{
    DEBUG_LOG("KymeraAncCommon_GetInstanceConfig");

    kymera_anc_common_anc_instance_config_t* instance_config = NULL;

    if (filter_config != NULL)
    {
        if (instance == AUDIO_ANC_INSTANCE_0)
        {
            instance_config = &filter_config->anc_instance_0_config;
        }
        else if (instance == AUDIO_ANC_INSTANCE_1)
        {
            instance_config = &filter_config->anc_instance_1_config;
        }
    }

    return instance_config;
}

static void KymeraAncCommon_AhmTriggerTransition(audio_anc_path_id anc_path, adaptive_anc_hw_channel_t anc_hw_channel, ahm_trigger_transition_ctrl_t transition)
{
    DEBUG_LOG("KymeraAncCommon_AhmTriggerTransition");

    if (transition == ahm_trigger_transition_ctrl_similar_filters ||
            transition == ahm_trigger_transition_ctrl_dissimilar_filters)
    {
        ahmRampingCompleteCallback = kymeraAncCommon_EnableRxMix; /* Enable RxMix on ramp down complete during mode change */
    }

    KymeraAhm_TriggerTransition(anc_path, anc_hw_channel, transition);

    /* Cancel any pending AHM Transition timeout messages */
    MessageCancelAll(kymeraAncCommon_GetTask(), KYMERA_ANC_COMMON_INTERNAL_AHM_TRANSITION_TIMEOUT);

    /*Inform Kymera module to put AHM in full proc after timeout */
    MessageSendLater(kymeraAncCommon_GetTask(), KYMERA_ANC_COMMON_INTERNAL_AHM_TRANSITION_TIMEOUT, NULL, AHM_TRANSITION_TIMEOUT_MS);
}

/* Sequence for Mode change is
i) KymeraAncCommon_PreAncModeChange: Store previous mode LPF, DC Filter, Small LPF coefficients
ii) Update ANC lib with new mode coefficients
iii) KymeraAncCommon_ApplyModeChange: Triggered from ANC SM
    - Update IIR coefficients and gains to AHM and trigger transition
    - Update LPF, DC, Small LPF coefficients to ANC HW if they differ from previous mode
    - Disconnect and Destroy ANC chains in capability except AHM
    - Enable new mode with new configuration in capabilities
*/

/* Transition the AHM in preparation for Mode change*/
void KymeraAncCommon_PreAncModeChange(void)
{
    DEBUG_LOG("KymeraAncCommon_PreAncModeChange");

    kymera_anc_common_anc_filter_config_t* filter_config = PanicUnlessMalloc((sizeof(kymera_anc_common_anc_filter_config_t)));
    memset(filter_config, 0, sizeof(kymera_anc_common_anc_filter_config_t));

    KymeraAncCommon_GetFilterConfig(filter_config);

    kymeraAncCommon_SetPrevFilterConfig(filter_config);
}

#else

/* Sequence for Mode change is
i) KymeraAncCommon_PreAncModeChange: Ramp gains
ii) kymeraAncCommon_ModeChangeActionPostGainRamp: Inform ANC SM to change mode in HW
iii) KymeraAncCommon_ApplyModeChange: Triggered from ANC SM
    - Disconnect and Destroy ANC chains in capability except AHM
    - Enable new mode with new configuration in capabilities
*/

/*Ramp down the gains in preparation for Mode change*/
void KymeraAncCommon_PreAncModeChange(void)
{
    DEBUG_LOG("KymeraAncCommon_PreAncModeChange");
    kymeraAncCommon_FreezeAdaptivity();
    kymeraAncCommon_RampGainsToMute(kymeraAncCommon_ModeChangeActionPostGainRamp);

    /* Handle Anc Mode change action if the AHM ramp completion message is not received */
    MessageSendLater(AncStateManager_GetTask(), KYMERA_ANC_COMMON_CAPABILITY_MODE_CHANGE_TRIGGER,
                     NULL, AHM_RAMP_TIMEOUT_MS);
    Kymera_CancelUltraQuietDac();
}

#endif /* ENABLE_ANC_FAST_MODE_SWITCH */

/*Apply Mode change to capabilties */
void KymeraAncCommon_ApplyModeChange(anc_mode_t mode, audio_anc_path_id anc_path, adaptive_anc_hw_channel_t anc_hw_channel)
{
    KYMERA_INTERNAL_AANC_ENABLE_T settings;
    settings.control_path = anc_path;
    settings.current_mode = mode;
    settings.hw_channel = anc_hw_channel;
    settings.in_ear = TRUE;

    kymeraAncCommon_SetModeChangeInProgress(TRUE);

#ifdef ENABLE_ANC_FAST_MODE_SWITCH
    ahm_trigger_transition_ctrl_t transition;

    transition = KymeraAncCommon_GetTransition(kymeraAncCommon_GetPrevFilterConfig()->mode,
                                                mode);

    KymeraAncCommon_AhmTriggerTransition(anc_path, anc_hw_channel, transition);

    KymeraAncCommon_SetFilterCoefficients();
    kymeraAncCommon_ResetPrevFilterConfig();

#endif /* ENABLE_ANC_FAST_MODE_SWITCH */

    kymeraAncCommon_StoreModeSettings(&settings);

    DEBUG_LOG("KymeraAncCommon_ApplyModeChange, Current config:eumn:aanc_config_t:%d, New Config:eumn:aanc_config_t:%d chain created",
        kymeraAncCommon_GetAancCurrentConfig(), kymeraAncCommon_GetAancConfigFromMode(mode));

    /* if current and new aanc config same type */
    if(kymeraAncCommon_GetAancCurrentConfig() == kymeraAncCommon_GetAancConfigFromMode(mode))
    {
        DEBUG_LOG("KymeraAncCommon_ApplyModeChange, Same AANC config type");
        kymeraAncCommon_SetAancCurrentConfig(kymeraAncCommon_GetAancConfigFromMode(mode));
        kymeraAncCommon_ApplyModeChangeSettings(&settings);
    }
    else
    {
        /* Tear down current audio graph */
        kymeraAncCommon_Disable();

        DEBUG_LOG("KymeraAncCommon_ApplyModeChange, Disabled current mode");
        kymeraAncCommon_SetAancCurrentConfig(kymeraAncCommon_GetAancConfigFromMode(mode));
        /* Bring new audio graph */

        KymeraAncCommon_AncEnable(&settings);
    }
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
    /* When Fast mode switch is enabled, mode change gets completed on receiving Transition complete message. */
    kymeraAncCommon_SetModeChangeInProgress(FALSE);
#endif
}

/* Sequence for ANC Disable is
i) KymeraAncCommon_PreAncDisable: Ramp gains
ii) kymeraAncCommon_DisableOnUserRequest:
   - kymeraAncCommon_Disable: Disconnect and Destroy ANC chains in capability
   - Inform ANC SM to disable ANC HW
*/
void KymeraAncCommon_PreAncDisable(void)
{
    if(kymeraAncCommon_GetAancCurrentConfig() != aanc_config_fit_test)
    {
        DEBUG_LOG("KymeraAncCommon_PreAncDisable");

        kymeraAncCommon_FreezeAdaptivity();
        /* Register callback action after gain ramping complete */
        kymeraAncCommon_RampGainsToMute(kymeraAncCommon_DisableOnUserRequest);

        /* Handle Anc disable action if the AHM ramp completion message is not received */
        MessageSendLater(kymeraAncCommon_GetTask(), KYMERA_ANC_COMMON_INTERNAL_AHM_RAMP_TIMEOUT,
                         NULL, AHM_RAMP_TIMEOUT_MS);
    }
}

void KymeraAncCommon_AdaptiveAncEnableGentleMute(void)
{
    /*No action for ANC Advanced as Gentle mute is now happening as ramp down in AHM*/
}

void KymeraAncCommon_EnterAdaptiveAncTuning(const adaptive_anc_tuning_connect_parameters_t *param)
{
    UNUSED(param);
}

void KymeraAncCommon_EnableAdaptivity(void)
{
    kymeraAncCommon_StartAdaptivity();
}

void KymeraAncCommon_DisableAdaptivity(void)
{
    kymeraAncCommon_FreezeAdaptivity();
}

void KymeraAncCommon_AdaptiveAncSetUcid(anc_mode_t mode)
{
    UNUSED(mode);
}

bool KymeraAncCommon_GetApdativeAncCurrentMode(adaptive_anc_mode_t *aanc_mode)
{
    UNUSED(aanc_mode);
    return FALSE;
}

bool KymeraAncCommon_GetApdativeAncV2CurrentMode(adaptive_ancv2_sysmode_t *aancv2_mode)
{
    PanicNull(aancv2_mode);
    return KymeraAdaptiveAnc_GetSysMode(aancv2_mode);
}

bool KymeraAncCommon_GetAhmMode(ahm_sysmode_t *ahm_mode)
{
    PanicNull(ahm_mode);
    return KymeraAhm_GetSysMode(ahm_mode);
}

bool KymeraAncCommon_AdaptiveAncIsNoiseLevelBelowQmThreshold(void)
{
    return KymeraAdaptiveAnc_IsNoiseLevelBelowQuietModeThreshold();
}

bool KymeraAncCommon_AdaptiveAncIsConcurrencyActive(void)
{
    return ((kymeraAncCommon_IsAancCurrentConfigValid() || (KymeraSelfSpeechDetect_IsActive())) && kymeraAncCommon_IsConcurrencyRequested());
}

void KymeraAncCommon_CreateAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *msg)
{
    UNUSED(msg);
}

void KymeraAncCommon_DestroyAdaptiveAncTuningChain(const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP_T *msg)
{
    UNUSED(msg);
}

void KymeraAncCommon_RampCompleteAction(void)
{
    DEBUG_LOG("KymeraAncCommon_RampCompleteAction!");

    if(ahmRampingCompleteCallback)
    {
        ahmRampingCompleteCallback();
        ahmRampingCompleteCallback = NULL;
    }
}

#ifdef ENABLE_ANC_FAST_MODE_SWITCH

static void kymeraAncCommon_EnableRxMix(void)
{
    AncSetRxMixEnables();
}

#endif /* ENABLE_ANC_FAST_MODE_SWITCH */

void KymeraAncCommon_AhmRampExpiryAction(void)
{
    DEBUG_LOG("KymeraAncCommon_AhmRampExpiryAction");
    if(KymeraAncCommon_AdaptiveAncIsEnabled())
    {
        DEBUG_LOG("Action, In the case of AHM Ramp completion message failed to reach appliation");
        kymeraAncCommon_DisableOnUserRequest();
    }
    /* Unregister the ANC disable callback because it has already been handled. */
    ahmRampingCompleteCallback = NULL;
}

void KymeraAncCommon_AncCompanderMakeupGainVolumeDown(void)
{
    KymeraAdaptiveAmbient_AncCompanderMakeupGainVolumeDown();
}

void KymeraAncCommon_AncCompanderMakeupGainVolumeUp(void)
{
    KymeraAdaptiveAmbient_AncCompanderMakeupGainVolumeUp();
}

bool KymeraAncCommon_UpdateAhmFfPathFineTargetGain(uint8 ff_fine_gain)
{
    bool status = FALSE;
    ahm_sysmode_t curr_ahm_mode;

    if (KymeraAhm_IsActive())
    {
        KymeraAncCommon_GetAhmMode(&curr_ahm_mode);

        if (curr_ahm_mode != ahm_sysmode_wind)
        {
            KymeraAhm_SetTargetGain((uint16)ff_fine_gain);
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
            if (!kymeraAncCommon_IsModeChangeInProgress())
#endif
            {
                /* AHM ramps to target gain only after setting it's sysmode to fullproc */
                KymeraAhm_SetSysMode(ahm_sysmode_full);
            }
            status = TRUE;
        }
    }

    return status;
}

bool KymeraAncCommon_UpdateAncCompanderMakeupGain(int32 makeup_gain_fixed_point)
{
    return KymeraAdaptiveAmbient_UpdateAncCompanderMakeupGain(makeup_gain_fixed_point);
}

bool KymeraAncCommon_GetAncCompanderMakeupQGain(int32* makeup_gain_fixed_point)
{
    return KymeraAdaptiveAmbient_GetCompanderGain(makeup_gain_fixed_point);
}

static uint16 kymeraAncCommon_GetTargetGain(void)
{
    uint16 target_gain=0;

    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            target_gain = KymeraAdaptiveAnc_GetFreezedGain();
        break;

        case aanc_config_adaptive_ambient:
            target_gain = KymeraAdaptiveAmbient_GetCompanderAdjustedGain();
        break;

        case aanc_config_static_anc:
            target_gain = KymeraAhm_GetStaticFeedForwardFineGain(data.mode_settings.hw_channel, data.mode_settings.control_path);
            break;

        case aanc_config_static_leakthrough:
            target_gain = AncStateManager_GetAncGain();/*The user configured gain*/
        break;

        default:
            break;
    }

    DEBUG_LOG("kymeraAncCommon_GetTargetGain %d", target_gain);
    return target_gain;
}

bool KymeraAncCommon_IsHowlingDetectionSupported(void)
{
    return TRUE;
}

bool KymeraAncCommon_IsHowlingDetectionEnabled(void)
{
    return KymeraHcgr_IsHowlingDetectionEnabled();
}

void KymeraAncCommon_UpdateHowlingDetectionState(bool enable)
{
    DEBUG_LOG("KymeraAncCommon_UpdateHowlingDetectionState");
    if(enable)
    {
        KymeraHcgr_UpdateUserState(howling_detection_enabled);
        KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_full);
    }
    else
    {
        KymeraHcgr_UpdateUserState(howling_detection_disabled);
        KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_standby);
    }
}

bool KymeraAncCommon_IsAancActive(void)
{
    switch(kymeraAncCommon_GetAancState())
    {
    case aanc_state_idle:
        return FALSE;

    case aanc_state_enable_initiated:
    case aanc_state_enabled:
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

#if defined(ENABLE_WIND_DETECT)

void KymeraAncCommon_EnableWindDetect(void)
{
    /*Always start with 1-mic mode*/
    KymeraWindDetect_SetSysMode1Mic();
}

void KymeraAncCommon_DisableWindDetect(void)
{
    KymeraWindDetect_SetSysModeStandby();
}

#ifdef ENABLE_ANC_FAST_MODE_SWITCH
static bool kymeraAncCommon_IsWindConfirmed(void)
{
    return (data.wind_confirmed);
}
#endif

static void kymeraAncCommon_SetWindConfirmed(void)
{
    data.wind_confirmed = TRUE;
}

static void kymeraAncCommon_ResetWindConfirmed(void)
{
    data.wind_confirmed = FALSE;
}

static bool kymeraAncCommon_IsWindDetectInStage2(void)
{
    return (data.wind_detect_in_stage2);
}

static void kymeraAncCommon_SetWindDetectInStage2(void)
{
    if (appConfigWindDetect2MicSupported())
    {
        data.wind_detect_in_stage2=TRUE;
    }
}

static void kymeraAncCommon_ResetWindDetectInStage2(void)
{
    kymeraAncCommon_ResetWindConfirmed(); /* AHM sysmode would have been set to full proc if Wind is released in stage-2 */
    data.wind_detect_in_stage2 = FALSE;
}


static void kymeraAncCommon_RampGainsToTarget(kymeraAncCommonCallback func)
{
    DEBUG_LOG("RampGainsToTarget");
    ahmRampingCompleteCallback = func;
    KymeraAhm_SetSysMode(ahm_sysmode_full);
}

static void kymeraAncCommon_WindConfirmed(void)
{
    DEBUG_LOG("ahm_sysmode_wind");
    kymeraAncCommon_SetWindConfirmed();
    KymeraAhm_SetSysMode(ahm_sysmode_wind);
}


static void kymeraAncCommon_StartOnWindRelease(void)
{
    DEBUG_LOG("kymeraAncCommon_StartOnWindRelease");
    switch(kymeraAncCommon_GetAancCurrentConfig())
    {
        case aanc_config_adaptive_anc:
            KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain());
            kymeraAncCommon_RampGainsToTarget(kymeraAncCommon_StartAdaptivity);
            KymeraHcgr_Start();
            kymeraAncCommon_AhmStart(); /* Start ramping to target gains */
            KymeraWindDetect_Start();
            KymeraAdaptiveAnc_StartChain();
            KymeraEchoCanceller_Start();
            KymeraAah_Start();
        break;

        case aanc_config_adaptive_ambient:
            KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain());
            kymeraAncCommon_RampGainsToTarget(kymeraAncCommon_StartAdaptivity);
            kymeraAncCommon_AhmStart(); /* Start ramping to target gains */
            KymeraWindDetect_Start();
            KymeraAdaptiveAmbient_StartChain();
            KymeraAah_Start();
            break;

        case aanc_config_static_anc:
            KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain());
            kymeraAncCommon_RampGainsToTarget(kymeraAncCommon_StartAdaptivity);
            KymeraHcgr_Start();
            kymeraAncCommon_AhmStart(); /* Start ramping to target gains */
            KymeraWindDetect_Start();
            KymeraEchoCanceller_Start();
            KymeraAah_Start();
        break;

        case aanc_config_static_leakthrough:
            KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain());
            kymeraAncCommon_RampGainsToTarget(NULL);
            kymeraAncCommon_AhmStart(); /* Start ramping to target gains */
            KymeraWindDetect_Start();
            KymeraAah_Start();
            break;

        default:
            break;
    }
}

/*Disconnect diversity mic*/
static void kymeraAncCommon_MoveToStage1(void)
{
    if (appConfigWindDetect2MicSupported())
    {
        kymeraAncCommon_AncMicDisconnect();

        /*Connect mic with new configuration*/
        kymeraAncCommon_ResetWindDetectInStage2();
        if (!Kymera_MicConnect(mic_user_aanc))
        {
            DEBUG_LOG("MicConnect failure on kymeraAncCommon_MoveToStage1");
        }
        else
        {
            kymeraAncCommon_Connect();
            KymeraWindDetect_SetSysMode1Mic();
            kymeraAncCommon_StartOnWindRelease();
        }
    }
}

/*Connect diversity mic*/
static void kymeraAncCommon_MoveToStage2(void)
{
    DEBUG_LOG("kymeraAncCommon_MoveToStage2");
    if (appConfigWindDetect2MicSupported())
    {
        kymeraAncCommon_AncMicDisconnect();

        /*Connect mic with new configuration*/
        kymeraAncCommon_SetWindDetectInStage2();
        if (!Kymera_MicConnect(mic_user_aanc))
        {
            DEBUG_LOG("MicConnect failure on kymeraAncCommon_MoveToStage2");
        }
        else
        {
            kymeraAncCommon_Connect();
            KymeraWindDetect_SetSysMode2Mic();
            kymeraAncCommon_Start();
        }
    }
}


/*Freeze AANC, disconnect mics, connect new mic and put wind detect in 2-mic mode*/
void KymeraAncCommon_WindDetectAttack(windDetectStatus_t attack_stage)
{
    DEBUG_LOG("KymeraAncCommon_WindDetectAttack attack_stage enum:windDetectStatus_t:%d", attack_stage);

    if (appConfigWindDetect2MicSupported())
    {
        if (attack_stage==stage1_wind_detected)
        {
            /*Move to stage 2: Ignore if already in stage 2*/
            if (!kymeraAncCommon_IsWindDetectInStage2())
            {
                kymeraAncCommon_MoveToStage2();
            }
        }
        else if (attack_stage==stage2_wind_detected)
        {
            /*Move AHM to windy, confirm if in stage 2*/
            if (kymeraAncCommon_IsWindDetectInStage2())
            {
                kymeraAncCommon_WindConfirmed();
            }
        }
    }
    else
    {
        /*Product supports only stage 1 wind*/
        if (attack_stage==stage1_wind_detected)
        {
            kymeraAncCommon_FreezeAdaptivity();
            kymeraAncCommon_WindConfirmed();
        }
    }
}

void KymeraAncCommon_WindDetectRelease(windDetectStatus_t release_stage)
{
    DEBUG_LOG("KymeraAncCommon_WindDetectRelease release_stage enum:windDetectStatus_t:%d", release_stage);

    if (appConfigWindDetect2MicSupported())
    {
        if ((release_stage==stage2_wind_released) || (release_stage==stage1_wind_released))
        {
            /*Move to stage 1*/
            if (kymeraAncCommon_IsWindDetectInStage2())
            {
                kymeraAncCommon_MoveToStage1();
            }
        }
    }
    else
    {
        /*Product supports only stage 1 wind*/
        if (release_stage==stage1_wind_released)
        {
            /*Move AHM to Full*/
            KymeraAhm_SetTargetGain(kymeraAncCommon_GetTargetGain());
            kymeraAncCommon_RampGainsToTarget(kymeraAncCommon_StartAdaptivity);

            /*Reset wind detect flag*/
            kymeraAncCommon_ResetWindDetectInStage2();
        }
    }
}
#endif

#if defined(ENABLE_SELF_SPEECH) && defined (ENABLE_AUTO_AMBIENT)

#define SELF_SPEECH_RETRY_MS (2000)

static bool kymeraAncCommon_IsVoiceConcurrencyActive(void)
{
    bool status = FALSE;
    
    if (((Kymera_OutputGetConnectedUsers() & output_user_sco)==output_user_sco) || Kymera_IsVaActive()
            || (data.connecting_user==output_user_sco) || (data.connecting_user==output_user_le_srec))
    {
        status = TRUE;
    }
    return status;
}

static bool kymeraAncCommon_IsModeChangeInProgress(void)
{
    return data.mode_change_in_progress;
}

static void kymeraAncCommon_SelfSpeechDetect_MicReConnect(void)
{
    if (KymeraSelfSpeechDetect_IsActive() && !kymeraAncCommon_isTransitionInProgress())
    {
        if (!Kymera_MicConnect(mic_user_aanc))
        {
            DEBUG_LOG_ERROR("KymeraAncCommon_SelfSpeechDetect_MicReConnect: Mic connection was not successful..");
            Panic();
        }
        KymeraSelfSpeechDetect_Connect();
        KymeraSelfSpeechDetect_Start();
    }
}

static void kymeraAncCommon_SelfSpeechDetect_MicDisconnect(void)
{
    if (KymeraSelfSpeechDetect_IsActive() && !kymeraAncCommon_isTransitionInProgress())        
        /*Since Mode change scenarios Disconnect and Connect mics, can optimise here for Mode change scenarios*/
    {
        KymeraSelfSpeechDetect_Stop();
        KymeraSelfSpeechDetect_Disconnect();
        Kymera_MicDisconnect(mic_user_aanc);
    }
}

static void kymeraAncCommon_AncMicReConnect(void)
{
    if (KymeraAncCommon_AdaptiveAncIsEnabled() && !kymeraAncCommon_isTransitionInProgress())
    {
        if (!Kymera_MicConnect(mic_user_aanc))
        {
            DEBUG_LOG_ERROR("kymeraAncCommon_AncMicReConnect: Mic connection was not successful..");
            Panic();
        }
        
        kymeraAncCommon_Connect();
        kymeraAncCommon_Start();
    }
}

void KymeraAncCommon_SelfSpeechDetectEnable(void)
{
    DEBUG_LOG("KymeraAncCommon_SelfSpeechDetectEnable");

    if (!KymeraSelfSpeechDetect_IsActive())
    {
        /*Message Cancel earlier Enable/Disable messages*/            
        MessageCancelAll(AncStateManager_GetTask(), KYMERA_INTERNAL_SELF_SPEECH_ENABLE_TIMEOUT);
        MessageCancelAll(AncStateManager_GetTask(), KYMERA_INTERNAL_SELF_SPEECH_DISABLE_TIMEOUT);

        if (kymeraAncCommon_isTransitionInProgress() ||
            kymeraAncCommon_IsVoiceConcurrencyActive())
        {
            MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_SELF_SPEECH_ENABLE_TIMEOUT,
                         NULL, SELF_SPEECH_RETRY_MS);
            return;
        }
        
        /*If ANC is active, Disconnect Mic*/
        kymeraAncCommon_AncMicDisconnect();

        KymeraSelfSpeechDetect_Create();
        KymeraSelfSpeechDetect_Configure();
        KymeraSelfSpeechDetect_SetSysMode(atr_vad_sysmode_1mic_detection);

        if(Kymera_OutputIsChainInUse())
        {
            kymeraAncCommon_UpdateConcurrencyRequest(TRUE);
        }
        
        appKymeraConfigureDspPowerMode();
        
        /*If ANC is active, Connect and Start ANC operators*/
        if (KymeraAncCommon_AdaptiveAncIsEnabled())
        {        
            /*If ANC is active, Connect both ANC and Self speech Mics*/
            Kymera_MicConnect(mic_user_aanc);
            kymeraAncCommon_Connect();
            kymeraAncCommon_Start();
        }
        else
        {
            kymeraAncCommon_SelfSpeechDetect_MicReConnect();
        }
    }
    else
    {
        DEBUG_LOG("Self Speech graph Already active");
    }
}

void KymeraAncCommon_SelfSpeechDetectDisable(void)
{
    DEBUG_LOG("KymeraSelfSpeechDetect_Disable");

    if (KymeraSelfSpeechDetect_IsActive())
    {
        /*Message Cancel earlier Enable/Disable messages*/
        MessageCancelAll(AncStateManager_GetTask(), KYMERA_INTERNAL_SELF_SPEECH_ENABLE_TIMEOUT);
        MessageCancelAll(AncStateManager_GetTask(), KYMERA_INTERNAL_SELF_SPEECH_DISABLE_TIMEOUT);

        if (kymeraAncCommon_isTransitionInProgress() ||
            kymeraAncCommon_IsVoiceConcurrencyActive())
        {
            MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_SELF_SPEECH_DISABLE_TIMEOUT,
                         NULL, SELF_SPEECH_RETRY_MS);
            return;
        }

        if (KymeraAncCommon_AdaptiveAncIsEnabled())
        {
            /*Stop ANC operators if ANC is ON*/
            kymeraAncCommon_AncStopOperators();
            Kymera_MicDisconnect(mic_user_aanc);
        }
        else
        {        
            /*Stop and Disconnect Self Speech*/
            kymeraAncCommon_SelfSpeechDetect_MicDisconnect();
        }
        
        /*Destroy Self Speech*/
        KymeraSelfSpeechDetect_Destroy();
        
        /*If ANC is active, ReConnect Mic*/
        kymeraAncCommon_AncMicReConnect();
    }
    else
    {
        DEBUG_LOG("No active Self Speech graph");
    }
}
#elif defined ENABLE_ANC_FAST_MODE_SWITCH

static bool kymeraAncCommon_IsModeChangeInProgress(void)
{
    return data.mode_change_in_progress;
}

#endif

void KymeraAncCommon_TransitionCompleteAction(void)
{
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
    DEBUG_LOG("KymeraAncCommon_TransitionCompleteAction!");

    /* Cancel any pending AHM Transition timeout messages */
    MessageCancelAll(kymeraAncCommon_GetTask(), KYMERA_ANC_COMMON_INTERNAL_AHM_TRANSITION_TIMEOUT);

#if defined(ENABLE_WIND_DETECT)
    if (!kymeraAncCommon_IsWindConfirmed())
#endif
    {
        KymeraAhm_SetSysMode(ahm_sysmode_full);
    }
#if defined(ENABLE_WIND_DETECT)
    else
    {
        KymeraAhm_SetSysMode(ahm_sysmode_wind);
    }
#endif /* ENABLE_WIND_DETECT */

    /* When Fast Mode switch is enabled, Mode change gets completed on receiving Transition complete message. */
    kymeraAncCommon_SetModeChangeInProgress(FALSE);

#endif /* ENABLE_ANC_FAST_MODE_SWITCH */
}

void KymeraAncCommon_SetFilterCoefficients(void)
{
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
    DEBUG_LOG("KymeraAncCommon_SetFilterCoefficients!");

    KymeraAncCommon_SetFilterCoefficientsForInstance(AUDIO_ANC_INSTANCE_0);
    if (appKymeraIsParallelAncFilterEnabled())
    {
        KymeraAncCommon_SetFilterCoefficientsForInstance(AUDIO_ANC_INSTANCE_1);
    }
#endif /* ENABLE_ANC_FAST_MODE_SWITCH */
}

static void KymeraAncCommon_StandaloneToConcurrencyReq(void)
{
    DEBUG_LOG("KymeraAncCommon_StandaloneToConcurrencyReq");
    AncStateManager_StandaloneToConcurrencyReq();
}

static void KymeraAncCommon_ConcurrencyToStandaloneReq(void)
{
    DEBUG_LOG("KymeraAncCommon_ConcurrencyToStandaloneReq");
    AncStateManager_ConcurrencyToStandaloneReq();
}

void KymeraAncCommon_HandleConcurrencyUpdate(bool is_concurrency_active)
{
    DEBUG_LOG_INFO("KymeraAncCommon_HandleConcurrencyUpdate");
    if(is_concurrency_active)
    {
        if(((kymeraAncCommon_GetAancCurrentConfig() == aanc_config_adaptive_anc) || (kymeraAncCommon_GetAancCurrentConfig() == aanc_config_static_anc))
                && !kymeraAncCommon_IsConcurrencyChainActive() && Kymera_OutputIsChainInUse())
        {
            kymeraAncCommon_UpdateConcurrencyRequest(TRUE);
            kymeraAncCommon_StandaloneToConcurrencyChain();
        }
    }
    else
    {
        if(((kymeraAncCommon_GetAancCurrentConfig() == aanc_config_adaptive_anc) || (kymeraAncCommon_GetAancCurrentConfig() == aanc_config_static_anc))
                && kymeraAncCommon_IsConcurrencyChainActive())
        {
            kymeraAncCommon_UpdateConcurrencyRequest(FALSE);
            kymeraAncCommon_ConcurrencyToStandaloneChain();
        }
    }
}

#endif /* ENABLE_ADAPTIVE_ANC */

