/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       fit_test.c
\brief      Fit test implementation.
*/
#include "fit_test.h"
#include "kymera.h"
#include "kymera_fit_test.h"
#include "kymera_anc_common.h"
#include "kymera_output_if.h"
#include <hydra_macros.h>
#include <message.h>
#include <logging.h>
#include "anc_state_manager.h"
#include "ui.h"
#include "multidevice.h"
#include "phy_state.h"
#include "microphones.h"
#include "feature_manager.h"

typedef enum
{
    fit_test_event_initialise,
    fit_test_event_make_test_ready,
    fit_test_event_start,
    fit_test_event_complete,
    fit_test_event_abort,
    fit_test_event_disable,
    fit_test_event_activate_tuning_mode,
    fit_test_event_deactivate_tuning_mode,
    fit_test_event_suspend_feature,
    fit_test_event_resume_feature
}fit_test_event_t;

/*! \brief Different state handled in the fit_test sub domain.
*    Flow diagram:
*    -----------------------------------------------
*    Current state | Event              | Next State
*    -----------------------------------------------
*    uninit        | init               | Ready
*    Ready         | start_test         | Running
*    Running       | complete           | Ready
*    Running       | abort              | Disabled
*    Disabled      | prepare_test       | Ready
*    Ready         | disable            | Disabled
*    Ready         | activate_tuning    | Tuning
*    Tuning        | deactivate_tuning  | Ready
*    Tuning        | abort              | Disabled
*    Ready         | suspend_feature    | Disabled
*    Running       | suspend_feature    | Disabled
*    Tuning        | suspend_feature    | Disabled
*    Disabled      | suspend_feature    | Disabled
*    Disabled      | resume_feature     | Ready
*    -----------------------------------------------
*/
typedef enum
{
    fit_test_state_uninitialised,
    fit_test_state_ready,
    fit_test_state_running,
    fit_test_state_disabled,
    fit_test_state_tuning
}fit_test_states_t;

typedef enum
{
    fit_test_tuning_mode,
    fit_test_run_mode
}fit_test_modes_t;

#ifdef ENABLE_EARBUD_FIT_TEST
typedef struct
{
    TaskData task;
    fit_test_states_t fit_test_current_state;
    fit_test_states_t fit_test_previous_state;
    fit_test_result_t local_result;/* 1 - Good fit and environment */
    fit_test_result_t remote_result;/* 1 - Good fit and environment */
    bool anc_state_prev;      /* Place holder to store ANC state before fit test starts */
    anc_mode_t anc_mode_prev; /* Place holder to store ANC mode before fit test starts */
    uint16 prompt_count;
    task_list_t client_tasks; /* List of tasks registered for notifications */
    bool feature_state;
    uint16 anc_hw_enable:1;  /* To idetify if ANC HW is enabled for EFT use case */
    uint16 unused:15;
}fitTestTaskData;

static fitTestTaskData fit_test_task_data;

#define fitTestGetTaskData()  (&fit_test_task_data)
#define fitTestGetTask()      (&fit_test_task_data.task)
#define FIT_TEST_PROMPTS_COUNT (1U)
#define ANC_SAMPLE_RATE (16000U)

#define SUSPENDED   TRUE
#define RESUMED     !SUSPENDED

typedef struct
{
    unsigned eft_good_fit:1; /* 0-Bad fit(default), 1-Good fit */
    unsigned aanc_bad_env:1; /* 0-Good environment(default), 1-Bad environment */
    unsigned unused:14;
}fit_test_flags_t;


static fit_test_flags_t flags;
static feature_manager_handle_t fitTest_feature_manager_handle = NULL;

static void fitTest_msgHandler(Task task, MessageId id, Message message);
static bool fitTest_SetupTest(void);
static bool fitTestStateUnitialisedHandleEvent(fit_test_event_t event);
static bool fitTestStateReadyHandleEvent(fit_test_event_t event);
static bool fitTestStateDisabledHandleEvent(fit_test_event_t event);
static bool fitTestStateRunningHandleEvent(fit_test_event_t event);

