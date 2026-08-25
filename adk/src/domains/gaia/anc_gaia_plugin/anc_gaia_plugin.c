/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the  gaia anc framework plugin
*/

#include "anc_gaia_plugin.h"
#include "anc_gaia_plugin_private.h"
#include "anc_state_manager.h"
#include "anc_auto_ambient.h"
#include "ui.h"
#include "phy_state.h"
#include "state_proxy.h"
#include "wind_detect.h"
#include "kymera_anc_common.h"
#include "multidevice.h"

#include <gaia.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#define ANC_GAIA_DEFAULT_GAIN      0x00
#define ANC_GAIA_LOCAL_DEVICE      TRUE
#define ANC_GAIA_REMOTE_DEVICE     !ANC_GAIA_LOCAL_DEVICE


/*! \brief Function pointer definition for the command handler

    \param pdu_id      PDU specific ID for the message

    \param length      Length of the payload

    \param payload     Payload data
*/
static gaia_framework_command_status_t ancGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);

/*! \brief Function that sends all available notifications
*/
static void ancGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t);

/*! \brief Function pointer definition for transport connect
*/
static void ancGaiaPlugin_TransportConnect(GAIA_TRANSPORT *t);

/*! \brief Function pointer definition for transport disconnect
*/
static void ancGaiaPlugin_TransportDisconnect(GAIA_TRANSPORT *t);

/*! \brief Function pointer definition for role change completed
*/
static void ancGaiaPlugin_RoleChangeCompleted(GAIA_TRANSPORT *t, bool is_primary);


/*! GAIA ANC Plugin Message Handler. */
static void ancGaiaPlugin_HandleMessage(Task task, MessageId id, Message message);

static void ancGaiaPlugin_GetAcState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_SetAcState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetNumOfModes(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_GetCurrentMode(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SetMode(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetGain(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SetGain(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetToggleConfigurationCount(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_GetToggleConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_SetToggleConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetScenarioConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_SetScenarioConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetDemoState(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_GetDemoSupport(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SetDemoState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetAdaptationStaus(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SetAdaptationStaus(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_SendWindReductionNotification(bool is_enabled);
static void ancGaiaPlugin_SendAutoTransparencyEnabledNotification(bool is_enabled);
static void ancGaiaPlugin_SendAutoTransparencyReleaseTimeNotification(uint16 config);

static void ancGaiaPlugin_SendAllNotificationsInDemoMode(void);
static void ancGaiaPlugin_SendAllNotificationsInConfigMode(void);

static void ancGaiaPlugin_GetFBGain(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SendFBGainUpdateNotification(uint8 left_gain, uint8 right_gain);

#ifndef INCLUDE_STEREO
/*! \brief To obtain the Static leakthrough gain of remote device, incase of earbud application. */
static uint8 ancGaiaPlugin_GetPeerFFGain(void)
{
    return StateProxy_GetPeerLeakthroughGain();
}
#endif

/*! \brief To identify if local device is left, incase of earbud application. */
static bool ancGaiaPlugin_IsLocalDeviceLeft(void)
{
    bool isLeft = TRUE;

#ifndef INCLUDE_STEREO
    isLeft = Multidevice_IsLeft();
#endif

    return isLeft;
}

static bool ancGaiaPlugin_CanSendNotification(uint8 notification_id)
{
    bool can_notify = TRUE;

#ifdef INCLUDE_STEREO
UNUSED(notification_id);
#else
    /* When the device is put in-case, Phy state gets updated and ANC will be switched off. 
	   But, GAIA link to the device will not be dropped immediately. This leads to some unwanted 
	   notifications being sent to device(e.g., ANC off). Gain notification will be an exception
	   to send zero gain to mobile app to convey that device is (about to enter)in-case */
    can_notify = (appPhyStateIsOutOfCase() || (notification_id == anc_gaia_gain_change_notification));
#endif

    return can_notify;
}

/*! \brief To identify if remote device is incase or not, for the earbud application. */
static bool ancGaiaPlugin_IsPeerIncase(void)
{
    bool isInCase = FALSE;
#ifndef INCLUDE_STEREO
    isInCase = StateProxy_IsPeerInCase();
#endif
    return isInCase;
}

static void ancGaiaPlugin_SendResponse(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 length, const uint8 *payload)
{
    GaiaFramework_SendResponse(t, GAIA_AUDIO_CURATION_FEATURE_ID, pdu_id, length, payload);
}

static void ancGaiaPlugin_SendError(GAIA_TRANSPORT *t, uint8 pdu_id, uint8 status_code)
{
    GaiaFramework_SendError(t, GAIA_AUDIO_CURATION_FEATURE_ID, pdu_id, status_code);
}

static void ancGaiaPlugin_SendNotification(uint8 notification_id, uint16 length, const uint8 *payload)
{
    if(ancGaiaPlugin_CanSendNotification(notification_id))
    {
        GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, notification_id, length, payload);
    }
}

static uint8 ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(anc_mode_t anc_mode)
{
    uint8 mode_payload = anc_mode+1;
    return mode_payload;
}

static anc_mode_t ancGaiaPlugin_ExtractAncModeFromGaiaPayload(uint8 mode_payload)
{
    anc_mode_t anc_mode = (anc_mode_t)(mode_payload-1);
    return anc_mode;
}

static uint8 ancGaiaPlugin_getModeTypeFromAncMode(anc_mode_t anc_mode)
{
    /* To avoid build errors when ENABLE_ANC is not included*/
    UNUSED(anc_mode);

    uint8 anc_mode_type = ANC_GAIA_STATIC_MODE;

    if(AncConfig_IsAncModeAdaptiveLeakThrough(anc_mode))
    {
        anc_mode_type = ANC_GAIA_ADAPTIVE_LEAKTHROUGH_MODE;
    }
    else if(AncConfig_IsAncModeLeakThrough(anc_mode))
    {
        anc_mode_type = ANC_GAIA_LEAKTHROUGH_MODE;
    }
    else if(AncConfig_IsAncModeAdaptive(anc_mode))
    {
        anc_mode_type = ANC_GAIA_ADAPTIVE_MODE;
    }
    else if(AncConfig_IsAncModeStatic(anc_mode))
    {
        anc_mode_type = ANC_GAIA_STATIC_MODE;
    }

    return anc_mode_type;
}

static bool ancIsValidScenarioId(uint8 scenario_id)
{
    if (scenario_id >= ANC_GAIA_MIN_VALID_SCENARIO_ID && scenario_id <= ANC_GAIA_MAX_VALID_SCENARIO_ID)
        return TRUE;
    else
        return FALSE;
}

static bool ancIsValidToggleWay(uint8 toggle_way)
{
    if (toggle_way >= ANC_GAIA_MIN_VALID_TOGGLE_WAY && toggle_way <= ANC_GAIA_MAX_VALID_TOGGLE_WAY)
        return TRUE;
    else
        return FALSE;
}

static bool ancIsValidConfig(uint8 config)
{
    if ((config >= ANC_GAIA_CONFIG_OFF && config <= (AncStateManager_GetNumberOfModes()+1))
        || (config == ANC_GAIA_CONFIG_SAME_AS_CURRENT)
        || (config == ANC_GAIA_TOGGLE_OPTION_NOT_CONFIGURED))
        return TRUE;
    else
        return FALSE;
}

static bool ancGaiaPlugin_CanInjectUiInput(void)
{
    bool can_inject = TRUE;

#ifndef INCLUDE_STEREO
    can_inject = appPhyStateIsOutOfCase(); /* Verify if device is 'out of case' incase of earbud application*/
#endif

    return can_inject;
}

static void ancGaiaPlugin_SetReceivedCommand(GAIA_TRANSPORT *t, uint8 received_command)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    anc_gaia_data -> command_received_transport = t;
    anc_gaia_data -> received_command = received_command;
}

static uint8 ancGaiaPlugin_GetReceivedCommand(void)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    return anc_gaia_data -> received_command;
}

static void ancGaiaPlugin_ResetReceivedCommand(void)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    anc_gaia_data -> command_received_transport = 0;
}

static bool ancGaiaPlugin_IsCommandReceived(void)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    return anc_gaia_data -> command_received_transport ? TRUE : FALSE;
}

static void ancGaiaPlugin_GetdBSliderConfig(anc_sm_world_volume_gain_mode_config_t* world_volume_gain_mode_config,
                                            uint8* num_steps, uint8* step_size, uint8* min_gain_dB, uint8* cur_step)
{
    int8 cur_gain = 0;
    int8 max_gain = world_volume_gain_mode_config->max_gain_dB;
    int8 min_gain = world_volume_gain_mode_config->min_gain_dB;

    if(AncStateManager_GetCurrentWorldVolumeGain(&cur_gain))
    {
        *step_size = AncStateManager_GetWorldVolumedBSliderStepSize();
        *min_gain_dB = (uint8)min_gain;
        *num_steps = ((max_gain - min_gain)/(*step_size))+1;
        *cur_step = ((cur_gain - min_gain)/(*step_size))+1;
    }
}

static uint8 ancGaiaPlugin_GetStepFromCurdBGain(int8 cur_gain)
{
    anc_sm_world_volume_gain_mode_config_t world_volume_gain_config = {0, 0};
    int8 min_gain;
    uint8 step_size;
    uint8 cur_step = 0;

    AncStateManager_GetCurrentWorldVolumeConfig(&world_volume_gain_config);
    min_gain = world_volume_gain_config.min_gain_dB;

    step_size = AncStateManager_GetWorldVolumedBSliderStepSize();

    if (step_size != 0)
    {
        cur_step = ((cur_gain - min_gain)/(step_size))+1;
    }

    return cur_step;
}

static int8 ancGaiaPlugin_GetdBGainFromStep(uint8 step)
{
    anc_sm_world_volume_gain_mode_config_t world_volume_gain_mode_config = {0, 0};
    int8 min_gain, requested_gain;
    uint8 step_size;

    AncStateManager_GetCurrentWorldVolumeConfig(&world_volume_gain_mode_config);
    min_gain = world_volume_gain_mode_config.min_gain_dB;

    step_size = AncStateManager_GetWorldVolumedBSliderStepSize();
    requested_gain = min_gain + ((step-1) * step_size);

    return requested_gain;
}

static void ancGaiaPlugin_SetAncEnable(void)
{
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG("ancGaiaPlugin_SetAncEnable");
        Ui_InjectUiInput(ui_input_anc_on);
    }
}

static void ancGaiaPlugin_SetAncDisable(void)
{
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG("ancGaiaPlugin_SetAncDisable");
        Ui_InjectUiInput(ui_input_anc_off);
    }
}

static void ancGaiaPlugin_SetAncMode(anc_mode_t anc_mode)
{
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG("ancGaiaPlugin_SetMode");
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
}

static void ancGaiaPlugin_SetAncLeakthroughGain(uint8 gain)
{
    UNUSED(gain);
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG("ancGaiaPlugin_SetAncLeakthroughGain");
        AncStateManager_StoreAncLeakthroughGain(gain);
        Ui_InjectRedirectableUiInput(ui_input_anc_set_leakthrough_gain, FALSE);
    }
}

static void ancGaiaPlugin_ToggleAncAdaptivity(void)
{
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG("ancGaiaPlugin_ToggleAncAdaptivity");
        Ui_InjectUiInput(ui_input_anc_adaptivity_toggle_on_off);
    }
}

static bool ancGaiaPlugin_SetAncWorldVolumeGain(uint8 step)
{
    bool status = FALSE;
    int8 required_gain_dB;

    if (ancGaiaPlugin_CanInjectUiInput())
    {
        required_gain_dB = ancGaiaPlugin_GetdBGainFromStep(step);
        if(AncStateManager_StoreWorldVolumeGain(required_gain_dB))
        {
            DEBUG_LOG("ancGaiaPlugin_SetAncWorldVolumeGain, required gain dB: %d", required_gain_dB);
            Ui_InjectRedirectableUiInput(ui_input_anc_set_world_volume_gain, FALSE);
            status = TRUE;
        }
    }

    return status;
}

static void ancGaiaPlugin_SetWorldVolumeBalance(uint8 device_side, uint8 percentage)
{
    anc_sm_world_volume_gain_balance_info_t balance_info;

    if (ancGaiaPlugin_CanInjectUiInput())
    {
        balance_info.balance_device_side = device_side;
        balance_info.balance_percentage = percentage;
        AncStateManager_StoreWorldVolumeBalanceInfo(balance_info);
        DEBUG_LOG("ancGaiaPlugin_SetWorldVolumeBalance device_side: %d, percentage: %d ", balance_info.balance_device_side, balance_info.balance_percentage);
        Ui_InjectRedirectableUiInput(ui_input_anc_set_world_volume_balance, FALSE);
    }
}

/*! \brief Handle local events for ANC data update.and Send response */
static void ancGaiaPlugin_SendResponseToReceivedCommand(GAIA_TRANSPORT *t)
{
    ancGaiaPlugin_SendResponse(t, ancGaiaPlugin_GetReceivedCommand(), 0, NULL);

    ancGaiaPlugin_ResetReceivedCommand();
}

static void ancGaiaPlugin_SendAcStateUpdateNotification(uint8 feature, bool enable)
{
    uint8 notification_id;
    uint8 payload_length;
    uint8* payload;

    notification_id = anc_gaia_ac_state_notification;
    payload_length = ANC_GAIA_AC_STATE_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_AC_FEATURE_OFFSET] = feature;
    payload[ANC_GAIA_AC_STATE_OFFSET] = enable ? ANC_GAIA_STATE_ENABLE : ANC_GAIA_STATE_DISABLE;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(notification_id, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_SendModeUpdateNotification(uint8 mode)
{
    uint8 notification_id;
    uint8 payload_length;
    uint8* payload;

    notification_id = anc_gaia_mode_change_notification;

    payload_length = ANC_GAIA_MODE_CHANGE_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(mode);

    payload[ANC_GAIA_CURRENT_MODE_TYPE_OFFSET] = ancGaiaPlugin_getModeTypeFromAncMode(mode);

    payload[ANC_GAIA_ADAPTATION_CONTROL_OFFSET] = AncConfig_IsAncModeAdaptive(mode) ?
                                    ANC_GAIA_GAIN_CONTROL_SUPPORTED : ANC_GAIA_GAIN_CONTROL_NOT_SUPPORTED;

    payload[ANC_GAIA_GAIN_CONTROL_OFFSET] = (AncConfig_IsAncModeLeakThrough(mode)) ?
                                    ANC_GAIA_GAIN_CONTROL_SUPPORTED : ANC_GAIA_GAIN_CONTROL_NOT_SUPPORTED;

    payload[ANC_GAIA_HOWLING_CONTROL_OFFSET] = (AncConfig_IsAncModeLeakThrough(mode)) ?
                                     ANC_GAIA_HOWLING_CONTROL_NOT_SUPPORTED : ANC_GAIA_HOWLING_CONTROL_SUPPORTED;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(notification_id, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_SendGainUpdateNotification(uint8 left_gain, uint8 right_gain)
{
    uint8 cur_mode;
    uint8 notification_id;
    uint8 payload_length;
    uint8* payload;

    notification_id = anc_gaia_gain_change_notification;
    cur_mode = AncStateManager_GetCurrentMode();

    payload_length = ANC_GAIA_GAIN_CHANGE_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_mode);
    payload[ANC_GAIA_CURRENT_MODE_TYPE_OFFSET] = ancGaiaPlugin_getModeTypeFromAncMode(cur_mode);
    payload[ANC_GAIA_LEFT_GAIN_OFFSET] = left_gain;
    payload[ANC_GAIA_RIGHT_GAIN_OFFSET] = right_gain;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(notification_id, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_SendToggleWayConfigUpdateNotification(anc_toggle_way_config_id_t anc_toggle_way_id, anc_toggle_config_t anc_toggle_config)
{
    uint8 notification_id;
    uint8 payload_length;
    uint8* payload;

    notification_id = anc_gaia_toggle_configuration_notification;
    payload_length = ANC_GAIA_TOGGLE_CONFIGURATION_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_TOGGLE_OPTION_NUM_OFFSET] = (uint8)anc_toggle_way_id;
    payload[ANC_GAIA_TOGGLE_OPTION_VAL_OFFSET] = (uint8)anc_toggle_config;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(notification_id, payload_length, payload);
    free(payload);
}

static void ancGaiaPlugin_SendScenarioConfigUpdateNotification(anc_scenario_config_id_t anc_scenario_config_id, anc_toggle_config_t anc_toggle_config)
{
    uint8 notification_id;
    uint8 payload_length;
    uint8* payload;

    notification_id = anc_gaia_scenario_configuration_notification;
    payload_length = ANC_GAIA_SCENARIO_CONFIGURATION_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_SCENARIO_OFFSET] = (uint8)anc_scenario_config_id;
    payload[ANC_GAIA_SCENARIO_BEHAVIOUR_OFFSET] = (uint8)anc_toggle_config;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(notification_id, payload_length, payload);
    free(payload);
}

static void ancGaiaPlugin_SendAancAdaptivityStatusNotification(bool adaptivity)
{
    uint8 notification_payload_length;
    uint8 notification_payload;

    notification_payload_length = ANC_GAIA_ADAPTATION_STATUS_NOTIFICATION_PAYLOAD_LENGTH;
    notification_payload = adaptivity ? ANC_GAIA_AANC_ADAPTIVITY_RESUMED : ANC_GAIA_AANC_ADAPTIVITY_PAUSED;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(anc_gaia_adaptation_status_notification,
                                        notification_payload_length, &notification_payload);
}

static void ancGaiaPlugin_SendWorldVolumeConfigUpdateNotification(uint8 num_steps, uint8 step_size, int8 min_gain_dB, uint8 cur_step)
{
    uint8 cur_mode;
    uint8 payload_length;
    uint8* payload;

    cur_mode = AncStateManager_GetCurrentMode();

    payload_length = ANC_GAIA_LEAKTHROUGH_dB_GAIN_SLIDER_CONFIG_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_mode);
    payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_NUM_STEPS_OFFSET] = num_steps;
    payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_STEP_SIZE_OFFSET] = step_size;
    payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_MIN_GAIN_OFFSET] = (uint8)min_gain_dB;
    payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_CUR_STEP_OFFSET] = cur_step;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(anc_gaia_leakthrough_dB_gain_slider_configuration_notification, payload_length, payload);
    free(payload);
}

