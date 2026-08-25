/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_state_manager.c
\brief      State manager implementation for Active Noise Cancellation (ANC) which handles transitions
            between init, powerOff, powerOn, enable, disable and tuning states.
*/


#ifdef ENABLE_ANC
#include "anc_state_manager.h"
#include "anc_session_data.h"
#include "anc_state_manager_private.h"
#include "anc_config.h"
#include "kymera.h"
#include "microphones.h"
#include "state_proxy.h"
#include "phy_state.h"
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
#include "usb_application.h"
#include "usb_app_default.h"
#include "usb_app_anc_tuning.h"
#ifdef ENABLE_ADAPTIVE_ANC
#include "usb_app_adaptive_anc_tuning.h"
#endif
#else
#include "usb_common.h"
#endif
#include "wind_detect.h"
#include "system_clock.h"
#include "kymera_anc_common.h"
#include "kymera_va.h"
#include "aanc_quiet_mode.h"
#include "microphones.h"
#include "kymera_output_if.h"
#include "ps_key_map.h"
#include "multidevice.h"
#include "kymera_anc.h"
#include "anc_auto_ambient.h"
#include "ui.h"

#include <panic.h>
#include <task_list.h>
#include <logging.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(anc_msg_t)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(ANC, ANC_MESSAGE_END)
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(anc_state_manager_event_set_world_volume_balance)
#endif

#define DEBUG_ASSERT(x, y) {if (!(x)) {DEBUG_LOG(y);Panic();}}

/*! USB configuration in use for anc state manger */
typedef enum
{
    ANC_USB_CONFIG_NO_USB,
    ANC_USB_CONFIG_STATIC_ANC_TUNING,
    ANC_USB_CONFIG_ADAPTIVE_ANC_TUNING
} anc_usb_config_t;

static void ancstateManager_HandleMessage(Task task, MessageId id, Message message);

#define ANC_SM_IS_ADAPTIVE_ANC_ENABLED() (KymeraAncCommon_AdaptiveAncIsEnabled())

#define ANC_SM_IS_ADAPTIVE_ANC_DISABLED() (!KymeraAncCommon_AdaptiveAncIsEnabled())

#define ANC_SM_READ_AANC_GAIN_TIMER                (250) /*ms*/
#define ANC_SM_DEFAULT_SECONDARY_FF_GAIN            (0) /*used when peer is not connected*/
/*! \brief Config timer to allow ANC Hardware to configure for QCC512x chip variants 
This timer is not applicable to QCC514x chip varaints and value can be set to zero*/
#define KYMERA_CONFIG_ANC_DELAY_TIMER     (0) /*ms*/

#define QUIET_MODE_DETECTED TRUE
#define QUIET_MODE_NOT_DETECTED FALSE

#define QUIET_MODE_TIME_DELAY_MS  (200U)
#define QUIET_MODE_TIME_DELAY_US  (US_PER_MS * QUIET_MODE_TIME_DELAY_MS)
#define US_TO_MS(us) ((us) / US_PER_MS)

#define STATIC_ANC_CONFIG_SETTLING_TIME (500U)
#define STATIC_ANC_MODE_CHANGE_SETTLING_TIME (500U)

#define AANC_GAIN_PASSIVE_ISOLATION     (0)

#define ANC_TOGGLE_NOT_CONFIGURED (0xFF)
#define ANC_TOGGLE_CONFIGURED_OFF (0x00)

#define ANC_HW_FF_FINE_GAIN_ADJUST_VALUE     (6) /* dB */

#define ANC_BALANCE_DEVICE_LEFT   (FALSE)
#define ANC_BALANCE_DEVICE_RIGHT  (TRUE)

#define ANC_WORLD_VOLUME_ZERO_BALANCE  (0U)

#define ANC_SM_GET_DEVICE_SIDE_FROM_ENCODED_BALANCE_INFO(info)  ((info & 0x80) != 0)
#define ANC_SM_GET_PERCENTAGE_FROM_ENCODED_BALANCE_INFO(info)   ((info) & 0x7F)
#define ANC_SM_ENCODE_BALNCE_INFO(percentage, device_side)      ((percentage) | ((device_side) << 7))

#define MAX_MODES                 appConfigNumOfAncModes()

#ifdef ENABLE_ADAPTIVE_ANC
#define ancConfigIsAdvancedAdaptiveAnc()     (AncConfig_IsAdvancedAnc())
#else
#define ancConfigIsAdvancedAdaptiveAnc()     (FALSE)
#endif

typedef STATE_PROXY_RECONNECTION_ANC_DATA_T anc_sm_reconnection_data;

/* ANC state manager data */
typedef struct
{
    /*! Anc StateManager task */
    TaskData task_data;
    /*! List of tasks registered for notifications */
    task_list_t *client_tasks;
    unsigned requested_enabled:1;
    unsigned actual_enabled:1;
    unsigned power_on:1;
    unsigned persist_anc_mode:1;
    unsigned persist_anc_enabled:1;
    unsigned enable_dsp_clock_boostup:1;
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    unsigned usb_enumerated:1;
    unsigned unused:5;
#else
    unsigned unused:6;
#endif
    anc_state_manager_t state;
    anc_mode_t current_mode;
    anc_mode_t requested_mode;
    uint8 num_modes;
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    anc_usb_config_t usb_config;
    Source spkr_src;
    Sink mic_sink;
    Task SavedUsbAudioTask;
    const usb_app_interface_t * SavedUsbAppIntrface;
#else
    uint16 usb_sample_rate;
#endif
    uint8 anc_gain;
    uint8 aanc_ff_gain;

    uint8 aanc_fb_gain;

    /*! Array to store ANC LeakThrough Gains. Index indicates ANC mode and value at index indicates
    updated or latest ANC LeakThrough gain */
    uint8 leakthrough_gain[MAX_MODES];

    /*! Particular bit will be set on update of ANC LeakThrough gain */
    uint16 leakthrough_gain_update_bits;

    marshal_rtime_t timestamp;
    Sink sink;/*L2CAP SINK*/

    /* added to test SCO disconnect issue in RDP */
    Source mic_src_ff_left;
    Source mic_src_fb_left;
    Source mic_src_ff_right;
    Source mic_src_fb_right;

    anc_toggle_way_config_t toggle_configurations;

    anc_toggle_config_during_scenario_t standalone_config;
    anc_toggle_config_during_scenario_t playback_config;
    anc_toggle_config_during_scenario_t sco_config;
    anc_toggle_config_during_scenario_t va_config;

    bool demo_state; /*GAIA ANC Demo Mode State*/
    uint16 previous_config; /*Used to fallback to previous config (includes modes and state) when concurrency is over*/
    uint16 previous_mode;   /*Used to fallback to previous mode when concurrency is over, in case of ANC disabled initially*/
    bool   system_triggered_disable; /*ANC disable triggered internally or system triggered and not by user*/
    bool adaptivity; /*Adaptivity status*/

    anc_sm_world_volume_gain_balance_info_t balance_info;
    /*! Array to store World volume dB Gains. Index indicates ANC mode and value at index indicates
    latest World volume dB gain */
    int8 world_volume_gain_dB[MAX_MODES];

    int8 requested_world_volume_gain_dB;
    uint8 requested_balance_info;

    anc_sm_reconnection_data* reconnection_data;

    bool self_speech_in_progress;/*Set when self speech trigger is detected, reset when on self speech release or feature disable */
    bool concurrency_in_progress;
    
    anc_scenario_config_id_t concurrency_scenario;
    uint16 anc_lock;
} anc_state_manager_data_t;

static anc_state_manager_data_t anc_data;


#define ancSmConvertAncToggleIdToToggleIndex(toggle_way_id) (toggle_way_id - anc_toggle_way_config_id_1)

/*! Get pointer to Anc state manager structure */
#define GetAncData() (&anc_data)
#define GetAncClients() TaskList_GetBaseTaskList(&GetAncData()->client_tasks)
#define GetBitStatus(pos)   (anc_data.leakthrough_gain_update_bits & (1 << pos))

#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
#define ancStateManager_StopPathGainsUpdateTimer() (MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_set_filter_path_gains))
#define ancStateManager_StartPathGainsUpdateTimer(time) (MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_set_filter_path_gains, NULL, time))
#define ancStateManager_StopModeChangeSettlingTimer() (MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_set_filter_path_gains_on_mode_change))
#define ancStateManager_StartModeChangeSettlingTimer(time) (MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_set_filter_path_gains_on_mode_change, NULL, time))
#else
#define ancStateManager_StopPathGainsUpdateTimer() ((void)(0))
#define ancStateManager_StartPathGainsUpdateTimer(x) ((void)(0 * (x)))
#define ancStateManager_StopModeChangeSettlingTimer() ((void)(0))
#define ancStateManager_StartModeChangeSettlingTimer(x) ((void)(0 * (x)))
#endif

static bool ancStateManager_HandleEvent(anc_state_manager_event_id_t event);
static void ancStateManager_DisableAnc(anc_state_manager_t next_state);
static void ancStateManager_UpdateAncMode(void);

static void ancStateManager_EnableAncMics(void);
static void ancStateManager_DisableAncMics(void);
static bool ancStateManager_EnableAncHw(void);
static bool ancStateManager_DisableAncHw(void);
static void ancStateManager_PreAncModeChange(void);
static void ancStateManager_PreAncDisable(void);
static bool ancStateManager_EnableAncHwWithMutePathGains(void);
static void setSessionData(void);
static bool AncIsModeValid(uint16 mode);

static void ancStateManager_SetPreviousConfig(uint16 config);
static void ancStateManager_SetPreviousMode(uint16 mode);
static void ancStateManager_ApplyConfigInEnabled(uint16 toggle_config);
static void ancStateManager_ApplyConfigInDisabled(uint16 toggle_config);

/*Allows for ANC state or mode change through internal event and synchronisation with peer is through state proxy*/
static void ancStateManager_ApplyConfig(uint16 config);

/*Allows for ANC state or mode change through UI event and synchronisation with peer is through peer UI mechanism*/
static void ancStateManager_ApplyConfigSynchronised(uint16 config);

static void ancStateManager_DeriveAndSetPreviousConfig(void);
static void ancStateManager_StandaloneAction(void);
static void ancStateManager_StandaloneActionSynchronised(void);
static void ancStateManager_ConcurrencyConnectAction(void);
static void ancStateManager_ConcurrencyDisconnectAction(void);

static bool ancStateManager_GetInEarStatus(void);

static void ancStateManager_OutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type);
static void ancStateManager_OutputDisconnectingIndication(output_users_t connecting_user, output_connection_t connection_type);

/* Store LeakThrough gain in the array in ANC data structure */
static void ancStateManager_PreserveLeakthroughGain(uint8 anc_leakthrough_gain);

static bool ancStateManager_SetWorldVolumeGain(void);
static bool ancStateManager_SetWorldVolumeBalance(void);

static void ancstateManager_MsgRegisteredClientsWithBothFFGains(uint8 local_ff_gain, uint8 remote_ff_gain);
static void ancStateManager_MsgRegisteredClientsOnWorldVolumeBalanceUpdate(bool balance_device_side, bool balance_percentage);
static void ancStateManager_MsgRegisteredClientsOnPrevConfigUpdate(void);
static void ancStateManager_MsgRegisteredClientsOnPrevModeUpdate(void);

static void ancStateManager_SetLock(anc_state_manager_event_id_t event);
static void ancStateManager_ClearLock(anc_state_manager_event_id_t event);

static void ancstateManager_MsgRegisteredClientsWithBothFBGains(uint8 local_fb_gain, uint8 remote_fb_gain);
#ifdef ENABLE_ADAPTIVE_ANC
static void ancStateManager_HandleFBGainTimerExpiryEvent(void);
#endif

#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
/* Read ANC path fine gain from ANC library or from ANC data structure in case of LT gain is preserved */
static void ancStateManager_ReadFineGain(anc_mode_t mode, audio_anc_path_id audio_anc_path, uint8 *fine_gain);

static uint8 readAncPathFineGain(audio_anc_path_id audio_anc_path);
#endif


/*Registering Callback with Output manager to configure ANC modes during concurrency*/
static const output_indications_registry_entry_t AncSmIndicationCallbacks =
{
    .OutputConnectingIndication = ancStateManager_OutputConnectingIndication,
    .OutputDisconnectedIndication = ancStateManager_OutputDisconnectingIndication,
};

static anc_sm_reconnection_data* ancStateManager_GetReconnectionData(void)
{
    return GetAncData()->reconnection_data;
}

static void ancStateManager_ResetReconnectionData(void)
{
    if (GetAncData()->reconnection_data != NULL)
    {
        free(GetAncData()->reconnection_data);
        GetAncData()->reconnection_data = NULL;
    }
}

static void ancStateManager_SetReconnectionData(anc_sm_reconnection_data* reconnection_data)
{
    if(ancStateManager_GetReconnectionData() != NULL)
    {
        ancStateManager_ResetReconnectionData();
    }
    GetAncData()->reconnection_data = PanicUnlessMalloc(sizeof(anc_sm_reconnection_data));
    memcpy(GetAncData()->reconnection_data, reconnection_data, sizeof(*reconnection_data));
}

TaskData *AncStateManager_GetTask(void)
{
   return (&anc_data.task_data);
}

task_list_t *AncStateManager_GetClientTask(void)
{
    return (anc_data.client_tasks);
}

void AncStateManager_PostInitSetup(void)
{
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_anc);

#ifdef ENABLE_ADAPTIVE_ANC
    /* To receive FF Gain from remote device*/
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_aanc_logging);

   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_aanc_fb_gain_logging);

    /* To identify if remote device has gone incase*/
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_phystate);
#endif
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_aanc);
}

bool AncStateManager_CheckIfDspClockBoostUpRequired(void)
{
   return (anc_data.enable_dsp_clock_boostup);
}

static void ancStateManager_WindDetect(void)
{
    DEBUG_LOG_ALWAYS("ancStateManager_WindDetect");
    WindDetect_HandleWindDetect();
}

static void ancStateManager_WindRelease(void)
{
    DEBUG_LOG_ALWAYS("ancStateManager_WindRelease");
    WindDetect_HandleWindRelease();
}


static void ancStateManager_PreserveLeakthroughGain(uint8 anc_leakthrough_gain)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Store ANC LeakThrough gain in array */
    anc_sm->leakthrough_gain[AncStateManager_GetCurrentMode()] = anc_leakthrough_gain;

    /* Set the bit for particular ANC LeakThrough mode */
    anc_sm->leakthrough_gain_update_bits |= (1 << AncStateManager_GetCurrentMode());
}

uint8 ancStateManager_GetAncLeakthroughPreservedGain(anc_mode_t leakthrough_mode)
{
    return (GetAncData()->leakthrough_gain[leakthrough_mode]);
}

bool AncStateManager_IsAncLeakthroughGainPreserved(anc_mode_t mode)
{
    return (AncConfig_IsAncModeLeakThrough(mode) && GetBitStatus(mode));
}
#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
static void ancStateManager_ReadFineGain(anc_mode_t mode, audio_anc_path_id audio_anc_path, uint8 *fine_gain)
{
    if(AncStateManager_IsAncLeakthroughGainPreserved(mode))
    {
        *fine_gain = ancStateManager_GetAncLeakthroughPreservedGain(mode);
    }
    else
    {
        *fine_gain = readAncPathFineGain(audio_anc_path);
    }
}
#endif

bool AncStateManager_IsFBGainTimerUpdateRequire(anc_mode_t anc_mode)
{
    if(AncConfig_IsAdvancedAnc())
    {
        if(AncConfig_IsAncModeAdaptiveLeakThrough(anc_mode) || AncConfig_IsAncModeStaticLeakThrough(anc_mode))
        {
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
    else
    {
        return FALSE;
    }
}

/*! \brief Interface to handle concurrency scenario connect requests*/
static void ancStateManager_HandleConcurrencyConnectReq(anc_scenario_config_id_t scenario)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_HandleConcurrencyConnectReq");

    MESSAGE_MAKE(req, ANC_CONCURRENCY_CONNECT_REQ_T);
    req->scenario = scenario;
    MessageSend(AncStateManager_GetTask(), anc_state_manager_event_concurrency_connect, req);
}

/*! \brief Interface to handle concurrency scenario disconnect requests*/
static void ancStateManager_HandleConcurrencyDisconnectReq(anc_scenario_config_id_t scenario)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_HandleConcurrencyDisconnectReq");

    MESSAGE_MAKE(req, ANC_CONCURRENCY_DISCONNECT_REQ_T);
    req->scenario = scenario;
    MessageSend(AncStateManager_GetTask(), anc_state_manager_event_concurrency_disconnect, req);
}

/*! \brief Convert to anc scenario IDs from the output manager concurrenct user*/
static anc_scenario_config_id_t ancGetScenarioIdFromOutputUsers(output_users_t users)
{
    anc_scenario_config_id_t scenario_id=anc_scenario_config_id_standalone;

    if ((users & output_user_sco) == output_user_sco || 
        (users & output_user_le_voice_mono) == output_user_le_voice_mono ||
        (users & output_user_le_voice_stereo) == output_user_le_voice_stereo)
    {
        scenario_id = anc_scenario_config_id_sco;
    }
    else if ((users & output_user_a2dp)==output_user_a2dp)
    {    
        scenario_id = anc_scenario_config_id_playback;
        if (Kymera_IsVaActive())
        {        
            scenario_id = anc_scenario_config_id_va;
        }
    }
    return scenario_id;
}

uint16 AncStateManager_GetPreviousConfig(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    DEBUG_LOG("AncStateManager_GetPreviousConfig %d", anc_sm->previous_config);
    return anc_sm->previous_config;
}

static void ancStateManager_SetPreviousConfig(uint16 config)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_sm->previous_config = config;
    DEBUG_LOG("ancStateManager_SetPreviousConfig %d", config);
}

static void ancStateManager_SetPreviousMode(uint16 mode)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    
    DEBUG_LOG("ancStateManager_SetPreviousMode before %d", anc_sm->previous_mode);
    anc_sm->previous_mode = mode;
    DEBUG_LOG("ancStateManager_SetPreviousMode after %d", anc_sm->previous_mode);
}

uint16 AncStateManager_GetPreviousMode(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    DEBUG_LOG("AncStateManager_GetPreviousMode %d", anc_sm->previous_mode);
    return anc_sm->previous_mode;
}

void AncStateManager_StandaloneToConcurrencyReq(void)
{
    DEBUG_LOG("AncStateManager_StandaloneToConcurrencyReq");

    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_concurrency_connect_req);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_concurrency_connect_req, NULL, &anc_sm->anc_lock);

}

void AncStateManager_ConcurrencyToStandaloneReq(void)
{
    DEBUG_LOG("AncStateManager_ConcurrencyToStandaloneReq");

    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_concurrency_disconnect_req);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_concurrency_disconnect_req, NULL, &anc_sm->anc_lock);
}

static void ancStateManager_SetSelfSpeechStatus(void)
{
    anc_data.self_speech_in_progress = TRUE;
}

static void ancStateManager_ResetSelfSpeechStatus(void)
{
    anc_data.self_speech_in_progress = FALSE;
}

static bool ancStateManager_IsSelfSpeechInProgress(void)
{
    return (anc_data.self_speech_in_progress == TRUE);
}

static void ancStateManager_OutputConnectingIndication(output_users_t connecting_user, output_connection_t connection_type)
{
    UNUSED(connection_type);

    if (ancStateManager_GetInEarStatus())
    {
        ancStateManager_HandleConcurrencyConnectReq(ancGetScenarioIdFromOutputUsers(connecting_user));
    }
}

static void ancStateManager_OutputDisconnectingIndication(output_users_t disconnected_user, output_connection_t connection_type)
{
    UNUSED(connection_type);
    ancStateManager_HandleConcurrencyDisconnectReq(ancGetScenarioIdFromOutputUsers(disconnected_user));
}

static anc_toggle_config_during_scenario_t* ancGetScenarioConfigData(anc_scenario_config_id_t scenario)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_toggle_config_during_scenario_t *config=NULL;

    switch (scenario)
    {
        case anc_scenario_config_id_sco:
            config = &anc_sm->sco_config;
            break;
        case anc_scenario_config_id_playback:
            config = &anc_sm->playback_config;
            break;
        case anc_scenario_config_id_va:
            config = &anc_sm->va_config;
            break;
        default:
            break;
    }

    return config;
}

static void ancStateManager_ApplyConfig(uint16 config)
{
    DEBUG_LOG("ancStateManager_ApplyConfig Config %d", config);

    if (AncStateManager_IsEnabled())
    {       
        DEBUG_LOG("Apply Config During ANC Enabled");
        ancStateManager_ApplyConfigInEnabled(config);
    }
    else
    {
        DEBUG_LOG("Apply Config During ANC Disabled");
        ancStateManager_ApplyConfigInDisabled(config);
    }
}

static void ancStateManager_SetAncEnableUI(void)
{
    if (appPhyStateIsOutOfCase())
    {
        DEBUG_LOG("ancStateManager_SetAncEnableUI");
        Ui_InjectUiInput(ui_input_anc_on);
    }
}

static void ancStateManager_SetAncDisableUI(void)
{
    DEBUG_LOG("ancStateManager_SetAncDisableUI");
    Ui_InjectUiInput(ui_input_anc_off);
}

static void ancStateManager_SetAncModeUI(anc_mode_t anc_mode)
{
    DEBUG_LOG("ancStateManager_SetAncModeUI %d", anc_mode);
    switch(anc_mode)
    {
        case anc_mode_1:
            Ui_InjectUiInput(ui_input_anc_set_mode_1);
            break;
        case anc_mode_2:
            Ui_InjectUiInput(ui_input_anc_set_mode_2);
            break;
        case anc_mode_3:
            Ui_InjectUiInput(ui_input_anc_set_mode_3);
            break;
        case anc_mode_4:
            Ui_InjectUiInput(ui_input_anc_set_mode_4);
            break;
        case anc_mode_5:
            Ui_InjectUiInput(ui_input_anc_set_mode_5);
            break;
        case anc_mode_6:
            Ui_InjectUiInput(ui_input_anc_set_mode_6);
            break;
        case anc_mode_7:
            Ui_InjectUiInput(ui_input_anc_set_mode_7);
            break;
        case anc_mode_8:
            Ui_InjectUiInput(ui_input_anc_set_mode_8);
            break;
        case anc_mode_9:
            Ui_InjectUiInput(ui_input_anc_set_mode_9);
            break;
        case anc_mode_10:
            Ui_InjectUiInput(ui_input_anc_set_mode_10);
            break;
        default:
            Ui_InjectUiInput(ui_input_anc_set_mode_1);
            break;
    }
}