static bool fitTest_Start(fit_test_modes_t fit_test_mode);
static bool fitTest_Stop(void);
static bool fitTest_Abort(void);
static bool fitTest_Disable(void);
static void fitTest_ChangeState(fit_test_states_t nextstate);
static void fitTest_UpdateFinalResult(MessageId id);
static void fitTest_HandlePromptEndInd(void);
static bool fitTest_HandleEvent(fit_test_event_t event);
static fit_test_states_t fitTest_GetCurrentState(void);
static fit_test_states_t fitTest_GetPreviousState(void);
static void fitTest_MessageRegisteredClients(MessageId id, uint8 left_eb_result, uint8 right_eb_result);
static void fitTest_EnableAncWithFitTestMode(void);
static void fitTest_DisableAncForFitTestMode(void);
static void fitTest_OutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type);
static void fitTest_OutputDisconnectingIndication(output_users_t disconnected_user, output_connection_t connection_type);

static bool fitTest_EnterTuning(void);
static bool fitTest_ExitTuning(fit_test_states_t next_state);

static bool fitTest_Resume(void);
static bool fitTest_Suspend(void);
static bool fitTest_StopTestAndSuspend(fit_test_modes_t fit_test_mode);
static feature_state_t fitTest_GetFeatureState(void);
static void fitTest_SuspendFitTestFeature(void);
static void fitTest_ResumeFitTestFeature(void);

static void fitTest_StartTest(void);

static void fitTest_resetStoredAncData(void);

static void fitTest_SetAncHwEnable(bool enable);
static bool fitTest_IsAncHwEnabled(void);

/*Registering Callback with Output manager to configure Fit test during concurrency*/
static const output_indications_registry_entry_t fitTest_IndicationCallbacks =
{
    .OutputConnectingIndication = fitTest_OutputConnectingIndication,
    .OutputDisconnectedIndication = fitTest_OutputDisconnectingIndication,
};

/*Registering Callback with feature manager to suspend/resume Fit test*/
static const feature_interface_t fitTest_feature_manager_if =
{
    .GetState = fitTest_GetFeatureState,
    .Suspend = fitTest_SuspendFitTestFeature,
    .Resume = fitTest_ResumeFitTestFeature
};

static feature_manager_handle_t fitTest_GetFeatureManagerHandle(void)
{
    return fitTest_feature_manager_handle;
}

static void fitTest_SetFeatureManagerHandle(feature_manager_handle_t handle)
{
    fitTest_feature_manager_handle = handle;
}

static bool fitTest_IsSuspended(void)
{
    return fitTestGetTaskData()->feature_state;
}

static void fitTest_UpdateFeatureState(bool state)
{
    fitTestGetTaskData()->feature_state = state;
}

static feature_state_t fitTest_GetFeatureState(void)
{
    feature_state_t state = feature_state_idle;

    if(FitTest_IsRunning() || FitTest_IsTuningModeActive())
    {
        state = feature_state_running;
    }
    else if(fitTest_IsSuspended())
    {
        state = feature_state_suspended;
    }

    return state;
}

static void fitTest_SuspendFitTestFeature(void)
{
    DEBUG_LOG("fitTest_SuspendFitTestFeature");
    fitTest_HandleEvent(fit_test_event_suspend_feature);
}

static void fitTest_ResumeFitTestFeature(void)
{
    DEBUG_LOG("fitTest_ResumeFitTestFeature");
    fitTest_HandleEvent(fit_test_event_resume_feature);
}

/*The connect disconnect indications are used to Abort fit test only in case output chain user is not prompt.
 The Fit test chain uses same user type as of standard prompt chain. In Order to differentiate Fit test prompt
 from standard tones/prompts FileIndex of fit test is compared when the tone/prompt start indication is received.*/