static void ancGaiaPlugin_SendWorldVolumeGainUpdateNotification(uint8 cur_step)
{
    uint8 cur_mode;
    uint8 payload_length;
    uint8* payload;

    cur_mode = AncStateManager_GetCurrentMode();

    payload_length = ANC_GAIA_LEAKTHROUGH_dB_GAIN_CHANGE_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_CURRENT_LKT_dB_GAIN_STEP_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_mode);
    payload[ANC_GAIA_CURRENT_LKT_dB_GAIN_STEP_CURRENT_STEP_OFFSET] = cur_step;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(anc_gaia_leakthrough_dB_gain_change_notification, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_SendLeftRightBalanceUpdateNotification(bool balance_device_side, uint8 balance_percentage)
{
    uint8 payload_length;
    uint8* payload;

    payload_length = ANC_GAIA_LEFT_RIGHT_BALANCE_UPDATE_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_LEFT_RIGHT_BALANCE_DEVICE_SIDE_OFFSET] = (uint8)balance_device_side;
    payload[ANC_GAIA_LEFT_RIGHT_BALANCE_DEVICE_PERCENTAGE_OFFSET] = balance_percentage;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(anc_gaia_left_right_balance_notification, payload_length, payload);
    free(payload);
}

/* In static/leakthrough modes, check if peer device is in-case before sending gain notification
   This will be called when app registers for notifications and upon static/leakthrough gain update from anc domain */
static void ancGaiaPlugin_NotifyGain(uint8 gain)
{
    uint8 left_gain;
    uint8 right_gain;

    if(ancGaiaPlugin_IsLocalDeviceLeft())
    {
        left_gain = gain;
#ifdef INCLUDE_STEREO
        right_gain = ancGaiaPlugin_IsPeerIncase() ? ANC_GAIA_DEFAULT_GAIN : left_gain;
#else
        right_gain = ancGaiaPlugin_IsPeerIncase() ? ANC_GAIA_DEFAULT_GAIN : ancGaiaPlugin_GetPeerFFGain();
#endif
    }
    else
    {
        right_gain = gain;
#ifdef INCLUDE_STEREO
        left_gain = ancGaiaPlugin_IsPeerIncase() ? ANC_GAIA_DEFAULT_GAIN : right_gain;
#else
        left_gain = ancGaiaPlugin_IsPeerIncase() ? ANC_GAIA_DEFAULT_GAIN : ancGaiaPlugin_GetPeerFFGain();
#endif
    }

    ancGaiaPlugin_SendGainUpdateNotification(left_gain, right_gain);
}

