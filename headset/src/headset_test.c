/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_test.c
\brief      Implementation of specifc application testing functions
*/

#include "headset_test.h"
#include "bt_device.h"
#include "logging.h"
#include "volume_messages.h"
#include "ui.h"
#include <hfp_profile.h>
#include <hfp_profile_instance.h>
#include <av.h>
#include <av_config.h>
#include <pairing.h>
#include <power_manager.h>
#include <battery_monitor.h>
#include <battery_region.h>
#include "charger_monitor_config.h"
#include "battery_monitor_config.h"
#include "peripherals/thermistor.h"
#include "temperature_config.h"
#include "charger_monitor.h"
#include <system_state.h>
#include <device_properties.h>
#include <device_list.h>
#include <connection_manager.h>
#include <focus_voice_source.h>
#include <focus_audio_source.h>
#include <stereo_topology_private.h>
#include <stereo_topology.h>
#include <bredr_scan_manager.h>
#include <profile_manager.h>
#include <feature.h>
#include <volume_service.h>
#include <ps.h>
#include "audio_sources.h"
#include "voice_sources.h"
#include "headset_tones.h"
/* TODO remove when power on UI is supported */
#include "headset_sm_private.h"
#include "anc_state_manager.h"
#include "headset_usb.h"
#include "usb_application.h"
#include "usb_device.h"
#include "usb_source.h"
#include "usb_source_hid.h"
#include "headset_phy_state.h"
#include "audio_plugin_if.h"
#include "audio_clock.h"
#include "audio_power.h"
#include "gaming_mode.h"
#include "kymera_latency_manager.h"
#include "kymera_dynamic_latency.h"
#include "kymera_data.h"
#include "volume_utils.h"
#include "handset_service.h"
#include "upgrade.h"

#include <usb_application.h>
#include <usb_hid_datalink.h>
#include <usb_app_cdc.h>
#include <usb_cdc.h>

#include <boot.h>
#include "state_of_charge.h"
#include "a2dp.h"

#ifdef INCLUDE_FAST_PAIR
#include "fast_pair_config.h"
#include <ps_key_map.h>
#endif
#include "temperature.h"

#ifdef INCLUDE_ACCESSORY
#include "accessory.h"
#include "request_app_launch.h"
#endif

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
#include "wired_audio_private.h"
#endif



/* #define _TODO_ */

static void testTaskHandler(Task task, MessageId id, Message message);
TaskData testTask = {testTaskHandler};

bool power_off_tone_have_been_played = FALSE;
bool power_off_prompt_have_been_played = FALSE;

static void testTaskHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch (id)
    {
        case KYMERA_NOTIFICATION_TONE_STARTED:
            {
                KYMERA_NOTIFICATION_TONE_STARTED_T *msg = (KYMERA_NOTIFICATION_TONE_STARTED_T *)message;
                DEBUG_LOG_VERBOSE("testTaskHandler KYMERA_NOTIFICATION_TONE_STARTED received tone %p", msg->tone);
                if(msg->tone == app_tone_battery_empty)
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received tone is app_tone_battery_empty");
                    power_off_tone_have_been_played = TRUE;
                }
                else if(msg->tone == app_tone_av_connected)
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received tone is app_tone_av_connected");
                }
                else
                {
                    DEBUG_LOG_VERBOSE("testTaskHandler Received tone is not yet recognised");
                }
            }
            break;
    }
}

static device_t appTest_GetHandset(void)
{
    bool is_mru_handset = TRUE;
    device_t handset_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    if (!handset_device)
    {
        bdaddr handset_address = {0,0,0};
        if (appDeviceGetHandsetBdAddr(&handset_address))
        {
            handset_device = BtDevice_GetDeviceForBdAddr(&handset_address);
        }
    }
    return handset_device;
}

static void appTest_ReportProperty(device_t device, device_property_t property)
{
    if (Device_IsPropertySet(device,property))
    {
        uint8 value = 0;
        uint16 u16_value = 0;
        switch (property)
        {
        case device_property_bdaddr:
            {
                bdaddr bd_addr = DeviceProperties_GetBdAddr(device);
                DEBUG_LOG_VERBOSE("\tbdaddr nap %x uap %x lap %x", bd_addr.nap, bd_addr.uap, bd_addr.lap);
            }
            break;
        case device_property_audio_volume:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\ta2dp_volume %02x", value);
            break;
        case device_property_hfp_profile:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\thfp_profile %02x", value);
            break;
        case device_property_type:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\ttype %02x", value);
            break;
        case device_property_link_mode:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tlink_mode %02x", value);
            break;
        case device_property_supported_profiles:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tsupported_profiles %02x", value);
            break;
        case device_property_connected_profiles:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tcurrently connected_profiles %02x", value);
            break;
        case device_property_flags:
            Device_GetPropertyU16(device, property, &u16_value);
            DEBUG_LOG_VERBOSE("\tflags %04x", u16_value);
            break;
        case device_property_mru:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tmru %1x", value);
            break;
        case device_property_profile_request_index:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\tprofile_request_index %1x", value);
            break;
        case device_property_profiles_connect_order:
            {
                profile_t *ptr_profile_order = NULL;
                size_t size;
                PanicFalse(Device_GetProperty(device, property, (void *)&ptr_profile_order, &size));
                profile_t array[5] = {profile_manager_bad_profile};
                memcpy(array, ptr_profile_order, MIN(size,5));
                DEBUG_LOG_VERBOSE("\tprofiles_connect_order %d,%d,%d,%d,%d", array[0], array[1], array[2], array[3], array[4]);
            }
            break;
        case device_property_voice_volume:
            Device_GetPropertyU8(device, property, &value);
            DEBUG_LOG_VERBOSE("\thfp_volume %02x", value);
            break;
        default:
            break;
        }
    }
}
static void appTest_ReportDeviceData(device_t device, void * data)
{
    UNUSED(data);

    DEBUG_LOG_ALWAYS("Device %x", device);
    for (int property=0; property < device_property_max_num; property++)
    {
        if (Device_IsPropertySet(device, property))
        {
            appTest_ReportProperty(device, property);
        }
    }
}

bool static headsetState_IsValidToInjectAncAndLeakthroughEvents(void)
{
    headsetState headset_state = SmGetTaskData()->state;

    return ((headset_state > HEADSET_STATE_POWERING_ON) && headset_state < HEADSET_STATE_TERMINATING);
}

/*! \brief Return if headset has an Handset HFP connection
*/
bool appTestIsHandsetHfpConnected(void)
{
    bool connected = appHfpIsConnected();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpConnected= %d", connected);

    return connected;
}

/*! \brief Return if Headset has an Handset HFP SCO connection
*/
bool appTestIsHandsetHfpScoActive(void)
{
    bool active = HfpProfile_IsScoActive();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpScoActive:%d", active);

    return active;
}

void appTestHandsetHfpCallAccept(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallAccept");
    Ui_InjectUiInput(ui_input_voice_call_accept);
}

void appTestHandsetHfpCallReject(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallReject");
    Ui_InjectUiInput(ui_input_voice_call_reject);
}

void appTestHandsetHfpCallHangup(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallHangup");
    Ui_InjectUiInput(ui_input_voice_call_hang_up);
}

void appTestHandsetVoiceCallCycle(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallCycle");
    Ui_InjectUiInput(ui_input_voice_call_cycle);
}

void appTestHandsetVoiceCallJoinCalls(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallJoinCalls");
    Ui_InjectUiInput(ui_input_voice_call_join_calls);
}

void appTestHandsetVoiceCallJoinCallsAndHangUp(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallJoinCallsAndHangUp");
    Ui_InjectUiInput(ui_input_voice_call_join_calls_and_hang_up);
}

bool appTestIsHandsetHfpMuted(void)
{
    bool muted = FALSE;
    voice_source_t source = voice_source_none;

    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &source))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

        if (instance != NULL)
        {
             muted = HfpProfile_IsMicrophoneMuted(instance);

            DEBUG_LOG_ALWAYS("appTestIsHandsetHfpMuted(%p):%d", instance, muted);
        }
    }

    return muted;
}

void appTestHandsetHfpMuteToggle(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpMuteToggle");
    Ui_InjectUiInput(ui_input_mic_mute_toggle);
}

void appTestHandsetHfpVoiceTransferToAg(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVoiceTransferToAg");
    Ui_InjectUiInput(ui_input_voice_transfer_to_ag);
}

void appTestHandsetHfpVoiceTransferToHeadset(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVoiceTransferToHeadset");
    Ui_InjectUiInput(ui_input_voice_transfer_to_headset);
}
bool appTestHandsetHfpCallLastDialed(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpCallLastDialed");
    voice_source_t source = voice_source_none;

    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &source))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

        if (instance != NULL)
        {
            if (appHfpIsConnectedForInstance(instance))
            {
                Ui_InjectUiInput(ui_input_voice_call_last_dialed);
                return TRUE;
            }
        }
    }
    return FALSE;
}

void appTestHandsetHfpVolumeDownStart(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVolumeDownStart");
    Ui_InjectUiInput(ui_input_volume_down_start);
}

void appTestHandsetHfpVolumeUpStart(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVolumeUpStart");
    Ui_InjectUiInput(ui_input_volume_up_start);
}

void appTestHandsetHfpVolumeStop(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpVolumeStop");
    Ui_InjectUiInput(ui_input_volume_stop);
}