static void fitTest_OutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    DEBUG_LOG_INFO("fitTest_OutputConnectingIndication connecting user %d current fit test state %d",connecting_user,fitTest_GetCurrentState());

    if(((connecting_user & output_user_prompt) != output_user_prompt) &&
            ((connecting_user & output_user_a2dp) != output_user_a2dp))
    {
        /* Someone (not standard prompt/fit_test prompt) is trying to use output chain */
        DEBUG_LOG("fitTest_OutputConnectingIndication FIT TEST is running %d, FIT TEST tuning active %d ",FitTest_IsRunning(), FitTest_IsTuningModeActive());
        if(FitTest_IsRunning() || FitTest_IsTuningModeActive())
        {
            fitTest_HandleEvent(fit_test_event_abort);
        }
        else
        {
            fitTest_HandleEvent(fit_test_event_disable);
        }
    }
}

static void fitTest_OutputDisconnectingIndication(output_users_t disconnected_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    UNUSED(disconnected_user);
    DEBUG_LOG_INFO("fitTest_OutputDisconnectingIndication disconnected user %d current fit test state %d",disconnected_user,fitTest_GetCurrentState());
    DEBUG_LOG_INFO("fitTest_OutputDisconnectingIndication Kymera_OutputIsChainInUse() %d",Kymera_OutputIsChainInUse());

    /*No user of output chain is using output chain and Fit Test is not in suspended state */
    if(!Kymera_OutputIsChainInUse() && !fitTest_IsSuspended())
    {
        /* Fit test state machine transitions to ready state.*/
        fitTest_HandleEvent(fit_test_event_make_test_ready);
    }
}

static void fitTest_RestoreAncState(void)
{
    DEBUG_LOG("fitTest_RestoreAncState");

    if(fitTestGetTaskData()->anc_state_prev)
    {
        AncStateManager_Enable();
    }
}

static void fitTest_msgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case KYMERA_PROMPT_END_IND:
            DEBUG_LOG_ALWAYS("RECEIVED KYMERA_PROMPT_END_IND");
            fitTest_HandlePromptEndInd();
        break;

        case KYMERA_AANC_BAD_ENVIRONMENT_TRIGGER_IND:
        case KYMERA_AANC_BAD_ENVIRONMENT_CLEAR_IND:
        case KYMERA_EFT_GOOD_FIT_IND:
        case KYMERA_EFT_BAD_FIT_IND:
            if(FitTest_IsRunning())
                fitTest_UpdateFinalResult(id);
        break;

        case ANC_UPDATE_STATE_DISABLE_IND:
            /* Enable Adaptive ANC with anc_mode_fit_test(63) after Standard ANC mode has disabled */
            if(FitTest_IsRunning() || FitTest_IsTuningModeActive())
            {
                fitTest_StartTest();
            }
        break;

        case KYMERA_NOTIFICATION_TONE_STARTED:
            fitTest_HandleEvent(fit_test_event_abort);
        break;

        case KYMERA_NOTIFICATION_PROMPT_STARTED:
        {
            const KYMERA_NOTIFICATION_PROMPT_STARTED_T *prompt_msg = (KYMERA_NOTIFICATION_PROMPT_STARTED_T *)message;
            DEBUG_LOG_ALWAYS("KYMERA_NOTIFICATION_PROMPT_STARTED %d %d",prompt_msg->id,KymeraFitTest_GetPromptIndex());
            if(prompt_msg->id != KymeraFitTest_GetPromptIndex())
                fitTest_HandleEvent(fit_test_event_abort);
        }
        break;

        default:
        break;
    }
}

void fitTest_UpdateFinalResult(MessageId id)
{
    switch(id)
    {
        case KYMERA_AANC_BAD_ENVIRONMENT_TRIGGER_IND:
            flags.aanc_bad_env  = TRUE; /* Bad environment */
        break;

        case KYMERA_AANC_BAD_ENVIRONMENT_CLEAR_IND:
            flags.aanc_bad_env = FALSE; /* Good environment */
            break;

        case KYMERA_EFT_GOOD_FIT_IND:
            flags.eft_good_fit = TRUE;
            break;
        case KYMERA_EFT_BAD_FIT_IND:
            flags.eft_good_fit = FALSE;
            break;

        default:
        break;
    }

    if(!flags.aanc_bad_env)
    {
        if(flags.eft_good_fit)
        {
            fitTestGetTaskData()->local_result = fit_test_result_good;
            DEBUG_LOG_ALWAYS("fitTest_UpdateFinalResult: Good Fit msg received from OP!!!");
        }
        else
        {
            fitTestGetTaskData()->local_result = fit_test_result_bad;
            DEBUG_LOG_ALWAYS("fitTest_UpdateFinalResult: Bad Fit msg received from OP!!!");
        }

    }
    else
    {
       fitTestGetTaskData()->local_result = fit_test_result_error;
       DEBUG_LOG_ALWAYS("fitTest_UpdateFinalResult: Bad envirnment/FitTest not started");
    }
}