static void ancGaiaPlugin_NotifyFBGain(uint8 gain)
{
    uint8 left_gain;
    uint8 right_gain;

    if(ancGaiaPlugin_IsLocalDeviceLeft())
    {
        left_gain = gain;
        right_gain = ancGaiaPlugin_IsPeerIncase() ? ANC_GAIA_DEFAULT_GAIN : left_gain;
    }
    else
    {
        right_gain = gain;
        left_gain = ancGaiaPlugin_IsPeerIncase() ? ANC_GAIA_DEFAULT_GAIN : right_gain;
    }

    ancGaiaPlugin_SendFBGainUpdateNotification(left_gain, right_gain);
}

static void ancGaiaPlugin_NotifyWorldVolumeConfig(anc_sm_world_volume_gain_mode_config_t* world_volume_gain_config)
{
    uint8 num_steps = 0;
    uint8 step_size = 0;
    uint8 min_gain_dB = 0;
    uint8 cur_step = 0;

    ancGaiaPlugin_GetdBSliderConfig(world_volume_gain_config, &num_steps, &step_size, &min_gain_dB, &cur_step);
    ancGaiaPlugin_SendWorldVolumeConfigUpdateNotification(num_steps, step_size, min_gain_dB, cur_step);
}

static void ancGaiaPlugin_NotifyWorldVolumeGain(int8 anc_world_volume_gain_dBgain)
{
    uint8 step;

    step = ancGaiaPlugin_GetStepFromCurdBGain(anc_world_volume_gain_dBgain);
    ancGaiaPlugin_SendWorldVolumeGainUpdateNotification(step);
}

static void ancGaiaPlugin_NotifyLeftRightBalance(anc_sm_world_volume_gain_balance_info_t* balance_info)
{
    bool balance_device_side = balance_info->balance_device_side;
    uint8 balance_percentage = balance_info->balance_percentage;

    ancGaiaPlugin_SendLeftRightBalanceUpdateNotification(balance_device_side, balance_percentage);
}

static void ancGaiaPlugin_SendDemoStateNotification(bool is_demo_active)
{
    if(AncStateManager_IsDemoSupported())
    {
        uint8 demo_state = is_demo_active ? ANC_GAIA_DEMO_STATE_ACTIVE: ANC_GAIA_DEMO_STATE_INACTIVE;

        if(ancGaiaPlugin_IsCommandReceived())
        {
            anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
            ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
        }

        ancGaiaPlugin_SendNotification(anc_gaia_demo_state_notification,
                                            ANC_GAIA_DEMO_STATE_NOTIFICATION_PAYLOAD_LENGTH, &demo_state);

        is_demo_active ? ancGaiaPlugin_SendAllNotificationsInDemoMode() :
                            ancGaiaPlugin_SendAllNotificationsInConfigMode();
    }
}

static void ancGaiaPlugin_SendWindNoiseDetectionStateNotification(bool is_enabled)
{
    DEBUG_LOG("ancGaiaPlugin_SendWindNoiseDetectionStateNotification, enabled: %d", is_enabled);

    if(WindDetect_IsSupported())
    {
        uint8 state = is_enabled ? ANC_GAIA_WIND_NOISE_REDUCTION_STATE_ENABLE: ANC_GAIA_WIND_NOISE_REDUCTION_STATE_DISABLE;

        if(ancGaiaPlugin_IsCommandReceived())
        {
            anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
            ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
        }
        DEBUG_LOG("ancGaiaPlugin_SendWindNoiseDetectionStateNotification, enabled: %d", is_enabled);

        ancGaiaPlugin_SendNotification(anc_gaia_wind_noise_detection_state_change_notification,
                                            ANC_GAIA_WIND_NOISE_DETECTION_STATE_NOTIFICATION_PAYLOAD_LENGTH, &state);
    }
}

static void ancGaiaPlugin_SendHowlingDetectionStateNotification(bool is_enabled)
{
    DEBUG_LOG("ancGaiaPlugin_SendHowlingDetectionStateNotification, enabled: %d", is_enabled);

    if(KymeraAncCommon_IsHowlingDetectionSupported())
    {
        uint8 state = is_enabled ? ANC_GAIA_HOWLING_DETECTION_STATE_ENABLE : ANC_GAIA_HOWLING_DETECTION_STATE_DISABLE;

        if(ancGaiaPlugin_IsCommandReceived())
        {
            anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
            ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
        }
        DEBUG_LOG("ancGaiaPlugin_SendHowlingDetectionStateNotification, enabled: %d", is_enabled);

        ancGaiaPlugin_SendNotification(anc_gaia_howling_detection_state_change_notification,
                                            ANC_GAIA_HOWLING_DETECTION_STATE_NOTIFICATION_PAYLOAD_LENGTH, &state);
    }
}

static void ancGaiaPlugin_SendFBGainUpdateNotification(uint8 left_gain, uint8 right_gain)
{
    uint8 cur_mode;
    uint8 notification_id;
    uint8 payload_length;
    uint8* payload;

    notification_id = anc_gaia_fb_gain_change_notification;
    cur_mode = AncStateManager_GetCurrentMode();

    payload_length = ANC_GAIA_FB_GAIN_CHANGE_NOTIFICATION_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_mode);
    payload[ANC_GAIA_CURRENT_MODE_TYPE_OFFSET] = ancGaiaPlugin_getModeTypeFromAncMode(cur_mode);
    payload[ANC_GAIA_LEFT_GAIN_OFFSET] = left_gain;
    payload[ANC_GAIA_RIGHT_GAIN_OFFSET] = right_gain;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(notification_id, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_SendAllNotificationsInDemoMode(void)
{
    anc_mode_t anc_mode = AncStateManager_GetCurrentMode();
    bool adaptivity;
    uint8 gain;
    anc_sm_world_volume_gain_mode_config_t world_volume_gain_config = {0, 0};
    int8 world_volume_gain_dB = 0;
    anc_sm_world_volume_gain_balance_info_t balance_info = {0, 0};

    ancGaiaPlugin_SendAcStateUpdateNotification(GAIA_FEATURE_ANC, AncStateManager_IsEnabled());

    ancGaiaPlugin_SendModeUpdateNotification(anc_mode);

    if(AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
    {
        adaptivity = AncStateManager_GetAdaptiveAncAdaptivity();
        ancGaiaPlugin_SendAancAdaptivityStatusNotification(adaptivity);
    }
    else
    {
        gain = AncStateManager_GetAncGain();
        ancGaiaPlugin_NotifyGain(gain);
    }

    if(AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
    {
        AncStateManager_GetCurrentWorldVolumeConfig(&world_volume_gain_config);
        ancGaiaPlugin_NotifyWorldVolumeConfig(&world_volume_gain_config);

        if(AncStateManager_GetCurrentWorldVolumeGain(&world_volume_gain_dB))
        {
            ancGaiaPlugin_NotifyWorldVolumeGain(world_volume_gain_dB);
        }

        if(AncStateManager_GetCurrentBalanceInfo(&balance_info))
        {
            ancGaiaPlugin_NotifyLeftRightBalance(&balance_info);
        }
    }

    if(WindDetect_IsSupported())
    {
        ancGaiaPlugin_SendWindNoiseDetectionStateNotification(WindDetect_IsWindNoiseReductionEnabled());
        ancGaiaPlugin_SendWindReductionNotification(FALSE);
    }
    if(KymeraAncCommon_IsHowlingDetectionSupported())
    {
        ancGaiaPlugin_SendHowlingDetectionStateNotification(KymeraAncCommon_IsHowlingDetectionEnabled());
    }
    if(AncConfig_IsAdvancedAnc())
    {
        gain = AncStateManager_GetAancFBGain();
        ancGaiaPlugin_NotifyFBGain(gain);
    }
}

static void ancGaiaPlugin_SendAllNotificationsInConfigMode(void)
{
    ancGaiaPlugin_SendAcStateUpdateNotification(GAIA_FEATURE_ANC, AncStateManager_IsEnabled());

    ancGaiaPlugin_SendToggleWayConfigUpdateNotification(anc_toggle_way_config_id_1, AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_1));
    ancGaiaPlugin_SendToggleWayConfigUpdateNotification(anc_toggle_way_config_id_2, AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_2));
    ancGaiaPlugin_SendToggleWayConfigUpdateNotification(anc_toggle_way_config_id_3, AncStateManager_GetAncToggleConfiguration(anc_toggle_way_config_id_3));

    ancGaiaPlugin_SendScenarioConfigUpdateNotification(anc_scenario_config_id_standalone, AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_standalone));
    ancGaiaPlugin_SendScenarioConfigUpdateNotification(anc_scenario_config_id_playback, AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_playback));
    ancGaiaPlugin_SendScenarioConfigUpdateNotification(anc_scenario_config_id_sco, AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_sco));
    ancGaiaPlugin_SendScenarioConfigUpdateNotification(anc_scenario_config_id_va, AncStateManager_GetAncScenarioConfiguration(anc_scenario_config_id_va));

    if (AncAutoAmbient_IsSupported())
    {
        ancGaiaPlugin_SendAutoTransparencyEnabledNotification(AncAutoAmbient_IsEnabled());
        ancGaiaPlugin_SendAutoTransparencyReleaseTimeNotification(AncAutoAmbient_GetReleaseTimeConfig());
    }
}

/* Update gain when 
       1. secondary device comes out of case or goes in-case
       2. primary device goes in-case */