static void ancStateManager_ApplyConfigSynchronised(uint16 config)
{
    DEBUG_LOG("ancStateManager_ApplyConfigSynchronised Config %d", config);

    if (AncStateManager_IsEnabled())
    {       
        DEBUG_LOG("Apply Config During ANC Enabled");
        if (config==ANC_TOGGLE_CONFIGURED_OFF)
        {
            ancStateManager_SetAncDisableUI();
        }
        else
        {
            if (AncIsModeValid(config))
            {
                ancStateManager_SetAncModeUI((anc_mode_t)(config-1));
            }
        }
    }
    else
    {
        DEBUG_LOG("Apply Config During ANC Disabled");
        if (AncIsModeValid(config))
        {
            ancStateManager_SetAncModeUI((anc_mode_t)(config-1));
            ancStateManager_SetAncEnableUI();
        }
    }
}

static void ancStateManager_DeriveAndSetPreviousConfig(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    
    anc_sm->previous_config = (AncStateManager_IsEnabled())?(AncStateManager_GetCurrentMode()+1):(ANC_TOGGLE_CONFIGURED_OFF);
    
    DEBUG_LOG("ancStateManager_DeriveAndSetPrevious Config %d", anc_sm->previous_config);
    DEBUG_LOG("ancStateManager_DeriveAndSetPrevious Mode %d", anc_sm->previous_mode);
    
    /* Notify ANC previous mode and config update to registered clients */
    ancStateManager_MsgRegisteredClientsOnPrevConfigUpdate();
    ancStateManager_MsgRegisteredClientsOnPrevModeUpdate();
}

static void ancStateManager_ConcurrencyConnectAction(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_toggle_config_during_scenario_t *config= ancGetScenarioConfigData(anc_sm->concurrency_scenario);

    if ((config) && (!config->is_same_as_current))
    {        
        DEBUG_LOG("ancStateManager_HandleConcurrencyConnect Prev Config %d, Configured Config %d", anc_sm->previous_config, config->anc_config);
        DEBUG_LOG("ancStateManager_HandleConcurrencyConnect Prev Mode %d", anc_sm->previous_mode);
        
        if (StateProxy_IsPrimary())
        {
            /*If Self speech was ongoing and SCO is started afterwards, the Mode will be in Ambient, but we need to maintain previous mode */
            ancStateManager_DeriveAndSetPreviousConfig();
        }
        ancStateManager_ApplyConfig(config->anc_config);
    }
    else
    {
        if(AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
        {
            /*Else be in the adapitve anc mode, but be prepared for some audio that might come on DAC.*/
            MessageSend(AncStateManager_GetTask(), KYMERA_AANC_QUIET_MODE_CLEAR_IND, NULL);
        }
    }
}

static void ancStateManager_HandleConcurrencyConnect(const ANC_CONCURRENCY_CONNECT_REQ_T* req)
{
    DEBUG_LOG_ALWAYS("ancStateManager_HandleConcurrencyConnect");
    anc_state_manager_data_t *anc_sm = GetAncData();
       
    anc_sm->concurrency_in_progress = TRUE;
    anc_sm->concurrency_scenario = req->scenario;
    
    /*If Self speech is in progress, do nothing and continue with ambient mode and apply concurrency connect config after Self speech release*/    
    /*Self speech or concurrency whichever comes first will store the previous mode*/
    if (!ancStateManager_IsSelfSpeechInProgress())
    {
        ancStateManager_ConcurrencyConnectAction();
    }
}

static void ancStateManager_StandaloneAction(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    
    if (anc_sm->previous_config==ANC_TOGGLE_CONFIGURED_OFF)
    {
        anc_sm->system_triggered_disable = TRUE;
        DEBUG_LOG("ANC SM system_triggered_disable true");
    }
    
    ancStateManager_ApplyConfig(AncStateManager_GetPreviousConfig());

    if (anc_sm->previous_config==ANC_TOGGLE_CONFIGURED_OFF)
    {
        /*Fallback to the previous mode in case of ANC disabled before*/
        if (AncIsModeValid(AncStateManager_GetPreviousMode()))
        {
            AncStateManager_SetMode((anc_mode_t)(AncStateManager_GetPreviousMode()-1));
        }
    }
}

static void ancStateManager_StandaloneActionSynchronised(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    
    if (anc_sm->previous_config==ANC_TOGGLE_CONFIGURED_OFF)
    {
        anc_sm->system_triggered_disable = TRUE;
        DEBUG_LOG("ANC SM sync system_triggered_disable true");
    }
    
    ancStateManager_ApplyConfigSynchronised(AncStateManager_GetPreviousConfig());

    if (anc_sm->previous_config==ANC_TOGGLE_CONFIGURED_OFF)
    {
        /*Fallback to the previous mode in case of ANC disabled before*/
        if (AncIsModeValid(AncStateManager_GetPreviousMode()))
        {
            ancStateManager_SetAncModeUI((anc_mode_t)(AncStateManager_GetPreviousMode()-1));
        }
    }
}

static void ancStateManager_ConcurrencyDisconnectAction(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_toggle_config_during_scenario_t *config= ancGetScenarioConfigData(anc_sm->concurrency_scenario);
    DEBUG_LOG_ALWAYS("ancStateManager_ConcurrencyDisconnectAction");

    /*Check if the mode was modified by a concurrency config*/
    if ((config) && (!config->is_same_as_current))
    {
        /*Fallback to Standalone/Idle config*/
        if (anc_sm->standalone_config.is_same_as_current)
        {
            /*Use the config stored before.
               It could the same as stored durring the SCO, Music, VA, LE Stereo Recording concurrency or it could be changed due to toggle by user*/
               ancStateManager_StandaloneAction();
        }
        else
        {        
            ancStateManager_ApplyConfig(anc_sm->standalone_config.anc_config);
        }
    }
    else
    {
        if(AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
        {
            /*Else be in the adapitve anc mode, but be prepared for some audio that might come on DAC.*/
            MessageSend(AncStateManager_GetTask(), KYMERA_AANC_QUIET_MODE_CLEAR_IND, NULL);
        }
    }
}

static void ancStateManager_HandleConcurrencyDisconnect(const ANC_CONCURRENCY_DISCONNECT_REQ_T* req)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    
    DEBUG_LOG_ALWAYS("ancStateManager_HandleConcurrencyDisconnect");
    
    anc_sm->concurrency_in_progress = FALSE;
    anc_sm->concurrency_scenario = req->scenario;
    
    /*If Self speech is in progress, do nothing and continue with ambient mode and apply concurrency disconnect config after Self speech release*/
    if (!ancStateManager_IsSelfSpeechInProgress())
    {    
        ancStateManager_ConcurrencyDisconnectAction();
    }

    /*Reset concurrency scenario*/
    anc_sm->concurrency_scenario = 0;
}

/***************************************************************************
DESCRIPTION
    Get path configured for ANC

RETURNS
    None
*/
static audio_anc_path_id ancStateManager_GetAncPath(void)
{
    audio_anc_path_id audio_anc_path = AUDIO_ANC_PATH_ID_NONE;
    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case feed_forward_mode:
        case feed_forward_mode_left_only: /* fallthrough */
        case feed_back_mode:
        case feed_back_mode_left_only:
            audio_anc_path = AUDIO_ANC_PATH_ID_FFA;
            break;

        case hybrid_mode:
        case hybrid_mode_left_only:
            audio_anc_path = AUDIO_ANC_PATH_ID_FFB;
            break;

        default:
            break;
    }

    return audio_anc_path;
}

static anc_mode_t getModeFromSetModeEvent(anc_state_manager_event_id_t event)
{
    anc_mode_t mode = anc_mode_1;
    
    switch(event)
    {
        case anc_state_manager_event_set_mode_2:
            mode = anc_mode_2;
            break;
        case anc_state_manager_event_set_mode_3:
            mode = anc_mode_3;
            break;
        case anc_state_manager_event_set_mode_4:
            mode = anc_mode_4;
            break;
        case anc_state_manager_event_set_mode_5:
            mode = anc_mode_5;
            break;
        case anc_state_manager_event_set_mode_6:
            mode = anc_mode_6;
            break;
        case anc_state_manager_event_set_mode_7:
            mode = anc_mode_7;
            break;
        case anc_state_manager_event_set_mode_8:
            mode = anc_mode_8;
            break;
        case anc_state_manager_event_set_mode_9:
            mode = anc_mode_9;
            break;
        case anc_state_manager_event_set_mode_10:
            mode = anc_mode_10;
            break;
        case anc_state_manager_event_set_mode_1:
        default:
            break;
    }
    return mode;
}

static anc_state_manager_event_id_t getSetModeEventFromMode(anc_mode_t mode)
{
    anc_state_manager_event_id_t state_event = anc_state_manager_event_set_mode_1;
    
    switch(mode)
    {
        case anc_mode_2:
            state_event = anc_state_manager_event_set_mode_2;
            break;
        case anc_mode_3:
            state_event = anc_state_manager_event_set_mode_3;
            break;
        case anc_mode_4:
            state_event = anc_state_manager_event_set_mode_4;
            break;
        case anc_mode_5:
            state_event = anc_state_manager_event_set_mode_5;
            break;
        case anc_mode_6:
            state_event = anc_state_manager_event_set_mode_6;
            break;
        case anc_mode_7:
            state_event = anc_state_manager_event_set_mode_7;
            break;
        case anc_mode_8:
            state_event = anc_state_manager_event_set_mode_8;
            break;
        case anc_mode_9:
            state_event = anc_state_manager_event_set_mode_9;
            break;
        case anc_mode_10:
            state_event = anc_state_manager_event_set_mode_10;
            break;
        case anc_mode_1:
        default:
            break;
    }
    return state_event;
}

static bool ancStateManager_UpdateState(bool new_anc_state)
{
    bool current_anc_state = AncStateManager_IsEnabled();
    bool status = FALSE;
    DEBUG_LOG("ancStateManager_UpdateState: current state = %u, new state = %u", current_anc_state, new_anc_state);

    if(current_anc_state != new_anc_state)
    {
        if(new_anc_state)
        {
            AncStateManager_Enable();
        }
        else
        {
            AncStateManager_Disable();
        }
        status = TRUE;
    }

    return status;
}

static void ancStateManager_UpdateMode(uint8 new_anc_mode)
{
    uint8 current_anc_mode = AncStateManager_GetMode();
    DEBUG_LOG("ancStateManager_UpdateMode: current mode = %u, new mode = %u", current_anc_mode, new_anc_mode);

    if(current_anc_mode != new_anc_mode)
    {
        AncStateManager_SetMode(new_anc_mode);
    }
}

static void ancStateManager_UpdatePreviousMode(uint16 new_prev_mode)
{
    uint16 previous_mode = AncStateManager_GetPreviousMode();
    DEBUG_LOG("ancStateManager_UpdatePreviousMode: prev mode = %u, new prev mode = %u", previous_mode, new_prev_mode);

    if(previous_mode != new_prev_mode)
    {
        ancStateManager_SetPreviousMode(new_prev_mode);
    }
}

static void ancStateManager_UpdatePreviousConfig(uint16 new_prev_config)
{
    uint16 previous_config = AncStateManager_GetPreviousConfig();
    DEBUG_LOG("ancStateManager_UpdatePreviousConfig: previous_config = %u, new prev config = %u", previous_config, new_prev_config);

    if(previous_config != new_prev_config)
    {
        ancStateManager_SetPreviousConfig(new_prev_config);
    }
}

static void ancStateManager_StoreAndUpdateAncLeakthroughGain(uint8 new_anc_leakthrough_gain)
{
    uint8 current_anc_leakthrough_gain = AncStateManager_GetAncGain();
    DEBUG_LOG("ancStateManager_StoreAndUpdateAncLeakthroughGain: current anc leakthrough gain  = %u, new anc leakthrough gain  = %u", current_anc_leakthrough_gain, new_anc_leakthrough_gain);

    if(current_anc_leakthrough_gain != new_anc_leakthrough_gain)
    {
        AncStateManager_StoreAncLeakthroughGain(new_anc_leakthrough_gain);
        ancStateManager_HandleEvent(anc_state_manager_event_set_anc_leakthrough_gain);
    }
}

static void ancStateManager_UpdateAncToggleWayConfig(anc_toggle_way_config_id_t id, anc_toggle_config_t new_config)
{
    anc_toggle_config_t current_config = AncStateManager_GetAncToggleConfiguration(id);
    DEBUG_LOG("ancStateManager_UpdateAncToggleWayConfig: current config = %u, new config = %u",
                    current_config, new_config);

    if(current_config != new_config)
    {
        AncStateManager_SetAncToggleConfiguration(id, new_config);
    }
}

static void ancStateManager_UpdateAncScenarioConfig(anc_scenario_config_id_t id, anc_toggle_config_t new_config)
{
    anc_toggle_config_t current_config = AncStateManager_GetAncScenarioConfiguration(id);
    DEBUG_LOG("ancStateManager_UpdateAncScenarioConfig: current config = %u, new config = %u",
                    current_config, new_config);

    if(current_config != new_config)
    {
        AncStateManager_SetAncScenarioConfiguration(id, new_config);
    }
}

static void ancStateManager_UpdateDemoState(bool new_state)
{
    anc_toggle_config_t current_state = AncStateManager_IsDemoStateActive();
    DEBUG_LOG("ancStateManager_UpdateDemoState: current state = %u, new state = %u",
                    current_state, new_state);

    if(current_state != new_state)
    {
        AncStateManager_SetDemoState(new_state);
    }
}

static void ancStateManager_UpdateAdaptivityStatus(bool new_state)
{
    anc_toggle_config_t current_state = AncStateManager_GetAdaptiveAncAdaptivity();
    DEBUG_LOG("ancStateManager_UpdateAdaptivityStatus: current state = %u, new state = %u",
                    current_state, new_state);

    if(current_state != new_state)
    {
        if(new_state)
        {
            AncStateManager_EnableAdaptiveAncAdaptivity();
        }
        else
        {
            AncStateManager_DisableAdaptiveAncAdaptivity();
        }
    }
}

static void ancStateManager_StoreAndUpdateWorldVolumeInfo(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_sm_world_volume_gain_balance_info_t current_world_volume_balance_info;
    anc_sm_world_volume_gain_balance_info_t new_world_volume_balance_info;

    DEBUG_LOG("ancStateManager_StoreAndUpdateWorldVolumeInfo");

    AncStateManager_GetCurrentBalanceInfo(&current_world_volume_balance_info);
    new_world_volume_balance_info = anc_sm->reconnection_data->balance_info;

    if(current_world_volume_balance_info.balance_device_side != new_world_volume_balance_info.balance_device_side ||
            current_world_volume_balance_info.balance_percentage != new_world_volume_balance_info.balance_percentage)
    {
        anc_sm->balance_info.balance_percentage = new_world_volume_balance_info.balance_percentage;
        anc_sm->balance_info.balance_device_side = new_world_volume_balance_info.balance_device_side;

        ancStateManager_MsgRegisteredClientsOnWorldVolumeBalanceUpdate(anc_sm->balance_info.balance_device_side, anc_sm->balance_info.balance_percentage);
    }

    AncStateManager_StoreWorldVolumeGain(anc_sm->reconnection_data->world_volume_gain_dB);
    ancStateManager_HandleEvent(anc_state_manager_event_set_world_volume_gain);
}

static void ancStateManager_StoreAndUpdateWorldVolumeGain(int8 new_world_volume_gain_dB)
{
    int8 current_world_volume_gain_dB = 0;

    AncStateManager_GetCurrentWorldVolumeGain(&current_world_volume_gain_dB);
    DEBUG_LOG("ancStateManager_StoreAndUpdateWorldVolumeGain: current world volume gain  = %d, new world volume gain  = %d", current_world_volume_gain_dB, new_world_volume_gain_dB);

    if(current_world_volume_gain_dB != new_world_volume_gain_dB)
    {
        AncStateManager_StoreWorldVolumeGain(new_world_volume_gain_dB);
        ancStateManager_HandleEvent(anc_state_manager_event_set_world_volume_gain);
    }
}

static bool ancStateManager_InternalSetMode(const ANC_STATE_MANAGER_INTERNAL_EVENT_MODE_CHANGE_T* msg)
{
    anc_state_manager_event_id_t state_event = getSetModeEventFromMode(msg->new_mode);
    ancStateManager_PreAncModeChange();

    if (ancStateManager_HandleEvent(state_event))
    {
        AancQuietMode_ResetQuietModeData();
        return TRUE;
    }
    return FALSE;
}

/*mode is the ANC modes set by GAIA, in the range 1 to 10*/
static bool AncIsModeValid(uint16 mode)
{
    if( (mode > 0) && (mode <= AncStateManager_GetNumberOfModes()))
        return TRUE;
    else
        return FALSE;
}

static uint16 ancStateManager_GetNextToggleMode(void)
{    
    anc_state_manager_data_t *anc_sm_data = GetAncData();
    uint16 next_mode=anc_sm_data->toggle_configurations.anc_toggle_way_config[0];
    uint16 temp_mode=0;
    uint16 index=0, id=0;

    for (index;
         (index<ANC_MAX_TOGGLE_CONFIG) && (anc_sm_data->toggle_configurations.anc_toggle_way_config[index]!=ANC_TOGGLE_NOT_CONFIGURED);
         index++)
    {
        /* Current mode ranges from 0 to MAX-1, whereas toggle config ranges from 1 to MAX.
                Here, MAX refers to maximum number of ANC modes supported*/
        if (((anc_sm_data->current_mode)+1) == anc_sm_data->toggle_configurations.anc_toggle_way_config[index])
        {       
            id = (index==ANC_MAX_TOGGLE_CONFIG-1)?(0):(index+1);/*wrap around*/
            temp_mode = anc_sm_data->toggle_configurations.anc_toggle_way_config[id];
            if ((AncIsModeValid(temp_mode)) || (temp_mode==ANC_TOGGLE_CONFIGURED_OFF))
            {
                next_mode = temp_mode;
                break;
            }
        }
    }        
    DEBUG_LOG("ancStateManager_GetNextToggleMode Current mode enum:anc_mode_t:%d, Next Mode enum:anc_mode_t:%d", anc_sm_data->current_mode, next_mode);
    return next_mode;
}

static uint16 ancStateManager_GetFirstValidModeFromToggleConfigOff(void)
{    
    anc_state_manager_data_t *anc_sm_data = GetAncData();
    uint16 next_mode=anc_sm_data->toggle_configurations.anc_toggle_way_config[0];
    uint16 index=0, id=0;

    /*Get the first OFF config in the toggle config. This will ensure to start the next mode from a valid mode config post OFF*/
    for (index;
         (index<ANC_MAX_TOGGLE_CONFIG) && (anc_sm_data->toggle_configurations.anc_toggle_way_config[index]!=ANC_TOGGLE_NOT_CONFIGURED);
         index++)
    {
        if (anc_sm_data->toggle_configurations.anc_toggle_way_config[index]==ANC_TOGGLE_CONFIGURED_OFF)
        {
            id = (index==ANC_MAX_TOGGLE_CONFIG-1)?(0):(index+1);/*wrap around*/
            
            if (AncIsModeValid(anc_sm_data->toggle_configurations.anc_toggle_way_config[id]))
            {            
                next_mode = anc_sm_data->toggle_configurations.anc_toggle_way_config[id];
                break;
            }
        }
    }
    DEBUG_LOG("ancStateManager_GetFirstValidModeFromToggleConfigOff Next Mode %d", next_mode);
    return next_mode;
}

static void ancStateManager_ApplyConfigInEnabled(uint16 toggle_config)
{
    if (toggle_config==ANC_TOGGLE_CONFIGURED_OFF)
    {
        AncStateManager_Disable();
    }
    else
    {
        if (AncIsModeValid(toggle_config))
        {
            AncStateManager_SetMode((anc_mode_t)(toggle_config-1));
        }
    }
}

static void ancStateManager_ApplyConfigInDisabled(uint16 toggle_config)
{
    if (AncIsModeValid(toggle_config))
    {
        AncStateManager_SetMode((anc_mode_t)(toggle_config-1));
        AncStateManager_Enable();
    }
}


/*Toggle option can be exercised by the user during standalone or concurrency use cases*/
/*If ANC is already enabled, go to the next toggle behaviour and take appropriate action*/    
/*If ANC is disabled, accept this as a trigger to enable ANC in the first valid mode*/
static void ancStateManager_HandleToggleWay(void)
{
    uint16 config=ANC_TOGGLE_NOT_CONFIGURED;

    config = (AncStateManager_IsEnabled())?(ancStateManager_GetNextToggleMode()):(ancStateManager_GetFirstValidModeFromToggleConfigOff());
    ancStateManager_SetPreviousConfig(config);
    ancStateManager_MsgRegisteredClientsOnPrevConfigUpdate();
    ancStateManager_ApplyConfig(config);
}


static void ancStateManager_HandleWorldVolume(anc_state_manager_event_id_t world_volume_event)
{
    switch(world_volume_event)
    {
        case anc_state_manager_event_world_volume_up:
            KymeraAncCommon_AncCompanderMakeupGainVolumeUp();
            ancStateManager_ClearLock(world_volume_event);
        break;

        case anc_state_manager_event_world_volume_down:
            KymeraAncCommon_AncCompanderMakeupGainVolumeDown();
            ancStateManager_ClearLock(world_volume_event);
        break;

        default:
        break;
    }

}
/******************************************************************************
DESCRIPTION
    Set leakthrough gain for parallel Anc filter configuration.

*/
static void setLeakthroughGainForParallelAncFilter(uint8 gain)
{
    anc_path_enable anc_path = appConfigAncPathEnable();

    DEBUG_LOG_FN_ENTRY("setLeakthroughGainForParallelAncFilter: %d \n", gain);

    if(AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
    {
        switch(anc_path)
        {
            case hybrid_mode_left_only:
                if(!AncConfigureParallelFilterFFBPathGain(gain,gain))
                {
                   DEBUG_LOG_INFO("setLeakthroughGainForParallelAncFilter failed for hybrid mode left only configuration!");
                }
                break;

            case feed_forward_mode_left_only:
                if(!AncConfigureParallelFilterFFAPathGain(gain,gain))
                {
                   DEBUG_LOG_INFO("setLeakthroughGainForParallelAncFilter failed for feed forward mode configuration!");
                }
                break;

            default:
                DEBUG_LOG_INFO("setLeakthroughGainForParallelAncFilter, cannot set Anc Leakthrough gain for anc_path:  %u", anc_path);
            break;
        }
    }
    else
    {
        DEBUG_LOG_INFO("Anc Leakthrough gain cannot be set in mode 0!");
    }
}

/******************************************************************************
DESCRIPTION
    Set the leakthrough gain for single Anc filter configuration

*/
static void setLeakthroughGainForSingleAncFilter(uint8 gain)
{
    anc_path_enable anc_path = appConfigAncPathEnable();

    DEBUG_LOG_FN_ENTRY("setLeakthroughGainForSingleAncFilter: %d \n",gain);

    if(AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
    {
        switch(anc_path)
        {
            case hybrid_mode:
                if(!(AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_0, gain) && AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_1, gain)))
                {
                    DEBUG_LOG_INFO("setLeakthroughGainForSingleAncFilter failed for hybrid mode configuration!");
                }
                break;

            case hybrid_mode_left_only:
                if(!(AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_0, gain)))
                {
                    DEBUG_LOG_INFO("setLeakthroughGainForSingleAncFilter failed for hybrid mode left only configuration!");
                }
                break;

            case feed_forward_mode:
                if(!(AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_0, gain) && AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_1, gain)))
                {
                    DEBUG_LOG_INFO("setLeakthroughGainForSingleAncFilter failed for feed forward mode configuration!");
                }
                break;

            case feed_forward_mode_left_only:
                if(!(AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_0, gain)))
                {
                    DEBUG_LOG_INFO("setLeakthroughGainForSingleAncFilter failed for feed forward mode left only configuration!");
                }
                break;
            default:
                DEBUG_LOG_INFO("setLeakthroughGainForSingleAncFilter, cannot set Anc Leakthrough gain for anc_path:  %u", anc_path);
                break;
        }
    }
}