fit_test_result_t FitTest_GetLocalDeviceTestResult(void)
{
    return (fitTestGetTaskData()->local_result);
}

fit_test_result_t FitTest_GetRemoteDeviceTestResult(void)
{
    return (fitTestGetTaskData()->remote_result);
}

static void fitTest_HandlePromptEndInd(void)
{
    if(fitTest_GetCurrentState() == fit_test_state_running)
    {
        DEBUG_LOG("SENDING fit_test_event_complete");
        fitTest_HandleEvent(fit_test_event_complete);
    }
    else
    {
        DEBUG_LOG("fitTest_HandlePromptEndInd ignored assuming the fit test is not running");
    }
}

static bool fitTestStateUnitialisedHandleEvent(fit_test_event_t event)
{
    bool result=FALSE;
    switch(event)
    {
        case fit_test_event_initialise:
            result=fitTest_SetupTest();
        break;

        default:
            DEBUG_LOG_ALWAYS("fitTestStateUnitialisedHandleEvent: Unexpected event");
            Panic();
        break;
    }

    return result;
}

static bool fitTestStateReadyHandleEvent(fit_test_event_t event)
{
    bool result=FALSE;
    switch(event)
    {
        case fit_test_event_disable:
            result=fitTest_Disable();
        break;

        case fit_test_event_start:
            result=fitTest_Start(fit_test_run_mode);
        break;

        case fit_test_event_activate_tuning_mode:
            result=fitTest_EnterTuning();
        break;

        case fit_test_event_suspend_feature:
            result=fitTest_Suspend();
        break;

        default:
            DEBUG_LOG_ALWAYS("fitTestStateReadyHandleEvent: Unexpected event");
        break;

    }
    return result;
}

static bool fitTestStateDisabledHandleEvent(fit_test_event_t event)
{
    bool result=FALSE;
    switch(event)
    {
        case fit_test_event_make_test_ready:
            fitTest_ChangeState(fit_test_state_ready);
            result = TRUE;
        break;

        case fit_test_event_resume_feature:
            result = fitTest_Resume();
        break;

        case fit_test_event_suspend_feature:
            result = fitTest_Suspend();
        break;

        default:
            DEBUG_LOG_ALWAYS("fitTestStateDisabledHandleEvent: Unexpected event");
        break;
    }
    return result;
}

static bool fitTestStateRunningHandleEvent(fit_test_event_t event)
{
    bool result=FALSE;
    switch(event)
    {
        case fit_test_event_abort:
            result=fitTest_Abort();
        break;

        case fit_test_event_complete:
            result=fitTest_Stop();
            Ui_InjectRedirectableUiInput(ui_input_fit_test_remote_result_ready, FALSE);//Inform remote device about the completion of the fit test with results.
                                                         // This Ui event assumes the FitTest_GetLocalDeviceTestResult() will return the
                                                         // latest copy of the fit test.
        break;

        case fit_test_event_suspend_feature:
            result=fitTest_StopTestAndSuspend(fit_test_run_mode);
        break;

        default:
            DEBUG_LOG_ALWAYS("fitTestStateRunningHandleEvent: Unexpected event");
        break;
    }

    return result;
}

static bool fitTestStateTuningModeHandleEvent(fit_test_event_t event)
{
    bool result = FALSE;

    switch(event)
    {
        case fit_test_event_deactivate_tuning_mode:
            result = fitTest_ExitTuning(fit_test_state_ready);
        break;

        case fit_test_event_abort:
            result = fitTest_ExitTuning(fit_test_state_disabled);
        break;

        case fit_test_event_suspend_feature:
            result=fitTest_StopTestAndSuspend(fit_test_tuning_mode);
        break;

        default:
            DEBUG_LOG_ALWAYS("fitTestStateTuningModeHandleEvent: Unexpected event");
        break;
    }

    return result;
}