bool appTestHandsetHfpSetScoVolume(uint8 volume)
{
    voice_source_t focused_source = voice_source_none;
    bool volume_set = FALSE;

    hfpInstanceTaskData * instance = HfpProfile_GetInstanceForVoiceSourceWithUiFocus();
    if (instance != NULL && appHfpIsCallForInstance(instance))
    {
        focused_source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        if (VoiceSource_IsHfp(focused_source))
        {
            Volume_SendVoiceSourceVolumeUpdateRequest(focused_source, event_origin_local, volume);
            volume_set = TRUE;
        }
    }
    DEBUG_LOG_ALWAYS("appTestHandsetHfpScoVolume enum:voice_source_t:%d", focused_source);
    return volume_set;
}

bool appTestIsHandsetHfpCall(void)
{
    bool is_call = appHfpIsCall();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpCall:%d", is_call);

    return is_call;
}

bool appTestIsHandsetHfpCallIncoming(void)
{
    bool incoming = appHfpIsCallIncoming();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpCallIncoming:%d", incoming);

    return incoming;
}

bool appTestIsHandsetHfpCallOutgoing(void)
{
    bool outgoing = appHfpIsCallOutgoing();

    DEBUG_LOG_ALWAYS("appTestIsHandsetHfpCallOutgoing:%d", outgoing);

    return outgoing;
}

void appTestHfpVoiceDial(void)
{
    DEBUG_LOG_ALWAYS("appTestHfpVoiceDial");
    Ui_InjectUiInput(ui_input_voice_dial);
}

bool appTestHandsetHfpActivateVoiceRecognition(bool activate)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpActivateVoiceRecognition: activate(%d)", activate);

    hfpInstanceTaskData * instance = HfpProfile_GetInstanceForVoiceSourceWithUiFocus();

    if (instance != NULL && appHfpIsConnectedForInstance(instance))
    {
        hfp_link_priority link_priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);
        HfpVoiceRecognitionEnableRequestEx(link_priority, (activate == TRUE) ? hfp_evra_action_enable : hfp_evra_action_disable);
        return TRUE;
    }
    return FALSE;
}

bool appTestHandsetHfpSetIndicatorValue(uint8 idx, uint8 value)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpSetIndicatorValue: idx(%d) value(%d)", idx, value);

    hfpInstanceTaskData * instance = HfpProfile_GetInstanceForVoiceSourceWithUiFocus();

    if (instance != NULL && appHfpIsConnectedForInstance(instance))
    {
        hfp_link_priority link_priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);
        HfpBievIndStatusRequest(link_priority, idx, value);
        return TRUE;
    }
    return FALSE;
}

bool appTestHandsetHfpGetCurrentCallList(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetHfpGetCurrentCallList");

    hfpInstanceTaskData * instance = HfpProfile_GetInstanceForVoiceSourceWithUiFocus();

    if (instance != NULL && appHfpIsConnectedForInstance(instance))
    {
        hfp_link_priority link_priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);
        HfpCurrentCallsRequest(link_priority);
        return TRUE;
    }

    return FALSE;
}

/*! \brief Initiate Headset AVRCP connection to the Handset
*/
bool appTestHandsetAvrcpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetAvrcpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
        return  appAvAvrcpConnectRequest(NULL, &bd_addr);
    return FALSE;
}

bool appTestIsHandsetAvrcpConnected(void)
{
    bool connected = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        connected = theInst && appAvrcpIsConnected(theInst);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetAvrcpConnected = %d", connected);

    /* If we get here then there's no AVRCP connected for handset */
    return connected;
}

bool appTestIsA2dpPlaying(void)
{
    bool playing = Av_IsPlaying();

    DEBUG_LOG_ALWAYS("appTestIsA2dpPlaying=%d", playing);

    return playing;
}

bool appTestIsHandsetA2dpMediaConnected(void)
{
    bool connected = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        connected = theInst && appA2dpIsConnectedMedia(theInst);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetA2dpMediaConnected:%d", connected);
    return connected;
}

bool appTestHandsetA2dpConnect(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpConnect");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        return appAvA2dpConnectRequest(&bd_addr, A2DP_CONNECT_NOFLAGS);
    }
    else
    {
        return FALSE;
    }
}

bool appTestIsHandsetA2dpConnected(void)
{
    bool connected = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        connected = theInst && appA2dpIsConnected(theInst);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetA2dpConnected:%d", connected);

    /* If we get here then there's no A2DP connected for handset */
    return connected;
}

bool appTestIsHandsetAclConnected(void)
{
    bool connected = FALSE;
    device_t handset_device = appTest_GetHandset();

    if (handset_device)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(handset_device);
        connected = ConManagerIsConnected(&addr);
        DEBUG_LOG_ALWAYS("appTestIsHandsetAclConnected:%d 0x%06x", connected, addr.lap);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestIsHandsetAclConnected:0 No handset device/addr");
    }
    return connected;
}

bool appTestIsHandsetConnected(void)
{
    bool connected = appTestIsHandsetA2dpConnected() ||
                     appTestIsHandsetAvrcpConnected() ||
                     appTestIsHandsetHfpConnected();

    DEBUG_LOG_ALWAYS("appTestIsHandsetConnected = %d", connected);

    return connected;
}

void appTestConnectHandset(void)
{
    DEBUG_LOG_ALWAYS("appTestLogicalInputHeadset");
    Ui_InjectUiInput(ui_input_connect_handset);
}

bool appTestIsInitialisationCompleted(void)
{
    bool completed = SystemState_GetState() > system_state_initialised;

    DEBUG_LOG_ALWAYS("appTestIsInitialisationCompleted:%d", completed);
    return completed;
}

bool appTestHeadsetPhyStateIsOnHead(void)
{
    bool on_head = (appHeadsetPhyStateGetState() == HEADSET_PHY_STATE_ON_HEAD);
    DEBUG_LOG_ALWAYS("appTestHeadsetPhyStateIsOnHead:%d", on_head);
    return on_head;
}

void appTestHeadsetPhyStateOnHeadEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestHeadsetPhyStateOnHeadEvent");
    appHeadsetPhyStateOnHeadEvent();
}

void appTestHeadsetPhyStateOffHeadEvent(void)
{
    DEBUG_LOG_ALWAYS("appTestHeadsetPhyStateOffHeadEvent");
    appHeadsetPhyStateOffHeadEvent();
}

/*! \brief Send the AV play/pause toggle command
*/
void appTestAvTogglePlayPause(void)
{
    DEBUG_LOG_ALWAYS("appTestAvTogglePlayPause");
    Ui_InjectUiInput(ui_input_toggle_play_pause);
}

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvPause(void)
{
    DEBUG_LOG_ALWAYS("appTestAvPause");
    Ui_InjectUiInput(ui_input_pause_all);
}

/*! \brief Send the Avrcp play command to the Handset
*/
void appTestAvPlay(void)
{
    DEBUG_LOG_ALWAYS("appTestAvPlay");
    Ui_InjectUiInput(ui_input_play);
}

/*! \brief Send the Avrcp stop command to the Handset
*/
void appTestAvStop(void)
{
    DEBUG_LOG_ALWAYS("appTestAvStop");
    Ui_InjectUiInput(ui_input_stop_av_connection);
}

/*! \brief Send the Avrcp forward command to the Handset
*/
void appTestAvForward(void)
{
    DEBUG_LOG_ALWAYS("appTestAvForward");
    Ui_InjectUiInput(ui_input_av_forward);
}

/*! \brief Send the Avrcp backward command to the Handset
*/
void appTestAvBackward(void)
{
    DEBUG_LOG_ALWAYS("appTestAvBackward");
    Ui_InjectUiInput(ui_input_av_backward);
}

/*! \brief Send the Avrcp fast forward state command to the Handset
*/
void appTestAvFastForwardStart(void)
{
    DEBUG_LOG_ALWAYS("appTestAvFastForwardStart");
    Ui_InjectUiInput(ui_input_av_fast_forward_start);
}

/*! \brief Send the Avrcp fast forward stop command to the Handset
*/
void appTestAvFastForwardStop(void)
{
    DEBUG_LOG_ALWAYS("appTestAvFastForwardStop");
    Ui_InjectUiInput(ui_input_fast_forward_stop);
}

/*! \brief Send the Avrcp rewind start command to the Handset
*/
void appTestAvRewindStart(void)
{
    DEBUG_LOG_ALWAYS("appTestAvRewindStart");
    Ui_InjectUiInput(ui_input_av_rewind_start);
}

/*! \brief Send the Avrcp rewind stop command to the Handset
*/
void appTestAvRewindStop(void)
{
    DEBUG_LOG_ALWAYS("appTestAvRewindStop");
    Ui_InjectUiInput(ui_input_rewind_stop);
}

