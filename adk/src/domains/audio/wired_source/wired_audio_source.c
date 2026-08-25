/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       wired_audio_source.c
\brief      Wired audio source moduleInterp
*/

#if defined(INCLUDE_WIRED_ANALOG_AUDIO) || defined(INCLUDE_A2DP_ANALOG_SOURCE)
#include <stdlib.h>
#include <vm.h>
#include <message.h>
#include <panic.h>

#include "logging.h"
#include "wired_audio_source.h"
#include "wired_audio_private.h"
#include "audio_sources.h"
#include "wired_audio_source_interface.h"
#include "wired_audio_volume_interface.h"
#include "wired_audio_media_control_interface.h"
#include "pio_common.h"
#include "ui.h"
#include "audio_sources.h"
#include "volume_messages.h"
#include <bt_device.h>
#include <device_list.h>
#include <device_properties.h>
#include <device_db_serialiser.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(wired_audio_detect_msg_t)

#ifndef HOSTED_TEST_ENVIRONMENT

/*! There is checking that the messages assigned by this module do
not overrun into the next module's message ID allocation */
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(WIRED_AUDIO_DETECT, WIRED_AUDIO_DETECT_MESSAGE_END)

#endif

static void wiredAudioSource_HandleMessage(Task task, MessageId id, Message message);

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
  #if !defined(ENABLE_WIRED_AUDIO_FEATURE_PRIORITY)
      #error ENABLE_WIRED_AUDIO_FEATURE_PRIORITY is missing in combination with INCLUDE_WIRED_ANALOG_AUDIO
  #endif
#endif  /* INCLUDE_WIRED_ANALOG_AUDIO */

#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY
/* We need feature priority, because VA is enabled by default and both wired analog & VA will share the same ADC source */
static feature_state_t wiredAudioSource_GetFeatureState(void);

static feature_manager_handle_t analog_audio_feature_manager_handle = NULL;
static feature_state_t analog_audio_feature_state = feature_state_idle;
static const feature_interface_t analog_audio_feature_manager_if =
{
    .GetState = wiredAudioSource_GetFeatureState,
    .Suspend = NULL,
    .Resume = NULL
};

static feature_state_t wiredAudioSource_GetFeatureState(void)
{
    return analog_audio_feature_state;
}
#endif /*#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY*/


/*! Initialise the task data */
wiredAudioSourceTaskData wiredaudio_source_taskdata = {.task = wiredAudioSource_HandleMessage};

static void wiredAudioSource_HandleBtDeviceSelfCreated(const BT_DEVICE_SELF_CREATED_IND_T *ind)
{
    DEBUG_LOG_DEBUG("wiredAudioSource_HandleBtDeviceSelfCreated");
    device_t device = PanicNull(ind->device);

    if(!Device_IsPropertySet(device, device_property_analog_audio_volume))
    {
        DEBUG_LOG_INFO("wiredAudioSource_HandleBtDeviceSelfCreated: SET device_property_analog_audio_volume");
        Device_SetPropertyU8(device, device_property_analog_audio_volume, WIRED_AUDIO_DEFAULT_VOLUME);
        DeviceDbSerialiser_Serialise();
    }
}

/*! \brief Message handler for Wired audio module.*/
static void wiredAudioSource_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch (id)
    {
        case MESSAGE_PIO_CHANGED:
        {
            WiredAudioDetect_HandlePioChanged((const MessagePioChanged *)message);
        }
        break;
        case BT_DEVICE_SELF_CREATED_IND:
        {
            wiredAudioSource_HandleBtDeviceSelfCreated((const BT_DEVICE_SELF_CREATED_IND_T*) message);
        }
        break;
        default:
        break;
    }
}

void WiredAudioSource_SetFeatureState(feature_state_t state)
{
#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY
	analog_audio_feature_state = state;
#else
	UNUSED(state);
#endif
}

bool WiredAudioSource_StartFeature(void)
{
#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY
	return FeatureManager_StartFeatureRequest(analog_audio_feature_manager_handle);
#else
	return TRUE;
#endif
}

void WiredAudioSource_StopFeature(void)
{
#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY
	FeatureManager_StopFeatureIndication(analog_audio_feature_manager_handle);
#endif
}

/*! \brief Send wired audio devices status message to registered clients */
void WiredAudioSource_SendStatusMessage(bool connected, audio_source_t src)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();

    if(TaskList_Size(sp->client_tasks) == 0)
        return;

    DEBUG_LOG("wiredAudioDetect_SendStatusMessage() Connected = %d, Devices = %d", connected,sp->wired_devices_mask);

    if(connected)
    {
       /* Send the present status to registered clients */
       MESSAGE_MAKE(ind, WIRED_AUDIO_DEVICE_CONNECT_IND_T);
       /* Send the present device status to registered clients */
       ind->audio_source = src;
       TaskList_MessageSend(sp->client_tasks, WIRED_AUDIO_DEVICE_CONNECT_IND, ind);
    }
    else
    {
       /* Send the present status to registered clients */
       MESSAGE_MAKE(ind, WIRED_AUDIO_DEVICE_DISCONNECT_IND_T);
       /* Send the present device status to registered clients */
       ind->audio_source = src;
       TaskList_MessageSend(sp->client_tasks, WIRED_AUDIO_DEVICE_DISCONNECT_IND, ind);
    }
}