static void ancGaiaPlugin_NotifyGainUpdateUponPhyStateUpdate(uint8 new_gain, bool is_local)
{
    DEBUG_LOG("ancGaiaPlugin_NotifyGainUpdateUponPhyStateUpdate");
    uint8 current_gain = AncStateManager_GetAncGain();
    uint8 left_gain;
    uint8 right_gain;

    if(AncStateManager_IsEnabled() &&
            !AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
    {
        if(ancGaiaPlugin_IsLocalDeviceLeft())
        {
            left_gain = is_local ? new_gain : current_gain;
            right_gain = is_local ? current_gain : new_gain;
        }
        else
        {
            left_gain = is_local ? current_gain : new_gain;
            right_gain = is_local ? new_gain : current_gain;
        }

        ancGaiaPlugin_SendGainUpdateNotification(left_gain, right_gain);
    }
}

/* Update gain when
       1. secondary device comes out of case or goes in-case
       2. primary device goes in-case */
static void ancGaiaPlugin_NotifyFBGainUpdateUponPhyStateUpdate(uint8 new_gain, bool is_local)
{
    uint8 current_gain = AncStateManager_GetAancFBGain();
    uint8 left_gain;
    uint8 right_gain;

    if(AncStateManager_IsEnabled() && AncConfig_IsAdvancedAnc())
    {
        if(ancGaiaPlugin_IsLocalDeviceLeft())
        {
            left_gain = is_local ? new_gain : current_gain;
            right_gain = is_local ? current_gain : new_gain;
        }
        else
        {
            left_gain = is_local ? current_gain : new_gain;
            right_gain = is_local ? new_gain : current_gain;
        }

        ancGaiaPlugin_SendFBGainUpdateNotification(left_gain, right_gain);
    }
}

static void ancGaiaPlugin_HandleLocalInCaseUpdate(void)
{
    DEBUG_LOG("ancGaiaPlugin_HandleLocalInCaseUpdate");
    if(!ancGaiaPlugin_IsPeerIncase())
    {
        /* Since the local device is going in case, peer will definitely be primary.
           Hence, update local device gain as zero indicating that device went in case */
        ancGaiaPlugin_NotifyGainUpdateUponPhyStateUpdate(ANC_GAIA_DEFAULT_GAIN, ANC_GAIA_LOCAL_DEVICE);
        ancGaiaPlugin_NotifyFBGainUpdateUponPhyStateUpdate(ANC_GAIA_DEFAULT_GAIN, ANC_GAIA_LOCAL_DEVICE);
    }
}

static void ancGaiaPlugin_HandleRemoteOutOfCaseUpdate(void)
{
    DEBUG_LOG("ancGaiaPlugin_HandleRemoteOutOfCaseUpdate");
    /* It is guaranteed that Anc gain on both devices
           will be same for non-adaptive modes */
    ancGaiaPlugin_NotifyGainUpdateUponPhyStateUpdate(AncStateManager_GetAncGain(), ANC_GAIA_REMOTE_DEVICE);
    ancGaiaPlugin_NotifyFBGainUpdateUponPhyStateUpdate(AncStateManager_GetAancFBGain(), ANC_GAIA_REMOTE_DEVICE);
}

static void ancGaiaPlugin_HandleRemoteInCaseUpdate(void)
{
    DEBUG_LOG("ancGaiaPlugin_HandleRemoteInCaseUpdate");
    ancGaiaPlugin_NotifyGainUpdateUponPhyStateUpdate(ANC_GAIA_DEFAULT_GAIN, ANC_GAIA_REMOTE_DEVICE);
    ancGaiaPlugin_NotifyFBGainUpdateUponPhyStateUpdate(ANC_GAIA_DEFAULT_GAIN, ANC_GAIA_REMOTE_DEVICE);
}

static void ancGaiaPlugin_HandleRemotePhyStateUpdate(PHY_STATE_CHANGED_IND_T* remote_phy)
{
    DEBUG_LOG_INFO("ancGaiaPlugin_HandleRemotePhyStateUpdate: state %d, event %d", remote_phy->new_state,
                                                                                    remote_phy->event);
    if(remote_phy->new_state == PHY_STATE_IN_CASE)
    {
        ancGaiaPlugin_HandleRemoteInCaseUpdate();
    }
    else if(remote_phy->event == phy_state_event_out_of_case ||
            remote_phy->event == phy_state_event_in_ear)
    {
        ancGaiaPlugin_HandleRemoteOutOfCaseUpdate();
    }
}

static void ancGaiaPlugin_HandleAncStateUpdateInd(bool enable)
{
    ancGaiaPlugin_SendAcStateUpdateNotification(GAIA_FEATURE_ANC, enable);
}

static void ancGaiaPlugin_HandleAncModeUpdateInd(ANC_UPDATE_MODE_CHANGED_IND_T* anc_data)
{
    anc_mode_t anc_mode = anc_data->mode;
    ancGaiaPlugin_SendModeUpdateNotification((uint8)anc_mode);
}

static void ancGaiaPlugin_HandleAncGainUpdateInd(ANC_UPDATE_GAIN_IND_T* anc_data)
{
    uint8 anc_gain = anc_data->anc_gain;
    ancGaiaPlugin_NotifyGain(anc_gain);
}

static void ancGaiaPlugin_HandleAncFeedBackGainUpdateInd(AANC_FB_GAIN_UPDATE_IND_T* anc_data)
{
    uint8 anc_gain = anc_data->aanc_fb_gain;
    ancGaiaPlugin_NotifyFBGain(anc_gain);
}

static void ancGaiaPlugin_HandleAncFFGainUpdateInd(ANC_FF_GAIN_NOTIFY_T* anc_data)
{
    uint8 left_gain;
    uint8 right_gain;

    left_gain = anc_data->left_ff_gain;
    right_gain = anc_data->right_ff_gain;

    ancGaiaPlugin_SendGainUpdateNotification(left_gain, right_gain);
}

static void ancGaiaPlugin_HandleFBGainUpdateNotification(ANC_FB_GAIN_NOTIFY_T* anc_data)
{
    uint8 left_gain;
    uint8 right_gain;

    left_gain = anc_data->left_fb_gain;
    right_gain = anc_data->right_fb_gain;

    ancGaiaPlugin_SendFBGainUpdateNotification(left_gain, right_gain);
}

static void ancGaiaPlugin_SendWindReductionNotification(bool is_enabled)
{
    uint8 payload_length = ANC_GAIA_WIND_NOISE_REDUCTION_NOTIFICATION_PAYLOAD_LENGTH;
    uint8* payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    DEBUG_LOG("ancGaiaPlugin_SendWindReductionNotification enabled %d", is_enabled);

    payload[ANC_GAIA_WNR_OFFSET_L] = is_enabled;
    payload[ANC_GAIA_WNR_OFFSET_R] = is_enabled;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(anc_gaia_wind_noise_reduction_notification, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_SendAutoTransparencyEnabledNotification(bool is_enabled)
{
    uint8 payload_length = ANC_GAIA_AUTO_TRANSPARENCY_NOTIFICATION_PAYLOAD_LENGTH;
    uint8* payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    DEBUG_LOG("ancGaiaPlugin_SendAutoTransparencyEnabledNotification enabled %d", is_enabled);

    payload[ANC_GAIA_AUTO_TRANSPARENCY_OFFSET] = is_enabled;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(anc_gaia_auto_transparency_state_notification, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_SendAutoTransparencyReleaseTimeNotification(uint16 config)
{
    uint8 payload_length = ANC_GAIA_AUTO_TRANSPARENCY_NOTIFICATION_PAYLOAD_LENGTH;
    uint8* payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    DEBUG_LOG("ancGaiaPlugin_SendAutoTransparencyReleaseTimeNotification %d", config);

    payload[ANC_GAIA_AUTO_TRANSPARENCY_OFFSET] = (uint8) config;

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponseToReceivedCommand(anc_gaia_data->command_received_transport);
    }

    ancGaiaPlugin_SendNotification(anc_gaia_auto_transparency_release_time_notification, payload_length, payload);

    free(payload);
}

static void ancGaiaPlugin_HandleAncToggleWayConfigUpdateInd(ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T* msg)
{
    anc_toggle_way_config_id_t anc_toggle_config_id = msg->anc_toggle_config_id;
    anc_toggle_config_t anc_config = msg->anc_config;

    ancGaiaPlugin_SendToggleWayConfigUpdateNotification(anc_toggle_config_id, anc_config);
}

static void ancGaiaPlugin_HandleAncScenarioConfigUpdateInd(ANC_SCENARIO_CONFIG_UPDATE_IND_T* msg)
{
    anc_scenario_config_id_t anc_scenario_config_id = msg->anc_scenario_config_id;
    anc_toggle_config_t anc_config = msg->anc_config;

    ancGaiaPlugin_SendScenarioConfigUpdateNotification(anc_scenario_config_id, anc_config);
}

static void ancGaiaPlugin_HandleWorldVolumedBGainUpdateInd(ANC_WORLD_VOLUME_GAIN_DB_UPDATE_IND_T* anc_data)
{
    int8 anc_world_volume_gain_dBgain = anc_data->world_volume_gain_dB;
    ancGaiaPlugin_NotifyWorldVolumeGain(anc_world_volume_gain_dBgain);
}

static void ancGaiaPlugin_HandleWorldVolumeBalanceUpdateInd(ANC_WORLD_VOLUME_BALANCE_UPDATE_IND_T* anc_data)
{
    ancGaiaPlugin_NotifyLeftRightBalance((anc_sm_world_volume_gain_balance_info_t*)anc_data);
}

static void ancGaiaPlugin_HandleWorldVolumeConfigUpdateInd(ANC_WORLD_VOLUME_CONFIG_UPDATE_IND_T* msg)
{
    ancGaiaPlugin_NotifyWorldVolumeConfig((anc_sm_world_volume_gain_mode_config_t*)msg);
}

static void ancGaiaPlugin_HandleStateProxyUpdate(STATE_PROXY_EVENT_T* msg)
{
    if(msg->source == state_proxy_source_remote &&
            msg->type == state_proxy_event_type_phystate)
    {
        ancGaiaPlugin_HandleRemotePhyStateUpdate(&msg->event.phystate);
    }
}

static void ancGaiaPlugin_HandlePhyStateUpdate(PHY_STATE_CHANGED_IND_T* msg)
{
    DEBUG_LOG_INFO("ancGaiaPlugin_HandlePhyStateUpdate: state %d, event %d", msg->new_state,
                   msg->event);

    if(msg->new_state == PHY_STATE_IN_CASE)
    {
        ancGaiaPlugin_HandleLocalInCaseUpdate();
    }
}

static void ancGaiaPlugin_HandleMessage(Task task, MessageId id, Message msg)
{
    UNUSED(task);

    switch (id)
    {
        case ANC_UPDATE_STATE_DISABLE_IND:
        case ANC_UPDATE_STATE_ENABLE_IND:
            {
                ancGaiaPlugin_HandleAncStateUpdateInd(id == ANC_UPDATE_STATE_ENABLE_IND);
            }
            break;

        case ANC_UPDATE_MODE_CHANGED_IND:
            {
                ancGaiaPlugin_HandleAncModeUpdateInd((ANC_UPDATE_MODE_CHANGED_IND_T *)msg);
            }
            break;

        case ANC_UPDATE_GAIN_IND:
            {
                ancGaiaPlugin_HandleAncGainUpdateInd((ANC_UPDATE_GAIN_IND_T *)msg);
            }
            break;

        /* ANC FF Gain notification */
        case ANC_FF_GAIN_NOTIFY:
            {
                ancGaiaPlugin_HandleAncFFGainUpdateInd((ANC_FF_GAIN_NOTIFY_T *)msg);
            }
            break;

        /* ANC config update notification */

        case ANC_TOGGLE_WAY_CONFIG_UPDATE_IND:
            {
                ancGaiaPlugin_HandleAncToggleWayConfigUpdateInd((ANC_TOGGLE_WAY_CONFIG_UPDATE_IND_T *)msg);
            }
            break;

        case ANC_SCENARIO_CONFIG_UPDATE_IND:
            {
                ancGaiaPlugin_HandleAncScenarioConfigUpdateInd((ANC_SCENARIO_CONFIG_UPDATE_IND_T *)msg);
            }
            break;

        /* AANC adaptivity status change notification */
        case ANC_UPDATE_AANC_ADAPTIVITY_PAUSED_IND:
        case ANC_UPDATE_AANC_ADAPTIVITY_RESUMED_IND:
            {
                if(AncStateManager_IsDemoStateActive())
                {
                    ancGaiaPlugin_SendAancAdaptivityStatusNotification(id == ANC_UPDATE_AANC_ADAPTIVITY_RESUMED_IND);
                }
            }
            break;

        /* Demo mode state change notification */
        case ANC_UPDATE_DEMO_MODE_DISABLE_IND:
        case ANC_UPDATE_DEMO_MODE_ENABLE_IND:
            {
                ancGaiaPlugin_SendDemoStateNotification(id == ANC_UPDATE_DEMO_MODE_ENABLE_IND);
            }
            break;
            
        case ANC_UPDATE_WIND_DETECTION_DISABLE_IND:
        case ANC_UPDATE_WIND_DETECTION_ENABLE_IND:
            {
                ancGaiaPlugin_SendWindNoiseDetectionStateNotification(id == ANC_UPDATE_WIND_DETECTION_ENABLE_IND);
            }
            break;

            
        case ANC_UPDATE_WIND_REDUCTION_DISABLE_IND:
        case ANC_UPDATE_WIND_REDUCTION_ENABLE_IND:
            {
                ancGaiaPlugin_SendWindReductionNotification(id == ANC_UPDATE_WIND_REDUCTION_ENABLE_IND);
            }
            break;    
        
        case ANC_AUTO_TRANSPARENCY_DISABLE_IND:
        case ANC_AUTO_TRANSPARENCY_ENABLE_IND:
            {
                ancGaiaPlugin_SendAutoTransparencyEnabledNotification(id == ANC_AUTO_TRANSPARENCY_ENABLE_IND);
            }
            break;

        case ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND:
            {
                ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND_T *msg1 = ((ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND_T*)msg);
                DEBUG_LOG_INFO("ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND: %d", msg1->release_time);
                ancGaiaPlugin_SendAutoTransparencyReleaseTimeNotification(((ANC_AUTO_TRANSPARENCY_RELEASE_TIME_IND_T*)msg)->release_time);
            }
            break;

        case ANC_UPDATE_HOWLING_DETECTION_DISABLE_IND:
        case ANC_UPDATE_HOWLING_DETECTION_ENABLE_IND:
            {
                ancGaiaPlugin_SendHowlingDetectionStateNotification(id == ANC_UPDATE_HOWLING_DETECTION_ENABLE_IND);
            }
            break;

        case ANC_WORLD_VOLUME_GAIN_DB_UPDATE_IND:
            {
                ancGaiaPlugin_HandleWorldVolumedBGainUpdateInd((ANC_WORLD_VOLUME_GAIN_DB_UPDATE_IND_T *)msg);
            }
            break;

        case ANC_WORLD_VOLUME_BALANCE_UPDATE_IND:
            {
                ancGaiaPlugin_HandleWorldVolumeBalanceUpdateInd((ANC_WORLD_VOLUME_BALANCE_UPDATE_IND_T *)msg);
            }
            break;

        case ANC_WORLD_VOLUME_CONFIG_UPDATE_IND:
            {
                ancGaiaPlugin_HandleWorldVolumeConfigUpdateInd((ANC_WORLD_VOLUME_CONFIG_UPDATE_IND_T *)msg);
            }
            break;

        case STATE_PROXY_EVENT:
            {
                DEBUG_LOG_INFO("ancGaiaPlugin_HandleMessage: STATE_PROXY_EVENT");
                ancGaiaPlugin_HandleStateProxyUpdate((STATE_PROXY_EVENT_T*)msg);
            }
            break;

        case PHY_STATE_CHANGED_IND:
            {
                DEBUG_LOG_INFO("ancGaiaPlugin_HandleMessage: PHY_STATE_CHANGED_IND");
                ancGaiaPlugin_HandlePhyStateUpdate((PHY_STATE_CHANGED_IND_T*)msg);
            }
            break;

        case AANC_FB_GAIN_UPDATE_IND:
            {
                ancGaiaPlugin_HandleAncFeedBackGainUpdateInd((AANC_FB_GAIN_UPDATE_IND_T *)msg);
            }
            break;

        case ANC_FB_GAIN_NOTIFY:
            {
                ancGaiaPlugin_HandleFBGainUpdateNotification((ANC_FB_GAIN_NOTIFY_T *)msg);
            }
            break;

        default:
            break;
    }
}

static void ancGaiaPlugin_GetAcState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_GetAcState");
    uint8 response_payload_length = ANC_GAIA_GET_AC_STATE_RESPONSE_PAYLOAD_LENGTH;
    uint8* response_payload;

    if(payload_length == ANC_GAIA_GET_AC_STATE_PAYLOAD_LENGTH)
    {
        response_payload = PanicUnlessMalloc(response_payload_length * sizeof(uint8));

        response_payload[ANC_GAIA_AC_FEATURE_OFFSET] = payload[ANC_GAIA_AC_FEATURE_OFFSET];

        if(payload[ANC_GAIA_AC_FEATURE_OFFSET] == GAIA_FEATURE_ANC)
        {
            response_payload[ANC_GAIA_AC_STATE_OFFSET] = AncStateManager_IsEnabled() ? ANC_GAIA_STATE_ENABLE : ANC_GAIA_STATE_DISABLE;

            DEBUG_LOG("ancGaiaPlugin_GetAcState, AC State for feature %d is %d",
                      response_payload[ANC_GAIA_AC_FEATURE_OFFSET], response_payload[ANC_GAIA_AC_STATE_OFFSET]);
            ancGaiaPlugin_SendResponse(t, anc_gaia_get_ac_state,
                                            ANC_GAIA_GET_AC_STATE_RESPONSE_PAYLOAD_LENGTH, response_payload);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_get_ac_state, invalid_parameter);
        }

        free(response_payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_ac_state, invalid_parameter);
    }
}

static void ancGaiaPlugin_SetAcState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetAcState");

    if(payload_length == ANC_GAIA_SET_AC_STATE_PAYLOAD_LENGTH)
    {
        if(payload[ANC_GAIA_AC_FEATURE_OFFSET] == GAIA_FEATURE_ANC)
        {
            if(payload[ANC_GAIA_AC_STATE_OFFSET] == ANC_GAIA_SET_ANC_STATE_ENABLE)
            {
                ancGaiaPlugin_SetAncEnable();
            }
            else if(payload[ANC_GAIA_AC_STATE_OFFSET] == ANC_GAIA_SET_ANC_STATE_DISABLE)
            {
                ancGaiaPlugin_SetAncDisable();
            }
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_ac_state);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_ac_state, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_ac_state, invalid_parameter);
    }
}