/*! \brief Send the Avrcp volume change command to the Handset
*/
bool appTestAvVolumeChange(int8 step)
{
    DEBUG_LOG_ALWAYS("appTestAvVolumeChange %d", step);

    if (step == 0)
        return FALSE;
    audio_source_t focused_source = audio_source_none;
    if (!Focus_GetAudioSourceForContext(&focused_source))
    {
        DEBUG_LOG_ALWAYS("no focused audio source");
        return FALSE;
    }

    if (focused_source != audio_source_a2dp_1 && focused_source != audio_source_a2dp_2)
    {
        DEBUG_LOG_ALWAYS("focused audio source is not A2dp");
        return FALSE;
    }
    int step_size = VolumeUtils_GetStepSize(AudioSources_GetVolume(focused_source).config);
    if ((step % step_size) != 0)
    {
       DEBUG_LOG_ALWAYS("step should be in multiple of step size  %d", step_size);
       return FALSE;
    }

    int ui_inputs_to_raise = ABS(step) / step_size;
    ui_input_t ui_input = (step > 0) ? ui_input_volume_up : ui_input_volume_down;
    for (int ui_input_counter = 0; ui_input_counter < ui_inputs_to_raise; ui_input_counter++)
    {
        Ui_InjectUiInput(ui_input);
    }
    DEBUG_LOG_VERBOSE("appTestAvVolumeChange applied %d volume steps", ui_inputs_to_raise);
    return TRUE;
}

/*! \brief Send the Avrcp volume set command to the Handset
*/
void appTestAvVolumeSet(uint8 volume)
{
    DEBUG_LOG_ALWAYS("appTestAvVolumeSet %d", volume);
    audio_source_t focused_source = audio_source_none;
    if (!Focus_GetAudioSourceForContext(&focused_source))
    {
        DEBUG_LOG_ALWAYS("no focused audio source");
        return;
    }
    if (focused_source != audio_source_a2dp_1 && focused_source != audio_source_a2dp_2)
    {
        DEBUG_LOG_ALWAYS("focused audio source is not A2dp");
        return;
    }
    Volume_SendAudioSourceVolumeUpdateRequest(focused_source, event_origin_local, volume);
}

/*! \brief report the saved handset information */
extern void appTestHandsetInfo(void)
{
    bdaddr bd_addr;
    DEBUG_LOG_ALWAYS("appTestHandsetInfo");
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        DEBUG_LOG_VERBOSE("appTestHandsetInfo, bdaddr %04x,%02x,%06lx",
                   bd_addr.nap, bd_addr.uap, bd_addr.lap);
    }
}

void appTestHeadsetPowerOn(void)
{
    DEBUG_LOG_ALWAYS("appTestHeadsetPowerOn");
    Ui_InjectUiInput(ui_input_sm_power_on);
}

void appTestHeadsetPowerOff(void)
{
    DEBUG_LOG_ALWAYS("appTestHeadsetPowerOff");
    Ui_InjectUiInput(ui_input_sm_power_off);
}

bool appTestPowerOff(void)
{
    DEBUG_LOG_ALWAYS("appTestPowerOff");
    return appPowerOffRequest();
}

/*! \brief Return if Headset is in a Pairing mode
*/
bool appTestIsPairingInProgress(void)
{
    bool isPairingInProgress = !appPairingIsIdle();

    DEBUG_LOG_ALWAYS("appTestIsPairingInProgress:%d", isPairingInProgress);

    return isPairingInProgress;
}

/*! \brief Put Headset into Handset Pairing mode
*/
void appTestPairHandset(void)
{
    DEBUG_LOG_ALWAYS("appTestPairHandset");
    Ui_InjectUiInput(ui_input_sm_pair_handset);
}

/*! \brief Return if Headset has a handset paired
 */
bool appTestIsHandsetPaired(void)
{
    bool paired = BtDevice_IsPairedWithHandset();

    DEBUG_LOG_ALWAYS("appTestIsHandsetPaired:%d",paired);

    return paired;
}


/*! \brief Delete all Handset pairing
*/
void appTestDeleteHandset(void)
{
    DEBUG_LOG_ALWAYS("appTestDeleteHandset");
    Ui_InjectUiInput(ui_input_sm_delete_handsets);
}

static bool reset_happened = TRUE;

/*! \brief Reset headset to factory defaults.
    Will drop any connections, delete all pairing and reboot.
*/
void appTestFactoryReset(void)
{
    DEBUG_LOG_ALWAYS("appTestFactoryReset");
    reset_happened = FALSE;
    Ui_InjectUiInput(ui_input_factory_reset_request);
}

/*! \brief Determine if a reset has happened
    Will return TRUE only once after a reset.
    All subsequent calls will return FALSE
*/
bool appTestResetHappened(void)
{
    bool result = reset_happened;

    DEBUG_LOG_ALWAYS("appTestResetHappened: %d", result);

    if(reset_happened) {
        reset_happened = FALSE;
    }

    return result;
}

bool appTestLicenseCheck(void)
{
    const uint8 license_table[7] =
    {
        APTX_CLASSIC,
        APTX_HD,
        CVC_RECV,
        CVC_SEND_HS_1MIC,
        CVC_SEND_HS_2MIC_B,
        CVC_SEND_HS_2MIC_MO,
        APTX_ADAPTIVE_DECODE
    };
    const bool license_enabled[7] =
    {
        appConfigAptxEnabled(),
        appConfigAptxHdEnabled(),
        TRUE,
        TRUE,
        TRUE,
        TRUE,
        appConfigAptxAdaptiveEnabled()
    };

    bool licenses_ok = TRUE;

    DEBUG_LOG_ALWAYS("appTestLicenseCheck");
    for (int i = 0; i < ARRAY_DIM(license_table); i++)
    {
        if (license_enabled[i])
        {
            if (!FeatureVerifyLicense(license_table[i]))
            {
                DEBUG_LOG_ALWAYS("appTestLicenseCheck: License for feature %d not valid", license_table[i]);
                licenses_ok = FALSE;
            }
            else
                DEBUG_LOG_ALWAYS("appTestLicenseCheck: License for feature %d valid", license_table[i]);
        }
    }

    return licenses_ok;
}

void appTestRegisterForKymeraNotifications(void)
{
    DEBUG_LOG_ALWAYS("appTestRegisterForKymeraNotifications");
    Kymera_RegisterNotificationListener(&testTask);
}

void appTestDisconnectHandsets(void)
{
    StereoTopology_DisconnectAllHandsets();
}

bool appTestIsPtsMode(void)
{
    return A2dpProfile_IsPtsMode();
}

void appTestSetPtsMode(bool enabled)
{
    A2dpProfile_SetPtsMode(enabled);
}

#ifdef INCLUDE_ACCESSORY

/* Find the first (only, if any) handset with an iAP2 link */
static bool appTest_FindAccessoryHandset(bdaddr *handset_address)
{
    device_t *devices = NULL;
    unsigned num_devices = BtDevice_GetConnectedBredrHandsets(&devices);
    unsigned i;
    bool connected = FALSE;

    for (i = 0; i < num_devices; ++i)
    {
        bdaddr address = DeviceProperties_GetBdAddr(devices[i]);

        if (Accessory_IsConnected(address))
        {
            if (handset_address)
            {
                *handset_address = address;
            }

            connected = TRUE;
            break;
        }
    }

    free(devices);
    return connected;
}

void appTestRequestAppLaunch(const char * app_name)
{
    bdaddr handset_address = {0,0,0};

    if (appTest_FindAccessoryHandset(&handset_address))
    {
        DEBUG_LOG_ALWAYS("appTestRequestAppLaunch: handset %04x %02x %06x",
                         handset_address.nap, handset_address.uap, handset_address.lap);

        AccessoryFeature_RequestAppLaunch(handset_address, app_name, launch_with_user_alert);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestRequestAppLaunch: no handset");
    }
}

bool appTestIsAccessoryConnected(void)
{
    bool is_connected = appTest_FindAccessoryHandset(NULL);
    DEBUG_LOG_ALWAYS("appTestIsAccessoryConnected: %u", is_connected);
    return is_connected;
}

bool appTestIsAccessoryHandsetConnected(bdaddr *addr)
{
    bool connected = Accessory_IsConnected(*addr);
    DEBUG_LOG_ALWAYS("appTestIsAccessoryHandsetConnected: addr=%04x %02x %06x conn=%u",
                     addr->nap, addr->uap, addr->lap, connected);

    return connected;
}

unsigned appTestGetIap2ConnectCount(void)
{
    DEBUG_LOG_ALWAYS("appTestGetIap2ConnectCount: %u", iap2_connect_count);
    return iap2_connect_count;
}

unsigned appTestGetIap2DisconnectCount(void)
{
    DEBUG_LOG_ALWAYS("appTestGetIap2DisconnectCount: %u", iap2_disconnect_count);
    return iap2_disconnect_count;
}

#endif /* INCLUDE_ACCESSORY */

bool appTestAnyBredrConnection(void)
{
    bool bredr_connection = ConManagerAnyTpLinkConnected(cm_transport_bredr);
    DEBUG_LOG_ALWAYS("appTestAnyBredrConnection: %d", bredr_connection);
    return bredr_connection;
}

int appTestGetCurrentAudioVolume(void)
{
    int volume = 0;

    audio_source_t focused_source = audio_source_none;
    if (Focus_GetAudioSourceForContext(&focused_source))
    {
        volume = AudioSources_GetVolume(focused_source).value;
    }

    DEBUG_LOG_ALWAYS("appTestGetCurrentAudioVolume enum:audio_source_t:%d volume %d",
                     focused_source, volume);

    return volume;
}

int appTestGetCurrentVoiceVolume(void)
{
    int volume = 0;

    voice_source_t focused_source = voice_source_none;
    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source))
    {
        volume = VoiceSources_GetVolume(focused_source).value;
    }

    DEBUG_LOG_ALWAYS("appTestGetCurrentVoiceVolume enum:voice_source_t:%d volume %d",
                     focused_source, volume);

    return volume;
}