static void fitTest_MessageRegisteredClients(MessageId id, uint8 left_eb_result, uint8 right_eb_result)
{
    fitTestTaskData *fitTest = fitTestGetTaskData();

    MESSAGE_MAKE(ind,FIT_TEST_RESULT_IND_T);
    ind->left_earbud_result = left_eb_result;
    ind->right_earbud_result = right_eb_result;

    TaskList_MessageSendWithSize(&fitTest->client_tasks,id,ind,sizeof(ind));
}

static void fitTest_ResetConfiguration(void)
{
     fitTestGetTaskData()->prompt_count = FIT_TEST_PROMPTS_COUNT;
     memset(&flags, 0, sizeof(fit_test_flags_t));
}

static void fitTest_ResetResults(void)
{
     fitTestGetTaskData()->local_result = fit_test_result_bad;
     fitTestGetTaskData()->remote_result = fit_test_result_bad;
}

static void fitTest_ResetData(void)
{
     fitTest_ResetConfiguration();
     fitTest_ResetResults();
}

static void fitTest_SetAncHwEnable(bool enable)
{
    DEBUG_LOG("fitTest_SetAncHwEnable enable: %d", enable);
    fitTestGetTaskData()->anc_hw_enable = enable;
}

static bool fitTest_IsAncHwEnabled(void)
{
    DEBUG_LOG("fitTest_IsAncHwEnabled anc_hw_enable: %d", fitTestGetTaskData()->anc_hw_enable);
    return fitTestGetTaskData()->anc_hw_enable;
}


static void fitTest_StoreAncData(void)
{
    fitTestGetTaskData()->anc_mode_prev = AncStateManager_GetCurrentMode();
    fitTestGetTaskData()->anc_state_prev = AncStateManager_IsEnabled();
}

static void fitTest_resetStoredAncData(void)
{
    fitTestGetTaskData()->anc_mode_prev = 0;
    fitTestGetTaskData()->anc_state_prev = FALSE;
}

static void fitTest_TurnOnAncMics(void)
{
    DEBUG_LOG_ALWAYS("fitTest_TurnOnAncMics");

    Microphones_SetMicRate(appConfigAncFeedForwardMic(), ANC_SAMPLE_RATE, non_exclusive_user);
    Microphones_TurnOnMicrophone(appConfigAncFeedForwardMic(), non_exclusive_user);
    Microphones_SetMicRate(appConfigAncFeedBackMic(), ANC_SAMPLE_RATE, non_exclusive_user);
    Microphones_TurnOnMicrophone(appConfigAncFeedBackMic(), non_exclusive_user);
}

static void fitTest_EnableAncHw(void)
{
    DEBUG_LOG_ALWAYS("fitTest_EnableAncHw");

    AncSetMode(anc_mode_fit_test);
    AncEnableWithMutePathGains();

    fitTest_SetAncHwEnable(TRUE);
}

static void fitTest_EnableAncWithFitTestMode(void)
{
    DEBUG_LOG_ALWAYS("fitTest_EnableAncWithFitTestMode");

    /* enable ANC mics */
    fitTest_TurnOnAncMics();
    /* Static ANC enable */
    fitTest_EnableAncHw();
    /* Adaptive ANC enable */
    KymeraFitTest_EnableAanc();
}

static void fitTest_DisableAncForFitTestMode(void)
{
    DEBUG_LOG_ALWAYS("fitTest_DisableAncForFitTestMode");

    /* Disable ANC which is enabled in EFT use case */
    KymeraFitTest_DisableAanc();
    AncEnable(FALSE);

    /* Disable ANC mics */
    Microphones_TurnOffMicrophone(appConfigAncFeedForwardMic(), non_exclusive_user);
    Microphones_TurnOffMicrophone(appConfigAncFeedBackMic(), non_exclusive_user);
}