static void ancGaiaPlugin_GetNumOfModes(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetNumOfModes");

    uint8 payload= AncStateManager_GetNumberOfModes();

    DEBUG_LOG("ancGaiaPlugin_GetNumOfModes, Number of modes = %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_num_modes,
                                    ANC_GAIA_GET_NUM_OF_MODES_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_GetCurrentMode(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetCurrentMode");

    uint8 payload_length;
    uint8 *payload;
    anc_mode_t anc_mode= AncStateManager_GetCurrentMode();

    payload_length = ANC_GAIA_GET_CURRENT_MODE_RESPONSE_PAYLOAD_LENGTH;
    payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

    payload[ANC_GAIA_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(anc_mode);

    payload[ANC_GAIA_CURRENT_MODE_TYPE_OFFSET] = ancGaiaPlugin_getModeTypeFromAncMode(anc_mode);

    payload[ANC_GAIA_ADAPTATION_CONTROL_OFFSET] = AncConfig_IsAncModeAdaptive(anc_mode) ?
                                    ANC_GAIA_GAIN_CONTROL_SUPPORTED : ANC_GAIA_GAIN_CONTROL_NOT_SUPPORTED;

    payload[ANC_GAIA_GAIN_CONTROL_OFFSET] = (AncConfig_IsAncModeLeakThrough(anc_mode)) ?
                                    ANC_GAIA_GAIN_CONTROL_SUPPORTED : ANC_GAIA_GAIN_CONTROL_NOT_SUPPORTED;
    payload[ANC_GAIA_HOWLING_CONTROL_OFFSET] = (AncConfig_IsAncModeLeakThrough(anc_mode)) ?
                                    ANC_GAIA_HOWLING_CONTROL_NOT_SUPPORTED : ANC_GAIA_HOWLING_CONTROL_SUPPORTED ;

    ancGaiaPlugin_SendResponse(t, anc_gaia_get_current_mode,payload_length, payload);
    
    free(payload);
}


static void ancGaiaPlugin_SetMode(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetAncMode");

    if(payload_length == ANC_GAIA_SET_MODE_PAYLOAD_LENGTH)
    {
        anc_mode_t mode = ancGaiaPlugin_ExtractAncModeFromGaiaPayload(*payload);
        ancGaiaPlugin_SetAncMode(mode);
        ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_mode);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_mode, invalid_parameter);
    }
}

static void ancGaiaPlugin_GetGain(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetGain");

    uint8 payload_length;
    uint8* payload;
    uint8 anc_gain;
    uint8 cur_anc_mode;

    cur_anc_mode = AncStateManager_GetCurrentMode();

    if(AncStateManager_IsEnabled())
    {
		if(AncConfig_IsAncModeAdaptive(cur_anc_mode))
		{
			anc_gain = AncStateManager_GetAancFFGain();
		}
		else
		{
		    anc_gain = AncStateManager_GetAncGain();
		}

        payload_length = ANC_GAIA_GET_GAIN_RESPONSE_PAYLOAD_LENGTH;
        payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

        payload[ANC_GAIA_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_anc_mode);
        payload[ANC_GAIA_CURRENT_MODE_TYPE_OFFSET] = ancGaiaPlugin_getModeTypeFromAncMode(cur_anc_mode);
        payload[ANC_GAIA_LEFT_GAIN_OFFSET] = anc_gain;
        payload[ANC_GAIA_RIGHT_GAIN_OFFSET] = anc_gain;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_gain, ANC_GAIA_GET_GAIN_RESPONSE_PAYLOAD_LENGTH, payload);

        free(payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_gain, incorrect_state);
    }

}