appKymeraScoMode appTestGetHfpCodec(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    if (theKymera)
    {
        DEBUG_LOG_ALWAYS("appTestGetHfpCodec codec=%d", theKymera->sco_info->mode);
        return theKymera->sco_info->mode;
    }
    else
    {
        return NO_SCO;
    }
}

void appTestCvcPassthrough(void)
{
    DEBUG_LOG_ALWAYS("appTestCvcPassthrough");
    Kymera_SetCvcPassthroughMode(KYMERA_CVC_RECEIVE_PASSTHROUGH | KYMERA_CVC_SEND_PASSTHROUGH, 0);
}

void appTest_DeviceDatabaseReport(void)
{
    DEBUG_LOG_ALWAYS("DeviceDatabase");
    DeviceList_Iterate(appTest_ReportDeviceData, NULL);
}

bool appTestIsTopologyRunning(void)
{
    bool running = stereoTopology_IsRunning();

    DEBUG_LOG_ALWAYS("appTestIsTopologyRunning:%d", running);

    return running;
}

bool appTestIsBredrScanEnabled(void)
{
    bool scan_enabled = !BredrScanManager_IsScanDisabled();

    DEBUG_LOG_ALWAYS("appTestIsBredrScanEnabled Enabled:%d", scan_enabled);

    return scan_enabled;
}

/*! List of application states to be included in specific debug.

    Macros are used as it allows us to define a string for log
    purposes, and create an entry in a switch statement while
    keeping the two synchronised.
 */
#define NAMED_STATES(APP_S) \
    APP_S(HEADSET_STATE_NULL) \
    APP_S(HEADSET_STATE_FACTORY_RESET) \
    APP_S(HEADSET_STATE_LIMBO) \
    APP_S(HEADSET_STATE_POWERING_ON) \
    APP_S(HEADSET_STATE_PAIRING) \
    APP_S(HEADSET_STATE_IDLE) \
    APP_S(HEADSET_STATE_BUSY) \
    APP_S(HEADSET_STATE_TERMINATING) \
    APP_S(HEADSET_STATE_POWERING_OFF)

/*! Macro to create a hydra logging string for each state */
#define HYD_STRING(_state) HYDRA_LOG_STRING(HLS_STATE_NAME_ ## _state, #_state);
/*! Macro to create a hydra logging string for each state */
#define STATE_CASE(_state) case _state: state_name = HLS_STATE_NAME_ ## _state; break;

bool appTestIsApplicationState(headsetState checked_state)
{
    bool state_matches;
    headsetState state = SmGetTaskData()->state;
    const char *state_name;

    NAMED_STATES(HYD_STRING)

    state_matches = state == checked_state;

    switch (checked_state)
    {
        NAMED_STATES(STATE_CASE)

        default:
            DEBUG_LOG_ALWAYS("appTestIsApplicationState. State:x%x:%d (state is x%x)",
                            checked_state, state_matches, state);
            return state_matches;
    }

    if (state_matches)
    {
        DEBUG_LOG_ALWAYS("appTestIsApplicationState. %s:%d", state_name, state_matches);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestIsApplicationState. %s:%d (state is x%x)",
                            state_name, state_matches, state);
    }
    return state_matches;
}

#ifdef GC_SECTIONS
/**
 * @brief appTestShowKeptSymbols
 *
 * A list of ADK and library functions and/or variables that are referenced
 * directly via pydbg test scripts, but which are otherwise unused given the
 * current software configuration.
 *
 * This is an alternative mechanism to scattering #pragma unitcodesection KEEP_PM
 * over the library code.  Doing that would unnecessarily include code in production
 * builds even if such code was not actually used.
 *
 * So here we fake our interest by making pointers to things of interest.
 * Nothing is called or dereferenced, but it's enough to stop the linker
 * garbage collecting otherwise unused resources.
 *
 * It's not necessary to call this function, but it's harmess if you do.
 */
static const void *tableOfSymbolsToKeep[] = {
    (void*)ConnectionReadTxPower,
    (void*)Kymera_RegisterNotificationListener,
    (void*)SinkGetRssi,
};
void appTestShowKeptSymbols(void);
void appTestShowKeptSymbols(void)
{
    for( size_t i = 0 ; i < sizeof(tableOfSymbolsToKeep)/sizeof(tableOfSymbolsToKeep[0]) ; i++ )
    {
        if( tableOfSymbolsToKeep[i] != NULL )
        {
            DEBUG_LOG_ALWAYS("Have %p",tableOfSymbolsToKeep[i]);
        }
    }
}
#endif

/*! \brief Macro to create function declaration/definition that returns the value
    of the specified configuration.
    \param returntype The type returned by the function.
    \param name Given a configuration named appConfigXYZ(), XYZ should be passed
    to the name argument and a function will be created called appTestConfigXYZ().*/
#define MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(returntype, name) \
returntype appTestConfig##name(void);                   \
returntype appTestConfig##name(void)                    \
{                                                       \
    return appConfig##name();                           \
}

/*! \name Config Accessor functions.
    \note These don't have a public declaration as they are not expected to be
    called within the headset code - the appConfig functions/macros should be used
    instead.
*/
//!@{
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryFullyCharged)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageOk)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageLow)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, BatteryVoltageCritical)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryChargingTemperatureMax)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryChargingTemperatureMin)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryDischargingTemperatureMax)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(int8, BatteryDischargingTemperatureMin)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, BatteryReadingPeriodMs)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTrickleCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerPreCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerPreFastThresholdVoltage)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerFastCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTerminationCurrent)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint16, ChargerTerminationVoltage)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, ChargerPreChargeTimeoutMs)
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, ChargerFastChargeTimeoutMs)
#ifdef INCLUDE_TEMPERATURE
MAKE_TEST_API_FUNC_FROM_CONFIG_FUNC(uint32, TemperatureReadingPeriodMs)
#endif
//!@}

/*! \brief Returns the current battery voltage
 */
uint16 appTestGetBatteryVoltage(void)
{
    uint16 voltage = appBatteryGetVoltageInstantaneous();
    DEBUG_LOG_ALWAYS("appTestGetBatteryVoltage, %u", voltage);
    return voltage;
}

void appTestSetBatteryVoltage(uint16 new_level)
{
    DEBUG_LOG_ALWAYS("appTestSetBatteryVoltage, %u", new_level);
    appBatteryTestSetFakeVoltage(new_level);
}

void appTestUnsetBatteryVoltage(void)
{
    DEBUG_LOG_ALWAYS("appTestUnsetBatteryVoltage");
    appBatteryTestUnsetFakeVoltage();
}

void appTestInjectBatteryTestVoltage(uint16 new_level)
{
    DEBUG_LOG_ALWAYS("appTestInjectBatteryTestVoltage, %u", new_level);
    appBatteryTestInjectFakeLevel(new_level);
}

void appTestRestartBatteryVoltageMeasurements(void)
{
    DEBUG_LOG_ALWAYS("appTestRestartBatteryVoltageMeasurements");
    appBatteryTestResumeAdcMeasurements();
}

uint8 appTestConvertBatteryVoltageToPercentage(uint16 voltage_mv)
{
    uint8 percent = Soc_ConvertLevelToPercentage(voltage_mv);
    DEBUG_LOG_ALWAYS("appTestConvertBatteryVoltageToPercentage %d mV -> %d%", voltage_mv, percent);

    return percent;
}

bool appTestBatteryStateIsOk(void)
{
    return (battery_region_ok == BatteryRegion_GetState());
}

bool appTestBatteryStateIsCritical(void)
{
    return (battery_region_critical == BatteryRegion_GetState());
}

bool appTestBatteryStateIsUnsafe(void)
{
    return (battery_region_unsafe == BatteryRegion_GetState());
}

void appTestPowerAllowDormant(bool enable)
{
    powerTaskData *thePower = PowerGetTaskData();
    DEBUG_LOG_ALWAYS("appTestPowerAllowDormant %d", enable);
    thePower->allow_dormant = enable;
}

/*! \brief Returns the DSP clock speed in Mhz during the active mode.
*/
uint32 HeadsetTest_DspClockSpeedInActiveMode(void)
{
    audio_dsp_clock kclocks;
    audio_power_save_mode mode = AUDIO_POWER_SAVE_MODE_3;

    PanicFalse(AudioDspGetClock(&kclocks));
    mode = AudioPowerSaveModeGet();
    DEBUG_LOG("HeadsetTest_DspClockSpeedInActiveMode, current mode of operation of kymera dsp is %d, Active mode: %d Mhz, Low Power mode: %d Mhz, Trigger mode: %d Mhz", mode, kclocks.active_mode, kclocks.low_power_mode, kclocks.trigger_mode);
    return kclocks.active_mode;
}


void appTestForceAllowSleep(void)
{
    DEBUG_LOG_ALWAYS("appTestForceAllowSleep");
    Charger_ForceAllowPowerOff(TRUE);
}

void appTestTriggerSleep(void)
{
    DEBUG_LOG_ALWAYS("appTestTriggerSleep");
    MessageCancelFirst(&SmGetTaskData()->task, SM_INTERNAL_TIMEOUT_IDLE);
    MessageSend(&SmGetTaskData()->task, SM_INTERNAL_TIMEOUT_IDLE, NULL);
}