/******************************************************************************
DESCRIPTION
    Set ANC Leakthrough gain for FeedForward path
    FFA path is used in FeedForward mode and FFB path in Hybrid mode
    ANC Leakthrough gain is applicable in only Leakthrough mode
*/
static void setAncLeakthroughGain(void)
{

    anc_state_manager_data_t *anc_sm = GetAncData();
    uint8 gain=  anc_sm->anc_gain;

    if(appKymeraIsParallelAncFilterEnabled())
    {
        setLeakthroughGainForParallelAncFilter(gain);
    }
    else
    {
        setLeakthroughGainForSingleAncFilter(gain);
    }
}

/*! \brief To identify if local device is left, incase of earbud application. */
static bool ancstateManager_IsLocalDeviceLeft(void)
{
    bool isLeft = TRUE;

#ifndef INCLUDE_STEREO
    isLeft = Multidevice_IsLeft();
#endif

    return isLeft;
}

#ifdef ENABLE_ADAPTIVE_ANC

/****************************************************************************
DESCRIPTION
    Stop aanc gain timer

RETURNS
    None
*/
static void ancStateManager_StopAancGainTimer(void)
{
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_read_aanc_gain_timer_expiry);
}

/****************************************************************************
DESCRIPTION
    Start aanc gain timer
    To read the FF and FB gain at regular intervals when AANC is enabled

RETURNS
    None
*/
static void ancStateManager_StartAancGainTimer(void)
{
    ancStateManager_StopAancGainTimer();

    MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_read_aanc_gain_timer_expiry,
                         NULL, ANC_SM_READ_AANC_GAIN_TIMER);
}

static void ancStateManager_SetAancFFGain(uint8 aanc_ff_gain)
{
    if (AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
    {
        anc_state_manager_data_t *anc_sm = GetAncData();
        anc_sm -> aanc_ff_gain = aanc_ff_gain;
    }
}

/*! \brief Notify Adaptive FF gain update to registered clients. */
static void ancstateManager_MsgRegisteredClientsOnFFGainUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Check if current mode is AANC mode and check if any of client registered */
    if (AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()) && (anc_sm->client_tasks))
    {
        MESSAGE_MAKE(ind, AANC_FF_GAIN_UPDATE_IND_T);
        ind->aanc_ff_gain = AncStateManager_GetAancFFGain();

        TaskList_MessageSend(anc_sm->client_tasks, AANC_FF_GAIN_UPDATE_IND, ind);
    }
}

/*! \brief Reads AANC FF gain from capability and stores it in anc data. Notifies ANC Clients and restarts timer.
                Timer will not be restarted if current mode is not Adaptive ANC mode. */
static void ancStateManager_HandleFFGainTimerExpiryEvent(void)
{
    if (AncStateManager_IsDemoStateActive()
            && AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode())
            && AncStateManager_IsEnabled())
    {
        uint8 aanc_ff_gain = AANC_GAIN_PASSIVE_ISOLATION;

        /* Read FF gain from AANC Capability, if active*/
        if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
        {
            KymeraAncCommon_GetFFGain(&aanc_ff_gain);
        }
        /*if AANC cap is active, store actual FF gain value in anc_data; if not, store passive isolation gain value*/
        ancStateManager_SetAancFFGain(aanc_ff_gain);

        /* restart the timer to read FF gain after specified time interval*/
        ancStateManager_StartAancGainTimer();

        /* Notifies ANC clients on FF gain update of local device*/
        ancstateManager_MsgRegisteredClientsOnFFGainUpdate();

        /* If secondary is in case, immediately notify ANC Clients with default Secondary gain */
        if (StateProxy_IsPeerInCase())
        {
            ancstateManager_MsgRegisteredClientsWithBothFFGains(AncStateManager_GetAancFFGain(), ANC_SM_DEFAULT_SECONDARY_FF_GAIN);
        }
    }
}

/*! \brief Starts/stops FF Gain timer based on ANC state and mode updates */
static void ancStateManager_ModifyGainTimerStatus(bool prev_anc_state, anc_mode_t prev_anc_mode, bool prev_adaptivity_status)
{
    if(AncStateManager_IsDemoStateActive())
    {
        bool aanc_enable; /* Adaptive ANC state*/
        bool cur_anc_state = anc_data.actual_enabled; /* Current ANC state*/
        anc_mode_t cur_anc_mode = anc_data.current_mode; /* Current ANC Mode */
        bool modify = FALSE;

        /* AANC mode is configured and ANC state has been changed*/
        if((cur_anc_state != prev_anc_state) && (AncConfig_IsAncModeAdaptive(cur_anc_mode)))
        {
            modify = TRUE;
        }
        /* Mode has been changed from AANC mode to non AANC mode or vice-versa; Mode is switched between
            two different adaptive anc modes and adaptivity is paused on previous mode */
        else if((cur_anc_mode != prev_anc_mode) &&
                ((AncConfig_IsAncModeAdaptive(cur_anc_mode) && !AncConfig_IsAncModeAdaptive(prev_anc_mode)) ||
                 (!AncConfig_IsAncModeAdaptive(cur_anc_mode) && AncConfig_IsAncModeAdaptive(prev_anc_mode)) ||
                 (AncConfig_IsAncModeAdaptive(cur_anc_mode) && AncConfig_IsAncModeAdaptive(prev_anc_mode) && !prev_adaptivity_status)))
        {
            modify = TRUE;
        }

        if(AncConfig_IsAdvancedAnc())
        {
            modify = TRUE;
        }

        if(modify)
        {
            /* Identify Adaptive ANC state based on current ANC state and current ANC mode*/
            aanc_enable = ((cur_anc_state) && (AncConfig_IsAncModeAdaptive(cur_anc_mode)));
            if(!aanc_enable && AncStateManager_IsFBGainTimerUpdateRequire(AncStateManager_GetCurrentMode()))
            {
                aanc_enable = cur_anc_state;
            }

            /* Start/stop AANC gain timer based on AANC is enabled/disbaled*/
            aanc_enable ? ancStateManager_StartAancGainTimer() : ancStateManager_StopAancGainTimer();
        }
    }
}

static void ancStateManager_SetAancFBGain(uint8 aanc_fb_gain)
{
    if (AncConfig_IsAdvancedAnc())
    {
        anc_state_manager_data_t *anc_sm = GetAncData();
        anc_sm -> aanc_fb_gain = aanc_fb_gain;
    }
}

/*! \brief Notify Adaptive FB gain update to registered clients. */
static void ancstateManager_MsgRegisteredClientsOnFBGainUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Check if current mode is AANC mode and check if any of client registered */
    if (AncConfig_IsAdvancedAnc() && (anc_sm->client_tasks))
    {
        MESSAGE_MAKE(ind, AANC_FB_GAIN_UPDATE_IND_T);
        ind->aanc_fb_gain = AncStateManager_GetAancFBGain();

        TaskList_MessageSend(anc_sm->client_tasks, AANC_FB_GAIN_UPDATE_IND, ind);
    }
}

/*! \brief Reads AANC FB gain from capability and stores it in anc data. Notifies ANC Clients and restarts timer.
                Timer will not be restarted if current mode is not does not support FB GAIN update. */
static void ancStateManager_HandleFBGainTimerExpiryEvent(void)
{
    if (AncStateManager_IsDemoStateActive()
            && AncConfig_IsAdvancedAnc()
            && AncStateManager_IsEnabled())
    {
        uint8 aanc_fb_gain = AANC_GAIN_PASSIVE_ISOLATION;

        /* Read FB gain from AANC Capability, if active*/
        if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
        {
            KymeraAncCommon_GetFBGain(&aanc_fb_gain);
        }
        /*if AANC cap is active, store actual FF gain value in anc_data; if not, store passive isolation gain value*/
        ancStateManager_SetAancFBGain(aanc_fb_gain);

        /* restart the timer to read FF gain after specified time interval*/
        ancStateManager_StartAancGainTimer();

        /* Notifies ANC clients on FF gain update of local device*/
        ancstateManager_MsgRegisteredClientsOnFBGainUpdate();

        /* If secondary is in case, immediately notify ANC Clients with default Secondary gain */
        if (StateProxy_IsPeerInCase())
        {
            ancstateManager_MsgRegisteredClientsWithBothFBGains(AncStateManager_GetAancFBGain(), ANC_SM_DEFAULT_SECONDARY_FF_GAIN);
        }
    }
}
#endif

static int8 ancStateManager_GetMinWorldVolumedBGain(void)
{
    return ancConfigMinWorldVolumeGaindB();
}

static int8 ancStateManager_GetMaxWorldVolumedBGain(void)
{
    return ancConfigMaxWorldVolumeGaindB();
}

/*! \brief Notify Anc state update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnStateUpdate(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        message_id = enable ? ANC_UPDATE_STATE_ENABLE_IND : ANC_UPDATE_STATE_DISABLE_IND;

        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

/*! \brief Notify Anc mode update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnModeUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_UPDATE_MODE_CHANGED_IND_T);
        ind->mode = AncStateManager_GetCurrentMode();

        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_MODE_CHANGED_IND, ind);
    }
}

static void ancStateManager_MsgRegisteredClientsOnModeUpdateInDisabledState(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_UPDATE_MODE_CHANGED_IND_T);
        ind->mode = anc_data.requested_mode;

        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_MODE_CHANGED_IND, ind);
    }
}

/*! \brief Notify Anc prev mode update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnPrevModeUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks)
    {
        MESSAGE_MAKE(ind, ANC_UPDATE_PREV_MODE_IND_T);
        ind->previous_mode = anc_sm->previous_mode;

        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_PREV_MODE_IND, ind);
    }
}

/*! \brief Notify Anc prev config update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnPrevConfigUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks)
    {
        MESSAGE_MAKE(ind, ANC_UPDATE_PREV_CONFIG_IND_T);
        ind->previous_config = anc_sm->previous_config;

        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_PREV_CONFIG_IND, ind);
    }
}


/*! \brief Notify Anc gain update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnGainUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_UPDATE_GAIN_IND_T);
        ind->anc_gain = AncStateManager_GetAncGain();
        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_GAIN_IND, ind);
    }
}

/*! \brief Notify Anc FF gains of both devices to registered clients. */
static void ancstateManager_MsgRegisteredClientsWithBothFFGains(uint8 local_ff_gain, uint8 remote_ff_gain)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if (anc_sm->client_tasks)  /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_FF_GAIN_NOTIFY_T);

        if(ancstateManager_IsLocalDeviceLeft())
        {
            ind->left_ff_gain = local_ff_gain;
            ind->right_ff_gain = remote_ff_gain;
        }
        else
        {
            ind->left_ff_gain = remote_ff_gain;
            ind->right_ff_gain = local_ff_gain;
        }

        TaskList_MessageSend(anc_sm->client_tasks, ANC_FF_GAIN_NOTIFY, ind);
    }
}

static void ancstateManager_MsgRegisteredClientsWithBothFBGains(uint8 local_fb_gain, uint8 remote_fb_gain)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if (anc_sm->client_tasks)  /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_FB_GAIN_NOTIFY_T);

        if(ancstateManager_IsLocalDeviceLeft())
        {
            ind->left_fb_gain = local_fb_gain;
            ind->right_fb_gain = remote_fb_gain;
        }
        else
        {
            ind->left_fb_gain = remote_fb_gain;
            ind->right_fb_gain = local_fb_gain;
        }

        TaskList_MessageSend(anc_sm->client_tasks, ANC_FB_GAIN_NOTIFY, ind);
    }
}

/*! \brief Notify Anc toggle configuration update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnAncToggleConfigurationUpdate(anc_toggle_way_config_id_t config_id, anc_toggle_config_t config)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks)
    {
        MESSAGE_MAKE(ind, ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T);
        ind->anc_toggle_config_id = config_id;
        ind->anc_config = config;
        TaskList_MessageSend(anc_sm->client_tasks, ANC_TOGGLE_WAY_CONFIG_UPDATE_IND, ind);
    }
}

/*! \brief Notify Anc scenario configuration update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnAncScenarioConfigurationUpdate(anc_scenario_config_id_t config_id, anc_toggle_config_t config)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks)
    {
        MESSAGE_MAKE(ind, ANC_SCENARIO_CONFIG_UPDATE_IND_T);
        ind->anc_scenario_config_id = config_id;
        ind->anc_config = config;
        TaskList_MessageSend(anc_sm->client_tasks, ANC_SCENARIO_CONFIG_UPDATE_IND, ind);
    }
}

/*! \brief Notify Adaptive Anc gain adaptivity status update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnAdaptiveAncAdaptivityUpdate(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        message_id = enable ? ANC_UPDATE_AANC_ADAPTIVITY_RESUMED_IND : ANC_UPDATE_AANC_ADAPTIVITY_PAUSED_IND;

        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

/*! \brief Notify Demo state update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnDemoStateUpdate(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        message_id = enable ? ANC_UPDATE_DEMO_MODE_ENABLE_IND : ANC_UPDATE_DEMO_MODE_DISABLE_IND;

        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

/*! \brief Notify Wind Detection state update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnWindDetectionStateUpdate(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;

    if(anc_sm->client_tasks)
    {
        message_id = enable ? ANC_UPDATE_WIND_DETECTION_ENABLE_IND : ANC_UPDATE_WIND_DETECTION_DISABLE_IND;
        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

/*! \brief Notify Wind Reduction update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnWindReductionUpdate(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;
    
    if (anc_sm->client_tasks)
    {    
        message_id = enable ? ANC_UPDATE_WIND_REDUCTION_ENABLE_IND : ANC_UPDATE_WIND_REDUCTION_DISABLE_IND;
        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

static void ancStateManager_MsgRegisteredClientsOnHowlingDetectionStateUpdate(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;

    if(anc_sm->client_tasks)
    {
        message_id = enable ? ANC_UPDATE_HOWLING_DETECTION_ENABLE_IND : ANC_UPDATE_HOWLING_DETECTION_DISABLE_IND;
        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

/*! \brief Notify Anc gain update to registered clients for static use cases when moving in and out of windy mode. */
static void ancstateManager_MsgRegisteredClientsWindFFGainUpdateInd(bool windy)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Check if current mode is static mode and check if any of client registered */
    if (AncConfig_IsAncModeStatic(AncStateManager_GetCurrentMode()) && (anc_sm->client_tasks))
    {
        uint8 ff_gain = 0;
        
        MESSAGE_MAKE(ind, ANC_UPDATE_GAIN_IND_T);
        if (windy)
        {
             KymeraAncCommon_GetFFGain(&ff_gain);/*AHM gain*/
        }
        else
        {
            ff_gain = AncStateManager_GetAncGain();/*Stored static gain*/
        }
        
        ind->anc_gain = ff_gain;
        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_GAIN_IND, ind);
    }
}

/*! \brief Notify world volume gain update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnWorldVolumeGainUpdate(int8 cur_gain_dB)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_WORLD_VOLUME_GAIN_DB_UPDATE_IND_T);
        ind->world_volume_gain_dB = cur_gain_dB;
        TaskList_MessageSend(anc_sm->client_tasks, ANC_WORLD_VOLUME_GAIN_DB_UPDATE_IND, ind);
    }
}

/*! \brief Notify world volume balance update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnWorldVolumeBalanceUpdate(bool balance_device_side, bool balance_percentage)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_WORLD_VOLUME_BALANCE_UPDATE_IND_T);
        ind->balance_device_side = balance_device_side;
        ind->balance_percentage = balance_percentage;
        TaskList_MessageSend(anc_sm->client_tasks, ANC_WORLD_VOLUME_BALANCE_UPDATE_IND, ind);
    }
}

/*! \brief Notify world volume balance update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnWorldVolumeConfigUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_WORLD_VOLUME_CONFIG_UPDATE_IND_T);
        ind->min_gain_dB = ancStateManager_GetMinWorldVolumedBGain();
        ind->max_gain_dB = ancStateManager_GetMaxWorldVolumedBGain();

        TaskList_MessageSend(anc_sm->client_tasks, ANC_WORLD_VOLUME_CONFIG_UPDATE_IND, ind);
    }
}

static void ancStateManager_HandleReconnectionDataBeforeAncStateUpdate(void)
{
    anc_sm_reconnection_data* reconnection_data = ancStateManager_GetReconnectionData();

    ancStateManager_UpdateMode(reconnection_data->mode);
    
    ancStateManager_UpdatePreviousConfig(reconnection_data->previous_config);
    ancStateManager_UpdatePreviousMode(reconnection_data->previous_mode);

    ancStateManager_UpdateAncToggleWayConfig(anc_toggle_way_config_id_1,
                                             reconnection_data->toggle_configurations.anc_toggle_way_config[0]);
    ancStateManager_UpdateAncToggleWayConfig(anc_toggle_way_config_id_2,
                                             reconnection_data->toggle_configurations.anc_toggle_way_config[1]);
    ancStateManager_UpdateAncToggleWayConfig(anc_toggle_way_config_id_3,
                                             reconnection_data->toggle_configurations.anc_toggle_way_config[2]);

    ancStateManager_UpdateAncScenarioConfig(anc_scenario_config_id_standalone,
                                            reconnection_data->standalone_config);
    ancStateManager_UpdateAncScenarioConfig(anc_scenario_config_id_playback,
                                            reconnection_data->playback_config);
    ancStateManager_UpdateAncScenarioConfig(anc_scenario_config_id_sco,
                                            reconnection_data->sco_config);
    ancStateManager_UpdateAncScenarioConfig(anc_scenario_config_id_va,
                                            reconnection_data->va_config);
}

static void ancStateManager_HandleReconnectionDataAfterAncStateUpdate(void)
{
    anc_sm_reconnection_data* reconnection_data = ancStateManager_GetReconnectionData();

    ancStateManager_StoreAndUpdateAncLeakthroughGain(reconnection_data->gain);
    ancStateManager_UpdateDemoState(reconnection_data->anc_demo_state);
    ancStateManager_UpdateAdaptivityStatus(reconnection_data->adaptivity);
    ancStateManager_StoreAndUpdateWorldVolumeInfo();
}

static void ancStateManager_HandleAncReconnectionData(const STATE_PROXY_RECONNECTION_ANC_DATA_T* reconnection_data)
{
    bool update = TRUE;

    ancStateManager_SetReconnectionData((anc_sm_reconnection_data*)reconnection_data);

    ancStateManager_HandleReconnectionDataBeforeAncStateUpdate();

    if (ancStateManager_UpdateState(reconnection_data->state) && ancConfigIsAdvancedAdaptiveAnc())
    {
        /* Incase of advanced adaptive ANC, enable ANC is not a blocking call. Rest of the reconnection data will be updated on
           receivng anc_state_manager_event_capability_enable_complete message */
        update = FALSE;
    }

    if (update)
    {
        ancStateManager_HandleReconnectionDataAfterAncStateUpdate();
        ancStateManager_ResetReconnectionData();
    }
}

static void ancStateManager_HandleStateProxyRemoteAncUpdate(const STATE_PROXY_ANC_DATA_T* anc_data)
{
    switch(anc_data->msg_id)
    {
        case state_proxy_anc_msg_id_gain:
            {
                if(StateProxy_IsPrimary())
                {
                    ancstateManager_MsgRegisteredClientsWithBothFFGains(AncStateManager_GetAncGain(), anc_data->msg.gain.anc_gain);
                }
            }
            break;

        case state_proxy_anc_msg_id_toggle_config:
            {
                ancStateManager_UpdateAncToggleWayConfig(anc_data->msg.toggle_config.anc_toggle_config_id,
                                                         anc_data->msg.toggle_config.anc_config);
            }
            break;

        case state_proxy_anc_msg_id_scenario_config:
            {
                ancStateManager_UpdateAncScenarioConfig(anc_data->msg.scenario_config.anc_scenario_config_id,
                                                         anc_data->msg.scenario_config.anc_config);
            }
            break;

        case state_proxy_anc_msg_id_demo_state_disable:
            {
                if(!StateProxy_IsPrimary())
                {
                    ancStateManager_UpdateDemoState(FALSE);
                }
            }
            break;

        case state_proxy_anc_msg_id_demo_state_enable:
            {
                if(!StateProxy_IsPrimary())
                {
                    ancStateManager_UpdateDemoState(TRUE);
                }
            }
            break;

        case state_proxy_anc_msg_id_world_volume_gain:
            {
                if(!StateProxy_IsPrimary())
                {
                    /* After reconnection, update world volume gain of secondary device if it is different from that of primary device */
                    ancStateManager_StoreAndUpdateWorldVolumeGain(anc_data->msg.world_volume_gain.world_volume_gain_dB);
                }
            }
            break;

        case state_proxy_anc_msg_id_prev_config:
            {
                if(!StateProxy_IsPrimary())
                {
                    /* After reconnection, update previous config of secondary device if it is different from that of primary device */
                    ancStateManager_SetPreviousConfig(anc_data->msg.prev_config.previous_config);
                }
            }
            break;
            
        case state_proxy_anc_msg_id_prev_mode:
            {
                if(!StateProxy_IsPrimary())
                {
                    /* After reconnection, update previous mode of secondary device if it is different from that of primary device */
                   ancStateManager_SetPreviousMode(anc_data->msg.prev_mode.previous_mode);
                }
            }
            break;
            
        case state_proxy_aanc_msg_id_fb_gain:
            if(StateProxy_IsPrimary())
            {
                ancstateManager_MsgRegisteredClientsWithBothFBGains(AncStateManager_GetAancFBGain(), anc_data->msg.fb_gain.aanc_fb_gain);
            }
            break;

        case state_proxy_anc_msg_id_reconnection:
            {
                ancStateManager_HandleAncReconnectionData(&anc_data->msg.reconnection_data);
            }
            break;

        default:
            break;
    }
}