static void ancGaiaPlugin_SetGain(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetGain");

    if(AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
    {
        if(payload_length == ANC_GAIA_SET_GAIN_PAYLOAD_LENGTH &&
                (payload[ANC_GAIA_SET_LEFT_GAIN_OFFSET] == payload[ANC_GAIA_SET_RIGHT_GAIN_OFFSET]))
        {
            ancGaiaPlugin_SetAncLeakthroughGain(payload[ANC_GAIA_SET_LEFT_GAIN_OFFSET]);
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_gain);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_gain, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_gain, incorrect_state);
    }
}

static void ancGaiaPlugin_GetToggleConfigurationCount(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetToggleConfigurationCount");

    uint8 payload = ANC_MAX_TOGGLE_CONFIG;

    DEBUG_LOG("ancGaiaPlugin_GetToggleConfigurationCount, count = %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_toggle_configuration_count,
                                    ANC_GAIA_GET_TOGGLE_CONFIGURATION_COUNT_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_GetToggleConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_GetToggleConfiguration");

    uint8 anc_toggle_option_num;
    uint8 anc_toggle_option_val;
    uint8 response_payload_length;
    uint8* response_payload;

    if((payload_length == ANC_GAIA_GET_TOGGLE_CONFIGURATION_PAYLOAD_LENGTH)
            && (ancIsValidToggleWay(*payload)))
    {
        anc_toggle_option_num = *payload;
        anc_toggle_option_val = (uint8)AncStateManager_GetAncToggleConfiguration((anc_toggle_way_config_id_t)anc_toggle_option_num);

        response_payload_length = ANC_GAIA_GET_TOGGLE_CONFIGURATION_RESPONSE_PAYLOAD_LENGTH;
        response_payload = PanicUnlessMalloc(response_payload_length * sizeof(uint8));

        response_payload[ANC_GAIA_TOGGLE_OPTION_NUM_OFFSET] = anc_toggle_option_num;
        response_payload[ANC_GAIA_TOGGLE_OPTION_VAL_OFFSET] = anc_toggle_option_val;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_toggle_configuration, response_payload_length, response_payload);

        free(response_payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_toggle_configuration, invalid_parameter);
    }
}

static void ancGaiaPlugin_SetToggleConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetToggleConfiguration");
    anc_toggle_way_config_id_t anc_toggle_option_num;
    anc_toggle_config_t anc_toggle_option_val;

    if((payload_length == ANC_GAIA_SET_TOGGLE_CONFIGURATION_PAYLOAD_LENGTH)
            && (ancIsValidToggleWay(payload[ANC_GAIA_TOGGLE_OPTION_NUM_OFFSET]))
            && (ancIsValidConfig(payload[ANC_GAIA_TOGGLE_OPTION_VAL_OFFSET])))
    {
        anc_toggle_option_num = (anc_toggle_way_config_id_t)payload[ANC_GAIA_TOGGLE_OPTION_NUM_OFFSET];
        anc_toggle_option_val = (anc_toggle_config_t)payload[ANC_GAIA_TOGGLE_OPTION_VAL_OFFSET];

        if((anc_toggle_option_num == anc_toggle_way_config_id_1) && (anc_toggle_option_val == anc_toggle_config_not_configured))
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_toggle_configuration, invalid_parameter);
        }
        else
        {
            AncStateManager_SetAncToggleConfiguration(anc_toggle_option_num, anc_toggle_option_val);
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_toggle_configuration);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_toggle_configuration, invalid_parameter);
    }
}

static void ancGaiaPlugin_GetScenarioConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_GetScenarioConfiguration");
    uint8 anc_scenario;
    uint8 anc_scenario_behaviour;
    uint8 response_payload_length;
    uint8* response_payload;

    if ((payload_length == ANC_GAIA_GET_SCENARIO_CONFIGURATION_PAYLOAD_LENGTH)
        && (ancIsValidScenarioId(*payload)))
    {
        anc_scenario = *payload;
        anc_scenario_behaviour = (uint8)AncStateManager_GetAncScenarioConfiguration((anc_scenario_config_id_t)anc_scenario);

        response_payload_length = ANC_GAIA_GET_SCENARIO_CONFIGURATION_RESPONSE_PAYLOAD_LENGTH;
        response_payload = PanicUnlessMalloc(response_payload_length * sizeof(uint8));

        response_payload[ANC_GAIA_SCENARIO_OFFSET] = anc_scenario;
        response_payload[ANC_GAIA_SCENARIO_BEHAVIOUR_OFFSET] = anc_scenario_behaviour;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_scenario_configuration, response_payload_length, response_payload);

        free(response_payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_scenario_configuration, invalid_parameter);
    }
}

static void ancGaiaPlugin_SetScenarioConfiguration(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetScenarioConfiguration");
    anc_scenario_config_id_t anc_scenario;
    anc_toggle_config_t anc_scenario_behaviour;

    if ((payload_length == ANC_GAIA_SET_SCENARIO_CONFIGURATION_PAYLOAD_LENGTH)
        && (ancIsValidScenarioId(payload[ANC_GAIA_SCENARIO_OFFSET]))
        && (ancIsValidConfig(payload[ANC_GAIA_SCENARIO_BEHAVIOUR_OFFSET])))
    {
        anc_scenario = (anc_scenario_config_id_t)payload[ANC_GAIA_SCENARIO_OFFSET];
        anc_scenario_behaviour = (anc_toggle_config_t)payload[ANC_GAIA_SCENARIO_BEHAVIOUR_OFFSET];

        AncStateManager_SetAncScenarioConfiguration(anc_scenario, anc_scenario_behaviour);
        ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_scenario_configuration);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_scenario_configuration, invalid_parameter);
    }
}

static void ancGaiaPlugin_GetDemoSupport(GAIA_TRANSPORT *t)
{
    uint8 demo_support = AncStateManager_IsDemoSupported() ? ANC_GAIA_DEMO_SUPPORTED : ANC_GAIA_DEMO_NOT_SUPPORTED;

    DEBUG_LOG("ancGaiaPlugin_GetDemoSupport, Demo Support is %d", demo_support);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_demo_support,
                                    ANC_GAIA_GET_DEMO_SUPPORT_RESPONSE_PAYLOAD_LENGTH, &demo_support);
}

static void ancGaiaPlugin_GetDemoState(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetDemoState");

    uint8 payload = AncStateManager_IsDemoStateActive() ? ANC_GAIA_DEMO_STATE_ACTIVE: ANC_GAIA_DEMO_STATE_INACTIVE;

    DEBUG_LOG("ancGaiaPlugin_GetDemoState, Demo State is %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_demo_state,
                                    ANC_GAIA_GET_DEMO_STATE_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_SetDemoState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetDemoState");

    if (AncStateManager_IsDemoSupported())
    {
        if(payload_length == ANC_GAIA_SET_DEMO_STATE_PAYLOAD_LENGTH)
        {
            if(*payload == ANC_GAIA_DEMO_STATE_ACTIVE)
            {
                AncStateManager_SetDemoState(TRUE);
            }
            else if(*payload == ANC_GAIA_DEMO_STATE_INACTIVE)
            {
                AncStateManager_SetDemoState(FALSE);
            }
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_demo_state);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_demo_state, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_demo_state, incorrect_state);
    }
}

static void ancGaiaPlugin_GetAdaptationStaus(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetAdaptationStaus");

    uint8 payload;

    if(AncStateManager_IsDemoStateActive() && AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
    {
        payload = (AncStateManager_GetAdaptiveAncAdaptivity()) ? ANC_GAIA_AANC_ADAPTIVITY_RESUMED :
                                                                         ANC_GAIA_AANC_ADAPTIVITY_PAUSED;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_adaptation_control_status,
                                        ANC_GAIA_ADAPTATION_STATUS_RESPONSE_PAYLOAD_LENGTH, &payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_adaptation_control_status, incorrect_state);
    }
}

static void ancGaiaPlugin_SetAdaptationStaus(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetAdaptationStaus");

    uint8 adaptivity_control = *payload;
    bool adaptivity_status;

    if(payload_length == ANC_GAIA_SET_ADAPTATION_STATUS_PAYLOAD_LENGTH)
    {
        adaptivity_status = (adaptivity_control == ANC_GAIA_AANC_ADAPTIVITY_RESUME);

        if((adaptivity_status != AncStateManager_GetAdaptiveAncAdaptivity()) &&
                AncStateManager_IsDemoStateActive() &&
                AncConfig_IsAncModeAdaptive(AncStateManager_GetCurrentMode()))
        {
            ancGaiaPlugin_ToggleAncAdaptivity();
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_adaptation_control_status);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_adaptation_control_status, incorrect_state);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_adaptation_control_status, invalid_parameter);
    }
}

