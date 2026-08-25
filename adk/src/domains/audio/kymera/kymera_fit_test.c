/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_fit_test.c
\brief      Kymera Earbud fit test business logic
*/

#include "kymera_fit_test.h"
#include "kymera_dsp_clock.h"
#include "kymera_anc_common.h"
#include "kymera_config.h"
#include "kymera_aec.h"
#include "kymera_mic_if.h"
#include "kymera_output_if.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "kymera.h"
#include "microphones.h"
#include "phy_state.h"
#include <vmal.h>
#include <file.h>
#include <stdlib.h>
#include <cap_id_prim.h>

#include "fit_test.h"

#if defined(ENABLE_EARBUD_FIT_TEST)
#define MAX_CHAIN (2U)
static kymera_chain_handle_t fit_test_chains[MAX_CHAIN] = {0};
#define CHAIN_FIT_TEST_MIC_PATH (MAX_CHAIN-1)
#define CHAIN_FIT_TEST_SPK_PATH (CHAIN_FIT_TEST_MIC_PATH-1)

#define MAX_FIT_TEST_MICS (1U)
#define FIT_TEST_MIC_PATH_SAMPLE_RATE (16000U)
#define PROMPT_INTERRUPTIBLE (1U)

/* Fit test statistics */
#define NUM_STATUS_VAR              (9U)
#define CUR_MODE_OFFSET             (0U)
#define OVR_CTRL_OFFSET             (1U)
#define IN_OUT_EAR_CTRL_OFFSET      (2U)
#define FIT_QUALITY_OFFSET          (3U)
#define FIT_QUALITY_EVENT_OFFSET    (4U)
#define FIT_QUALITY_TIMER_OFFSET    (5U)
#define POWER_PLAYBACK_OFFSET       (6U)
#define POWER_INT_MIC_OFFSET        (7U)
#define POWER_RATIO_OFFSET          (8U)
#define FIT_TEST_OUTPUT_RATE        (48000U)

#define kymeraFitTest_IsFitTestMicPathActive() (kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH) != NULL)

typedef enum
{
    KYMERA_FIT_TEST_INTERNAL_START_TEST,
    KYMERA_FIT_TEST_INTERNAL_ENABLE_AANC
} kymera_fit_test_internal_message_ids;

static const char prompt_filename[]   = "fit_test.sbc";

static void kymeraFitTest_HandleMessage(Task task, MessageId id, Message message);
static const TaskData kymera_fit_test_task = { .handler=kymeraFitTest_HandleMessage };

#define kymeraFitTest_GetTask() (Task)&kymera_fit_test_task

/* Mic interface callback functions */
static bool kymeraFitTest_MicGetConnectionParameters(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink);
static bool kymeraFitTest_MicDisconnectIndication(const mic_change_info_t *info);
static void kymeraFitTest_MicReconnectedIndication(void);

static kymera_chain_handle_t kymeraFitTest_GetChain(uint8 index);
static void kymeraFitTest_StartEBFitTestMicPathChain(void);
static void kymeraFitTest_StopEBFitTestMicPathChain(void);

static const mic_callbacks_t kymera_FitTestCallbacks =
{
    .MicGetConnectionParameters = kymeraFitTest_MicGetConnectionParameters,
    .MicDisconnectIndication = kymeraFitTest_MicDisconnectIndication,
    .MicReconnectedIndication = kymeraFitTest_MicReconnectedIndication,
};

static const mic_user_state_t kymera_FitTestUserState =
{
    mic_user_state_interruptible,
};
static const bool kymera_FitTestUseAecRef = TRUE;

static const mic_registry_per_user_t kymera_FitTestRegistry =
{
    .user = mic_user_fit_test,
    .callbacks = &kymera_FitTestCallbacks,
    .mandatory_mic_ids = NULL,
    .num_of_mandatory_mics = 0,
    .mic_user_state = &kymera_FitTestUserState,
    .use_aec_ref = &kymera_FitTestUseAecRef,
};