static void ancStateManager_HandleStateProxyEvent(const STATE_PROXY_EVENT_T* event)
{
    switch(event->type)
    {
    //Message sent by state proxy - on remote device for update.
        case state_proxy_event_type_anc:
            DEBUG_LOG_INFO("ancStateManager_HandleStateProxyEvent: state proxy anc sync");
            if (!StateProxy_IsPeerInCase() && event->source == state_proxy_source_remote)
            {
                ancStateManager_HandleStateProxyRemoteAncUpdate(&event->event.anc_data);
            }
        break;

#ifdef ENABLE_ADAPTIVE_ANC
        case state_proxy_event_type_aanc_logging:
            /* received FF Gain from remote device, Update ANC Clients with local and remote FF Gains */
            ancstateManager_MsgRegisteredClientsWithBothFFGains(AncStateManager_GetAancFFGain(), event->event.aanc_logging.aanc_ff_gain);
        break;

        case state_proxy_event_type_phystate:
            DEBUG_LOG_INFO("ancStateManager_HandleStateProxyEvent: state_proxy_event_type_phystate");
            /* Checking if peer has gone incase. If yes, update ANC clients with default FF gain irrespective of timer expiry */
            if ((event->source == state_proxy_source_remote) && (event->event.phystate.new_state == PHY_STATE_IN_CASE))
            {
                if (AncStateManager_IsEnabled() && AncConfig_IsAncModeAdaptive(anc_data.current_mode))
                {
                    ancstateManager_MsgRegisteredClientsWithBothFFGains(AncStateManager_GetAancFFGain(), ANC_SM_DEFAULT_SECONDARY_FF_GAIN);
                    /*Restart the timer*/
                    ancStateManager_StartAancGainTimer();
                }
            }
        break;

        case state_proxy_event_type_aanc_fb_gain_logging:
            {
                /* received FB Gain from remote device, Update ANC Clients with local and remote FB Gains */
                ancstateManager_MsgRegisteredClientsWithBothFBGains(AncStateManager_GetAancFBGain(), event->event.aanc_fb_gain_logging.aanc_fb_gain);
            }
        break;
#endif

        case state_proxy_event_type_aanc:
             DEBUG_LOG_INFO("ancStateManager_HandleStateProxyEvent: state proxy aanc sync");
             if (!StateProxy_IsPeerInCase())
             {
                 AancQuietMode_HandleQuietModeRx(&event->event.aanc_data);
             }
        break;

        default:
            break;
    }
}


static void ancStateManager_HandlePhyStateChangedInd(PHY_STATE_CHANGED_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_HandlePhyStateChangedInd  new state %d, event %d ", ind->new_state, ind->event);

    if ((anc_data.actual_enabled) && (anc_data.state==anc_state_manager_enabled))
    {
        switch(ind->new_state)
        {
            case PHY_STATE_IN_EAR:
            {
                if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
                    KymeraAncCommon_UpdateInEarStatus();
            }
            break;

            case PHY_STATE_OUT_OF_EAR:
            case PHY_STATE_OUT_OF_EAR_AT_REST:
            {
                /* Do-nothing */
            }
            break;
                
            case PHY_STATE_IN_CASE:
                AncStateManager_Disable();
                break;

            default:
                break;
        }
    }
}

#ifndef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
static uint16 ancStateManager_GetUsbSampleRate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm->usb_sample_rate;
}

static void ancStateManager_SetUsbSampleRate(uint16 usb_sample_rate)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_sm->usb_sample_rate = usb_sample_rate;
}
#endif

static void ancstateManager_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case STATE_PROXY_EVENT:
            ancStateManager_HandleStateProxyEvent((const STATE_PROXY_EVENT_T*)message);
            break;

        case anc_state_manager_event_config_timer_expiry:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
            break;

        case PHY_STATE_CHANGED_IND:
            ancStateManager_HandlePhyStateChangedInd((PHY_STATE_CHANGED_IND_T*)message);
            break;

        case KYMERA_AANC_QUIET_MODE_TRIGGER_IND:
            ancStateManager_HandleEvent(anc_state_manager_event_aanc_quiet_mode_detected);
            break;

        case KYMERA_AANC_QUIET_MODE_CLEAR_IND:
            ancStateManager_HandleEvent(anc_state_manager_event_aanc_quiet_mode_not_detected);
            break;

        case anc_state_manager_internal_event_mode_change:
        {
            ancStateManager_SetLock(id);

            if (!ancStateManager_InternalSetMode((const ANC_STATE_MANAGER_INTERNAL_EVENT_MODE_CHANGE_T *)message))
            {
                DEBUG_LOG("Set ANC Mode event failed");
                ancStateManager_ClearLock(id);
            }
        }
        break;

        case anc_state_manager_internal_event_enable:
        {
            ancStateManager_SetLock(id);

            /* ENABLE ANC */
            if (!ancStateManager_HandleEvent(anc_state_manager_event_enable))
            {
                DEBUG_LOG("Enable ANC event failed");
                ancStateManager_ClearLock(id);
            }
        }
        break;

       case anc_state_manager_internal_event_disable:
           {
               ancStateManager_SetLock(id);

               /* Disable ANC */
               if (!ancStateManager_HandleEvent(anc_state_manager_event_disable))
               {
                   DEBUG_LOG("Disable ANC event failed");
                   ancStateManager_ClearLock(id);
               }
           }
           break;

        case anc_state_manager_event_world_volume_up:
        case anc_state_manager_event_world_volume_down:
        {
           ancStateManager_SetLock(id);

           if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
           {
                ancStateManager_ClearLock(id);
                DEBUG_LOG("World volume event failed\n");
           }
        }
        break;

        case anc_state_manager_event_activate_anc_tuning_mode:
        {
            ancStateManager_SetLock(id);

            if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
            {
                 ancStateManager_ClearLock(id);
                 DEBUG_LOG("Enter Tuning mode event failed\n");
            }
        }
        break;

        case anc_state_manager_event_deactivate_tuning_mode:
        {
            ancStateManager_SetLock(id);

            if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
            {
                 ancStateManager_ClearLock(id);
                 DEBUG_LOG("Exit Tuning mode event failed\n");
            }
        }
        break;

        case anc_state_manager_event_wind_detect:
        {
           ancStateManager_SetLock(id);

           if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
           {
                ancStateManager_ClearLock(id);
                DEBUG_LOG("Wind detect event failed\n");
           }
        }
        break;

        case anc_state_manager_event_wind_release:
        {
            ancStateManager_SetLock(id);

            if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
            {
                ancStateManager_ClearLock(id);
                DEBUG_LOG("Wind release event failed\n");
            }
         }
         break;

         case anc_state_manager_event_wind_enable:
         {
             ancStateManager_SetLock(id);

             if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
             {
                  ancStateManager_ClearLock(id);
                  DEBUG_LOG("Wind enable event failed\n");
             }
          }
          break;

          case anc_state_manager_event_wind_disable:
          {
              ancStateManager_SetLock(id);

              if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
              {
                  ancStateManager_ClearLock(id);
                  DEBUG_LOG("Wind disable event failed\n");
              }
          }
          break;

         case anc_state_manager_event_anti_howling_enable:
         {
             ancStateManager_SetLock(id);

             if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
             {
                 ancStateManager_ClearLock(id);
                 DEBUG_LOG("Anti-howling enable event failed\n");
             }
          }
          break;

          case anc_state_manager_event_anti_howling_disable:
          {
              ancStateManager_SetLock(id);

             if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
             {
                 ancStateManager_ClearLock(id);
                 DEBUG_LOG("Anti-howling disable event failed\n");
             }
          }
          break;

          case anc_state_manager_event_activate_adaptive_anc_tuning_mode:
          {
              ancStateManager_SetLock(id);

              if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
              {
                  ancStateManager_ClearLock(id);
                  DEBUG_LOG("Activate adaptive anc tuning event failed\n");
              }
          }
          case anc_state_manager_event_deactivate_adaptive_anc_tuning_mode:
          {
              ancStateManager_SetLock(id);

              if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
              {
                  ancStateManager_ClearLock(id);
                  DEBUG_LOG("Deactivate adaptive anc event failed\n");
              }
          }
          break;

          case anc_state_manager_event_set_anc_leakthrough_gain:
          {
              ancStateManager_SetLock(id);

              if(!ancConfigWorldVolumedBScale() && AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
              {
                  if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
                  {
                      ancStateManager_ClearLock(id);
                      DEBUG_LOG("Set leakthrough gain event failed\n");
                  }
               }
               else
               {
                  ancStateManager_ClearLock(id);
                  DEBUG_LOG("Set leakthrough gain event failed\n");
                }
            }
            break;

            case anc_state_manager_event_toggle_way:
            {
                ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
            }
            break;

             case anc_state_manager_event_set_world_volume_gain:
             {
                 ancStateManager_SetLock(id);

                 if(ancConfigWorldVolumedBScale() && AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
                 {
                     if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
                     {
                         ancStateManager_ClearLock(id);
                         DEBUG_LOG("Set world volume gain event failed\n");
                     }
                 }
                 else
                 {
                     ancStateManager_ClearLock(id);
                     DEBUG_LOG("Set world volume gain event failed\n");
                  }
              }
              break;

              case anc_state_manager_event_set_world_volume_balance:
              {
                  ancStateManager_SetLock(id);
                  if(ancConfigWorldVolumedBScale())
                  {
                      if(!ancStateManager_HandleEvent((anc_state_manager_event_id_t)id))
                      {
                           ancStateManager_ClearLock(id);
                           DEBUG_LOG("Set world volume balance event failed\n");
                      }
                  }
                  else
                  {
                      ancStateManager_ClearLock(id);
                      DEBUG_LOG("Set world volume balance event failed\n");
                  }
               }
               break;

        case anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry:
        case anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
            break;
            
        case anc_state_manager_event_aanc_quiet_mode_enable:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
        case USB_DEVICE_ENUMERATED:
        {
            ancStateManager_HandleEvent(anc_state_manager_event_usb_enumerated_start_tuning);
            anc_data.usb_enumerated = TRUE;
        }
        break;

        case USB_DEVICE_DECONFIGURED:
        {
            if(anc_data.usb_enumerated)
            {
                ancStateManager_HandleEvent(anc_state_manager_event_usb_detached_stop_tuning);
                anc_data.usb_enumerated = FALSE;
            }
        }
        break;
#else
        case MESSAGE_USB_ENUMERATED:
        {
            const MESSAGE_USB_ENUMERATED_T *m = (const MESSAGE_USB_ENUMERATED_T *)message;

            ancStateManager_SetUsbSampleRate(m -> sample_rate);

            anc_state_manager_event_id_t state_event = anc_state_manager_event_usb_enumerated_start_tuning;
            ancStateManager_HandleEvent(state_event);
        }
        break;

        case MESSAGE_USB_DETACHED:
        {
            anc_state_manager_event_id_t state_event = anc_state_manager_event_usb_detached_stop_tuning;
            ancStateManager_HandleEvent(state_event);
        }
        break;
#endif
#ifdef ENABLE_ADAPTIVE_ANC
        case anc_state_manager_event_read_aanc_gain_timer_expiry:
        {
            ancStateManager_HandleFFGainTimerExpiryEvent();
            if(AncStateManager_IsFBGainTimerUpdateRequire(AncStateManager_GetCurrentMode()))
            {
                ancStateManager_HandleFBGainTimerExpiryEvent();
            }
        }
            break;
#endif
        case anc_state_manager_event_aanc_quiet_mode_disable:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        case anc_state_manager_event_set_filter_path_gains:
        case anc_state_manager_event_set_filter_path_gains_on_mode_change:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        case anc_state_manager_event_concurrency_connect:
            ancStateManager_HandleConcurrencyConnect((const ANC_CONCURRENCY_CONNECT_REQ_T*)message);
        break;
            
        case anc_state_manager_event_concurrency_disconnect:
            ancStateManager_HandleConcurrencyDisconnect((const ANC_CONCURRENCY_DISCONNECT_REQ_T*)message);
        break;

        case anc_state_manager_event_concurrency_connect_req:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        case anc_state_manager_event_concurrency_disconnect_req:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        case KYMERA_ANC_COMMON_CAPABILITY_DISABLE_COMPLETE:
            ancStateManager_HandleEvent(anc_state_manager_event_capability_disable_complete);
            break;

        case KYMERA_ANC_COMMON_CAPABILITY_ENABLE_COMPLETE:
            ancStateManager_HandleEvent(anc_state_manager_event_capability_enable_complete);
            ancStateManager_ClearLock(anc_state_manager_event_enable);

            break;

        case KYMERA_ANC_COMMON_CAPABILITY_MODE_CHANGE_TRIGGER:
                ancStateManager_HandleEvent(anc_state_manager_event_capability_mode_change_trigger);
                break;

        default:
            DEBUG_LOG("ancstateManager_HandleMessage: Event not handled");
        break;
    }
}



/****************************************************************************
DESCRIPTION
    Stop config timer

RETURNS
    None
*/
static void ancStateManager_StopConfigTimer(void)
{
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_config_timer_expiry);
}

/****************************************************************************
DESCRIPTION
    Start config timer
    To cater to certain chip variants (QCC512x) where ANC hardware takes around 300ms to configure, 
    it is essential to wait for the configuration to complete before starting Adaptive ANC chain

RETURNS
    None
*/
#ifdef ENABLE_ADAPTIVE_ANC
static void ancStateManager_StartConfigTimer(void)
{
    DEBUG_LOG("Timer value: %d\n", KYMERA_CONFIG_ANC_DELAY_TIMER);

    ancStateManager_StopConfigTimer();
    MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_config_timer_expiry,
                     NULL, KYMERA_CONFIG_ANC_DELAY_TIMER);
}
#endif

static void ancStateManager_StopGentleMuteTimer(void)
{
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry);
}

static void ancStateManager_DisableAncPostGentleMute(void)
{
    DEBUG_LOG("ancStateManager_DisableAncPostGentleMute");
    /* Cancel if any outstanding message in the queue */
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry);

    MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry,
                     NULL, KYMERA_CONFIG_ANC_GENTLE_MUTE_TIMER);
}

static void ancStateManager_UpdateAncModePostGentleMute(void)
{
    DEBUG_LOG("ancStateManager_UpdateAncModePostGentleMute");
    /* Cancel if any outstanding message in the queue */
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry);

    MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry,
                     NULL, KYMERA_CONFIG_ANC_GENTLE_MUTE_TIMER);
}

/****************************************************************************
DESCRIPTION
    Get In Ear Status from Phy state

RETURNS
    TRUE if Earbud is in Ear, FALSE otherwise
*/
static bool ancStateManager_GetInEarStatus(void)
{
    return (appPhyStateGetState()==PHY_STATE_IN_EAR) ? (TRUE):(FALSE);
}


/****************************************************************************
DESCRIPTION
    This ensures on Config timer expiry ANC hardware is now setup
    It is safe to enable Adaptive ANC capability
    On ANC Enable request, enable Adatpive ANC independent of the mode

RETURNS
    None
*/
static void ancStateManager_EnableAdaptiveAnc(void)
{
    if ((anc_data.actual_enabled) && 
        (anc_data.state==anc_state_manager_enabled) && 
        ANC_SM_IS_ADAPTIVE_ANC_DISABLED())
    {
        DEBUG_LOG("ancStateManager_EnableAdaptiveAnc \n");
        Kymera_EnableAdaptiveAnc(ancStateManager_GetInEarStatus(), /*Use the current Phy state*/
                                 ancStateManager_GetAncPath(), 
                                 adaptive_anc_hw_channel_0, anc_data.current_mode);
    }
}

/******************************************************************************
DESCRIPTION
    Handle the transition into a new state. This function is responsible for
    generating the state related system events.
*/
static void changeState(anc_state_manager_t new_state)
{
    DEBUG_LOG("changeState: ANC State %d -> %d\n", anc_data.state, new_state);

    if ((new_state == anc_state_manager_power_off) && (anc_data.state != anc_state_manager_uninitialised))
    {
        /*Stop Internal Timers, if running*/
        ancStateManager_StopConfigTimer();
        
        /* When we power off from an on state persist any state required */
        setSessionData();
    }
    /* Update state */
    anc_data.state = new_state;
}

/******************************************************************************
DESCRIPTION
    Enumerate as USB device to enable ANC tuning
    Common for both Static ANC and Adaptive ANC tuning
*/
static void ancStateManager_UsbEnumerateTuningDevice(anc_usb_config_t new_config)
{
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    anc_state_manager_data_t *anc_sm = GetAncData();
    if(anc_sm->usb_config != new_config)
    {
        /* Does not support switching between static & adaptive anc tuning
         * without properly stoping the current anc tuning */
        PanicNotNull(anc_sm->SavedUsbAppIntrface);
        PanicNotNull(anc_sm->SavedUsbAudioTask);
        anc_sm->SavedUsbAppIntrface = UsbApplication_GetActiveApp();

        switch(new_config)
        {
            case ANC_USB_CONFIG_STATIC_ANC_TUNING:
                UsbApplication_Open(&usb_app_anc_tuning);
                break;
#ifdef ENABLE_ADAPTIVE_ANC
            case ANC_USB_CONFIG_ADAPTIVE_ANC_TUNING:
                UsbApplication_Open(&usb_app_adaptive_anc_tuning);
                break;
#endif
            default:
                DEBUG_LOG_ERROR("ANC STATE MANGER: UNEXPECTED USB CONFIG");
                Panic();
                break;
        }
        anc_sm->usb_config = new_config;
        anc_sm->SavedUsbAudioTask = UsbAudio_ClientRegister(AncStateManager_GetTask(),
                                                    USB_AUDIO_REGISTERED_CLIENT_MEDIA);
    }
    UsbDevice_ClientRegister(AncStateManager_GetTask());
#else
    static anc_usb_config_t config_done = ANC_USB_CONFIG_NO_USB;
    if(config_done != new_config)
    {
        Usb_TimeCriticalInit();
        config_done = new_config;
    }
    Usb_ClientRegister(AncStateManager_GetTask());
    Usb_AttachtoHub();
#endif
}

/******************************************************************************
DESCRIPTION
    Exits tuning by suspending USB enumeration
    Common for both Static ANC and Adaptive ANC tuning
*/
static void ancStateManager_UsbDetachTuningDevice(void)
{
    DEBUG_LOG_VERBOSE("ancStateManager_UsbDetachTuningDevice");
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Unregister ANC task from USB Clients */
    UsbDevice_ClientUnregister(AncStateManager_GetTask());
    UsbAudio_ClientUnRegister(AncStateManager_GetTask(), USB_AUDIO_REGISTERED_CLIENT_MEDIA);

    UsbApplication_Close();

    if(anc_sm->SavedUsbAudioTask)
    {
        UsbAudio_ClientRegister(anc_sm->SavedUsbAudioTask, USB_AUDIO_REGISTERED_CLIENT_MEDIA);
        anc_sm->SavedUsbAudioTask = NULL;
    }
    if(anc_sm->SavedUsbAppIntrface)
    {
        DEBUG_LOG_VERBOSE("ancStateManager: Open saved USB Application");
        UsbApplication_Open(anc_sm->SavedUsbAppIntrface);
        anc_sm->SavedUsbAppIntrface = NULL;
    }
    anc_sm->usb_config = ANC_USB_CONFIG_NO_USB;
#else
    Usb_DetachFromHub();
#endif
}

/******************************************************************************
DESCRIPTION
    Sets up static ANC tuning mode by disabling ANC and changes state to tuning mode active.
*/
static void ancStateManager_SetupAncTuningMode(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_SetupAncTuningMode\n");

    if(AncStateManager_IsEnabled())
    {
        /*Stop Internal Timers, if running*/
        ancStateManager_StopConfigTimer();
        ancStateManager_StopGentleMuteTimer();

        /* Disables ANC and sets the state to Tuning mode active */
        ancStateManager_DisableAnc(anc_state_manager_tuning_mode_active);
    }
    else
    {
        /*Sets the state to tuning mode active */
        changeState(anc_state_manager_tuning_mode_active);
    }

    ancStateManager_UsbEnumerateTuningDevice(ANC_USB_CONFIG_STATIC_ANC_TUNING);
}

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
/******************************************************************************
DESCRIPTION
    Enter into static Anc tuning mode
*/
static void ancStateManager_EnterAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_EnterAncTuning");
    anc_state_manager_data_t *anc_sm = GetAncData();
    usb_audio_interface_info_t spkr_interface_info;
    usb_audio_interface_info_t mic_interface_info;
    anc_tuning_connect_parameters_t connect_param;

    PanicFalse(UsbAudio_GetInterfaceInfoFromDeviceType(USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER, &spkr_interface_info));
    PanicFalse(UsbAudio_GetInterfaceInfoFromDeviceType(USB_AUDIO_DEVICE_TYPE_AUDIO_MIC, &mic_interface_info));
    PanicFalse(spkr_interface_info.sampling_rate == mic_interface_info.sampling_rate);
    PanicFalse(spkr_interface_info.frame_size == mic_interface_info.frame_size);
    PanicNotZero(spkr_interface_info.is_to_host);
    PanicZero(mic_interface_info.is_to_host);

    connect_param.usb_rate = spkr_interface_info.sampling_rate;
    connect_param.spkr_src = spkr_interface_info.streamu.spkr_src;
    connect_param.mic_sink = mic_interface_info.streamu.mic_sink;
    connect_param.spkr_channels = spkr_interface_info.channels;
    connect_param.mic_channels = mic_interface_info.channels;
    connect_param.frame_size = spkr_interface_info.frame_size;

    anc_sm->spkr_src = connect_param.spkr_src;
    anc_sm->mic_sink = connect_param.mic_sink;

    PanicFalse(UsbAudio_SetAudioChainBusy(anc_sm->spkr_src));
    KymeraAnc_EnterTuning(&connect_param);
}
/******************************************************************************
DESCRIPTION
    Exits from static Anc tuning mode and unregisters ANC task from USB
*/
static void ancStateManager_ExitTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_ExitTuning");
    anc_state_manager_data_t *anc_sm = GetAncData();

    anc_tuning_disconnect_parameters_t disconnect_param = {
        .spkr_src = anc_sm->spkr_src,
        .mic_sink = anc_sm->mic_sink,
        .kymera_stopped_handler = UsbAudio_ClearAudioChainBusy,
    };
    KymeraAnc_ExitTuning(&disconnect_param);
}
#else
/******************************************************************************
DESCRIPTION
    Enter into static Anc tuning mode
*/
static void ancStateManager_EnterAncTuning(void)
{
    anc_tuning_connect_parameters_t connect_param;
    connect_param.usb_rate = ancStateManager_GetUsbSampleRate();
    KymeraAnc_EnterTuning(&connect_param);
}