static void ancGaiaPlugin_GetLeakthroughdBSliderConfig(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetLeakthroughdBSliderConfig");

    uint8 payload_length;
    uint8* payload;
    uint8 cur_anc_mode;
    uint8 num_steps;
    uint8 step_size;
    uint8 min_gain_dB;
    uint8 cur_step;
    anc_sm_world_volume_gain_mode_config_t world_volume_config;

    cur_anc_mode = AncStateManager_GetCurrentMode();

    if(AncConfig_IsAncModeLeakThrough(cur_anc_mode))
    {
        AncStateManager_GetCurrentWorldVolumeConfig(&world_volume_config);
        ancGaiaPlugin_GetdBSliderConfig(&world_volume_config, &num_steps, &step_size, &min_gain_dB, &cur_step);

        payload_length = ANC_GAIA_GET_LEAKTHROUGH_dB_GAIN_SLIDER_RESPONSE_PAYLOAD_LENGTH;
        payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

        payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_anc_mode);
        payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_NUM_STEPS_OFFSET] = num_steps;
        payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_STEP_SIZE_OFFSET] = step_size;
        payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_MIN_GAIN_OFFSET] = min_gain_dB;
        payload[ANC_GAIA_LKT_dB_GAIN_SLIDER_CONFIG_CUR_STEP_OFFSET] = cur_step;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_leakthrough_dB_gain_slider_configuration,
                                   payload_length, payload);

        free(payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_leakthrough_dB_gain_slider_configuration, incorrect_state);
    }

}

static void ancGaiaPlugin_GetCurrentLeakthroughdBGainStep(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetCurrentLeakthroughdBGainStep");

    uint8 payload_length;
    uint8* payload;
    uint8 cur_anc_mode;
    uint8 cur_step;
    int8 cur_world_volume_gain_dB = 0;

    cur_anc_mode = AncStateManager_GetCurrentMode();

    if(AncConfig_IsAncModeLeakThrough(cur_anc_mode) &&
            AncStateManager_GetCurrentWorldVolumeGain(&cur_world_volume_gain_dB))
    {
        cur_step = ancGaiaPlugin_GetStepFromCurdBGain(cur_world_volume_gain_dB);

        payload_length = ANC_GAIA_GET_CURRENT_LEAKTHROUGH_dB_GAIN_STEP_RESPONSE_PAYLOAD_LENGTH;
        payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

        payload[ANC_GAIA_CURRENT_LKT_dB_GAIN_STEP_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_anc_mode);
        payload[ANC_GAIA_CURRENT_LKT_dB_GAIN_STEP_CURRENT_STEP_OFFSET] = cur_step;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_current_leakthrough_dB_gain_step,
                                   payload_length, payload);

        free(payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_current_leakthrough_dB_gain_step, incorrect_state);
    }

}

static void ancGaiaPlugin_SetCurrentLeakthroughdBGainStep(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetCurrentLeakthroughdBGainStep");

    if(AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
    {
        if(payload_length == ANC_GAIA_SET_LEAKTHROUGH_dB_GAIN_STEP_PAYLOAD_LENGTH &&
                ancGaiaPlugin_SetAncWorldVolumeGain(*payload))
        {
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_leakthrough_dB_gain_step);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_leakthrough_dB_gain_step, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_leakthrough_dB_gain_step, incorrect_state);
    }
}

static void ancGaiaPlugin_GetLeftRightBalance(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetLeftRightBalance");

    uint8 payload_length;
    uint8* payload;
    anc_sm_world_volume_gain_balance_info_t balance_info = {0, 0};

    if(AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()) &&
            AncStateManager_GetCurrentBalanceInfo(&balance_info))
    {

        payload_length = ANC_GAIA_GET_LEFT_RIGHT_BALANCE_RESPONSE_PAYLOAD_LENGTH;
        payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

        payload[ANC_GAIA_LEFT_RIGHT_BALANCE_DEVICE_SIDE_OFFSET] = balance_info.balance_device_side;
        payload[ANC_GAIA_LEFT_RIGHT_BALANCE_DEVICE_PERCENTAGE_OFFSET] = balance_info.balance_percentage;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_left_right_balance, payload_length, payload);

        free(payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_left_right_balance, incorrect_state);
    }

}

static void ancGaiaPlugin_SetLeftRightBalance(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetLeftRightBalance");

    if(AncConfig_IsAncModeLeakThrough(AncStateManager_GetCurrentMode()))
    {
        if(payload_length == ANC_GAIA_SET_LEFT_RIGHT_BALANCE_PAYLOAD_LENGTH &&
                payload[ANC_GAIA_LEFT_RIGHT_BALANCE_DEVICE_PERCENTAGE_OFFSET] <= ANC_GAIA_MAX_LEFT_RIGHT_BALANCE_PERCENTAGE)
        {
            ancGaiaPlugin_SetWorldVolumeBalance(payload[ANC_GAIA_LEFT_RIGHT_BALANCE_DEVICE_SIDE_OFFSET],
                                                payload[ANC_GAIA_LEFT_RIGHT_BALANCE_DEVICE_PERCENTAGE_OFFSET]);
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_left_right_balance);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_left_right_balance, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_left_right_balance, incorrect_state);
    }
}