void appTestTriggerLimboTimerTimeout(void)
{
    DEBUG_LOG_ALWAYS("appTestTriggerLimboTimerTimeout");
    MessageCancelAll(headsetSmGetTask(), SM_INTERNAL_TIMEOUT_LIMBO);
    MessageSend(&SmGetTaskData()->task, SM_INTERNAL_TIMEOUT_LIMBO, NULL);
}

void HeadsetTest_SetLeakthroughEnable(void)
{
        DEBUG_LOG_ALWAYS("HeadsetTest_SetLeakthroughEnable");
        if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
        {
            Ui_InjectUiInput(ui_input_leakthrough_on);
        }
}

void HeadsetTest_SetLeakthroughDisable(void)
{
    DEBUG_LOG_ALWAYS("HeadsetTest_SetLeakthroughDisable");
    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        Ui_InjectUiInput(ui_input_leakthrough_off);
    }
}

void HeadsetTest_SetLeakthroughToggleOnOff(void)
{
    DEBUG_LOG_ALWAYS("HeadsetTest_SetLeakthroughToggleOnOff");
    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        Ui_InjectUiInput(ui_input_leakthrough_toggle_on_off);
    }
}

void HeadsetTest_SetLeakthroughMode(leakthrough_mode_t leakthrough_mode)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_SetLeakthroughMode");
    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        switch(leakthrough_mode)
        {
            case LEAKTHROUGH_MODE_1:
                Ui_InjectUiInput(ui_input_leakthrough_set_mode_1);
                break;
            case LEAKTHROUGH_MODE_2:
                Ui_InjectUiInput(ui_input_leakthrough_set_mode_2);
                break;
            case LEAKTHROUGH_MODE_3:
                Ui_InjectUiInput(ui_input_leakthrough_set_mode_3);
                break;
            default:
                DEBUG_LOG_INFO("Invalid value of leakthrough mode is passed");
                break;
        }
    }
}

void HeadsetTest_SetLeakthroughNextMode(void)
{
    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        Ui_InjectUiInput(ui_input_leakthrough_set_next_mode);
    }
}

leakthrough_mode_t HeadsetTest_GetLeakthroughMode(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_GetLeakthroughMode");
    return AecLeakthrough_GetMode();
}

bool HeadsetTest_IsLeakthroughEnabled(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_IsLeakthroughEnabled");
    return AecLeakthrough_IsLeakthroughEnabled();
}

uint32 appTestBatteryFilterResponseTimeMs(void)
{
    return appConfigBatteryReadingPeriodMs();
}

#ifdef HAVE_THERMISTOR
uint16 appTestThermistorDegreesCelsiusToMillivolts(int8 temperature)
{
    return appThermistorDegreesCelsiusToMillivolts(temperature);
}

void appTestSetBatteryTemperature(int8 new_temp)
{
    DEBUG_LOG_ALWAYS("appTestSetBatteryTemperature, %u", new_temp);
    appTemperatureSetFakeValue(new_temp);
}

void appTestUnsetBatteryTemperature(void)
{
    DEBUG_LOG_ALWAYS("appTestUnsetBatteryTemperature");
    appTemperatureUnsetFakeValue();
}

void appTestInjectBatteryTestTemperature(int8 new_temp)
{
    DEBUG_LOG_ALWAYS("appTestInjectBatteryTestTemperature, %d", new_temp);
    appTemperatureTestInjectFakeLevel(new_temp);
}

void appTestRestartTemperatureMeasurements(void)
{
    DEBUG_LOG_ALWAYS("appTestRestartTemperatureMeasurements");
    appTemperatureResumeAdcMeasurements();
}

int8 appTestGetBatteryTemperature(void)
{
    int8 temp = appTemperatureGetInstantaneous();
    DEBUG_LOG_ALWAYS("appTestGetBatteryTemperature, %d", temp);
    return temp;
}

#endif

void HeadsetTest_SetAncEnable(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_SetAncEnable");

    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        Ui_InjectUiInput(ui_input_anc_on);
    }
}

void HeadsetTest_SetAncDisable(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_SetAncDisable");

    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        Ui_InjectUiInput(ui_input_anc_off);
    }
}

void HeadsetTest_SetAncToggleOnOff(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_SetAncToggleOnOff");

    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        Ui_InjectUiInput(ui_input_anc_toggle_on_off);
    }
}

void HeadsetTest_SetAncMode(anc_mode_t mode)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_SetAncMode");

    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        switch(mode)
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

void HeadsetTest_SetAncNextMode(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_SetAncNextMode");

    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        Ui_InjectUiInput(ui_input_anc_set_next_mode);
    }
}

bool HeadsetTest_GetAncstate(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_GetAncstate");

    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        return AncStateManager_IsEnabled();
    }
    return FALSE;
}

anc_mode_t HeadsetTest_GetAncMode(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_GetAncMode");

    if(headsetState_IsValidToInjectAncAndLeakthroughEvents())
    {
        return AncStateManager_GetMode();
    }
    return FALSE;

}

void HeadsetTest_SetAncLeakthroughGain(uint8 gain)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_SetAncLeakthroughGain %d", gain);
    AncStateManager_StoreAncLeakthroughGain(gain);
    Ui_InjectUiInput(ui_input_anc_set_leakthrough_gain);
}

uint8 HeadsetTest_GetAncLeakthroughGain(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_GetAncLeakthroughGain");
    return AncStateManager_GetAncGain();
}

/*! \brief Write ANC delta gains into ANC_DELTA_USR_PSKEY(USR13 PSKEY) for FFA, FFB and FB paths in one call.
    \param ffa_delta ANC FFA path delta gain (in dB scale S6.9 representation; i.e., (int16) dB * 512)
    \param ffb_delta ANC FFB path delta gain (in dB scale S6.9 representation; i.e., (int16) dB * 512)
    \param fb_delta  ANC FB/EC path delta gain (in dB scale S6.9 representation; i.e., (int16) dB * 512)

    \return Returns delta gain write status
*/
bool HeadsetTest_WriteAncDeltaGains(int16 inst_0_ffa_delta, int16 inst_0_ffb_delta,
    int16 inst_0_fb_delta, int16 inst_1_ffa_delta, int16 inst_1_ffb_delta,
    int16 inst_1_fb_delta)
{
#define FINE_DELTA_GAIN_ENTRIES     (6U)
#define ANC_DELTA_USR_PSKEY         (13U)
    uint16 fine_delta_gains[FINE_DELTA_GAIN_ENTRIES] = {0};

    fine_delta_gains[0] = (uint16)inst_0_ffa_delta;
    fine_delta_gains[1] = (uint16)inst_0_ffb_delta;
    fine_delta_gains[2] = (uint16)inst_0_fb_delta;

    fine_delta_gains[3] = (uint16)inst_1_ffa_delta;
    fine_delta_gains[4] = (uint16)inst_1_ffb_delta;
    fine_delta_gains[5] = (uint16)inst_1_fb_delta;

    return (PsStore(ANC_DELTA_USR_PSKEY, fine_delta_gains, FINE_DELTA_GAIN_ENTRIES) == FINE_DELTA_GAIN_ENTRIES);
}

/*! \brief Write ANC delta gain into ANC_DELTA_USR_PSKEY(USR13 PSKEY) for specific ANC path and instance.

    \param AUDIO_ANC_PATH_ID_FFA(1) or AUDIO_ANC_PATH_ID_FFB(2) or AUDIO_ANC_PATH_ID_FB(3)
    \param AUDIO_ANC_INSTANCE_0(1) or AUDIO_ANC_INSTANCE_1(2)
    \param delta  Delta gain to write (in dB scale S6.9 representation; i.e., (int16) dB * 512)

    \return Returns delta gain write status
*/
bool HeadsetTest_WriteAncDeltaGain(audio_anc_path_id path, audio_anc_instance inst, int16 delta)
{
#define FINE_DELTA_GAIN_ENTRIES     (6U)
#define ANC_DELTA_USR_PSKEY         (13U)
    uint16 fine_delta_gains[FINE_DELTA_GAIN_ENTRIES] = {0};
    uint16 instance_offset = (inst == AUDIO_ANC_INSTANCE_0)? (0) : (3);

    /* Read current delta gain from persistant */
    PsRetrieve(ANC_DELTA_USR_PSKEY, fine_delta_gains, FINE_DELTA_GAIN_ENTRIES);

    switch(path)
    {
        case AUDIO_ANC_PATH_ID_FFA:
            fine_delta_gains [0 + instance_offset] = (uint16)delta;
            break;

        case AUDIO_ANC_PATH_ID_FFB:
            fine_delta_gains [1 + instance_offset] = (uint16)delta;
            break;

        case AUDIO_ANC_PATH_ID_FB:
            fine_delta_gains [2 + instance_offset] = (uint16)delta;
            break;

        default:
            break;
    }

    return (PsStore(ANC_DELTA_USR_PSKEY, fine_delta_gains, FINE_DELTA_GAIN_ENTRIES) == FINE_DELTA_GAIN_ENTRIES);
}