static void fitTest_DisableAnc(void)
{
    DEBUG_LOG_ALWAYS("fitTest_DisableAnc");

    if (fitTest_IsAncHwEnabled()) /* Anc is enabled with EFT use case */
    {
        fitTest_DisableAncForFitTestMode();
        AncSetMode(fitTestGetTaskData()->anc_mode_prev);
    }

    /* Re-enable Standard ANC incase if it was enabled before during Fit Test Start */
    fitTest_RestoreAncState();
    fitTest_resetStoredAncData();

    fitTest_SetAncHwEnable(FALSE);
}

static bool fitTest_EnterTuning(void)
{
    DEBUG_LOG_ALWAYS("fitTest_EnterTuning");
    return fitTest_Start(fit_test_tuning_mode);
}

static bool fitTest_ExitTuning(fit_test_states_t next_state)
{
    DEBUG_LOG_ALWAYS("fitTestExitTuning");
    KymeraFitTest_CancelPrompt();
    fitTest_DisableAnc();
    KymeraFitTest_Stop();

    fitTest_ChangeState(next_state);
    FeatureManager_StopFeatureIndication(fitTest_GetFeatureManagerHandle());
    return TRUE;
}

static bool fitTest_Resume(void)
{
    DEBUG_LOG("fitTest_Resume");
    if(!Kymera_OutputIsChainInUse())
    {
        fitTest_ChangeState(fit_test_state_ready);
    }
    fitTest_UpdateFeatureState(RESUMED);
    return TRUE;
}

static bool fitTest_Suspend(void)
{
    DEBUG_LOG("fitTest_Suspend");
    fitTest_ResetResults();
    fitTest_ChangeState(fit_test_state_disabled);
    fitTest_UpdateFeatureState(SUSPENDED);
    return TRUE;
}

static bool fitTest_StopTestAndSuspend(fit_test_modes_t fit_test_mode)
{
    DEBUG_LOG("fitTest_StopTestAndSuspend");
    KymeraFitTest_CancelPrompt();
    fitTest_DisableAnc();
    KymeraFitTest_Stop();

    if(fit_test_mode == fit_test_run_mode)
    {
        fitTest_ResetConfiguration();
        fitTest_MessageRegisteredClients(FIT_TEST_RESULT_IND,fit_test_result_error,fit_test_result_error);
    }

    fitTest_Suspend();
    return TRUE;
}

static void fitTest_StartTest(void)
{
    /* Start EFT mic path graph */
    KymeraFitTest_Start();
    /* Enable ANC for Fit Test usecase */
    fitTest_EnableAncWithFitTestMode();
    /* Start EFT speaker path graph */
    KymeraFitTest_StartPrompt();
}

static bool fitTest_Start(fit_test_modes_t fit_test_mode)
{
    DEBUG_LOG_ALWAYS("fitTest_Start");
    bool status = FALSE;
    fit_test_states_t next_state;

    if(FeatureManager_StartFeatureRequest(fitTest_GetFeatureManagerHandle()))
    {
        fitTest_ResetData();

        next_state = (fit_test_mode == fit_test_run_mode) ? fit_test_state_running :
                                                            fit_test_state_tuning;
        fitTest_ChangeState(next_state);

        fitTest_StoreAncData();
        /* Before starting test, disable Standard ANC if already enabled */
        if(AncStateManager_IsEnabled())
        {
            AncStateManager_Disable();
            /* Fit Test will be started on receiving ANC_UPDATE_STATE_DISABLE_IND */
        }
        else
        {
            fitTest_StartTest();
        }

        status = TRUE;
    }
    return status;
}


static bool fitTest_Stop(void)
{
    DEBUG_LOG_ALWAYS("fitTest_Stop");

    fitTest_DisableAnc();
    KymeraFitTest_Stop();

    fitTest_ChangeState(fitTest_GetPreviousState());
    fitTest_ResetConfiguration();

    FeatureManager_StopFeatureIndication(fitTest_GetFeatureManagerHandle());
    return TRUE;
}