/******************************************************************************
DESCRIPTION
    Exits from static Anc tuning mode and unregisters ANC task from USB
*/
static void ancStateManager_ExitTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_ExitTuning");

    KymeraAnc_ExitTuning(NULL);
    Usb_ClientUnRegister(AncStateManager_GetTask());
}
#endif
/******************************************************************************
DESCRIPTION
    Sets up Adaptive ANC tuning mode and changes state to adaptive anc tuning mode active.
    Enables ANC, as Adaptive ANC needs ANC HW to be running
*/
static void ancStateManager_setupAdaptiveAncTuningMode(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_setupAdaptiveAncTuningMode\n");

    changeState(anc_state_manager_adaptive_anc_tuning_mode_active);

    /* Enable ANC if disabled */
    if(!AncIsEnabled())
    {
        ancStateManager_EnableAncHw();
    }

    ancStateManager_UsbEnumerateTuningDevice(ANC_USB_CONFIG_ADAPTIVE_ANC_TUNING);
}

#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
/******************************************************************************
DESCRIPTION
    Enter into Adaptive ANC tuning mode
*/
static void ancStateManager_EnterAdaptiveAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_EnterAdaptiveAncTuning");
    anc_state_manager_data_t *anc_sm = GetAncData();
    usb_audio_interface_info_t spkr_interface_info;
    usb_audio_interface_info_t mic_interface_info;
    adaptive_anc_tuning_connect_parameters_t connect_param;

    PanicFalse(UsbAudio_GetInterfaceInfoFromDeviceType(USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER, &spkr_interface_info));
    PanicFalse(UsbAudio_GetInterfaceInfoFromDeviceType(USB_AUDIO_DEVICE_TYPE_AUDIO_MIC, &mic_interface_info));
    PanicFalse(spkr_interface_info.sampling_rate == mic_interface_info.sampling_rate);
    PanicFalse(spkr_interface_info.frame_size == mic_interface_info.frame_size);
    PanicNotZero(spkr_interface_info.is_to_host);
    PanicZero(mic_interface_info.is_to_host);

    connect_param.usb_rate = spkr_interface_info.sampling_rate;
    connect_param.spkr_src = spkr_interface_info.streamu.spkr_src;
    connect_param.mic_sink = mic_interface_info.streamu.mic_sink;
    connect_param.spkr_channels = spkr_interface_info.channels;
    connect_param.mic_channels = mic_interface_info.channels;
    connect_param.frame_size = spkr_interface_info.frame_size;

    anc_sm->spkr_src = connect_param.spkr_src;
    anc_sm->mic_sink = connect_param.mic_sink;

    PanicFalse(UsbAudio_SetAudioChainBusy(anc_sm->spkr_src));
    KymeraAncCommon_EnterAdaptiveAncTuning(&connect_param);
}
/******************************************************************************
DESCRIPTION
    Exits from tuning mode and unregisters ANC task from USB clients
*/
static void ancStateManager_ExitAdaptiveAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_ExitAdaptiveAncTuning");
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Disable Anc*/
    if(AncIsEnabled())
    {
       ancStateManager_DisableAncHw();
    }

    adaptive_anc_tuning_disconnect_parameters_t disconnect_param = {
        .spkr_src = anc_sm->spkr_src,
        .mic_sink = anc_sm->mic_sink,
        .kymera_stopped_handler = UsbAudio_ClearAudioChainBusy,
    };
    KymeraAncCommon_ExitAdaptiveAncTuning(&disconnect_param);
}
#else
/******************************************************************************
DESCRIPTION
    Enter into Adaptive ANC tuning mode
*/
static void ancStateManager_EnterAdaptiveAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_EnterAdaptiveAncTuning");
    adaptive_anc_tuning_connect_parameters_t connect_param;
    connect_param.usb_rate = ancStateManager_GetUsbSampleRate();
    KymeraAncCommon_EnterAdaptiveAncTuning(&connect_param);
}
/******************************************************************************
DESCRIPTION
    Exits from tuning mode and unregisters ANC task from USB clients
*/
static void ancStateManager_ExitAdaptiveAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_ExitAdaptiveAncTuning");

    /* Disable Anc*/
    if(AncIsEnabled())
    {
       ancStateManager_DisableAncHw();
    }

    KymeraAncCommon_ExitAdaptiveAncTuning(NULL);
    
    /* Unregister ANC task from USB Clients */
    Usb_ClientUnRegister(AncStateManager_GetTask());
}
#endif

static uint8 AncStateManager_ReadFineGainFromInstance(void)
{
    uint8 gain;
    audio_anc_path_id gain_path = ancStateManager_GetAncPath();

    AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_0, gain_path, &gain);

    return gain;
}

static void ancStateManager_UpdatePathGainsAfterSettlingTime(void)
{
#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
    DEBUG_LOG_FN_ENTRY("ancStateManager_UpdatePathGainsAfterSettlingTime");

    ancStateManager_StopPathGainsUpdateTimer();
    ancStateManager_StartPathGainsUpdateTimer(STATIC_ANC_CONFIG_SETTLING_TIME);
#endif
}

#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
static bool ancStateManager_IsLeftChannelPathEnabled(void)
{
    bool status = FALSE;
    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case feed_forward_mode:
        case feed_forward_mode_left_only: /* fallthrough */
        case feed_back_mode: /* fallthrough */
        case feed_back_mode_left_only: /* fallthrough */
        case hybrid_mode: /* fallthrough */
        case hybrid_mode_left_only: /* fallthrough */
            status = TRUE;
            break;

        default:
            status = FALSE;
            break;
    }

    return status;
}

static bool ancStateManager_IsRightChannelPathEnabled(void)
{
    bool status = FALSE;

    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case feed_forward_mode:
        case feed_forward_mode_right_only: /* fallthrough */
        case feed_back_mode: /* fallthrough */
        case feed_back_mode_right_only: /* fallthrough */
        case hybrid_mode: /* fallthrough */
        case hybrid_mode_right_only: /* fallthrough */
            status = TRUE;
            break;

        default:
            status = FALSE;
            break;
    }

    return status;
}

static void ancStateManager_SetSingleFilterFFAPathGain(uint8 gain)
{
    /*Using bool variables to hold the left and right channel enabled path in order
      to reduce the time difference between the trap used for configuring FFA
      path gains on both instances.*/

    bool isLeftChannelEnabled = ancStateManager_IsLeftChannelPathEnabled();
    bool isRightChannelEnabled = ancStateManager_IsRightChannelPathEnabled();

    if(isLeftChannelEnabled)
    {
        AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_0, gain);
    }
    if(isRightChannelEnabled)
    {
        AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_1, gain);
    }
}

static void ancStateManager_SetSingleFilterFFBPathGain(uint8 gain)
{
    /*Using bool variables to hold the left and right channel enabled path in order
      to reduce the time difference between the trap used for configuring FFA
      path gains on both instances.*/

    bool isLeftChannelEnabled = ancStateManager_IsLeftChannelPathEnabled();
    bool isRightChannelEnabled = ancStateManager_IsRightChannelPathEnabled();

    if(isLeftChannelEnabled)
    {
        AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_0, gain);
    }
    if(isRightChannelEnabled)
    {
        AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_1, gain);
    }
}

static void ancStateManager_SetSingleFilterFBPathGain(uint8 gain)
{
    /*Using bool variables to hold the left and right channel enabled path in order
      to reduce the time difference between the trap used for configuring FFA
      path gains on both instances.*/

    bool isLeftChannelEnabled = ancStateManager_IsLeftChannelPathEnabled();
    bool isRightChannelEnabled = ancStateManager_IsRightChannelPathEnabled();

    if(isLeftChannelEnabled)
    {
        AncConfigureFBPathGain(AUDIO_ANC_INSTANCE_0, gain);
    }
    if(isRightChannelEnabled)
    {
        AncConfigureFBPathGain(AUDIO_ANC_INSTANCE_1, gain);
    }
}

#endif

/*! \brief Interface to ramp-down filter path fine gain.
 * To avoid sudden dip in dB level the gain value is reduced by 2steps on higher dB level
 * and reduced by 1 step on lower dB level .

*/
static void ancStateManager_RampDownFilterPathFineGain(void)
{
#ifndef ENABLE_ADAPTIVE_ANC  /* Static ANC build */

    DEBUG_LOG_ALWAYS("ancStateManager_RampDownFilterPathFineGain, ramp-down start");

    uint8 fine_gain = 0;
    uint8 cnt = 0;
    uint8 fine_gain_lower_db_offset = 0;
    anc_mode_t mode = AncStateManager_GetCurrentMode();
    audio_anc_path_id audio_anc_path = ancStateManager_GetAncPath();

#define GAIN_LOWER_DB_LEVEL_OFFSET (12U)

    /* Get a FFA filter path fine gain value from audio PS key for current mode */
    /* Ramp down internal mic path filter fine gain incase of hybrid/feedback mode, and
     * external mic path filter gain incase of feedforward mode */

    ancStateManager_ReadFineGain(mode, audio_anc_path, &fine_gain);

    if(fine_gain > GAIN_LOWER_DB_LEVEL_OFFSET)
    {
        /* Ramp down by 2 steps until the raw value of 12 */
        for(cnt = fine_gain; cnt > GAIN_LOWER_DB_LEVEL_OFFSET; cnt = (cnt - 2))
        {
            DEBUG_LOG("Fine Gain: %d", cnt);

            if(appKymeraIsParallelAncFilterEnabled())
            {
                AncConfigureParallelFilterFFAPathGain(cnt,cnt);
            }
            else
            {
                ancStateManager_SetSingleFilterFFAPathGain(cnt);
            }
        }
        fine_gain_lower_db_offset = GAIN_LOWER_DB_LEVEL_OFFSET;
    }
    else
    {
        fine_gain_lower_db_offset = fine_gain;
    }

    /* and afterwards in setp of 1 */
    for(cnt = fine_gain_lower_db_offset; cnt > 0; cnt--)
    {
        PanicFalse(cnt > 0);
        DEBUG_LOG("Fine Gain: %d", cnt);

        if(appKymeraIsParallelAncFilterEnabled())
        {
            AncConfigureParallelFilterFFAPathGain(cnt,cnt);
        }
        else
        {
            ancStateManager_SetSingleFilterFFAPathGain(cnt);
        }
    }

    if(appKymeraIsParallelAncFilterEnabled())
    {
        AncConfigureParallelFilterFFAPathGain(0U,0U);
    }
    else
    {
        ancStateManager_SetSingleFilterFFAPathGain(0U);
    }

        DEBUG_LOG_ALWAYS("ancStateManager_RampDownFilterPathFineGain, ramp-down end");
#endif /*ENABLE_ADAPTIVE_ANC*/
}

/**********************************************************************/
/************** Ramping Algorithm *************************************/
/**********************************************************************/
#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
static uint8 readAncPathFineGain(audio_anc_path_id audio_anc_path)
{
    uint8 fine_gain = 0;

    if(ancStateManager_IsLeftChannelPathEnabled())
    {
        AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_0, audio_anc_path, &fine_gain);
    }
    else
    {
        AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_1, audio_anc_path, &fine_gain);
    }

    return fine_gain;
}

static void setAncPathFineGain(uint8 fine_gain, audio_anc_path_id audio_anc_path)
{
    switch(audio_anc_path)
    {

        case AUDIO_ANC_PATH_ID_FFA:

            if(appKymeraIsParallelAncFilterEnabled())
            {
                AncConfigureParallelFilterFFAPathGain(fine_gain,fine_gain);
            }
            else
            {
                ancStateManager_SetSingleFilterFFAPathGain(fine_gain);
            }
        break;

        case AUDIO_ANC_PATH_ID_FFB:

            if(appKymeraIsParallelAncFilterEnabled())
            {
                AncConfigureParallelFilterFFBPathGain(fine_gain,fine_gain);
            }
            else
            {
                ancStateManager_SetSingleFilterFFBPathGain(fine_gain);
            }
            break;

        case AUDIO_ANC_PATH_ID_FB:

            if(appKymeraIsParallelAncFilterEnabled())
            {
                AncConfigureParallelFilterFBPathGain(fine_gain,fine_gain);
            }
            else
            {
                ancStateManager_SetSingleFilterFBPathGain(fine_gain);
            }
            break;

        default:
            break;
    }
}

static void rampUpAncPathFineGainHelper(uint8 start_gain, uint8 end_gain, uint8 step_size, audio_anc_path_id audio_anc_path)
{
#define MAX_GAIN (255U)
    DEBUG_LOG_ALWAYS("rampUpAncPathFineGainHelper, Start Gain:%d, End Gain:%d, Step Size:%d",start_gain, end_gain, step_size);

    /* if step increment is exceeding max value then apply previous step */
    if((MAX_GAIN - end_gain) < step_size)
        end_gain = end_gain - step_size;

    for(uint8 cnt = start_gain; cnt <= end_gain; cnt = cnt + step_size)
    {
        setAncPathFineGain(cnt,audio_anc_path);
    }
}

static void updateFFAPathFineGain(void)
{
    anc_mode_t mode = GetAncData()->requested_mode;
    uint8 fine_gain = 0;

    ancStateManager_ReadFineGain(mode, AUDIO_ANC_PATH_ID_FFA, &fine_gain);

    if(appKymeraIsParallelAncFilterEnabled())
    {
        AncConfigureParallelFilterFFAPathGain(fine_gain,fine_gain);
    }
    else
    {
        ancStateManager_SetSingleFilterFFAPathGain(fine_gain);
    }
}

static void updateFBPathFineGain(void)
{
    uint8 fine_gain = 0;

    if(ancStateManager_IsLeftChannelPathEnabled())
    {
        AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FB, &fine_gain);
    }
    else
    {
        AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_1, AUDIO_ANC_PATH_ID_FB, &fine_gain);
    }

    if(appKymeraIsParallelAncFilterEnabled())
    {
        AncConfigureParallelFilterFBPathGain(fine_gain,fine_gain);
    }
    else
    {
        ancStateManager_SetSingleFilterFBPathGain(fine_gain);
    }
}

static void rampUpAncPathFineGain(anc_mode_t mode, audio_anc_path_id audio_anc_path)
{
    uint8 fine_gain = 0;

    ancStateManager_ReadFineGain(mode, audio_anc_path, &fine_gain);

    if(fine_gain > 128U)
    {
        rampUpAncPathFineGainHelper(1U, 32U, 1U, audio_anc_path); /* 1-step increment upto 32 fine gain */
        rampUpAncPathFineGainHelper(33U, 64U, 2U, audio_anc_path); /* 2-step increment upto 64 fine gain */
        rampUpAncPathFineGainHelper(65U, 128U, 4U, audio_anc_path); /* 4-step increment upto 128 fine gain */
        rampUpAncPathFineGainHelper(129U, fine_gain, 8U, audio_anc_path); /* 8-step increment upto 255 fine gain */
    }
    else if(fine_gain > 64U)
    {
        rampUpAncPathFineGainHelper(1U, 32U, 1U, audio_anc_path); /* 1-step increment upto 32 fine gain */
        rampUpAncPathFineGainHelper(33U, 64U, 2U, audio_anc_path); /* 2-step increment upto 64 fine gain */
        rampUpAncPathFineGainHelper(65U, fine_gain, 4U, audio_anc_path); /* 4-step increment upto 128 fine gain */
    }
    else if(fine_gain > 32U)
    {
        rampUpAncPathFineGainHelper(1U, 32U, 1U, audio_anc_path); /* 1-step increment upto 32 fine gain */
        rampUpAncPathFineGainHelper(33U, fine_gain, 2U, audio_anc_path); /* 2-step increment upto 64 fine gain */
    }
    else
    {
        rampUpAncPathFineGainHelper(1U, fine_gain, 1U, audio_anc_path); /* 1-step increment upto 32 fine gain */
    }

    /* Update final target fine gain */
    setAncPathFineGain(fine_gain,audio_anc_path);
}

static void ancStateManager_RampupFineGain(anc_mode_t mode)
{
    DEBUG_LOG_ALWAYS("ancStateManager_RampupFineGain, ramp-up start");

    if(ancStateManager_GetAncPath() == AUDIO_ANC_PATH_ID_FFB)
    {
        updateFFAPathFineGain();
        updateFBPathFineGain();
        rampUpAncPathFineGain(mode, AUDIO_ANC_PATH_ID_FFB);
    }
    else if(ancStateManager_GetAncPath() == AUDIO_ANC_PATH_ID_FFA)
    {
        if(appConfigAncPathEnable() == feed_back_mode_left_only || appConfigAncPathEnable() == feed_back_mode)
        {
            updateFBPathFineGain();
        }
        rampUpAncPathFineGain(mode, AUDIO_ANC_PATH_ID_FFA);
    }

    DEBUG_LOG_ALWAYS("ancStateManager_RampupFineGain, ramp-up end");
}
#endif /*ENABLE_ADAPTIVE_ANC*/

static void ancStateManager_RampupOnModeChange(void)
{
#ifndef ENABLE_ADAPTIVE_ANC  /* Static ANC build */
    anc_mode_t mode = GetAncData()->requested_mode;
    ancStateManager_RampupFineGain(mode);
#endif /*ENABLE_ADAPTIVE_ANC*/
}

static void ancStateManager_RampupOnANCEnable(void)
{
#ifndef ENABLE_ADAPTIVE_ANC  /* Static ANC build */
    anc_mode_t mode = GetAncData()->current_mode;
    ancStateManager_RampupFineGain(mode);
#endif /*ENABLE_ADAPTIVE_ANC*/
}

#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
static void rampDownAncPathFineGainHelper(uint8 start_gain, uint8 end_gain, uint8 step_size, audio_anc_path_id audio_anc_path)
{
    DEBUG_LOG_ALWAYS("rampDownAncPathFineGainHelper, Start Gain:%d, End Gain:%d, Step Size:%d",start_gain, end_gain, step_size);

    for(uint8 cnt = start_gain; cnt >= end_gain; cnt = cnt - step_size)
    {
        setAncPathFineGain(cnt,audio_anc_path);
    }
}

static void rampDownPathFineGain(audio_anc_path_id audio_anc_path)
{
    anc_mode_t mode = AncStateManager_GetCurrentMode();
    uint8 fine_gain = 0;

    ancStateManager_ReadFineGain(mode, audio_anc_path, &fine_gain);

    if(fine_gain > 128U)
    {
        rampDownAncPathFineGainHelper(fine_gain, 129U, 8U, audio_anc_path); /* 8-step decrement upto 129 fine gain */
        rampDownAncPathFineGainHelper(128U, 65U, 4U, audio_anc_path); /* 4-step decrement upto 65 fine gain */
        rampDownAncPathFineGainHelper(64U, 33U, 2U, audio_anc_path); /* 2-step decrement upto 33 fine gain */
        rampDownAncPathFineGainHelper(32U, 1U, 1U, audio_anc_path); /* 1-step decrement upto 1 fine gain */
    }
    else if(fine_gain > 64U)
    {
        rampDownAncPathFineGainHelper(fine_gain, 65U, 4U, audio_anc_path); /* 4-step decrement upto 65 fine gain */
        rampDownAncPathFineGainHelper(64U, 33U, 2U, audio_anc_path); /* 2-step decrement upto 33 fine gain */
        rampDownAncPathFineGainHelper(32U, 1U, 1U, audio_anc_path); /* 1-step decrement upto 1 fine gain */
    }
    else if(fine_gain > 32U)
    {
        rampDownAncPathFineGainHelper(fine_gain, 33U, 2U, audio_anc_path); /* 2-step decrement upto 33 fine gain */
        rampDownAncPathFineGainHelper(32U, 1U, 1U, audio_anc_path); /* 1-step decrement upto 1 fine gain */
    }
    else
    {
        rampDownAncPathFineGainHelper(fine_gain, 1U, 1U, audio_anc_path); /* 1-step decrement upto 1 fine gain */
    }
}
#endif /*ENABLE_ADAPTIVE_ANC*/

static void ancStateManager_RampDownOnModeChange(void)
{
#ifndef ENABLE_ADAPTIVE_ANC  /* Static ANC build */

    DEBUG_LOG_ALWAYS("ancStateManager_RampDownOnModeChange, ramp-down start");

    if(ancStateManager_GetAncPath() == AUDIO_ANC_PATH_ID_FFA)
    {
        rampDownPathFineGain(AUDIO_ANC_PATH_ID_FFA);

        /* Mute fine gains in FFA path */
        setAncPathFineGain(0, AUDIO_ANC_PATH_ID_FFA);
    }
    else if(ancStateManager_GetAncPath() == AUDIO_ANC_PATH_ID_FFB)
    {
        rampDownPathFineGain(AUDIO_ANC_PATH_ID_FFB);

        /* Mute fine gains in FFA, FFB and FB paths */
        setAncPathFineGain(0, AUDIO_ANC_PATH_ID_FFB);
        setAncPathFineGain(0, AUDIO_ANC_PATH_ID_FFA);
        setAncPathFineGain(0, AUDIO_ANC_PATH_ID_FB);
    }

    DEBUG_LOG_ALWAYS("ancStateManager_RampDownOnModeChange, ramp-down end");
#endif /*ENABLE_ADAPTIVE_ANC*/
}

/****************************************************************************
DESCRIPTION
    Call appropriate ANC Enable API based on Adaptive ANC support

RETURNS
    None
*/
static bool ancStateManager_EnableAnc(bool enable)
{
    bool status=FALSE;

    if(!enable)
    {
        /* Static ANC build */
        ancStateManager_StopPathGainsUpdateTimer();
        ancStateManager_RampDownFilterPathFineGain();

        status = ancStateManager_DisableAncHw();
        appKymeraExternalAmpControl(FALSE);
    }
    else
    {
        status = ancStateManager_EnableAncHwWithMutePathGains();
        if(status) /* ANC HW if enabled in Static ANC build */
        {
            ancStateManager_UpdatePathGainsAfterSettlingTime();
        }
        appKymeraExternalAmpControl(TRUE);
    }
    return status;
}

static void ancStateManager_DisableAdaptiveAnc(void)
{
    DEBUG_LOG("ancStateManager_DisableAdaptiveAnc \n");

    /*Stop config timer if running, if ANC is getting disabled*/
    ancStateManager_StopConfigTimer();

    if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
    {
        /* Disable Adaptive ANC */
        KymeraAncCommon_AdaptiveAncDisable();
    }
}

static void ancStateManager_StartAdaptiveAncTimer(void)
{
#ifdef ENABLE_ADAPTIVE_ANC
    if (ANC_SM_IS_ADAPTIVE_ANC_DISABLED())
    {
        /*To accomodate the ANC hardware delay to configure and to start Adaptive ANC capability*/
        ancStateManager_StartConfigTimer();
    }
#endif
}