static FILE_INDEX fitTest_prompt;

static void kymeraFitTest_EnableEftMicClient(void);

static void kymeraFitTest_StartTest(void)
{
    /* Enable Mic path audio graph */
    kymeraFitTest_EnableEftMicClient();
}

static void kymeraFitTest_EnableAanc(void)
{
    DEBUG_LOG_ALWAYS("kymeraFitTest_EnableAanc");
    KYMERA_INTERNAL_AANC_ENABLE_T msg;

    /* Adaptive ANC enable */
    msg.in_ear = (FitTest_IsTuningModeActive()) ? TRUE: (appPhyStateGetState()==PHY_STATE_IN_EAR);

    msg.control_path = AUDIO_ANC_PATH_ID_FFB; /* hardcoded to hybrid mode */
    msg.hw_channel = adaptive_anc_hw_channel_0;
    msg.current_mode = anc_mode_fit_test;

    KymeraAncCommon_AncEnable(&msg);
    KymeraAncCommon_AdaptiveAncSetUcid(anc_mode_fit_test);
    KymeraAncCommon_AdaptiveAncEnableGentleMute();
}

static void kymeraFitTest_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch(id)
    {
    case KYMERA_FIT_TEST_INTERNAL_START_TEST:
        kymeraFitTest_StartTest();
        break;

    case KYMERA_FIT_TEST_INTERNAL_ENABLE_AANC:
        kymeraFitTest_EnableAanc();
        break;

    default:
        break;
    }
}

/*! For a reconnection the mic parameters are sent to the mic interface.
 *  return TRUE to reconnect with the given parameters
 */
static bool kymeraFitTest_MicGetConnectionParameters(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint32 *sample_rate, Sink *aec_ref_sink)
{
    *sample_rate = FIT_TEST_MIC_PATH_SAMPLE_RATE;
    *num_of_mics = MAX_FIT_TEST_MICS;
    mic_ids[0] = appConfigAncFeedBackMic();

    DEBUG_LOG("kymeraFitTest_MicGetConnectionParameters");

    mic_sinks[0] = ChainGetInput(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH), EPR_FIT_TEST_INT_MIC_IN);
    aec_ref_sink[0] = ChainGetInput(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH), EPR_FIT_TEST_PLAYBACK_IN);

    return TRUE;
}

/*! Before the microphones are disconnected, all users get informed with a DisconnectIndication.
 * return FALSE: accept disconnection
 * return TRUE: Try to reconnect the microphones. This will trigger a kymeraFitTest_MicGetConnectionParameters
 */
static bool kymeraFitTest_MicDisconnectIndication(const mic_change_info_t *info)
{
    DEBUG_LOG("kymeraFitTest_MicDisconnectIndication user %d, event %d",info->user, info->event);
    UNUSED(info);

    /* Stop only EFT graph.
     * AANC graph will be stopped by AANC domain.
     * Required only for AEC_REF to disconnect operators safely */

    kymeraFitTest_StopEBFitTestMicPathChain();

    return TRUE;
}


/*! This indication is sent if the microphones have been reconnected after a DisconnectIndication.
 */
static void kymeraFitTest_MicReconnectedIndication(void)
{
    DEBUG_LOG("kymeraFitTest_MicReconnectedIndication");

    /* Restart the EFT graph which was stopped earlier in the kymeraFitTest_MicDisconnectIndication
     * AANC graph wil be restarted by AANC */

    kymeraFitTest_StartEBFitTestMicPathChain();
}


/*!
 *  Init function for KymeraFitTest
 *  FileIndex for fit test made available.
 *  Registers AANC callbacks in the mic interface layer
 */
void KymeraFitTest_Init(void)
{
    fitTest_prompt = FileFind (FILE_ROOT, prompt_filename,strlen(prompt_filename));
    Kymera_MicRegisterUser(&kymera_FitTestRegistry);
}

/************* Fit test audio graphs ****************/