/*! \brief Read delta gain value from ANC_DELTA_USR_PSKEY(USR13 PSKEY) for given ANC path and instance.
    \param AUDIO_ANC_PATH_ID_FFA(1) or AUDIO_ANC_PATH_ID_FFB(2) or AUDIO_ANC_PATH_ID_FB(3)
    \param AUDIO_ANC_INSTANCE_0(1) or AUDIO_ANC_INSTANCE_1(2)
    \return Returns delta gain for given path (in dB scale S6.9 representation; i.e., (int16) dB * 512)
*/
int16 HeadsetTest_ReadAncDeltaGain(audio_anc_path_id path, audio_anc_instance inst)
{
#define FINE_DELTA_GAIN_ENTRIES (6U)
#define ANC_DELTA_USR_PSKEY         (13U)
    uint16 fine_delta_gains[FINE_DELTA_GAIN_ENTRIES] = {0};
    uint16 instance_offset = 3 * (inst - 1);

    /* check if key having required entries */
    if(PsRetrieve(ANC_DELTA_USR_PSKEY, fine_delta_gains, FINE_DELTA_GAIN_ENTRIES) == FINE_DELTA_GAIN_ENTRIES)
    {
        DEBUG_LOG_ALWAYS("Valid PSKEY");
        switch(path)
        {
            case AUDIO_ANC_PATH_ID_FFA:
                return (int16)fine_delta_gains[0 + instance_offset];
            case AUDIO_ANC_PATH_ID_FFB:
                return (int16)fine_delta_gains[1 + instance_offset];
            case AUDIO_ANC_PATH_ID_FB:
                return (int16)fine_delta_gains[2 + instance_offset];
            default:
                return 0;
        }
    }
    else
    {
        DEBUG_LOG_ALWAYS("Invalid PSKEY");
        return 0;
    }
}

void HeadsetTest_StartAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_StartTuningMode");
    Ui_InjectUiInput(ui_input_anc_enter_tuning_mode);
}

void HeadsetTest_StopAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("HeadsetTest_StopTuningMode");
    Ui_InjectUiInput(ui_input_anc_exit_tuning_mode);
}

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
void HeadsetTest_WiredAnalogAudioPlugged(void)
{
     DEBUG_LOG_ALWAYS("HeadsetTest_StartWiredAnalogAudio");
    WiredAudioDetect_UpdateMask(FALSE, WIRED_AUDIO_SOURCE_LINE_IN);
    WiredAudioSource_UpdateClient();
}

void HeadsetTest_WiredAnalogAudioUnplugged(void)
{
     DEBUG_LOG_ALWAYS("HeadsetTest_StopWiredAnalogAudio");

    WiredAudioDetect_UpdateMask(TRUE, WIRED_AUDIO_SOURCE_LINE_IN);
    WiredAudioSource_UpdateClient();
}

void HeadsetTest_WiredAudioIncrementVolume(audio_source_t source)
{
	DEBUG_LOG_ALWAYS("HeadsetTest_WiredAudioIncrementVolume: Incremented volume by one step");

	WiredAudioSource_IncrementVolume(source);
}

void HeadsetTest_WiredAudioDecrementVolume(audio_source_t source)
{
	DEBUG_LOG_ALWAYS("HeadsetTest_WiredAudioDecrementVolume: Decremented volume by one step");

	WiredAudioSource_DecrementVolume(source);
}

bool HeadsetTest_IsLineInAudioAvailable(void)
{
	bool line_in_available = FALSE;
	line_in_available = WiredAudioSource_IsAudioAvailable(audio_source_line_in);
	DEBUG_LOG_ALWAYS("HeadsetTest_IsLineInAudioAvailable: result %d ", line_in_available);
	return line_in_available;
}

#endif

bool HeadsetTest_UsbAudioVoice1Af_Connect(void)
{
    if(HeadsetUsb_SetInactiveAppType(HEADSET_USB_APP_TYPE_DEFAULT))
    {
        return HeadsetUsb_SetActiveAppType(HEADSET_USB_APP_TYPE_AUDIO_VOICE_1AF);
    }

    return FALSE;
}

bool HeadsetTest_UsbAudioVoice2Af_Connect(void)
{
    if(HeadsetUsb_SetInactiveAppType(HEADSET_USB_APP_TYPE_DEFAULT))
    {
        return HeadsetUsb_SetActiveAppType(HEADSET_USB_APP_TYPE_AUDIO_VOICE_2AF);
    }

    return FALSE;
}

bool HeadsetTest_UsbAudio_Connect(void)
{
    if(HeadsetUsb_SetInactiveAppType(HEADSET_USB_APP_TYPE_DEFAULT))
    {
        return HeadsetUsb_SetActiveAppType(HEADSET_USB_APP_TYPE_AUDIO);
    }

    return FALSE;
}

bool HeadsetTest_UsbVoice_Connect(void)
{
    if(HeadsetUsb_SetInactiveAppType(HEADSET_USB_APP_TYPE_DEFAULT))
    {
        return HeadsetUsb_SetActiveAppType(HEADSET_USB_APP_TYPE_VOICE);
    }

    return FALSE;
}

bool HeadsetTest_UsbDefault_Connect(void)
{
    if(HeadsetUsb_SetInactiveAppType(HEADSET_USB_APP_TYPE_DEFAULT))
    {
        return HeadsetUsb_SetActiveAppType(HEADSET_USB_APP_TYPE_DEFAULT);
    }

    return FALSE;
}

#ifdef INCLUDE_USB_NB_WB_TEST
bool HeadsetTest_UsbVoiceNb_Connect(void)
{
    if(HeadsetUsb_SetInactiveAppType(HEADSET_USB_APP_TYPE_DEFAULT))
    {
        return HeadsetUsb_SetActiveAppType(HEADSET_USB_APP_TYPE_VOICE_NB);
    }

    return FALSE;
}

bool HeadsetTest_UsbVoiceWb_Connect(void)
{
    if(HeadsetUsb_SetInactiveAppType(HEADSET_USB_APP_TYPE_DEFAULT))
    {
        return HeadsetUsb_SetActiveAppType(HEADSET_USB_APP_TYPE_VOICE_WB);
    }

    return FALSE;
}
#endif/* INCLUDE_USB_NB_WB_TEST */

bool HeadsetTest_IsUsbAudioEnumerated(void)
{
    unsigned usb_ctx = UsbSource_GetAudioContext(audio_source_usb);

    DEBUG_LOG_ALWAYS("HeadsetTest_IsUsbAudioEnumerated AudioCtx %d ", usb_ctx);

    return (usb_ctx == context_audio_disconnected) ?
            FALSE : TRUE;
}

bool HeadsetTest_IsUsbVoiceEnumerated(void)
{
    unsigned usb_ctx = UsbSource_GetVoiceContext(voice_source_usb);

    DEBUG_LOG_ALWAYS("HeadsetTest_IsUsbVoiceEnumerated VoiceCtx %d ", usb_ctx);

    return (usb_ctx == context_voice_disconnected) ?
            FALSE : TRUE;
}

bool HeadsetTest_IsUsbDualAudioEnumerated(void)
{
    unsigned usb_ctx = UsbSource_GetAudioContext(audio_source_usb);

    DEBUG_LOG_ALWAYS("HeadsetTest_IsUsbDualAudioEnumerated AudioCtx %d ", usb_ctx);
    if(usb_ctx == context_audio_disconnected)
    {
        return FALSE;
    }

    usb_ctx = UsbSource_GetVoiceContext(voice_source_usb);
    DEBUG_LOG_ALWAYS("HeadsetTest_IsUsbDualAudioEnumerated VoiceCtx %d ", usb_ctx);
    if(usb_ctx == context_voice_disconnected)
    {
        return FALSE;
    }

    return TRUE;
}

bool HeadsetTest_UsbDetach(void)
{
    if(UsbApplication_IsAttachedToHub())
    {
        UsbDevice_HandleMessage(MESSAGE_USB_DETACHED, NULL);
        UsbApplication_Detach();
        return TRUE;
    }

    return FALSE;
}

bool HeadsetTest_UsbAttach(void)
{
    if(!UsbApplication_IsAttachedToHub())
    {
        UsbApplication_Attach();
        return TRUE;
    }

    return FALSE;
}

bool HeadsetTest_UsbHidTransportPlayPause(void)
{
    return UsbSource_SendEvent(USB_SOURCE_PLAY_PAUSE);
}

bool HeadsetTest_UsbHidTransportPlay(void)
{
    return UsbSource_SendEvent(USB_SOURCE_PLAY);
}

bool HeadsetTest_UsbHidTransportPause(void)
{
    return UsbSource_SendEvent(USB_SOURCE_PAUSE);
}

bool HeadsetTest_UsbHidTransportVolumeUp(void)
{
    return UsbSource_SendEvent(USB_SOURCE_VOL_UP);
}

bool HeadsetTest_UsbHidTransportVolumeDown(void)
{
    return UsbSource_SendEvent(USB_SOURCE_VOL_DOWN);
}

bool HeadsetTest_UsbHidTransportMute(void)
{
    return UsbSource_SendEvent(USB_SOURCE_MUTE);
}

bool HeadsetTest_UsbHidTransportFfwdOn(void)
{
    return UsbSource_SendEvent(USB_SOURCE_FFWD_ON);
}

bool HeadsetTest_UsbHidTransportFfwdOff(void)
{
    return UsbSource_SendEvent(USB_SOURCE_FFWD_OFF);
}

bool HeadsetTest_UsbHidTransportRewOn(void)
{
    return UsbSource_SendEvent(USB_SOURCE_REW_ON);
}

bool HeadsetTest_UsbHidTransportRewOff(void)
{
    return UsbSource_SendEvent(USB_SOURCE_REW_OFF);
}

bool HeadsetTest_UsbHidTransportNextTrack(void)
{
    return UsbSource_SendEvent(USB_SOURCE_NEXT_TRACK);
}