static bool fitTest_Abort(void)
{
    DEBUG_LOG_ALWAYS("fitTest_Abort");

    KymeraFitTest_CancelPrompt();
    fitTest_DisableAnc();
    fitTest_ChangeState(fit_test_state_disabled);
    KymeraFitTest_Stop();

    FeatureManager_StopFeatureIndication(fitTest_GetFeatureManagerHandle());

    /*No user of output chain is using output chain and Fit Test is not in suspended state */
    if(!Kymera_OutputIsChainInUse() && !fitTest_IsSuspended())
    {
        fitTest_ChangeState(fit_test_state_ready);
    }

    fitTest_ResetData();
    fitTest_MessageRegisteredClients(FIT_TEST_RESULT_IND,fit_test_result_error,fit_test_result_error);

    return TRUE;
}

static void fitTest_ChangeState(fit_test_states_t next_state)
{
    fitTestTaskData *fitTestTask = fitTestGetTaskData();
    fit_test_states_t current_state = fitTest_GetCurrentState();

    if(next_state!=current_state)
    {
        DEBUG_LOG_ALWAYS("fitTest_ChangeState enum:fit_test_states_t:%d -> enum:fit_test_states_t:%d", current_state,next_state);

        fitTestTask->fit_test_previous_state=current_state;
        fitTestTask->fit_test_current_state=next_state;
    }
}

static bool fitTest_SetupTest(void)
{
    DEBUG_LOG_ALWAYS("fitTest_SetupTest");
    /* initialisation tasks*/
    fitTestTaskData *fitTestTask = fitTestGetTaskData();
    fitTestTask->task.handler=fitTest_msgHandler;
    Kymera_ClientRegister(&fitTestTask->task);

    /*Listener for tone/prompt start indication*/
    Kymera_RegisterNotificationListener(&fitTestTask->task);
    AncStateManager_ClientRegister(&fitTestTask->task);

    fitTest_ResetData();

    fitTestTask->fit_test_previous_state = fit_test_state_uninitialised;
    fitTestTask->fit_test_current_state = fit_test_state_ready;
    fitTest_ChangeState(fit_test_state_ready);
    return TRUE;
}


/*! \brief To identify if local device is left, incase of earbud application. */
static bool fitTest_IsLocalDeviceLeft(void)
{
    bool isLeft = TRUE;

#ifndef INCLUDE_STEREO
    isLeft = Multidevice_IsLeft();
#endif

    return isLeft;
}

static bool fitTest_Disable(void)
{
    DEBUG_LOG_ALWAYS("fitTest_Disable");

    fitTest_ResetResults();
    /* Disable Wear Detect feature temporarily */
    fitTest_ChangeState(fit_test_state_disabled);
    return TRUE;
}

static uint8 fitTest_GetLeftEarbudResult(void)
{
    return fitTest_IsLocalDeviceLeft()?FitTest_GetLocalDeviceTestResult():FitTest_GetRemoteDeviceTestResult();
}

static uint8 fitTest_GetRightEarbudResult(void)
{
    return !fitTest_IsLocalDeviceLeft()?FitTest_GetLocalDeviceTestResult():FitTest_GetRemoteDeviceTestResult();
}

static bool fitTest_HandleEvent(fit_test_event_t event)
{
    bool result=FALSE;
    switch(fitTest_GetCurrentState())
    {
        case fit_test_state_uninitialised:
            result=fitTestStateUnitialisedHandleEvent(event);
        break;

        case fit_test_state_ready:
            result=fitTestStateReadyHandleEvent(event);
        break;

        case fit_test_state_disabled:
            result=fitTestStateDisabledHandleEvent(event);
        break;

        case fit_test_state_running:
            result=fitTestStateRunningHandleEvent(event);
        break;

        case fit_test_state_tuning:
            result=fitTestStateTuningModeHandleEvent(event);
        break;

        default:
            DEBUG_LOG_ALWAYS("FitTest_HandleEvent unhandle event");
        break;
    }
    return result;
}

static fit_test_states_t fitTest_GetCurrentState(void)
{
    fitTestTaskData *fitTestTask = fitTestGetTaskData();
    return fitTestTask->fit_test_current_state;
}

static fit_test_states_t fitTest_GetPreviousState(void)
{
    fitTestTaskData *fitTestTask = fitTestGetTaskData();
    return fitTestTask->fit_test_previous_state;
}