static kymera_chain_handle_t kymeraFitTest_GetChain(uint8 index)
{
    return ((index < MAX_CHAIN) ? fit_test_chains[index] : NULL);
}

static void kymeraFitTest_SetChain(uint8 index, kymera_chain_handle_t chain)
{
    if(index < MAX_CHAIN)
        fit_test_chains[index] = chain;
}

static void kymeraFitTest_ConfigureEBFitTestMicPathChain(bool in_ear)
{
    Operator op = ChainGetOperatorByRole(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH), OPR_FIT_TEST);
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if(op)
    {
        OperatorsEarbudFitTestSetInEarCtrl(op, in_ear);
        OperatorsStandardSetUCID(op, UCID_EFT);
        /* Regsiter a listener with the AANC*/
        MessageOperatorTask(op, &theKymera->task);
    }

    Operator op_pt = ChainGetOperatorByRole(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH), OPR_FIT_TEST_BASIC_PT);

    if(op_pt)
    {
        OperatorsSetPassthroughDataFormat(op_pt, operator_data_format_pcm);
        OperatorsSetPassthroughGain(op_pt, 0U); //0dB gain
    }
}

static void kymeraFitTest_CreateEBFitTestMicPathChain(void)
{
    DEBUG_LOG("kymeraFitTest_CreateEBFitTestMicPathChain");
    PanicNotNull(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH));
    kymeraFitTest_SetChain(CHAIN_FIT_TEST_MIC_PATH, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_fit_test_mic_path_config)));

    kymeraFitTest_ConfigureEBFitTestMicPathChain(TRUE); /* TODO: must be updated depending on EB physical state */
    ChainConnect(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH));
}

static void kymeraFitTest_DestroyEBFitTestMicPathChain(void)
{
    DEBUG_LOG("kymeraFitTest_DestroyEBFitTestMicPathChain");
    PanicNull(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH));
    ChainDestroy(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH));
    kymeraFitTest_SetChain(CHAIN_FIT_TEST_MIC_PATH, NULL);
}

static void kymeraFitTest_StartEBFitTestMicPathChain(void)
{
    if(kymeraFitTest_IsFitTestMicPathActive())
    {
        DEBUG_LOG("kymeraFitTest_StartEBFitTestMicPathChain");
        ChainStart(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH));
    }
}

static void kymeraFitTest_StopEBFitTestMicPathChain(void)
{
    if(kymeraFitTest_IsFitTestMicPathActive())
    {
        DEBUG_LOG("kymeraFitTest_StopEBFitTestMicPathChain");
        ChainStop(kymeraFitTest_GetChain(CHAIN_FIT_TEST_MIC_PATH));
    }
}

static void kymeraFitTest_ConfigureDspClock(void)
{
    /* Enable operator framwork before updating DSP clock */
    OperatorsFrameworkEnable();
    appKymeraBoostDspClockToMax();
}

static void kymeraFitTest_ResetDspPowerMode(void)
{
    /*Revert to low power mode (if applicable) at the end of Fit Test*/
    appKymeraConfigureDspPowerMode();
    /*Disable operator framwork after reverting DSP config*/
    OperatorsFrameworkDisable();
}

static void kymeraFitTest_EnableEftMicClient(void)
{
    DEBUG_LOG("kymeraFitTest_EnableEftMicClient");

    kymeraFitTest_CreateEBFitTestMicPathChain();
    if (Kymera_MicConnect(mic_user_fit_test))
    {
        kymeraFitTest_StartEBFitTestMicPathChain();
    }
    else
    {
        DEBUG_LOG_ERROR("kymeraFitTest_EnableEftMicClient: Fit Test Mic connection was not successful.");
        Panic();
    }
}

static void kymeraFitTest_DisableEftMicClient(void)
{
    DEBUG_LOG("kymeraFitTest_DisableEftMicClient");
    if (kymeraFitTest_IsFitTestMicPathActive())
    {
        kymeraFitTest_StopEBFitTestMicPathChain();
        kymeraFitTest_DestroyEBFitTestMicPathChain();
        Kymera_MicDisconnect(mic_user_fit_test);
    }
}