/*! utility function to set the wired audio configuration */
static void wiredAudioSource_SetConfiguration(const wired_audio_config_t *config)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    sp->rate = config->rate;
    sp->min_latency = config->min_latency;
    sp->max_latency = config->max_latency;
    sp->target_latency = config->target_latency;
}

/*! \brief Increment wired audio volume */
void WiredAudioSource_IncrementVolume(audio_source_t source)
{
    if(WiredAudioSource_IsAudioAvailable(source))
       Volume_SendAudioSourceVolumeIncrementRequest(source, event_origin_local);
}

/*! \brief Decrement wired audio volume */
void WiredAudioSource_DecrementVolume(audio_source_t source)
{
    if(WiredAudioSource_IsAudioAvailable(source))
       Volume_SendAudioSourceVolumeDecrementRequest(source, event_origin_local);
}

/*! \brief provides wired audio current context for the given audio source

    \param[in]  void

    \return     current context of wired audio module for the given audio source.
*/
unsigned WiredAudioSource_GetContext(audio_source_t source)
{
    audio_source_provider_context_t context = BAD_CONTEXT;

    if (WiredAudioSource_IsAudioAvailable(source))
        context = context_audio_is_playing;
    else
        context = context_audio_disconnected;

    return (unsigned)context;
}

/*! \brief Initialisation routine for Wired audio source detect module. */
void WiredAudioSource_Init(const wired_audio_pio_t * source_pio)
{
    wired_audio_config_t default_config = {
        .rate = 48000,
        .min_latency = 10,
        .max_latency = 40,
        .target_latency = 30
    };

    device_t device = BtDevice_GetSelfDevice();

    if(device)
    {
        if(!Device_IsPropertySet(device, device_property_analog_audio_volume))
        {
            DEBUG_LOG_WARN("WiredAudioSource_Init: SET device_property_analog_audio_volume");
            Device_SetPropertyU8(device, device_property_analog_audio_volume, WIRED_AUDIO_DEFAULT_VOLUME);
            DeviceDbSerialiser_Serialise();
        }
    }
    else
    {
        /* If self device is not yet created, default analogue audio volume will be set once self device is created */
        BtDevice_RegisterListener(WiredAudioSourceGetTask());
    }

    if(source_pio == NULL)
        Panic();

    /* Register Audio source Interfaces */
    AudioSources_RegisterAudioInterface(audio_source_line_in, WiredAudioSource_GetWiredAudioInterface());
    AudioSources_RegisterVolume(audio_source_line_in, WiredAudioSource_GetWiredVolumeInterface());
    AudioSources_RegisterMediaControlInterface(audio_source_line_in, WiredAudioSource_GetMediaControlInterface());

    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    sp->client_tasks = TaskList_Create();

    WiredAudioDetect_ResetMask();
    wiredAudioSource_SetConfiguration(&default_config);

    WiredAudioDetect_ConfigurePio(source_pio);
#ifdef ENABLE_WIRED_AUDIO_FEATURE_PRIORITY
    analog_audio_feature_manager_handle = FeatureManager_Register(feature_id_wired_analog_audio, &analog_audio_feature_manager_if);
#endif
}

void WiredAudioSource_Configure(const wired_audio_config_t *config)
{
    if(config == NULL)
        Panic();

    wiredAudioSource_SetConfiguration(config);
}

/*! \brief Registers the client task to send wired audio source notifications later.*/
void WiredAudioSource_ClientRegister(Task client_task)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();
    TaskList_AddTask(sp->client_tasks, client_task);
}

/*! \brief Unregister the client task to stop sending wired audio source notifications.

    If no clients registered, stop scanning the PIO's for monitoring.
*/
void WiredAudioSource_ClientUnregister(Task client_task)
{
    wiredAudioSourceTaskData *sp = WiredAudioSourceGetTaskData();

    PanicZero(TaskList_Size(sp->client_tasks));

    TaskList_RemoveTask(sp->client_tasks, client_task);
}

/*! \brief Checks whether a wired audio source is available or not.

    Returns TRUE if wired audio source is available.
*/
bool WiredAudioSource_IsAudioAvailable(audio_source_t source)
{
    switch(source)
    {
        case audio_source_line_in:
           return WiredAudioDetect_IsSet(WIRED_AUDIO_SOURCE_LINE_IN);

        default:
           break;
    }
    return FALSE;
}

void WiredAudioSource_UpdateClient(void)
{
    bool connected = FALSE;

    connected = WiredAudioDetect_IsSet(WIRED_AUDIO_SOURCE_LINE_IN);
    WiredAudioSource_SendStatusMessage(connected, audio_source_line_in);

    /* Add other sources if req */

}

/*! \brief start monitoring wired audio detection */
bool WiredAudioSource_StartMonitoring(Task requesting_task)
{
    UNUSED(requesting_task);
    return WiredAudioDetect_StartMonitoring();
}

/*! \brief stop monitoring wired audio detection */
bool WiredAudioSource_StopMonitoring(Task requesting_task)
{
    UNUSED(requesting_task);
    return WiredAudioDetect_StopMonitoring();
}

#endif /* INCLUDE_WIRED_ANALOG_AUDIO*/