bool FitTest_MakeTestReady(void)
{
    bool result=FALSE;
    result=fitTest_HandleEvent(fit_test_event_make_test_ready);
    return result;
}

bool FitTest_InformClients(void)
{
    //Remote results are already available in local data-structure - need to update the registered clients.
    fitTest_MessageRegisteredClients(FIT_TEST_RESULT_IND,fitTest_GetLeftEarbudResult(),fitTest_GetRightEarbudResult());
    return TRUE;
}

bool FitTest_StartTest(void)
{
    bool result=FALSE;
    result=fitTest_HandleEvent(fit_test_event_start);
    return result;
}

bool FitTest_AbortTest(void)
{
    bool result=FALSE;
    result=fitTest_HandleEvent(fit_test_event_abort);
    return result;
}

bool FitTest_DisableTest(void)
{
    bool result=FALSE;
    result=fitTest_HandleEvent(fit_test_event_disable);
    return result;
}

void FitTest_ClientRegister(Task client_task)
{
    DEBUG_LOG_ALWAYS("FitTest_ClientRegister %d", client_task);
    fitTestTaskData *fitTest = fitTestGetTaskData();

    if(client_task)
    {
        DEBUG_LOG_ALWAYS("Client Registered");
        TaskList_AddTask(&fitTest->client_tasks,client_task);
    }
}

void FitTest_ClientUnRegister(Task client_task)
{
    DEBUG_LOG_ALWAYS("FitTest_ClientUnRegister");
    fitTestTaskData *fitTest = fitTestGetTaskData();
    if(client_task)
    {
        DEBUG_LOG_ALWAYS("Client Un Registered");
        TaskList_RemoveTask(&fitTest->client_tasks,client_task);
    }
}

bool FitTest_IsRunning(void)
{
    return (fitTest_GetCurrentState() == fit_test_state_running? TRUE:FALSE);
}

bool FitTest_IsReady(void)
{
    return(fitTest_GetCurrentState() == fit_test_state_ready ? TRUE:FALSE);
}

void FitTest_StoreRemotePeerResults(uint8 result)
{
    DEBUG_LOG_ALWAYS("FitTest_StoreRemotePeerResults");
    fitTestTaskData *fitTestTask = fitTestGetTaskData();
    fitTestTask->remote_result = (fit_test_result_t)result;
}


bool FitTest_init(Task init_task)
{
    UNUSED(init_task);
    fitTestTaskData *fitTestTask = fitTestGetTaskData();
    TaskList_Initialise(&fitTestTask->client_tasks);
    fitTest_HandleEvent(fit_test_event_initialise);
    Kymera_OutputRegisterForIndications(&fitTest_IndicationCallbacks);
    fitTest_SetFeatureManagerHandle(FeatureManager_Register(feature_id_fit_test, &fitTest_feature_manager_if));
    fitTest_SetAncHwEnable(FALSE);
    return TRUE;
}

bool FitTest_PromptReplayRequired(void)
{
    bool replay_required = FALSE;

    if(FitTest_IsTuningModeActive())
    {
        replay_required = TRUE;
    }
    else if(FitTest_IsRunning())
    {
        fitTestGetTaskData()->prompt_count--;
        if(fitTestGetTaskData()->prompt_count != 0)
        {
            DEBUG_LOG_INFO("FitTest_PromptReplayRequired %d prompt count remaining", fitTestGetTaskData()->prompt_count);
            replay_required = TRUE;
        }
    }

    return replay_required;
}

bool FitTest_EnterFitTestTuningMode(void)
{
    bool result=FALSE;
    result=fitTest_HandleEvent(fit_test_event_activate_tuning_mode);
    return result;
}

bool FitTest_ExitFitTestTuningMode(void)
{
    bool result=FALSE;
    result=fitTest_HandleEvent(fit_test_event_deactivate_tuning_mode);
    return result;
}

bool FitTest_IsTuningModeActive(void)
{
    return (fitTestGetTaskData()->fit_test_current_state == fit_test_state_tuning);
}

#endif