static void ancGaiaPlugin_GetWindNoiseReductionSupport(GAIA_TRANSPORT *t)
{
    uint8 payload = WindDetect_IsSupported() ? ANC_GAIA_WIND_NOISE_REDUCTION_SUPPORTED: ANC_GAIA_WIND_NOISE_REDUCTION_NOT_SUPPORTED;

    DEBUG_LOG("ancGaiaPlugin_GetWindNoiseReductionSupport, WNR Support is %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_wind_noise_reduction_support,
                                    ANC_GAIA_GET_WIND_NOISE_REDUCTION_SUPPORT_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_GetWindNoiseDetectionState(GAIA_TRANSPORT *t)
{
    uint8 payload = ANC_GAIA_WIND_NOISE_REDUCTION_STATE_DISABLE;

    if (WindDetect_IsSupported() && WindDetect_IsWindNoiseReductionEnabled())
    {
        payload = ANC_GAIA_WIND_NOISE_REDUCTION_STATE_ENABLE;
    }
        
    DEBUG_LOG("ancGaiaPlugin_GetWindNoiseDetectionState, State is %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_wind_noise_detection_state,
                                    ANC_GAIA_GET_WIND_NOISE_DETECTION_STATE_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_SetWindNoiseDetectionState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetWindNoiseDetectionState");

    if (WindDetect_IsSupported())
    {
        if(payload_length == ANC_GAIA_SET_WIND_NOISE_DETECTION_STATE_PAYLOAD_LENGTH)
        {
            if(*payload == ANC_GAIA_WIND_NOISE_REDUCTION_STATE_ENABLE)
            {
                Ui_InjectRedirectableUiInput(ui_input_anc_wind_enable,FALSE);
            }
            else if(*payload == ANC_GAIA_WIND_NOISE_REDUCTION_STATE_DISABLE)
            {
                Ui_InjectRedirectableUiInput(ui_input_anc_wind_disable,FALSE);
            }
            
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_wind_noise_detection_state);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_wind_noise_detection_state, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_wind_noise_detection_state, incorrect_state);
    }
}

static void ancGaiaPlugin_GetHowlingDetectionSupport(GAIA_TRANSPORT *t)
{
    uint8 payload = KymeraAncCommon_IsHowlingDetectionSupported() ? ANC_GAIA_HOWLING_DETECTION_SUPPORTED: ANC_GAIA_HOWLING_DETECTION_NOT_SUPPORTED;

    DEBUG_LOG("ancGaiaPlugin_GetHowlingDetectionSupport: %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_howling_detection_support,
                                    ANC_GAIA_GET_HOWLING_DETECTION_SUPPORT_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_GetHowlingDetectionState(GAIA_TRANSPORT *t)
{
    uint8 payload = ANC_GAIA_HOWLING_DETECTION_STATE_DISABLE;

    if (KymeraAncCommon_IsHowlingDetectionSupported() && KymeraAncCommon_IsHowlingDetectionEnabled())
    {
        payload = ANC_GAIA_HOWLING_DETECTION_STATE_ENABLE;
    }

    DEBUG_LOG("ancGaiaPlugin_GetHowlingDetectionState, State is %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_wind_noise_detection_state,
                                    ANC_GAIA_GET_WIND_NOISE_DETECTION_STATE_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_SetHowlingDetectionState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetHowlingeDetectionState");

    if (KymeraAncCommon_IsHowlingDetectionSupported())
    {
        if(payload_length == ANC_GAIA_SET_HOWLING_DETECTION_STATE_PAYLOAD_LENGTH)
        {
            if(*payload == ANC_GAIA_HOWLING_DETECTION_STATE_ENABLE)
            {
                Ui_InjectRedirectableUiInput(ui_input_anc_anti_howling_enable,FALSE);
            }
            else if(*payload == ANC_GAIA_HOWLING_DETECTION_STATE_DISABLE)
            {
                Ui_InjectRedirectableUiInput(ui_input_anc_anti_howling_disable,FALSE);
            }

            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_howling_detection_state);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_howling_detection_state, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_howling_detection_state, incorrect_state);
    }
}

static void ancGaiaPlugin_GetAutoTransparencySupport(GAIA_TRANSPORT *t)
{
    uint8 payload = ANC_GAIA_AUTO_TRANSPARENCY_NOT_SUPPORTED;

    if (AncAutoAmbient_IsSupported() && AncAutoAmbient_IsAmbientModeConfigured())
    {
        payload = ANC_GAIA_AUTO_TRANSPARENCY_SUPPORTED;
    }

    DEBUG_LOG("ancGaiaPlugin_GetAutoTransparencySupport is %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_auto_transparency_support,
                                    ANC_GAIA_GET_AUTO_TRANSPARENCY_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_GetAutoTransparencyState(GAIA_TRANSPORT *t)
{
    uint8 payload = ANC_GAIA_AUTO_TRANSPARENCY_STATE_DISABLE;

    if (AncAutoAmbient_IsEnabled())
    {
        payload = ANC_GAIA_AUTO_TRANSPARENCY_STATE_ENABLE;
    }
        
    DEBUG_LOG("ancGaiaPlugin_GetAutoTransparencyState, State is %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_auto_transparency_state,
                                    ANC_GAIA_GET_AUTO_TRANSPARENCY_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_SetAutoTransparencyState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetAutoTransparencyState");

    if (AncAutoAmbient_IsSupported())
    {
        if(payload_length == ANC_GAIA_AUTO_TRANSPARENCY_COMMAND_PAYLOAD_LENGTH)
        {
            if(*payload == ANC_GAIA_AUTO_TRANSPARENCY_STATE_ENABLE)
            {
                Ui_InjectRedirectableUiInput(ui_input_anc_auto_transparency_enable, FALSE);
            }
            else if(*payload == ANC_GAIA_AUTO_TRANSPARENCY_STATE_DISABLE)
            {
                Ui_InjectRedirectableUiInput(ui_input_anc_auto_transparency_disable, FALSE);
            }
            
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_auto_transparency_state);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_auto_transparency_state, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_auto_transparency_state, incorrect_state);
    }
}

static void ancGaiaPlugin_GetAutoTransparencyReleaseTimeConfig(GAIA_TRANSPORT *t)
{
    uint8 payload = AncAutoAmbient_GetReleaseTimeConfig();
        
    DEBUG_LOG("ancGaiaPlugin_GetAutoTransparencyReleaseTimeConfig %d", payload);
    ancGaiaPlugin_SendResponse(t, anc_gaia_get_auto_transparency_release_time,
                                    ANC_GAIA_GET_AUTO_TRANSPARENCY_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_SetAutoTransparencyReleaseTimeConfig(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_SetAutoTransparencyReleaseTimeConfig %d ", *payload);

    if (AncAutoAmbient_IsSupported())
    {
        if(payload_length == ANC_GAIA_AUTO_TRANSPARENCY_COMMAND_PAYLOAD_LENGTH)
        {
            AncAutoAmbient_StoreReleaseConfig(*payload);
            Ui_InjectRedirectableUiInput(ui_input_anc_auto_transparency_release_time, FALSE);
            ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_auto_transparency_release_time);
        }
        else
        {
            ancGaiaPlugin_SendError(t, anc_gaia_set_auto_transparency_release_time, invalid_parameter);
        }
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_set_auto_transparency_release_time, incorrect_state);
    }
}

static void ancGaiaPlugin_GetFBGain(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetFBGain");

    uint8 payload_length;
    uint8* payload;
    uint8 anc_fb_gain;
    uint8 cur_anc_mode;

    cur_anc_mode = AncStateManager_GetCurrentMode();

    if(AncConfig_IsAdvancedAnc())
    {
        anc_fb_gain = AncStateManager_GetAncGain();

        payload_length = ANC_GAIA_GET_FB_GAIN_RESPONSE_PAYLOAD_LENGTH;
        payload = PanicUnlessMalloc(payload_length * sizeof(uint8));

        payload[ANC_GAIA_CURRENT_MODE_OFFSET] = ancGaiaPlugin_ConvertAncModeToGaiaPayloadFormat(cur_anc_mode);
        payload[ANC_GAIA_CURRENT_MODE_TYPE_OFFSET] = ancGaiaPlugin_getModeTypeFromAncMode(cur_anc_mode);
        payload[ANC_GAIA_LEFT_GAIN_OFFSET] = anc_fb_gain;
        payload[ANC_GAIA_RIGHT_GAIN_OFFSET] = anc_fb_gain;

        ancGaiaPlugin_SendResponse(t, anc_gaia_get_fb_gain, ANC_GAIA_GET_FB_GAIN_RESPONSE_PAYLOAD_LENGTH, payload);

        free(payload);
    }
    else
    {
        ancGaiaPlugin_SendError(t, anc_gaia_get_fb_gain, incorrect_state);
    }

}

static gaia_framework_command_status_t ancGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_MainHandler, called for %d", pdu_id);

    switch (pdu_id)
    {
        case anc_gaia_get_ac_state:
            ancGaiaPlugin_GetAcState(t, payload_length, payload);
            break;

        case anc_gaia_set_ac_state:
            ancGaiaPlugin_SetAcState(t, payload_length, payload);
            break;

        case anc_gaia_get_num_modes:
            ancGaiaPlugin_GetNumOfModes(t);
            break;

        case anc_gaia_get_current_mode:
            ancGaiaPlugin_GetCurrentMode(t);
            break;

        case anc_gaia_set_mode:
            ancGaiaPlugin_SetMode(t, payload_length, payload);
            break;

        case anc_gaia_get_gain:
            ancGaiaPlugin_GetGain(t);
            break;

        case anc_gaia_set_gain:
            ancGaiaPlugin_SetGain(t, payload_length, payload);
            break;

        case anc_gaia_get_toggle_configuration_count:
            ancGaiaPlugin_GetToggleConfigurationCount(t);
            break;

        case anc_gaia_get_toggle_configuration:
            ancGaiaPlugin_GetToggleConfiguration(t, payload_length, payload);
            break;

        case anc_gaia_set_toggle_configuration:
            ancGaiaPlugin_SetToggleConfiguration(t, payload_length, payload);
            break;

        case anc_gaia_get_scenario_configuration:
            ancGaiaPlugin_GetScenarioConfiguration(t, payload_length, payload);
            break;

        case anc_gaia_set_scenario_configuration:
            ancGaiaPlugin_SetScenarioConfiguration(t, payload_length, payload);
            break;

        case anc_gaia_get_demo_support:
            ancGaiaPlugin_GetDemoSupport(t);
            break;

        case anc_gaia_get_demo_state:
            ancGaiaPlugin_GetDemoState(t);
            break;

        case anc_gaia_set_demo_state:
            ancGaiaPlugin_SetDemoState(t, payload_length, payload);
            break;

        case anc_gaia_get_adaptation_control_status:
            ancGaiaPlugin_GetAdaptationStaus(t);
            break;

        case anc_gaia_set_adaptation_control_status:
            ancGaiaPlugin_SetAdaptationStaus(t, payload_length, payload);
            break;

        case anc_gaia_get_leakthrough_dB_gain_slider_configuration:
            ancGaiaPlugin_GetLeakthroughdBSliderConfig(t);
            break;

        case anc_gaia_get_current_leakthrough_dB_gain_step:
            ancGaiaPlugin_GetCurrentLeakthroughdBGainStep(t);
            break;

        case anc_gaia_set_leakthrough_dB_gain_step:
            ancGaiaPlugin_SetCurrentLeakthroughdBGainStep(t, payload_length, payload);
            break;

        case anc_gaia_get_left_right_balance:
            ancGaiaPlugin_GetLeftRightBalance(t);
            break;

        case anc_gaia_set_left_right_balance:
            ancGaiaPlugin_SetLeftRightBalance(t, payload_length, payload);
            break;

        case anc_gaia_get_wind_noise_reduction_support:
            ancGaiaPlugin_GetWindNoiseReductionSupport(t);
            break;

        case anc_gaia_get_wind_noise_detection_state:
            ancGaiaPlugin_GetWindNoiseDetectionState(t);
            break;

        case anc_gaia_set_wind_noise_detection_state:
            ancGaiaPlugin_SetWindNoiseDetectionState(t, payload_length, payload);
            break;

        case anc_gaia_get_auto_transparency_support:
            ancGaiaPlugin_GetAutoTransparencySupport(t);
            break;
        
        case anc_gaia_get_auto_transparency_state:
            ancGaiaPlugin_GetAutoTransparencyState(t);
            break;
        
        case anc_gaia_set_auto_transparency_state:
            ancGaiaPlugin_SetAutoTransparencyState(t, payload_length, payload);
            break;
            
        case anc_gaia_get_auto_transparency_release_time:
            ancGaiaPlugin_GetAutoTransparencyReleaseTimeConfig(t);
            break;
        
        case anc_gaia_set_auto_transparency_release_time:
            ancGaiaPlugin_SetAutoTransparencyReleaseTimeConfig(t, payload_length, payload);
            break;

        case anc_gaia_get_howling_detection_support:
            ancGaiaPlugin_GetHowlingDetectionSupport(t);
        break;             

        case anc_gaia_get_howling_detection_state:
           ancGaiaPlugin_GetHowlingDetectionState(t);
           break;

        case anc_gaia_set_howling_detection_state:
            ancGaiaPlugin_SetHowlingDetectionState(t,payload_length, payload);
        break;

        case anc_gaia_get_fb_gain:
            ancGaiaPlugin_GetFBGain(t);
        break;
            
        default:
            DEBUG_LOG_ERROR("ancGaiaPlugin_MainHandler, unhandled call for %u", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}

static void ancGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    DEBUG_LOG("ancGaiaPlugin_SendAllNotifications");

    ancGaiaPlugin_SendDemoStateNotification(AncStateManager_IsDemoStateActive());
    ancGaiaPlugin_SendAllNotificationsInDemoMode();
    ancGaiaPlugin_SendAllNotificationsInConfigMode();
}

static void ancGaiaPlugin_TransportConnect(GAIA_TRANSPORT *t)
{
    UNUSED(t);

    AncStateManager_ClientRegister(ancGaiaPlugin_GetTask());
#ifndef INCLUDE_STEREO
    StateProxy_EventRegisterClient(ancGaiaPlugin_GetTask(), state_proxy_event_type_phystate);
    appPhyStateRegisterClient(ancGaiaPlugin_GetTask());
#endif
}

static void ancGaiaPlugin_TransportDisconnect(GAIA_TRANSPORT *t)
{
    UNUSED(t);

    AncStateManager_SetDemoState(FALSE);
    AncStateManager_ClientUnregister(ancGaiaPlugin_GetTask());
#ifndef INCLUDE_STEREO
    StateProxy_EventUnregisterClient(ancGaiaPlugin_GetTask(), state_proxy_event_type_phystate);
    appPhyStateUnregisterClient(ancGaiaPlugin_GetTask());
#endif
}

static void ancGaiaPlugin_RoleChangeCompleted(GAIA_TRANSPORT *t, bool is_primary)
{
    UNUSED(t);
    UNUSED(is_primary);
}

void AncGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = ancGaiaPlugin_MainHandler,
        .send_all_notifications = ancGaiaPlugin_SendAllNotifications,
        .transport_connect = ancGaiaPlugin_TransportConnect,
        .transport_disconnect = ancGaiaPlugin_TransportDisconnect,
        .role_change_completed = ancGaiaPlugin_RoleChangeCompleted,
    };

    DEBUG_LOG("AncGaiaPlugin_Init");

    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    /* Initialise plugin framework task data */
    memset(anc_gaia_data, 0, sizeof(*anc_gaia_data));
    anc_gaia_data->task.handler = ancGaiaPlugin_HandleMessage;

    GaiaFramework_RegisterFeature(GAIA_AUDIO_CURATION_FEATURE_ID, ANC_GAIA_PLUGIN_VERSION, &functions);
}