static kymera_chain_handle_t kymeraFitTest_GetTonePromptChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    return theKymera->chain_tone_handle;
}

static void kymeraFitTest_SetupPromptSource(Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageStreamTaskFromSource(source, &theKymera->task);
}

static void kymeraFitTest_ClosePromptSource(Source source)
{
    if (source)
    {
        MessageStreamTaskFromSource(source, NULL);
        StreamDisconnect(source, NULL);
        SourceClose(source);
    }
}

static void kymeraFitTest_StartPromptSource(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    theKymera->prompt_source = PanicNull(StreamFileSource(fitTest_prompt));
    kymeraFitTest_SetupPromptSource(theKymera->prompt_source);
    PanicFalse(ChainConnectInput(kymeraFitTest_GetTonePromptChain(), theKymera->prompt_source, EPR_PROMPT_IN));
    KymeraOutput_SetAuxVolume(KYMERA_CONFIG_PROMPT_VOLUME);
}

static void kymeraFitTest_StopPromptSource(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    KymeraOutput_SetAuxVolume(0);
    if (theKymera->prompt_source)
    {
        kymeraFitTest_ClosePromptSource(theKymera->prompt_source);
        theKymera->prompt_source = NULL;
    }
}

void KymeraFitTest_Start(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    kymeraFitTest_ConfigureDspClock();

    MessageSendConditionally(kymeraFitTest_GetTask(), KYMERA_FIT_TEST_INTERNAL_START_TEST,
                                NULL, &theKymera->lock);
}

void KymeraFitTest_CancelPrompt(void)
{
    appKymeraTonePromptCancel();
}

void KymeraFitTest_Stop(void)
{
    DEBUG_LOG("KymeraFitTest_Stop");

    /* If fit test domain attemped to start EFT graph, disable the graph and revert DSP clock. */
    if (MessageCancelAll(kymeraFitTest_GetTask(), KYMERA_FIT_TEST_INTERNAL_START_TEST) ||
            kymeraFitTest_IsFitTestMicPathActive())
    {
        kymeraFitTest_DisableEftMicClient();
        kymeraFitTest_ResetDspPowerMode();
    }
}

FILE_INDEX KymeraFitTest_GetPromptIndex(void)
{
    return fitTest_prompt;
}

bool KymeraFitTest_PromptReplayRequired(void)
{
    return FitTest_PromptReplayRequired();
}

void KymeraFitTest_ReplayPrompt(void)
{
    kymeraFitTest_StopPromptSource();
    kymeraFitTest_StartPromptSource();
}

bool KymeraFitTest_IsTuningModeActive(void)
{
    return FitTest_IsTuningModeActive();
}

void KymeraFitTest_EnableAanc(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MessageSendConditionally(kymeraFitTest_GetTask(), KYMERA_FIT_TEST_INTERNAL_ENABLE_AANC,
                                NULL, &theKymera->lock);
}

void KymeraFitTest_DisableAanc(void)
{
    DEBUG_LOG("KymeraFitTest_DisableAanc");

    /* If AANC is enabled for EFT use case, disable it. */
    if (!MessageCancelAll(kymeraFitTest_GetTask(), KYMERA_FIT_TEST_INTERNAL_ENABLE_AANC) &&
            KymeraAncCommon_AdaptiveAncIsEnabled())
    {
        KymeraAncCommon_AdaptiveAncDisable();
    }
}

void KymeraFitTest_StartPrompt(void)
{
    DEBUG_LOG("KymeraFitTest_StartPrompt");

    /* Enable Speaker path audio graph */
    appKymeraPromptPlay(fitTest_prompt, PROMPT_FORMAT_SBC, FIT_TEST_OUTPUT_RATE, 0,
                        PROMPT_INTERRUPTIBLE, 0, 0);
}

#endif /* ENABLE_EARBUD_FIT_TEST */