/*Maintain AANC chain even on mode change, so do not disable AANC
On Mode change, Set UCID for the new mode, Enable Gentle mute, 
Tell ANC hardware to change filters, LPFs using static ANC APIs (through Set Mode)
And Un mute FF and FB through operator message to AANC operator with static gain values */
static void ancStateManager_UpdateAdaptiveAncOnModeChange(anc_mode_t new_mode)
{
#ifdef ENABLE_ADAPTIVE_ANC
    /* check if ANC is enabled */
    if (anc_data.actual_enabled)
    {
        DEBUG_LOG("ancStateManager_UpdateAdaptiveAncOnModeChange");
        KymeraAncCommon_ApplyModeChange(new_mode, ancStateManager_GetAncPath(), adaptive_anc_hw_channel_0);
    }
#else
    UNUSED(new_mode);
#endif
}

static bool ancStateManager_SetAncMode(anc_mode_t new_mode)
{
    if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
    {
        DEBUG_LOG("ancStateManager_SetAncMode: Adaptive ANC mode change request");
        /* Set ANC filter coefficients alone if requested mode is Adaptive ANC. Path gain would be handled by Adaptive ANC operator */
        bool update_filter_coefficients = TRUE;
        bool update_path_gains = FALSE;

#ifdef ENABLE_ANC_FAST_MODE_SWITCH
        /* To support dynamic filter switching on Mora2.0 Adaptive ANC builds */
        update_filter_coefficients = AncConfig_IsAdvancedAnc() ? FALSE : TRUE;
#endif

        return AncSetModeWithConfig(new_mode, update_filter_coefficients, update_path_gains);
    }
    else
    {
        DEBUG_LOG("ancStateManager_SetAncMode: Static ANC or passthrough mode change request");
        /* Static ANC build */
        ancStateManager_StopPathGainsUpdateTimer();

         if(!anc_data.actual_enabled)
        {
            /* apply new filter coefficients with coarse and path gains immediately */
            return AncSetMode(new_mode);
        }
        else
        {
            bool return_val = FALSE;
            ancStateManager_RampDownOnModeChange();

            /* Apply new filter coefficients and coarse gains */
            return_val = AncSetModeWithSelectedGains(new_mode, TRUE, FALSE);

            ancStateManager_StopModeChangeSettlingTimer();
            ancStateManager_StartModeChangeSettlingTimer(STATIC_ANC_MODE_CHANGE_SETTLING_TIME);
            /* Update fine gains after settling time */
            return return_val;
        }
    }
}

static void ancStateManager_SetAdaptiveAncAdaptivity(bool adaptivity)
{
    anc_data.adaptivity = adaptivity;
}

static void ancStateManager_SetAncGain(uint8 anc_gain)
{
    if (!AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
    {
        anc_data.anc_gain = anc_gain;
    }
}

static void ancStateManager_UpdateStaticLeakthroughCurrentWorldVolumeGain(void)
{
    uint8 cur_ff_gain = kymeraAnc_GetStaticLeakthroughFfFineGain();

    /* Update gain in ANC data structure */
    ancStateManager_SetAncGain(cur_ff_gain);
    ancStateManager_PreserveLeakthroughGain(cur_ff_gain);
    ancStateManager_MsgRegisteredClientsOnGainUpdate();
}

static void ancStateManager_SetWorldVolumeGainInAncData(anc_mode_t mode, int8 world_volume_gain_dB)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    if (AncConfig_IsAncModeLeakThrough(mode))
    {
        anc_sm->world_volume_gain_dB[mode] = world_volume_gain_dB;
    }
}

static int8 ancStateManager_GetWorldVolumeGainFromAncData(anc_mode_t mode)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    int8 world_volume_gain_dB = 0;

    if (AncConfig_IsAncModeLeakThrough(mode))
    {
        world_volume_gain_dB = anc_sm->world_volume_gain_dB[mode];
    }

    return world_volume_gain_dB;
}

static void ancStateManager_UpdateAdaptiveLeakthroughCurrentWorldVolumeGain(void)
{
    int8 makeup_gain_dB = 0;
    bool status;

    status = kymeraAnc_GetAncCompanderMakeupdBGain(&makeup_gain_dB);
    if(status)
    {
        DEBUG_LOG_INFO("ancStateManager_UpdateAdaptiveLeakthroughCurrentWorldVolumeGain: Current world volume gain: %d\n", makeup_gain_dB);
        /* It is considered that makeup gain starts at 0dB to provide 0dB world volume gain
           Hence, makeup gain is treated as world volume gain */
        ancStateManager_SetWorldVolumeGainInAncData(AncStateManager_GetCurrentMode(), makeup_gain_dB);
    }
}

static void ancStateManager_UpdateWorldVolumeGain(void)
{
    anc_mode_t cur_mode = AncStateManager_GetCurrentMode();
    int8 world_volume_gain_dB;

    if(AncConfig_IsAncModeAdaptiveLeakThrough(cur_mode))
    {
        /* As the ANC compander has makeup gain persistance, read makeup gain from ANC compander and update world_volume_gain_dB in ANC SM data */
        ancStateManager_UpdateAdaptiveLeakthroughCurrentWorldVolumeGain();
    }

    AncStateManager_GetCurrentWorldVolumeGain(&world_volume_gain_dB);

    if (KymeraAnc_UpdateWorldVolumeGain(world_volume_gain_dB, AncStateManager_GetBalancePercentage()))
    {
        ancStateManager_MsgRegisteredClientsOnWorldVolumeConfigUpdate();
        ancStateManager_MsgRegisteredClientsOnWorldVolumeGainUpdate(world_volume_gain_dB);

        if (AncConfig_IsAncModeStatic(cur_mode))
        {
            ancStateManager_UpdateStaticLeakthroughCurrentWorldVolumeGain();
        }
    }
}

/******************************************************************************
DESCRIPTION
    Update the state of the ANC VM library. This is the 'actual' state, as opposed
    to the 'requested' state and therefore the 'actual' state variables should
    only ever be updated in this function.
    
    RETURNS
    Bool indicating if updating lib state was successful or not.

*/  
static bool updateLibState(bool enable, anc_mode_t new_mode)
{
    bool retry_later = TRUE;
    anc_data.enable_dsp_clock_boostup = TRUE;

#ifdef ENABLE_ADAPTIVE_ANC
    anc_mode_t prev_mode = anc_data.current_mode;
    bool prev_anc_state = anc_data.actual_enabled;
    bool prev_adaptivity = anc_data.adaptivity;
    bool adaptivity = FALSE;
    uint8 aanc_fb_gain = 0;
#endif

    /* Enable operator framwork before updating DSP clock */
    OperatorFrameworkEnable(1);

    /*Change the DSP clock before enabling ANC and changing up the mode*/
    KymeraAnc_UpdateDspClock();

    DEBUG_LOG("updateLibState: ANC Current Mode enum:anc_mode_t:%d, Requested Mode enum:anc_mode_t:%d\n", anc_data.current_mode, new_mode);
    /* Check to see if we are changing mode */
    if (anc_data.current_mode != new_mode)
    {
        if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
        {
             KymeraAncCommon_AdaptiveAncSetUcid(anc_data.requested_mode);
        }
        
        /* Set ANC Mode */
        if (!ancStateManager_SetAncMode(new_mode) || (anc_data.requested_mode >=  AncStateManager_GetNumberOfModes()))
        {
            DEBUG_LOG("updateLibState: ANC Set Mode enum:anc_mode_t:%d failed\n", new_mode);
            retry_later = FALSE;
            /* fallback to previous success mode set */
            anc_data.requested_mode = anc_data.current_mode;
            /*Revert UCID*/
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                KymeraAncCommon_AdaptiveAncSetUcid(anc_data.current_mode);
            }
        }
        else
        {           
            /* Update mode state */
            DEBUG_LOG("updateLibState: ANC Set Mode enum:anc_mode_t:%d\n", new_mode);
            anc_data.current_mode = new_mode;
            ancStateManager_UpdateAdaptiveAncOnModeChange(new_mode);

            /* Notify ANC mode update to registered clients */
            ancStateManager_MsgRegisteredClientsOnModeUpdate();
        }
     }

     /* Determine state to update in VM lib */
     if (anc_data.actual_enabled != enable)
     {
        if (!enable)
        {
            ancStateManager_DisableAdaptiveAnc();
        }
        
        if (ancStateManager_EnableAnc(enable))
        {
            if (enable)
            {
                ancStateManager_StartAdaptiveAncTimer();
            }
            /* Notify ANC state update to registered clients */
            ancStateManager_MsgRegisteredClientsOnStateUpdate(enable);
        }
        else
        {
            /* If this does fail in a release build then we will continue
             and updating the ANC state will be tried again next time
             an event causes a state change. */
            DEBUG_LOG("updateLibState: ANC Enable failed %d\n", enable);
            retry_later = FALSE;
        }

         /* Update enabled state */
         DEBUG_LOG("updateLibState: ANC Enable %d\n", enable);
         anc_data.actual_enabled = enable;
     }

     if (anc_data.actual_enabled && !AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
     {
         /* Update gain in ANC data structure */
         ancStateManager_SetAncGain(AncStateManager_IsAncLeakthroughGainPreserved(anc_data.current_mode)?
                                        ancStateManager_GetAncLeakthroughPreservedGain(anc_data.current_mode):
                                        AncStateManager_ReadFineGainFromInstance());

         /* Notify ANC gain update to registered clients */
         ancStateManager_MsgRegisteredClientsOnGainUpdate();
     }

     if (anc_data.actual_enabled && AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
     {
         ancStateManager_UpdateWorldVolumeGain();
     }

#ifdef ENABLE_ADAPTIVE_ANC
     adaptivity = (anc_data.actual_enabled) && (AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()));
     /* Update adaptivity in ANC data structure */
     ancStateManager_SetAdaptiveAncAdaptivity(adaptivity);
     /* Notify adaptivity update to registered clients */
     ancStateManager_MsgRegisteredClientsOnAdaptiveAncAdaptivityUpdate(adaptivity);

     ancStateManager_ModifyGainTimerStatus(prev_anc_state, prev_mode, prev_adaptivity);

     if(anc_data.actual_enabled && AncConfig_IsAdvancedAnc())
     {
         if(!AncStateManager_IsFBGainTimerUpdateRequire(AncStateManager_GetCurrentMode()))
         {
             AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_0, AUDIO_ANC_PATH_ID_FFA, &aanc_fb_gain);
             ancStateManager_SetAancFBGain(aanc_fb_gain);
         }
         ancstateManager_MsgRegisteredClientsOnFBGainUpdate();
     }
#endif

     anc_data.enable_dsp_clock_boostup = FALSE;

     /*Revert DSP clock to its previous speed*/
     KymeraAnc_UpdateDspClock();

     /*Disable operator framwork after reverting DSP clock*/
     OperatorFrameworkEnable(0);
     return retry_later;
}

static void ancStateManager_UpdateAncData(anc_session_data_t* session_data)
{
    anc_state_manager_data_t *anc_sm_data = GetAncData();

    anc_sm_data->toggle_configurations = session_data->toggle_configurations;
    anc_sm_data->playback_config = session_data->playback_config;
    anc_sm_data->standalone_config = session_data->standalone_config;
    anc_sm_data->sco_config = session_data->sco_config;
    anc_sm_data->va_config = session_data->va_config;
    anc_sm_data->requested_mode = session_data->requested_mode;
}

static void ancStateManager_UpdateAncSessionData(anc_session_data_t* session_data)
{
    anc_state_manager_data_t *anc_sm_data = GetAncData();

    session_data->toggle_configurations = anc_sm_data->toggle_configurations;
    session_data->playback_config = anc_sm_data->playback_config;
    session_data->standalone_config = anc_sm_data->standalone_config;
    session_data->sco_config = anc_sm_data->sco_config;
    session_data->va_config = anc_sm_data->va_config;
    session_data->requested_mode = anc_sm_data->requested_mode;
}

static void ancStateManager_GetAncConfigs(void)
{
    anc_session_data_t* session_data = PanicUnlessMalloc(sizeof(anc_session_data_t));

    AncSessionData_GetSessionData(session_data);
    ancStateManager_UpdateAncData(session_data);
    
#ifdef ENABLE_AUTO_AMBIENT
    AncAutoAmbient_UpdateTaskData(session_data->atr_enabled, session_data->release_time_config);
#endif

    free(session_data);
}

static void ancStateManager_SetAncConfigs(void)
{
    anc_session_data_t* session_data = PanicUnlessMalloc(sizeof(anc_session_data_t));

    ancStateManager_UpdateAncSessionData(session_data);
    
#ifdef ENABLE_AUTO_AMBIENT
    AncAutoAmbient_UpdateSessionData(&session_data->atr_enabled, &session_data->release_time_config);
#endif

    AncSessionData_SetSessionData(session_data);

    free(session_data);
}

/******************************************************************************
DESCRIPTION
    Update session data retrieved from PS

RETURNS
    Bool Always TRUE
*/
static bool getSessionData(void)
{
    anc_writeable_config_def_t *write_data = NULL;

    ancConfigManagerGetWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID, (void **)&write_data, (uint16)sizeof(anc_writeable_config_def_t));

    /* Extract session data */
    anc_data.requested_enabled = write_data->initial_anc_state;
    anc_data.persist_anc_enabled = write_data->persist_initial_state;
    anc_data.requested_mode = write_data->initial_anc_mode;
    anc_data.persist_anc_mode = write_data->persist_initial_mode;
    
    ancConfigManagerReleaseConfig(ANC_WRITEABLE_CONFIG_BLK_ID);

    /* Get ANC configurations set by user */
    ancStateManager_GetAncConfigs();

    return TRUE;
}

/******************************************************************************
DESCRIPTION
    This function is responsible for persisting any of the ANC session data
    that is required.
*/
static void setSessionData(void)
{
    anc_writeable_config_def_t *write_data = NULL;

    if(ancConfigManagerGetWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID, (void **)&write_data, 0))
    {
        if (anc_data.persist_anc_enabled)
        {
            DEBUG_LOG("setSessionData: Persisting ANC enabled state %d\n", anc_data.requested_enabled);
            write_data->initial_anc_state =  anc_data.requested_enabled;
        }

        if (anc_data.persist_anc_mode)
        {
            DEBUG_LOG("setSessionData: Persisting ANC mode enum:anc_mode_t:%d\n", anc_data.requested_mode);
            write_data->initial_anc_mode = anc_data.requested_mode;
        }

        ancConfigManagerUpdateWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID);
    }

    /* Store ANC configurations set by user */
    ancStateManager_SetAncConfigs();
}

static void ancStateManager_EnableAncMics(void)
{
    anc_readonly_config_def_t *read_data = NULL;

    DEBUG_LOG_FN_ENTRY("ancStateManager_EnableAncMics");

    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
/* Since ANC HW is running in PDM domain and sample rate config is ideally ignored;
 * On concurrency case probably keeping sample rate at 16kHz is an optimal value */
#define ANC_SAMPLE_RATE        (16000U)
        uint16 feedForwardLeftMic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
        uint16 feedForwardRightMic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
        uint16 feedBackLeftMic = read_data->anc_mic_params_r_config.feed_back_left_mic;
        uint16 feedBackRightMic = read_data->anc_mic_params_r_config.feed_back_right_mic;

        if(feedForwardLeftMic != MICROPHONE_NONE)
        {
            Microphones_SetMicRate(feedForwardLeftMic, ANC_SAMPLE_RATE, non_exclusive_user);
            anc_data.mic_src_ff_left = Microphones_TurnOnMicrophone(feedForwardLeftMic, non_exclusive_user);
        }

        if(feedForwardRightMic != MICROPHONE_NONE)
        {
            Microphones_SetMicRate(feedForwardRightMic, ANC_SAMPLE_RATE, non_exclusive_user);
            anc_data.mic_src_ff_right = Microphones_TurnOnMicrophone(feedForwardRightMic, non_exclusive_user);
        }

        if(feedBackLeftMic != MICROPHONE_NONE)
        {
            Microphones_SetMicRate(feedBackLeftMic, ANC_SAMPLE_RATE, non_exclusive_user);
            anc_data.mic_src_fb_left = Microphones_TurnOnMicrophone(feedBackLeftMic, non_exclusive_user);
        }

        if(feedBackRightMic != MICROPHONE_NONE)
        {
            Microphones_SetMicRate(feedBackRightMic, ANC_SAMPLE_RATE, non_exclusive_user);
            anc_data.mic_src_fb_right = Microphones_TurnOnMicrophone(feedBackRightMic, non_exclusive_user);
        }
    }
}

static void ancStateManager_DisableAncMics(void)
{
    anc_readonly_config_def_t *read_data = NULL;

    DEBUG_LOG_FN_ENTRY("ancStateManager_DisableAncMics");

    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        uint16 feedForwardLeftMic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
        uint16 feedForwardRightMic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
        uint16 feedBackLeftMic = read_data->anc_mic_params_r_config.feed_back_left_mic;
        uint16 feedBackRightMic = read_data->anc_mic_params_r_config.feed_back_right_mic;

        if(feedForwardLeftMic != MICROPHONE_NONE)
        {
            Microphones_TurnOffMicrophone(feedForwardLeftMic, non_exclusive_user);
            anc_data.mic_src_ff_left = NULL;
        }

        if(feedForwardRightMic != MICROPHONE_NONE)
        {
            Microphones_TurnOffMicrophone(feedForwardRightMic, non_exclusive_user);
            anc_data.mic_src_ff_right = NULL;
        }

        if(feedBackLeftMic != MICROPHONE_NONE)
        {
            Microphones_TurnOffMicrophone(feedBackLeftMic, non_exclusive_user);
            anc_data.mic_src_fb_left = NULL;
        }

        if(feedBackRightMic != MICROPHONE_NONE)
        {
            Microphones_TurnOffMicrophone(feedBackRightMic, non_exclusive_user);
            anc_data.mic_src_fb_right = NULL;
        }
    }
}

static bool ancStateManager_EnableAncHw(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_EnableAncHw");
    ancStateManager_EnableAncMics();

    return AncEnable(TRUE);
}

static bool ancStateManager_DisableAncHw(void)
{
    bool ret_val = FALSE;

    DEBUG_LOG_FN_ENTRY("ancStateManager_DisableAncHw");
    ret_val = AncEnable(FALSE);
    ancStateManager_DisableAncMics();

    return ret_val;
}

static bool ancStateManager_EnableAncHwWithMutePathGains(void)
{
    bool ret_val = FALSE;
    DEBUG_LOG_FN_ENTRY("ancStateManager_EnableAncHwWithMutePathGains");
    ancStateManager_EnableAncMics();

    bool update_filter_coefficients = TRUE;
    bool mute_path_gains = TRUE;

#if defined ENABLE_ADAPTIVE_ANC && defined ENABLE_ANC_FAST_MODE_SWITCH
    /* To support dynamic filter switching on Mora2.0 Adaptive ANC builds */
    update_filter_coefficients = AncConfig_IsAdvancedAnc() ? FALSE : TRUE;
#endif

    ret_val = AncEnableWithConfig(update_filter_coefficients, mute_path_gains);

    if (ret_val && !update_filter_coefficients)
    {
        KymeraAncCommon_SetFilterCoefficients();
    }

    if(ret_val)
    {
        KymeraAnc_CreateAndDestroySwitchedPassThroughGraph();
    }

    return ret_val;
}

/******************************************************************************
DESCRIPTION
    Read the configuration from the ANC Mic params.
*/
static bool readMicConfigParams(anc_mic_params_t *anc_mic_params)
{
    anc_readonly_config_def_t *read_data = NULL;

    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        uint16 feedForwardLeftMic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
        uint16 feedForwardRightMic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
        uint16 feedBackLeftMic = read_data->anc_mic_params_r_config.feed_back_left_mic;
        uint16 feedBackRightMic = read_data->anc_mic_params_r_config.feed_back_right_mic;

        memset(anc_mic_params, 0, sizeof(anc_mic_params_t));

        if (feedForwardLeftMic != MICROPHONE_NONE)
        {
            anc_mic_params->enabled_mics |= feed_forward_left;
            anc_mic_params->feed_forward_left = *Microphones_GetMicrophoneConfig(feedForwardLeftMic);
        }

        if (feedForwardRightMic != MICROPHONE_NONE)
        {
            anc_mic_params->enabled_mics |= feed_forward_right;
            anc_mic_params->feed_forward_right = *Microphones_GetMicrophoneConfig(feedForwardRightMic);
        }

        if (feedBackLeftMic != MICROPHONE_NONE)
        {
            anc_mic_params->enabled_mics |= feed_back_left;
            anc_mic_params->feed_back_left = *Microphones_GetMicrophoneConfig(feedBackLeftMic);
        }

        if (feedBackRightMic != MICROPHONE_NONE)
        {
            anc_mic_params->enabled_mics |= feed_back_right;
            anc_mic_params->feed_back_right = *Microphones_GetMicrophoneConfig(feedBackRightMic);
        }

        ancConfigManagerReleaseConfig(ANC_READONLY_CONFIG_BLK_ID);

        return TRUE;
    }
    DEBUG_LOG("readMicConfigParams: Failed to read ANC Config Block\n");
    return FALSE;
}

/****************************************************************************    
DESCRIPTION
    Read the number of configured Anc modes.
*/
static uint8 readNumModes(void)
{
    anc_readonly_config_def_t *read_data = NULL;
    uint8 num_modes = 0;

    /* Read the existing Config data */
    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        num_modes = read_data->num_anc_modes;
        ancConfigManagerReleaseConfig(ANC_READONLY_CONFIG_BLK_ID);
    }
    return num_modes;
}

anc_mode_t AncStateManager_GetMode(void)
{
    return (anc_data.requested_mode);
}