bool HeadsetTest_UsbHidTransportPreviousTrack(void)
{
    return UsbSource_SendEvent(USB_SOURCE_PREVIOUS_TRACK);
}

bool HeadsetTest_UsbHidTransportStop(void)
{
    return UsbSource_SendEvent(USB_SOURCE_STOP);
}

bool appTestHandsetA2dpMediaSuspend(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaSuspend");

    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        appAvStreamingSuspend(AV_SUSPEND_REASON_LOCAL);
        return TRUE;
    }
    return FALSE;
}

bool appTestHandsetA2dpMediaClose(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaClose");

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);

        if (theInst)
        {
            return A2dpMediaCloseRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id);
        }
    }

    return FALSE;
}

bool appTestHandsetA2dpAbort(void)
{
    bdaddr bd_addr;

    DEBUG_LOG_ALWAYS("appTestHandsetA2dpAbort");

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        return FALSE;
    }
    return FALSE;
}

bool appTestHandsetA2dpMediaReconfigureRequest(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaReconfigureRequest");
    bdaddr bd_addr;
/* Refer Codec Specific Information Elements for SBC section in A2DP specification.
Service category - media codec = 7
Length of service capability = 6
Media type - audio = 0
Media code type - SBC = 0
codec information in bit field values - - 28,17
|-------------------------------------------------------|
|   7    6    5    4     3      2       1      0        |
|-------------------------------------------------------|
|  sample Frequency  |  Channel mode - mono-1 (b3)      |
|  48000 - 1(b4)     |       Dual mode - 1 (b2)         |
|-------------------------------------------------------|
| Block length       | subbands -00 | Allocation Method |
| 16= 1 (b4)         |              | Loudness = 01(b0) |
|-------------------------------------------------------|
|             minimum bit pool value = 31               |
|-------------------------------------------------------|
|             maximum bit pool value = 31               |
|-------------------------------------------------------|
*/
    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        const uint8 service_caps[] = {7,6,0,0,28,17,31,31};
        return A2dpMediaReconfigureRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id, (uint16)(sizeof(service_caps)), service_caps);
    }
    return FALSE;
}

bool appTestHandsetA2dpMediaStart(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetA2dpMediaStart");
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        /* Find handset AV instance */
        avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);
        return A2dpMediaStartRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id);
    }
    return FALSE;
}

app_test_a2dp_codec_t HeadsetTest_GetA2dpCodec(void)
{
    DEBUG_LOG_ALWAYS("HeadsetTest_GetA2dpCodec");

    bdaddr bd_addr;

    if(appDeviceGetHandsetBdAddr(&bd_addr))
    {
      /*Find Handset AV instance*/
      avInstanceTaskData *theInst = appAvInstanceFindFromBdAddr(&bd_addr);

      if(theInst)
      {

          switch(theInst->a2dp.current_seid)
          {
                case AV_SEID_SBC_SNK:
                    return a2dp_codec_sbc;

                case AV_SEID_AAC_SNK:
                    return a2dp_codec_aac;

                case AV_SEID_APTX_SNK:
                    return a2dp_codec_aptx;

                case AV_SEID_APTX_ADAPTIVE_SNK:
                    return a2dp_codec_aptx_adaptive;

                case AV_SEID_APTXHD_SNK:
                    return a2dp_codec_aptx_hd;

                default:
                    break;
          }
      }
   }

    return a2dp_codec_unknown;
}



uint32 appTestUpgradeGetVersion(void)
{
    uint32 result = 0;
    uint16 major, minor, config;
    if (UpgradeGetVersion(&major, &minor, &config))
    {
        DEBUG_LOG_ALWAYS("appTestUpgradeGetVersion: %d.%d.%d", major, minor, config);
        result = (major << 24);
        result |= (minor << 8);
        result |= (config & 0xFF);
    }
    else
    {
        DEBUG_LOG_ALWAYS("appTestUpgradeGetVersion: UpgradeGetVersion failed");
    }

    return result;
}




#ifdef INCLUDE_GAA
uint8 HeadsetTest_GetGaaOtaState(void)
{
    return Gaa_OtaGetState();
}

uint32 HeadsetTest_GetGaaModelId(void)
{
    uint32 model_id = 0;
    if (Gaa_GetModelId(&model_id))
    {
        DEBUG_LOG_ALWAYS("HeadsetTest_GetGaaModelId: 0x%08lx", model_id);
    }
    else
    {
        DEBUG_LOG_ERROR("HeadsetTest_GetGaaModelId: Gaa_GetModelId failed");
    }
    return model_id;
}

bool HeadsetTest_SetGaaModelId(uint32 model_id)
{
    bool result = Gaa_SetModelId(model_id);
    if (result)
    {
        DEBUG_LOG_ALWAYS("HeadsetTest_SetGaaModelId(0x%08lx) successful", model_id);
    }
    else
    {
        DEBUG_LOG_ERROR("HeadsetTest_SetGaaModelId(0x%08lx) failed", model_id);
    }
    return result;
}

uint32 appTestGetGaaOtaControl(void)
{
    uint32 result = 0xFFFFFFFFLU;
    uint16 GaaOtaControl = 0;
    bool   present = Gaa_OtaGetControlPsKey(&GaaOtaControl);
    if (present)
    {
        result = GaaOtaControl;
    }
    return result;
}

void appTestSetGaaOtaControl(uint16 GaaOtaControl)
{
    Gaa_OtaSetControlPsKey(GaaOtaControl);
}

#endif  /* INCLUDE_GAA */

#ifdef INCLUDE_GAMING_MODE
bool appTestIsGamingModeOn(void)
{
    DEBUG_LOG_ALWAYS("appTestIsGamingModeOn");
    return GamingMode_IsGamingModeEnabled();
}
bool appTestSetGamingMode(bool new_gaming_state)
{
    bool current_gaming_state = appTestIsGamingModeOn();
    DEBUG_LOG_ALWAYS("appTestSetGamingMode %d current_gaming_state %d ", new_gaming_state, current_gaming_state);
    if(current_gaming_state != new_gaming_state)
    {
        Ui_InjectUiInput(ui_input_gaming_mode_toggle);
        return TRUE;
    }

    return FALSE;
}
#endif /* INCLUDE_GAMING_MODE */
#ifdef INCLUDE_LATENCY_MANAGER
bool appTestIsDynamicLatencyAdjustmentEnabled(void)
{
    DEBUG_LOG_ALWAYS("appTestIsDynamicLatencyAdjustmentEnabled");
    return Kymera_DynamicLatencyIsEnabled();
}

uint16 appTestGetCurrentA2dpLatency(void)
{
    DEBUG_LOG_ALWAYS("appTestGetCurrentA2dpLatency");
    return Kymera_LatencyManagerGetLatency();
}

void appTestSetDynamicLatencyAdjustment(bool enable)
{
    DEBUG_LOG_ALWAYS("appTestSetDynamicLatencyAdjustment %d", enable);
    if (enable)
    {
        Kymera_DynamicLatencyEnableDynamicAdjustment();
    }
    else
    {
        Kymera_DynamicLatencyDisableDynamicAdjustment();
    }
}
#endif /* INCLUDE_LATENCY_MANAGER */

bool appTestIsHandsetQhsConnected(void)
{
    bool qhs_connected_status = FALSE;
    bdaddr bd_addr;

    if (appDeviceGetHandsetBdAddr(&bd_addr))
    {
        qhs_connected_status = ConManagerGetQhsConnectStatus(&bd_addr);
    }

    DEBUG_LOG_ALWAYS("appTestIsHandsetQhsConnected:%d", qhs_connected_status);

    return qhs_connected_status;
}

bool appTestTransmitEnable(bool val)
{
    bool successful = VmTransmitEnable(val);

    DEBUG_LOG_ALWAYS("appTestTransmitEnable(%d)  success=%d", val, successful);

    return successful;
}


#ifdef INCLUDE_MUSIC_PROCESSING
#include "music_processing.h"

void appTest_MusicProcessingGetAvailablePresets(void)
{
    DEBUG_LOG_ALWAYS("appTest_MusicProcessingGetAvailablePresets");
}

void appTest_MusicProcessingSetPreset(uint8 preset)
{
    DEBUG_LOG_ALWAYS("appTest_MusicProcessingSetPreset Preset:%d", preset);
    MusicProcessing_SetPreset(preset);
}

void appTest_MusicProcessingSetUserEqBand(uint8 band, int16 gain)
{
    int16 band_gain = gain;
    DEBUG_LOG_ALWAYS("appTest_MusicProcessingSetUserEqBand Band:%d Gain: %d", band, gain);
    MusicProcessing_SetUserEqBands(band, band, &band_gain);
}


#endif /* INCLUDE_MUSIC_PROCESSING */


#ifdef INCLUDE_FAST_PAIR
/* Refer comments given in the function prototype to configure fast pair model ID.
*/
void appTestSetFPModelID(uint32 fast_pair_model_id)
{
    DEBUG_LOG("appTestSetFPModelID");

    uint32 buffer = fast_pair_model_id;

    /* Size of Model ID to be passed to PsStore API should be in words (2 words) */
    PanicFalse(PsStore(PS_KEY_FAST_PAIR_MODEL_ID, &buffer, FAST_PAIR_CONFIG_MODEL_ID_LEN/2));
}