/******************************************************************************
DESCRIPTION
    This function reads the ANC configuration and initialises the ANC library
    returns TRUE on success FALSE otherwise 
*/ 
static bool configureAndInit(void)
{
    anc_mic_params_t anc_mic_params;
    bool init_success = FALSE;

    /* ANC state manager task creation */
    anc_data.client_tasks = TaskList_Create();

    if(readMicConfigParams(&anc_mic_params) && getSessionData())
    {
        AncSetDevicePsKey(PS_KEY_ANC_FINE_GAIN_TUNE_KEY);

        if(AncInit(&anc_mic_params, AncStateManager_GetMode()))
        {
            /* Update local state to indicate successful initialisation of ANC */
            anc_data.current_mode = anc_data.requested_mode;
            anc_data.previous_config = ANC_TOGGLE_CONFIGURED_OFF;
            anc_data.previous_mode = anc_data.requested_mode+1; /*Stored as toggle config ranging from 1 to MAX*/
            anc_data.actual_enabled = FALSE;
            anc_data.num_modes = readNumModes();
            anc_data.demo_state = FALSE;
            anc_data.adaptivity = FALSE;
            anc_data.task_data.handler = ancstateManager_HandleMessage;
#ifdef ENABLE_USB_DEVICE_FRAMEWORK_IN_ANC_TUNING
            anc_data.usb_enumerated = FALSE;
            anc_data.SavedUsbAppIntrface = NULL;
            anc_data.usb_config = ANC_USB_CONFIG_NO_USB;
#endif
            init_success = TRUE;
            anc_data.leakthrough_gain_update_bits = 0;            
            anc_data.self_speech_in_progress = FALSE;
            anc_data.concurrency_in_progress = FALSE;            
            anc_data.concurrency_scenario = 0;
            anc_data.system_triggered_disable = FALSE;
            memset(anc_data.leakthrough_gain, 0, sizeof(anc_data.leakthrough_gain));

            AncSetTopology(ancConfigFilterTopology());
        }
    }

    return init_success;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Uninitialised State

RETURNS
    Bool indicating if processing event was successful or not.
*/ 
static bool ancStateManager_HandleEventsInUninitialisedState(anc_state_manager_event_id_t event)
{
    bool init_success = FALSE;

    switch (event)
    {
        case anc_state_manager_event_initialise:
        {
            if(configureAndInit())
            {
                init_success = TRUE;
                changeState(anc_state_manager_power_off);
            }
            else
            {
                DEBUG_LOG("handleUninitialisedEvent: ANC Failed to initialise due to incorrect mic configuration/ licencing issue \n");
                /* indicate error by Led */
            }
        }
        break;
        
        default:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInUninitialisedState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return init_success;
}

static void ancStateManager_SetPreviousModeInAncDisable(void)
{
    if (anc_data.system_triggered_disable)
    {
        anc_data.system_triggered_disable = FALSE;
        DEBUG_LOG("system_triggered_disable reset");
    }
    else    
    {
       if (!((anc_data.concurrency_in_progress) || (ancStateManager_IsSelfSpeechInProgress())))
       {
            ancStateManager_SetPreviousMode(anc_data.requested_mode + 1);
            ancStateManager_MsgRegisteredClientsOnPrevModeUpdate();
       }
    }
}

/******************************************************************************
DESCRIPTION
    Event handler for the Power Off State

RETURNS
    Bool indicating if processing event was successful or not.
*/ 
static bool ancStateManager_HandleEventsInPowerOffState(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;

    DEBUG_ASSERT(!anc_data.actual_enabled, "ancStateManager_HandleEventsInPowerOffState: ANC actual enabled in power off state\n");

    switch (event)
    {
        case anc_state_manager_event_power_on:
        {
            anc_state_manager_t next_state = anc_state_manager_disabled;
            anc_data.power_on = TRUE;

            /* If we were previously enabled then enable on power on */
            if (anc_data.requested_enabled)
            {
                if(updateLibState(anc_data.requested_enabled, anc_data.requested_mode))
                {
                    /* Lib is enabled */
                    next_state = anc_state_manager_enabled;
                }
            }
            /* Update state */
            changeState(next_state);
            
            event_handled = TRUE;
        }
        break;

        default:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInPowerOffState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Enabled State

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool ancStateManager_HandleEventsInEnabledState(anc_state_manager_event_id_t event)
{
    /* Assume failure until proven otherwise */
    bool event_handled = FALSE;
    anc_state_manager_t next_state = anc_state_manager_disabled;

    switch (event)
    {
        case anc_state_manager_event_power_off:
        {
            /* When powering off we need to disable ANC in the VM Lib first */
            next_state = anc_state_manager_power_off;
            anc_data.power_on = FALSE;
        }
        /* fallthrough */
        case anc_state_manager_event_disable:
        {
            ancStateManager_PreAncDisable();
            /* Only update requested enabled if not due to a power off event */
            anc_data.requested_enabled = (next_state == anc_state_manager_power_off);

#ifdef INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN
            KymeraAnc_DisconnectPassthroughSupportChainFromDac();
            KymeraAnc_DestroyPassthroughSupportChain();
#endif

            /*Stop Internal Timers, if running*/
            ancStateManager_StopConfigTimer();
            ancStateManager_StopGentleMuteTimer();
            ancStateManager_StopModeChangeSettlingTimer();

            if (next_state == anc_state_manager_power_off)
            {
                ancStateManager_DisableAnc(anc_state_manager_power_off);
            }
            else
            {
                if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
                {
                    KymeraAncCommon_AdaptiveAncEnableGentleMute();
                    ancStateManager_DisableAncPostGentleMute();

                    /*Clear Quiet mode buffers */
                    AancQuietMode_ResetQuietModeData();

                    /*Gentle mute is not applicable to QCC517x. Instead AHM will ramp down the gain*/
                    /*Post AHM ramp down anc_state_manager_event_capability_disable_complete handles the ANC HW disable*/
                    KymeraAncCommon_PreAncDisable();
                }
                else
                {
                    ancStateManager_DisableAnc(anc_state_manager_disabled);
                }
            }
            
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_set_mode_1: /* fallthrough */
        case anc_state_manager_event_set_mode_2:
        case anc_state_manager_event_set_mode_3:
        case anc_state_manager_event_set_mode_4:
        case anc_state_manager_event_set_mode_5:
        case anc_state_manager_event_set_mode_6:
        case anc_state_manager_event_set_mode_7:
        case anc_state_manager_event_set_mode_8:
        case anc_state_manager_event_set_mode_9:
        case anc_state_manager_event_set_mode_10:            
        {
            anc_data.requested_mode = getModeFromSetModeEvent(event);

            if (anc_data.requested_mode != anc_data.current_mode)
            {
                if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
                {
                    if (AncConfig_IsAdvancedAnc())
                    {
                        /*Gentle mute is not applicable to QCC517x. Instead AHM will ramp down the gain*/
                        KymeraAncCommon_PreAncModeChange();
#ifdef ENABLE_ANC_FAST_MODE_SWITCH
                        ancStateManager_UpdateAncMode();
#endif
                    }
                    else
                    {
                        KymeraAncCommon_AdaptiveAncEnableGentleMute();
                        ancStateManager_UpdateAncModePostGentleMute();
                    }
                }
                else
                {
                    ancStateManager_UpdateAncMode();
                }
            }
            else
            {
                ancStateManager_ClearLock(event);
            }
            event_handled = TRUE;
        }
        break;
       
        case anc_state_manager_event_toggle_way:
        {
            ancStateManager_HandleToggleWay();           
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_world_volume_up:
        case anc_state_manager_event_world_volume_down:
        {
            ancStateManager_HandleWorldVolume(event);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_activate_anc_tuning_mode:
        {            
            ancStateManager_SetupAncTuningMode ();
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        }
        break;

        case anc_state_manager_event_activate_adaptive_anc_tuning_mode:
        {
            ancStateManager_setupAdaptiveAncTuningMode();
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        }
        break;

        case anc_state_manager_event_set_anc_leakthrough_gain:
        {
            setAncLeakthroughGain();

            /* Notify ANC gain update to registered clients */
            ancStateManager_MsgRegisteredClientsOnGainUpdate();

            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        }
        break;
        
        case anc_state_manager_event_config_timer_expiry:
        {            
            ancStateManager_EnableAdaptiveAnc();
            event_handled = TRUE;
        }
        break;
        
        case anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry:
        {
            if (!AncConfig_IsAdvancedAnc())
            {
                ancStateManager_DisableAnc(anc_state_manager_disabled);
            }
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_capability_disable_complete:
        {
            ancStateManager_DisableAnc(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_capability_enable_complete:
        {
            if (ancStateManager_GetReconnectionData() != NULL)
            {
                ancStateManager_HandleReconnectionDataAfterAncStateUpdate();
                ancStateManager_ResetReconnectionData();
            }
            else if (AncConfig_IsAncModeLeakThrough(anc_data.current_mode))
            {
                ancStateManager_UpdateWorldVolumeGain();
            }
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry:
        {           
            if (!AncConfig_IsAdvancedAnc())
            {
                ancStateManager_UpdateAncMode();
            }
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_capability_mode_change_trigger:
        {
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
            ancStateManager_UpdateAncMode();
#endif
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_detected:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeDetected();
            }
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_not_detected:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeCleared();
            }
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_enable:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeEnable();
            }
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_disable:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeDisable();
            }
        }
        break;

        case anc_state_manager_event_set_filter_path_gains:
            ancStateManager_StopModeChangeSettlingTimer();
            ancStateManager_RampupOnANCEnable();
        break;

        case anc_state_manager_event_set_filter_path_gains_on_mode_change:
             ancStateManager_StopModeChangeSettlingTimer();
             ancStateManager_RampupOnModeChange();
        break;

        case anc_state_manager_event_set_world_volume_gain:
        {
            event_handled = ancStateManager_SetWorldVolumeGain();
            ancStateManager_ClearLock(event);
        }
        break;

        case anc_state_manager_event_set_world_volume_balance:
        {
            event_handled = ancStateManager_SetWorldVolumeBalance();
            ancStateManager_ClearLock(event);
        }
        break;
        
        case anc_state_manager_event_wind_detect:
            ancStateManager_WindDetect();
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        break;

        case anc_state_manager_event_wind_release:
            ancStateManager_WindRelease();
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        break;

        case anc_state_manager_event_wind_enable:
            WindDetect_Enable(TRUE);
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        break;
        
        case anc_state_manager_event_wind_disable:
            WindDetect_Enable(FALSE);
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        break;

       case anc_state_manager_event_anti_howling_enable:
            KymeraAncCommon_UpdateHowlingDetectionState(TRUE);
            ancStateManager_ClearLock(event);
            break;

       case anc_state_manager_event_anti_howling_disable:
            KymeraAncCommon_UpdateHowlingDetectionState(FALSE);
            ancStateManager_ClearLock(event);
            break;

       case anc_state_manager_event_concurrency_connect_req:
            KymeraAncCommon_HandleConcurrencyUpdate(TRUE);
            break;

       case anc_state_manager_event_concurrency_disconnect_req:
            KymeraAncCommon_HandleConcurrencyUpdate(FALSE);
            break;
        
        default:
        {
            DEBUG_LOG_INFO("ancStateManager_HandleEventsInEnabledState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Disabled State

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool ancStateManager_HandleEventsInDisabledState(anc_state_manager_event_id_t event)
{
    /* Assume failure until proven otherwise */
    bool event_handled = FALSE;

    switch (event)
    {
        case anc_state_manager_event_power_off:
        {
            /* Nothing to do, just update state */
            changeState(anc_state_manager_power_off);
            anc_data.power_on = FALSE;
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_enable:
        {
            /* Try to enable */
            anc_state_manager_t next_state = anc_state_manager_enabled;
            anc_data.requested_enabled = TRUE;

            KymeraAnc_CreatePassthroughSupportChain();
            KymeraAnc_ConnectPassthroughSupportChainToDac();

            /* Enable ANC */
            updateLibState(anc_data.requested_enabled, anc_data.requested_mode);
            
            /* Update state */
            changeState(next_state);
           
            event_handled = TRUE;
#ifndef ENABLE_ADAPTIVE_ANC
            ancStateManager_ClearLock(event);
#endif
        }
        break;

        case anc_state_manager_event_set_mode_1: /* fallthrough */
        case anc_state_manager_event_set_mode_2:
        case anc_state_manager_event_set_mode_3:
        case anc_state_manager_event_set_mode_4:
        case anc_state_manager_event_set_mode_5:
        case anc_state_manager_event_set_mode_6:
        case anc_state_manager_event_set_mode_7:
        case anc_state_manager_event_set_mode_8:
        case anc_state_manager_event_set_mode_9:
        case anc_state_manager_event_set_mode_10:
        {
            /* Update the requested ANC Mode, will get applied next time we enable */
            anc_data.requested_mode = getModeFromSetModeEvent(event);            
            ancStateManager_MsgRegisteredClientsOnModeUpdateInDisabledState();
            ancStateManager_ClearLock(event);

            ancStateManager_SetPreviousModeInAncDisable();
            event_handled = TRUE;
        }
        break;
        
        case anc_state_manager_event_activate_anc_tuning_mode:
        {
            ancStateManager_SetupAncTuningMode ();
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        }
        break;

        case anc_state_manager_event_activate_adaptive_anc_tuning_mode:
        {
            ancStateManager_setupAdaptiveAncTuningMode();
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        }
        break;
            
        case anc_state_manager_event_toggle_way:           
        {
            ancStateManager_HandleToggleWay();           
            event_handled = TRUE;
        }
        break;
                   
        default:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInDisabledState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

static bool ancStateManager_HandleEventsInTuningState(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;
    
    switch(event)
    {
        case anc_state_manager_event_usb_enumerated_start_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_usb_enumerated_start_tuning\n");

            ancStateManager_EnterAncTuning();

            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_power_off:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_power_off\n");

            ancStateManager_ExitTuning();
            ancStateManager_UsbDetachTuningDevice();

            changeState(anc_state_manager_power_off);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_deactivate_tuning_mode:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_deactivate_tuning_mode\n");

            ancStateManager_ExitTuning();
            ancStateManager_UsbDetachTuningDevice();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
            ancStateManager_ClearLock(event);
        }
        break;

        case anc_state_manager_event_usb_detached_stop_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_usb_detached_stop_tuning\n");

            ancStateManager_ExitTuning();
            ancStateManager_UsbDetachTuningDevice();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;
        
        default:
        break;
    }
    return event_handled;
}

static bool ancStateManager_HandleEventsInAdaptiveAncTuningState(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;
    
    switch(event)
    {
        case anc_state_manager_event_usb_enumerated_start_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_usb_enumerated_start_tuning\n");

            ancStateManager_EnterAdaptiveAncTuning();

            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_power_off:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_power_off\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitAdaptiveAncTuning();

            changeState(anc_state_manager_power_off);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_deactivate_adaptive_anc_tuning_mode:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_deactivate_adaptive_anc_tuning_mode\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitAdaptiveAncTuning();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
            ancStateManager_ClearLock(event);

        }
        break;

        case anc_state_manager_event_usb_detached_stop_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_usb_detached_stop_tuning\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitAdaptiveAncTuning();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;
        
        default:
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Entry point to the ANC State Machine.

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool ancStateManager_HandleEvent(anc_state_manager_event_id_t event)
{
    bool ret_val = FALSE;

    DEBUG_LOG("ancStateManager_HandleEvent: ANC Handle Event %d in State %d\n", event, anc_data.state);

    switch(anc_data.state)
    {
        case anc_state_manager_uninitialised:
            ret_val = ancStateManager_HandleEventsInUninitialisedState(event);
        break;
        
        case anc_state_manager_power_off:
            ret_val = ancStateManager_HandleEventsInPowerOffState(event);
        break;

        case anc_state_manager_enabled:
            ret_val = ancStateManager_HandleEventsInEnabledState(event);
        break;

        case anc_state_manager_disabled:
            ret_val = ancStateManager_HandleEventsInDisabledState(event);
        break;

        case anc_state_manager_tuning_mode_active:
            ret_val = ancStateManager_HandleEventsInTuningState(event);
        break;

        case anc_state_manager_adaptive_anc_tuning_mode_active:
            ret_val = ancStateManager_HandleEventsInAdaptiveAncTuningState(event);
        break;

        default:
            DEBUG_LOG("ancStateManager_HandleEvent: Unhandled state [%d]\n", anc_data.state);
        break;
    }
    return ret_val;
}

static void ancStateManager_DisableAnc(anc_state_manager_t next_state)
{
    DEBUG_LOG("ancStateManager_DisableAnc");
    
    /* Disable ANC */
    updateLibState(FALSE, anc_data.requested_mode);
    
    /* Update state */
    changeState(next_state);

    ancStateManager_SetPreviousModeInAncDisable();

    /*If ANC is disabled by user when self speech is in trigger mode*/
    if (ancStateManager_IsSelfSpeechInProgress())
    {
        ancStateManager_ResetSelfSpeechStatus();
        AncStateManager_SetMode((anc_mode_t)(AncStateManager_GetPreviousMode()-1));
    }

    ancStateManager_ClearLock(anc_state_manager_event_disable);
}

static void ancStateManager_UpdateAncMode(void)
{
    anc_state_manager_event_id_t event = anc_state_manager_event_set_mode_1 + anc_data.requested_mode;
    DEBUG_LOG("ancStateManager_UpdateAncMode");
    /* Update the ANC Mode */
    updateLibState(anc_data.requested_enabled, anc_data.requested_mode);
    ancStateManager_ClearLock(event);
}

static void ancStateManager_ResetBalanceIfAdaptiveLeakthrough(void)
{
    anc_mode_t cur_mode = AncStateManager_GetMode();

    if(AncConfig_IsAncModeAdaptiveLeakThrough(cur_mode))
    {
        if (AncStateManager_GetBalancePercentage() != ANC_WORLD_VOLUME_ZERO_BALANCE)
        {
            /* ANC compander persists makeup gain and comes up with the same makeup gain on next enable.
               If the balance is not restored, apps P1 wouldn't know if it should update the balance on next enable.
               Restoring the balance ensures that same makeup gain is persisted on both devices and configured balance can be applied on next enable. */
            KymeraAnc_UpdateWorldVolumeBalance(ANC_WORLD_VOLUME_ZERO_BALANCE);
        }
    }
}

static void ancStateManager_PreAncPowerOff(void)
{
    ancStateManager_ResetBalanceIfAdaptiveLeakthrough();
}

static void ancStateManager_PreAncDisable(void)
{
    ancStateManager_ResetBalanceIfAdaptiveLeakthrough();
}

static void ancStateManager_PreAncModeChange(void)
{
    ancStateManager_ResetBalanceIfAdaptiveLeakthrough();
}

static bool ancStateManager_SetWorldVolumeGain(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_mode_t cur_mode = AncStateManager_GetMode();
    int8 balance_percentage = AncStateManager_GetBalancePercentage();
    int8 requested_world_volume_gain_dB=  anc_sm->requested_world_volume_gain_dB;
    bool status = FALSE;

    if(AncStateManager_IsEnabled() && AncConfig_IsAncModeLeakThrough(cur_mode) &&
            KymeraAnc_UpdateWorldVolumeGain(requested_world_volume_gain_dB, balance_percentage))
    {
        status = TRUE;
        ancStateManager_SetWorldVolumeGainInAncData(cur_mode, requested_world_volume_gain_dB);

        ancStateManager_MsgRegisteredClientsOnWorldVolumeGainUpdate(requested_world_volume_gain_dB);

        if (AncConfig_IsAncModeStatic(GetAncData()->current_mode))
        {
            ancStateManager_UpdateStaticLeakthroughCurrentWorldVolumeGain();
        }
    }

    return status;
}

static bool ancStateManager_SetWorldVolumeBalance(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    uint8 requested_balance_info=  anc_sm->requested_balance_info;

    anc_sm->balance_info.balance_percentage = ANC_SM_GET_PERCENTAGE_FROM_ENCODED_BALANCE_INFO(requested_balance_info);
    anc_sm->balance_info.balance_device_side = ANC_SM_GET_DEVICE_SIDE_FROM_ENCODED_BALANCE_INFO(requested_balance_info);

    if(AncStateManager_IsEnabled() && AncConfig_IsAncModeLeakThrough(AncStateManager_GetMode()))
    {
        KymeraAnc_UpdateWorldVolumeBalance(AncStateManager_GetBalancePercentage());

        if (AncConfig_IsAncModeStatic(GetAncData()->current_mode))
        {
            ancStateManager_UpdateStaticLeakthroughCurrentWorldVolumeGain();
        }
    }

    ancStateManager_MsgRegisteredClientsOnWorldVolumeBalanceUpdate(anc_sm->balance_info.balance_device_side, anc_sm->balance_info.balance_percentage);

    return TRUE;
}

static void ancStateManager_SetLock(anc_state_manager_event_id_t event)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    DEBUG_LOG("ancStateManager_SetLock, Event: enum:anc_state_manager_event_id_t %d", event);

    if(anc_sm->anc_lock)
    {
        DEBUG_LOG("ANC lock is already set");
    }
    else
    {
        anc_sm->anc_lock = TRUE;
        DEBUG_LOG("ANC lock is set");
    }
}

static void ancStateManager_ClearLock(anc_state_manager_event_id_t event)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    DEBUG_LOG("ancStateManager_ClearLock, Event: enum:anc_state_manager_event_id_t %d", event);

    if(anc_sm->anc_lock)
    {
        anc_sm->anc_lock = FALSE;
        DEBUG_LOG("ANC lock is cleared");
    }
    else
    {
        DEBUG_LOG("ANC lock is already cleared");
    }
}

/*******************************************************************************
 * All the functions from this point onwards are the ANC module API functions
 * The functions are simply responsible for injecting
 * the correct event into the ANC State Machine, which is then responsible
 * for taking the appropriate action.
 ******************************************************************************/

bool AncStateManager_Init(Task init_task)
{
    UNUSED(init_task);

    anc_readonly_config_def_t *read_data = NULL;
    ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data);

    uint16 feed_forward_left_mic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
    uint16 feed_forward_right_mic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
    uint16 internal_mic = appConfigMicInternal();

    /* Check if feedforward mics are configured */
    bool is_ff_mic_config_valid = (feed_forward_left_mic != MICROPHONE_NONE) || (feed_forward_right_mic != MICROPHONE_NONE);

    if(is_ff_mic_config_valid && (internal_mic != MICROPHONE_NONE))
    {
        /* Check if SCO and ANC mics are same in case of feedforward mics are configured only. */
        if((internal_mic == feed_forward_left_mic) || (internal_mic == feed_forward_right_mic))
        {
            /*Unsupported configuration*/
            DEBUG_LOG_ALWAYS("AncStateManager_Init: Unsupported CVC Mic Configuration with ANC");
            Panic();
        }
    }

    /* Initialise the ANC VM Lib */
    if(ancStateManager_HandleEvent(anc_state_manager_event_initialise))
    {
        /* Register with Physical state as observer to know if there are any physical state changes */
        appPhyStateRegisterClient(AncStateManager_GetTask());

        /*Register with Kymera for unsolicited  messaging */
        Kymera_ClientRegister(AncStateManager_GetTask());

        /*Register with Output manager for setting the ANC mode beahviour during concurrency*/
        Kymera_OutputRegisterForIndications(&AncSmIndicationCallbacks);

        /* Initialisation successful, go ahead with ANC power ON*/
        AncStateManager_PowerOn();
    }
    return TRUE;
}

void AncStateManager_PowerOn(void)
{
    /* Power On ANC */
    if(!ancStateManager_HandleEvent(anc_state_manager_event_power_on))
    {
        DEBUG_LOG("AncStateManager_PowerOn: Power On ANC failed\n");
    }
}

void AncStateManager_PowerOff(void)
{
    ancStateManager_PreAncPowerOff();

    /* Power Off ANC */
    if (!ancStateManager_HandleEvent(anc_state_manager_event_power_off))
    {
        DEBUG_LOG("AncStateManager_PowerOff: Power Off ANC failed\n");
    }
}

void AncStateManager_Enable(void)
{
    /* Enable ANC */
    DEBUG_LOG("AncStateManager_Enable");
    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_internal_event_disable);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_internal_event_enable);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_internal_event_enable, NULL, &anc_sm->anc_lock);
}

void AncStateManager_Disable(void)
{
    DEBUG_LOG("AncStateManager_Disable");
    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_internal_event_enable);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_internal_event_disable);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_internal_event_disable, NULL, &anc_sm->anc_lock);
}

void AncStateManager_SetMode(anc_mode_t mode)
{
    DEBUG_LOG("AncStateManager_SetMode");
    anc_state_manager_data_t *anc_sm = GetAncData();
    MESSAGE_MAKE(req, ANC_STATE_MANAGER_INTERNAL_EVENT_MODE_CHANGE_T);
    req->new_mode = mode;
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_internal_event_mode_change);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_internal_event_mode_change, req, &anc_sm->anc_lock);
}

void AncStateManager_SetWorldVolumeUp(void)
{
    DEBUG_LOG("AncStateManager_SetWorldVolumeUp");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_world_volume_down);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_world_volume_up);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_world_volume_up, NULL, &anc_sm->anc_lock);
}

void AncStateManager_SetWorldVolumeDown(void)
{
    DEBUG_LOG("AncStateManager_SetWorldVolumeDown");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_world_volume_up);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_world_volume_down);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_world_volume_down, NULL, &anc_sm->anc_lock);
}

void AncStateManager_EnterAncTuningMode(void)
{
    DEBUG_LOG("AncStateManager_EnterAncTuningMode");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_deactivate_tuning_mode);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_activate_anc_tuning_mode);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_activate_anc_tuning_mode, NULL, &anc_sm->anc_lock);
}

void AncStateManager_ExitAncTuningMode(void)
{
    DEBUG_LOG("AncStateManager_ExitAncTuningMode");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_activate_anc_tuning_mode);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_deactivate_tuning_mode);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_deactivate_tuning_mode, NULL, &anc_sm->anc_lock);
}

void AncStateManager_HandleWindDetect(void)
{
    DEBUG_LOG("AncStateManager_HandleWindDetect");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_release);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_detect);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_wind_detect, NULL, &anc_sm->anc_lock);
}

void AncStateManager_HandleWindRelease(void)
{
    DEBUG_LOG("AncStateManager_HandleWindDetect");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_detect);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_release);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_wind_release, NULL, &anc_sm->anc_lock);
}

void AncStateManager_HandleWindEnable(void)
{
    DEBUG_LOG("AncStateManager_HandleWindDetect");

    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_disable);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_enable);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_wind_enable, NULL, &anc_sm->anc_lock);
}

void AncStateManager_HandleWindDisable(void)
{
    DEBUG_LOG("AncStateManager_HandleWindDetect");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_enable);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_wind_disable);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_wind_disable, NULL, &anc_sm->anc_lock);
}

void AncStateManager_HandleAntiHowlingEnable(void)
{
    DEBUG_LOG("AncStateManager_HandleAntiHowlingEnable");
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_anti_howling_disable);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_anti_howling_enable);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_anti_howling_enable, NULL, &anc_sm->anc_lock);
}

void AncStateManager_HandleAntiHowlingDisable(void)
{
    DEBUG_LOG("AncStateManager_HandleAntiHowlingDisable");
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_anti_howling_disable);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_anti_howling_enable);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_anti_howling_disable, NULL, &anc_sm->anc_lock);
}

#if defined(ENABLE_ADAPTIVE_ANC)
void AncStateManager_EnterAdaptiveAncTuningMode(void)
{
    DEBUG_LOG("AncStateManager_EnterAdaptiveAncTuningMode");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_deactivate_adaptive_anc_tuning_mode);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_activate_adaptive_anc_tuning_mode);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_activate_adaptive_anc_tuning_mode, NULL, &anc_sm->anc_lock);
}

void AncStateManager_ExitAdaptiveAncTuningMode(void)
{
    DEBUG_LOG("AncStateManager_EnterAdaptiveAncTuningMode");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_activate_adaptive_anc_tuning_mode);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_deactivate_adaptive_anc_tuning_mode);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_deactivate_adaptive_anc_tuning_mode, NULL, &anc_sm->anc_lock);
}

bool AncStateManager_IsAdaptiveAncTuningModeActive(void)
{
    return (anc_data.state == anc_state_manager_adaptive_anc_tuning_mode_active);
}
#endif

void AncStateManager_UpdateAncLeakthroughGain(void)
{
    DEBUG_LOG("AncStateManager_EnterAdaptiveAncTuningMode");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_set_anc_leakthrough_gain);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_set_anc_leakthrough_gain, NULL, &anc_sm->anc_lock);
}

bool AncStateManager_IsEnabled(void)
{
    return (anc_data.state == anc_state_manager_enabled);
}

anc_mode_t AncStateManager_GetCurrentMode(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm->current_mode;
}

uint8 AncStateManager_GetNumberOfModes(void)
{
    return anc_data.num_modes;
}

static anc_mode_t ancStateManager_GetNextMode(anc_mode_t anc_mode)
{
    anc_mode++;
    if(anc_mode >= AncStateManager_GetNumberOfModes())
    {
       anc_mode = anc_mode_1;
    }
    return anc_mode;
}

void AncStateManager_SetNextMode(void)
{
    DEBUG_LOG("AncStateManager_SetNextMode cur:enum:anc_mode_t:%d req:enum:anc_mode_t:%d", anc_data.current_mode, anc_data.requested_mode);
    anc_data.requested_mode = ancStateManager_GetNextMode(anc_data.current_mode);
    AncStateManager_SetMode(anc_data.requested_mode);
 }

bool AncStateManager_IsTuningModeActive(void)
{
    return (anc_data.state == anc_state_manager_tuning_mode_active);
}

void AncStateManager_ClientRegister(Task client_task)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks)
    {
        TaskList_AddTask(anc_sm->client_tasks, client_task);
    }
}

void AncStateManager_ClientUnregister(Task client_task)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks)
    {
        TaskList_RemoveTask(anc_sm->client_tasks, client_task);
    }
}

uint8 AncStateManager_GetAncGain(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm->anc_gain;
}

uint8 AncStateManager_GetAancFBGain(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm -> aanc_fb_gain;
}

uint8 AncStateManager_GetAancFFGain(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm -> aanc_ff_gain;
}

void AncStateManager_StoreAncLeakthroughGain(uint8 anc_leakthrough_gain)
{
    if (!ancConfigWorldVolumedBScale() && AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
    {
        anc_state_manager_data_t *anc_sm = GetAncData();
        anc_sm->anc_gain = anc_leakthrough_gain;

        ancStateManager_PreserveLeakthroughGain(anc_leakthrough_gain);
    }
}

void AncStateManager_GetAdaptiveAncEnableParams(bool *in_ear, audio_anc_path_id *control_path, adaptive_anc_hw_channel_t *hw_channel, anc_mode_t *current_mode)
{
    *in_ear=ancStateManager_GetInEarStatus();
    *control_path = ancStateManager_GetAncPath();
    *hw_channel = adaptive_anc_hw_channel_0;
    *current_mode = anc_data.current_mode;
}

void AncStateManager_HandleToggleWay(void)
{
    DEBUG_LOG("AncStateManager_HandleToggleWay");

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_toggle_way);
    MessageSend(AncStateManager_GetTask(), anc_state_manager_event_toggle_way, NULL);
}

/*! \brief Interface to get ANC toggle configuration */
anc_toggle_config_t AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_t config_id)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_GetAncToggleConfiguration");

    anc_state_manager_data_t *anc_sm_data = GetAncData();

    return anc_sm_data->toggle_configurations.anc_toggle_way_config[ancSmConvertAncToggleIdToToggleIndex(config_id)];
}

/*! \brief Interface to set ANC toggle configuration */
void AncStateManager_SetAncToggleConfiguration(anc_toggle_way_config_id_t config_id, anc_toggle_config_t config)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_SetAncToggleConfiguration");

    anc_state_manager_data_t *anc_sm_data = GetAncData();
    uint8 anc_toggle_way_index = ancSmConvertAncToggleIdToToggleIndex(config_id);

    anc_sm_data->toggle_configurations.anc_toggle_way_config[anc_toggle_way_index] = config;

    ancStateManager_MsgRegisteredClientsOnAncToggleConfigurationUpdate(config_id, config);
}

/*! \brief Interface to get ANC scenario configuration */
anc_toggle_config_t AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_t config_id)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_GetAncScenarioConfiguration");

    anc_toggle_config_t config = anc_toggle_config_is_same_as_current;
    anc_state_manager_data_t *anc_sm_data = GetAncData();

    switch(config_id)
    {
        case anc_scenario_config_id_standalone:
             config = anc_sm_data->standalone_config.anc_config;
            break;

        case anc_scenario_config_id_playback:
            config = anc_sm_data->playback_config.anc_config;
            break;

        case anc_scenario_config_id_sco:
            config = anc_sm_data->sco_config.anc_config;
            break;

        case anc_scenario_config_id_va:
            config = anc_sm_data->va_config.anc_config;
            break;
		
    }

    return config;
}

/*! \brief Interface to set ANC scenario configuration */
void AncStateManager_SetAncScenarioConfiguration(anc_scenario_config_id_t config_id, anc_toggle_config_t config)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_SetAncScenarioConfiguration");

    anc_state_manager_data_t *anc_sm_data = GetAncData();

    switch(config_id)
    {
        case anc_scenario_config_id_standalone:
            anc_sm_data->standalone_config.anc_config = config;
            anc_sm_data->standalone_config.is_same_as_current = (config == anc_toggle_config_is_same_as_current);
            break;

        case anc_scenario_config_id_playback:
            anc_sm_data->playback_config.anc_config = config;
            anc_sm_data->playback_config.is_same_as_current = (config == anc_toggle_config_is_same_as_current);
            break;

        case anc_scenario_config_id_sco:
            anc_sm_data->sco_config.anc_config = config;
            anc_sm_data->sco_config.is_same_as_current = (config == anc_toggle_config_is_same_as_current);
            break;

        case anc_scenario_config_id_va:
            anc_sm_data->va_config.anc_config = config;
            anc_sm_data->va_config.is_same_as_current = (config == anc_toggle_config_is_same_as_current);
            break;

    }

    ancStateManager_MsgRegisteredClientsOnAncScenarioConfigurationUpdate(config_id, config);
}

/*! \brief Interface to enable Adaptive ANC Adaptivity */
void AncStateManager_EnableAdaptiveAncAdaptivity(void)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_EnableAdaptiveAncAdaptivity");

    if((AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode())) &&
            (!AncStateManager_GetAdaptiveAncAdaptivity()))
    {
        KymeraAncCommon_EnableAdaptivity();
        ancStateManager_SetAdaptiveAncAdaptivity(TRUE);

        if(AncStateManager_IsDemoStateActive())
        {
#ifdef ENABLE_ADAPTIVE_ANC
            ancStateManager_StartAancGainTimer();
#endif
        }

        ancStateManager_MsgRegisteredClientsOnAdaptiveAncAdaptivityUpdate(TRUE);
    }
}

/*! \brief Interface to disable Adaptive ANC Adaptivity */
void AncStateManager_DisableAdaptiveAncAdaptivity(void)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_EnableAdaptiveAncAdaptivity");

    if((AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode())) &&
            (AncStateManager_GetAdaptiveAncAdaptivity()))
    {
        KymeraAncCommon_DisableAdaptivity();
        ancStateManager_SetAdaptiveAncAdaptivity(FALSE);

        if(AncStateManager_IsDemoStateActive())
        {
#ifdef ENABLE_ADAPTIVE_ANC
            ancStateManager_StopAancGainTimer();
#endif
        }

        ancStateManager_MsgRegisteredClientsOnAdaptiveAncAdaptivityUpdate(FALSE);
    }
}

/*! \brief Interface to get Adaptive ANC Adaptivity */
bool AncStateManager_GetAdaptiveAncAdaptivity(void)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_GetAdaptiveAncAdaptivity");
    return (anc_data.adaptivity);
}

bool AncStateManager_IsDemoSupported(void)
{
    return ancConfigDemoMode();
}

bool AncStateManager_IsDemoStateActive(void)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_IsDemoStateActive %d", anc_data.demo_state);
    return (anc_data.demo_state);
}

void AncStateManager_SetDemoState(bool demo_active)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_SetDemoState %d", demo_active);
    anc_data.demo_state = demo_active;

#ifdef ENABLE_ADAPTIVE_ANC
    bool is_enable = FALSE;
    if(AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
    {
        is_enable = demo_active && AncStateManager_GetAdaptiveAncAdaptivity();
    }
    if(!is_enable && AncStateManager_IsFBGainTimerUpdateRequire(AncStateManager_GetCurrentMode()))
    {
        is_enable = TRUE;
    }
    is_enable ? ancStateManager_StartAancGainTimer() : ancStateManager_StopAancGainTimer();
#endif

    ancStateManager_MsgRegisteredClientsOnDemoStateUpdate(demo_active);
}

bool AncStateManager_StoreWorldVolumeGain(int8 world_volume_gain_dB)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    int8 min_gain_dB = ancStateManager_GetMinWorldVolumedBGain();
    int8 max_gain_dB = ancStateManager_GetMaxWorldVolumedBGain();
    int8 cur_gain_dB;
    bool status = FALSE;

    if(ancConfigWorldVolumedBScale())
    {
        AncStateManager_GetCurrentWorldVolumeGain(&cur_gain_dB);
        if(cur_gain_dB != world_volume_gain_dB &&
                world_volume_gain_dB >= min_gain_dB && world_volume_gain_dB <= max_gain_dB)
        {
            anc_sm->requested_world_volume_gain_dB = world_volume_gain_dB;
            status = TRUE;
        }
    }

    return status;
}

void AncStateManager_UpdateWorldVolumeGain(void)
{
    DEBUG_LOG("AncStateManager_EnterAdaptiveAncTuningMode");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_set_world_volume_gain);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_set_world_volume_gain, NULL, &anc_sm->anc_lock);
}

int8 AncStateManager_GetRequestedWorldVolumeGain(void)
{
    return GetAncData()->requested_world_volume_gain_dB;
}

bool AncStateManager_GetCurrentWorldVolumeGain(int8* cur_world_volume_gain_dB)
{
    bool status = FALSE;
    anc_mode_t cur_mode = AncStateManager_GetCurrentMode();

    if(AncConfig_IsAncModeLeakThrough(cur_mode))
    {
        *cur_world_volume_gain_dB = ancStateManager_GetWorldVolumeGainFromAncData(cur_mode);
        status = TRUE;
    }

    return status;
}

uint8 AncStateManager_GetWorldVolumedBSliderStepSize(void)
{
    return ancConfigWorldVolumedBScaleStepSize();
}

void AncStateManager_GetCurrentWorldVolumeConfig(anc_sm_world_volume_gain_mode_config_t* world_volume_config)
{
    world_volume_config->max_gain_dB = ancStateManager_GetMaxWorldVolumedBGain();
    world_volume_config->min_gain_dB = ancStateManager_GetMinWorldVolumedBGain();
}

void AncStateManager_StoreWorldVolumeBalanceInfo(anc_sm_world_volume_gain_balance_info_t balance_info)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(ancConfigWorldVolumedBScale())
    {
        anc_sm->requested_balance_info = ANC_SM_ENCODE_BALNCE_INFO(balance_info.balance_percentage, balance_info.balance_device_side);
    }
}

void AncStateManager_UpdateWorldVolumeBalance(void)
{
    DEBUG_LOG("AncStateManager_EnterAdaptiveAncTuningMode");

    anc_state_manager_data_t *anc_sm = GetAncData();

    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_set_world_volume_balance);
    MessageSendConditionally(AncStateManager_GetTask(), anc_state_manager_event_set_world_volume_balance, NULL, &anc_sm->anc_lock);
}

void AncStateManager_StoreEncodedWorldVolumeBalanceInfo(uint8 balance_info)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(ancConfigWorldVolumedBScale())
    {
        anc_sm->requested_balance_info = balance_info;

    }
}

uint8 AncStateManager_GetEncodedWorldVolumeBalanceInfo(void)
{
    return GetAncData()->requested_balance_info;
}

bool AncStateManager_GetCurrentBalanceInfo(anc_sm_world_volume_gain_balance_info_t* balance_info)
{
    *balance_info = GetAncData()->balance_info;
    return TRUE;
}

int8 AncStateManager_GetBalancePercentage(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Balance percentage is positive on 'left' device if balance_device_side is 'left' and vice-versa */
    int8 balance_percentage = anc_sm->balance_info.balance_percentage;

    /* Balance percentage is negative on 'left' device if balance_device_side is 'right' and vice-versa */
    if((ancstateManager_IsLocalDeviceLeft() && anc_sm->balance_info.balance_device_side == ANC_BALANCE_DEVICE_RIGHT) ||
       (!ancstateManager_IsLocalDeviceLeft() && anc_sm->balance_info.balance_device_side == ANC_BALANCE_DEVICE_LEFT))
    {
        balance_percentage = anc_sm->balance_info.balance_percentage * -1;
    }

    return balance_percentage;
}

void AncStateManager_WindDetectionStateUpdateInd(bool enable)
{
    if (WindDetect_IsSupported())
    {
        ancStateManager_MsgRegisteredClientsOnWindDetectionStateUpdate(enable);
    }
}

void AncStateManager_WindReductionUpdateInd(bool enable)
{
    if (WindDetect_IsSupported())
    {
        ancStateManager_MsgRegisteredClientsOnWindReductionUpdate(enable);
        ancstateManager_MsgRegisteredClientsWindFFGainUpdateInd(enable);
    }
}

void AncStateManager_HowlingDetectionStateUpdateInd(bool enable)
{
    if (KymeraAncCommon_IsHowlingDetectionSupported())
    {
        ancStateManager_MsgRegisteredClientsOnHowlingDetectionStateUpdate(enable);
    }
}

/*! \brief Notify Auto transparency state update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnAutoAmbientStateInd(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;
    
    if (anc_sm->client_tasks)
    {    
        message_id = enable ? ANC_AUTO_TRANSPARENCY_ENABLE_IND : ANC_AUTO_TRANSPARENCY_DISABLE_IND;
        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

void AncStateManager_AutoAmbientStateInd(bool enable)
{
    if (AncAutoAmbient_IsSupported())
    {
        ancStateManager_MsgRegisteredClientsOnAutoAmbientStateInd(enable);
    }
}

/*! \brief Notify Auto transparency release time update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnAutoAmbientReleaseTimeInd(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks)
    {
        MESSAGE_MAKE(ind, ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND_T);
        ind->release_time = AncAutoAmbient_GetReleaseTimeConfig();
        TaskList_MessageSend(anc_sm->client_tasks, ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND, ind);
    }
}

void AncStateManager_AutoAmbientReleaseTimeInd(void)
{
    if (AncAutoAmbient_IsSupported())
    {
        ancStateManager_MsgRegisteredClientsOnAutoAmbientReleaseTimeInd();
    }
}

static bool ancStateManager_CanProcessSelfSpeech(void)
{
    bool can_process = TRUE;
    
    /*Ignore the trigger during SCO or VA or LEA Stereo recording concurrency*/
    if (((Kymera_OutputGetConnectedUsers() & output_user_sco)==output_user_sco)
            || Kymera_IsVaActive()
            )
    {
        can_process = FALSE;
    }

    return can_process;
}

void AncStateManager_AutoAmbientTrigger(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_mode_t ambient_mode=appConfigAutoAmbientMode();
    
    DEBUG_LOG_ALWAYS("AncStateManager_AutoAmbientTrigger");

    if (ancStateManager_CanProcessSelfSpeech())
    {
        if (AncAutoAmbient_IsAmbientModeConfigured())
        {
            anc_toggle_config_during_scenario_t *config= ancGetScenarioConfigData(anc_sm->concurrency_scenario);

            ancStateManager_SetSelfSpeechStatus();
            
            if ((anc_sm->concurrency_in_progress) && config && !config->is_same_as_current)
            {  
                DEBUG_LOG("AncStateManager_AutoAmbientTrigger: Concurrency with mode configured");
                ancStateManager_ApplyConfigSynchronised(ambient_mode+1);
            }
            else
            { 
                /*Self speech or concurrency whichever comes first will store the previous mode*/
                ancStateManager_DeriveAndSetPreviousConfig();
                DEBUG_LOG("AncStateManager_AutoAmbientTrigger: Previous Config %d, Configured Ambient Config %d", anc_sm->previous_config, (ambient_mode+1));
                ancStateManager_ApplyConfigSynchronised(ambient_mode+1);
            }
        }
    }
    else
    {    
        DEBUG_LOG_ALWAYS("AncStateManager_AutoAmbientTrigger ignored in SCO/VA/LE Stereo Rec concurrencies");
    }
}

void AncStateManager_AutoAmbientRelease(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    DEBUG_LOG_ALWAYS("AncStateManager_AutoAmbientRelease");

    if (ancStateManager_CanProcessSelfSpeech())
    {
        if (ancStateManager_IsSelfSpeechInProgress())
        {    
            ancStateManager_ResetSelfSpeechStatus();
            
            if (AncAutoAmbient_IsAmbientModeConfigured() /*Action only if ambient is configured*/
                && (AncAutoAmbient_GetReleaseTimeConfig()!=atr_vad_no_release)) /*Ignore if 'No Release' is configured*/
            {
                anc_toggle_config_during_scenario_t *config= ancGetScenarioConfigData(anc_sm->concurrency_scenario);

                if ((anc_sm->concurrency_in_progress) && config && !config->is_same_as_current)
                {
                    DEBUG_LOG("AncStateManager_AutoAmbientRelease concurrency with mode configured");
                    ancStateManager_ConcurrencyConnectAction();
                }
                else
                {
                    DEBUG_LOG("AncStateManager_AutoAmbientRelease concurrency/standalone");
                    ancStateManager_StandaloneActionSynchronised();
                }
            }
        }
    }    
    else
    {    
        DEBUG_LOG_ALWAYS("AncStateManager_AutoAmbientTrigger ignored in SCO/VA/LE Stereo Rec concurrencies");
    }
}

#ifdef ANC_TEST_BUILD
void AncStateManager_ResetStateMachine(anc_state_manager_t state)
{
    anc_data.state = state;
}
#endif /* ANC_TEST_BUILD */

#endif /* ENABLE_ANC */