/* Refer comments given in the function prototype to configure scrambled aspk.
*/
void appTestSetFPASPK(uint32 dword1, uint32 dword2, uint32 dword3, uint32 dword4,
                      uint32 dword5, uint32 dword6, uint32 dword7, uint32 dword8)
{
    DEBUG_LOG("appTestSetFPASPK");
    uint32 aspk [8];

    aspk[0] = dword1;
    aspk[1] = dword2;
    aspk[2] = dword3;
    aspk[3] = dword4;
    aspk[4] = dword5;
    aspk[5] = dword6;
    aspk[6] = dword7;
    aspk[7] = dword8;

    /* Size of ASPK(32 bytes) to be passed to PsStore  API should be in words (16 words) */
    PanicFalse(PsStore(PS_KEY_FAST_PAIR_SCRAMBLED_ASPK, (void *)aspk, FAST_PAIR_CONFIG_ASPK_LEN/2));
}
#endif /* INCLUDE_FAST_PAIR */

#ifdef INCLUDE_CHARGER

void appTestChargerStatus(charger_status chg_status)
{
    Charger_TestModeControl(TRUE);
    Charger_TestChargerStatus(chg_status);
}

void appTestChargerDetected(usb_attached_status attached_status,
                                       uint16 charger_dp_millivolts,
                                       uint16 charger_dm_millivolts,
                                       usb_type_c_advertisement cc_status)
{
    Charger_TestModeControl(TRUE);
    Charger_TestChargerDetected(attached_status,
                                charger_dp_millivolts,
                                charger_dm_millivolts,
                                cc_status);
}

void appTestChargerConnected(bool is_connected)
{
    Charger_TestModeControl(TRUE);
    Charger_TestChargerConnected(is_connected);
}

void appTestChargerRestore(void)
{
    Charger_TestModeControl(FALSE);
}

charger_detect_type appTestChargerGetType(void)
{
#ifdef INCLUDE_CHARGER_DETECT
    return ChargerDetect_GetChargerType();
#else
    return CHARGER_TYPE_NOT_RESOLVED;
#endif
}


#endif /* INCLUDE_CHARGER */

#ifdef INCLUDE_USB_DEVICE

typedef enum
{
    CMD_LOOPBACK,
    CMD_RETURN_HEADER,
    CMD_DISCARD,
    CMD_REBOOT
} usb_hid_test_command;

typedef struct
{
    unsigned char report_id;
    unsigned char cmd;
    unsigned char padding[2];
    unsigned checksum;
    unsigned id;
} usb_hid_test_header;

static void appTestUsbHidHandler(uint8 report_id, const uint8 *data, uint16 size)
{
    usb_result_t result;

    if (report_id != HID_REPORTID_TEST_TRANSFER)
    {
        /* ignore this report */
        return;
    }

    const usb_hid_test_header *hdr = (const usb_hid_test_header *)data;

    /* transfer is complete */
    DEBUG_LOG_ALWAYS("USB_HID_TEST: RX report %d cmd %d, %d bytes",
        report_id, hdr->cmd, size);


    switch (hdr->cmd)
    {
    case CMD_LOOPBACK:
        DEBUG_LOG_ALWAYS("USB_HID_TEST: TX report %d, %d bytes",
                HID_REPORTID_TEST_RESPONSE, size - 1);

        result = UsbHid_Datalink_SendReport(HID_REPORTID_TEST_RESPONSE,
                                            &data[1], size - 1);
        /* don't bother re-trying */
        if (result != USB_RESULT_OK)
        {
            DEBUG_LOG_ALWAYS("USB_HID_TEST: SendReport failed");
            Panic();
        }
        break;

    case CMD_RETURN_HEADER:
        DEBUG_LOG_ALWAYS("USB_HID_TEST: TX report %d, %d bytes",
                HID_REPORTID_TEST_SHORT_RESPONSE, sizeof(usb_hid_test_header) - 1);

        result = UsbHid_Datalink_SendReport(HID_REPORTID_TEST_SHORT_RESPONSE,
                                            &data[1], sizeof(usb_hid_test_header) - 1);
        /* don't bother re-trying */
        if (result != USB_RESULT_OK)
        {
            DEBUG_LOG_ALWAYS("USB_HID_TEST: SendReport failed");
            Panic();
        }
        break;

    case CMD_DISCARD:
        /* data is silently discarded */
        break;

    case CMD_REBOOT:
        DEBUG_LOG_ALWAYS("USB_HID_TEST: reboot device");
        BootSetMode(BootGetMode());
        break;

    default:
        DEBUG_LOG_ALWAYS("USB_HID_TEST: unexpected command 0x%x", hdr->cmd);
        Panic();
    }
}

void appTestUsbHidInit(void)
{
    DEBUG_LOG_ALWAYS("USB_HID_TEST: register HID report handler");
    UsbHid_Datalink_RegisterHandler(appTestUsbHidHandler);
}

void appTestUsbHidDeInit(void)
{
    DEBUG_LOG_ALWAYS("USB_HID_TEST: unregister HID report handler");
    UsbHid_Datalink_UnregisterHandler(appTestUsbHidHandler);
}


/* Communications Devices class test case */

/* Add streaming to/from SPI SRAM to the USB CDC test */
#include "peripherals/spi_sram_23lc1024.h"

static bool headset_test_enable_spi_sram;

typedef enum
{
    USB_CDC_WRITE,
    USB_CDC_WRITE_RESP,
    USB_CDC_READ,
    USB_CDC_READ_RESP
} usb_cdc_test_cmd_t;

static void appTestUsbCdc_SpiSramWriteComplete(void *cb_context, bool result)
{
    uint32 offset = (uint32)cb_context;

    if (!result)
    {
        DEBUG_LOG_ALWAYS("USB_CDC_TEST: SPI SRAM write failed");
        return;
    }

    uint8 resp[4];

    resp[0] = USB_CDC_WRITE_RESP;
    resp[1] = (uint8)(offset >> 16);
    resp[2] = (uint8)(offset >> 8);
    resp[3] = (uint8)(offset);

    /* don't bother re-trying */
    if (UsbCdc_SendData(resp, 4) != USB_RESULT_OK)
    {
        DEBUG_LOG_ALWAYS("USB_CDC_TEST: SendData USB_CDC_WRITE_RESP failed");
        Panic();
    }
}

static void appTestUsbCdc_SpiSramReadComplete(void *cb_context, bool result)
{
    uint8 *read_data = (uint8 *)cb_context;

    if (!result)
    {
        DEBUG_LOG_ALWAYS("USB_CDC_TEST: SPI SRAM read failed");
    }

    read_data[0] = USB_CDC_READ_RESP;

    uint16 size = (read_data[4] << 8) | read_data[5];

    /* don't bother re-trying */
    if (UsbCdc_SendData(read_data, size + 6) != USB_RESULT_OK)
    {
        DEBUG_LOG_ALWAYS("USB_CDC_TEST: SendData USB_CDC_READ_RESP failed");
        Panic();
    }
    free(read_data);
}

static void appTestUsbCdcHandler(const uint8 *data, uint16 size)
{
    if (!headset_test_enable_spi_sram)
    {
        /* Simple loopback */
        usb_result_t result;

        DEBUG_LOG_ALWAYS("USB_CDC_TEST: RX %d bytes", size);

        result = UsbCdc_SendData(data, size);
        /* don't bother re-trying */
        if (result != USB_RESULT_OK)
        {
            DEBUG_LOG_ALWAYS("USB_CDC_TEST: SendData failed");
            Panic();
        }
        return;
    }

    if (size < 5)
    {
        DEBUG_LOG_ALWAYS("USB_CDC_TEST: short packet size %d", size);
        return;
    }

    usb_cdc_test_cmd_t cmd = data[0];
    uint32 offset = (data[1] << 16) | (data[2] << 8) | data[3];

    /* transfer is complete */

    switch (cmd)
    {
    case USB_CDC_WRITE:
    {
        spi_sram_23lc1024_write(offset, size - 4, &data[4],
                                appTestUsbCdc_SpiSramWriteComplete, (void *)offset);

        break;
    }

    case USB_CDC_READ:
    {
        uint16 read_size = (data[4] << 8) | data[5];
        uint8 *read_data = PanicUnlessMalloc(read_size + 6);

        /* copy offset and size */
        memcpy(&read_data[1], &data[1], 5);

        spi_sram_23lc1024_read(offset, read_size, &read_data[6],
                               appTestUsbCdc_SpiSramReadComplete, read_data);

        break;
    }
    default:
        break;
    }
}

void appTestUsbCdcInit(bool enable_spi_ram_test)
{
    DEBUG_LOG_ALWAYS("USB_CDC_TEST: open UsbCdc device and register handler");

    UsbApplication_Open(&usb_app_cdc);
    UsbCdc_RegisterHandler(appTestUsbCdcHandler);

    headset_test_enable_spi_sram = enable_spi_ram_test;

    if (headset_test_enable_spi_sram)
    {
        DEBUG_LOG_ALWAYS("USB_CDC_TEST: init Bitserial SPI SRAM");
        spi_sram_23lc1024_init();
    }
}

void appTestUsbCdcDeInit(void)
{
    DEBUG_LOG_ALWAYS("USB_CDC_TEST: close");

    UsbCdc_UnregisterHandler(appTestUsbCdcHandler);
    UsbApplication_Close();
}

#endif /* INCLUDE_USB_DEVICE */
