/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       usb_audio_silence.c
\brief      USB Audio Silence Generator for unrouted sink endpoints.
*/

#include "usb_audio_silence.h"

#ifndef DISABLE_USB_AUDIO_SILENCE

#include "usb_audio.h"
#include "usb_audio_class_10.h"
#include "usb_audio_fd.h"

#include <usb_device_utils.h>
#include <power_manager.h>
#include <logging.h>

/* At the fixed 1 kHz packet rate, 3 packets seems a safe enough number to ensure we never starve the host
 * and also ensure we keep well away from the sink buffer's limits */
#define MAX_QUEUED_PACKETS      3


typedef struct UsbAudioSilenceTaskData
{
    TaskData task;

    /* the current USB audio device receiving a silence stream */
    usb_audio_info_t * usb_audio;

    /* the current parameters of the silence stream */
    uint32 curr_sample_rate;
    uint32 curr_packet_size;
} UsbAudioSilenceTaskData;

static UsbAudioSilenceTaskData usbAudioSilenceTd;

#define UsbAudioSilenceGetTaskData()    (&usbAudioSilenceTd)
#define UsbAudioSilenceGetTask()        (&(usbAudioSilenceTd.task))

static void usbAudioSilence_SendPackets(Sink sink, uint8 num_packets)
{
    uint8 count = 0;

    while (SinkSlack(sink) >= UsbAudioSilenceGetTaskData()->curr_packet_size)
    {
        uint8 *dest = SinkMapClaim(sink, UsbAudioSilenceGetTaskData()->curr_packet_size);
        if (dest == NULL)
        {
            break;
        }

        /* write a hunk of silence/zeroes into the sink */
        memset(dest, 0, UsbAudioSilenceGetTaskData()->curr_packet_size);
        SinkFlush(sink, UsbAudioSilenceGetTaskData()->curr_packet_size);

        if (++count == num_packets)
        {
            break;
        }
    }
}

static void usbAudioSilence_HandleMMS(const MessageMoreSpace * msg)
{
    usb_audio_info_t *usb_audio = UsbAudioSilenceGetTaskData()->usb_audio;

    /* if the source isn't connected or there's already a DSP chain active */
    if (!usb_audio || !usb_audio->headset->source_connected || usb_audio->headset->chain_active)
    {
        return;
    }

    PanicFalse(msg->sink == usb_audio->headset->mic_sink);

    usb_audio_streaming_info_t * streaming_info = UsbAudio_GetStreamingInfo(usb_audio, USB_AUDIO_DEVICE_TYPE_VOICE_MIC);

    PanicNull(streaming_info);

    /* if the sample rate has since changed, update our packet size accordingly */
    if (streaming_info->current_sampling_rate != UsbAudioSilenceGetTaskData()->curr_sample_rate)
    {
        DEBUG_LOG("usbAudioSilence_HandleMMS, sample rate changed");
        UsbAudioSilenceGetTaskData()->curr_sample_rate = streaming_info->current_sampling_rate;
        UsbAudioSilenceGetTaskData()->curr_packet_size = UAC_MAX_PACKET_SIZE(streaming_info->current_sampling_rate, streaming_info->channels, streaming_info->frame_size);
    }

    usbAudioSilence_SendPackets(msg->sink, MAX_QUEUED_PACKETS);
}

static void usbAudioSilence_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case MESSAGE_MORE_SPACE:
            usbAudioSilence_HandleMMS((const MessageMoreSpace *)message);
            break;

        default:
            break;
    }
}

static bool usbAudioSilence_StartStream(usb_audio_info_t * usb_audio)
{
    DEBUG_LOG_FN_ENTRY("UsbAudioSilence_StartStream");

    /* if the source isn't connected or there's already a silence stream or DSP chain active */
    if (!usb_audio || !usb_audio->headset->source_connected || usb_audio->headset->chain_active
            || UsbAudioSilenceGetTaskData()->usb_audio != NULL)
    {
        return FALSE;
    }

    usb_audio_streaming_info_t * streaming_info = UsbAudio_GetStreamingInfo(usb_audio, USB_AUDIO_DEVICE_TYPE_VOICE_MIC);

    PanicNull(streaming_info);

    UsbAudioSilenceGetTaskData()->usb_audio = usb_audio;
    UsbAudioSilenceGetTaskData()->curr_sample_rate = streaming_info->current_sampling_rate;
    UsbAudioSilenceGetTaskData()->curr_packet_size = UAC_MAX_PACKET_SIZE(streaming_info->current_sampling_rate, streaming_info->channels, streaming_info->frame_size);

    /* Request performance profile so we're able to keep up with bidir traffic */
    appPowerPerformanceProfileRequest();

    /* bind the task to the sink to receive MMS messages */
    UsbAudioSilenceGetTaskData()->task.handler = usbAudioSilence_MessageHandler;
    MessageStreamTaskFromSink(usb_audio->headset->mic_sink, UsbAudioSilenceGetTask());
    PanicFalse(SinkConfigure(usb_audio->headset->mic_sink, VM_SINK_MESSAGES, VM_MESSAGES_SOME));

    /* Send off one packet to get things rolling */
    usbAudioSilence_SendPackets(usb_audio->headset->mic_sink, 1);
    DEBUG_LOG_VERBOSE("usbAudioSilence_StartStream success");

    return TRUE;
}

static bool usbAudioSilence_StopStream(usb_audio_info_t * usb_audio)
{
    DEBUG_LOG_FN_ENTRY("UsbAudioSilence_StopStream");

    /* if USB isn't connected or the idle stream is already stopped */
    if (!usb_audio || UsbAudioSilenceGetTaskData()->usb_audio != usb_audio)
    {
        return FALSE;
    }

    UsbAudioSilenceGetTaskData()->usb_audio = NULL;
    UsbAudioSilenceGetTaskData()->curr_sample_rate = 0;
    UsbAudioSilenceGetTaskData()->curr_packet_size = 0;
    MessageStreamTaskFromSink(usb_audio->headset->mic_sink, NULL);

    appPowerPerformanceProfileRelinquish();

    DEBUG_LOG_VERBOSE("UsbAudioSilence_StopStream success");
    return TRUE;
}

bool UsbAudioSilence_SetStreamState(usb_audio_info_t * usb_audio, bool start)
{
    DEBUG_LOG_FN_ENTRY("UsbAudioSilence_SetStreamState(%p), start %u", usb_audio, start);

    if (start)
    {
        return usbAudioSilence_StartStream(usb_audio);
    }
    else
    {
        return usbAudioSilence_StopStream(usb_audio);
    }
}

#endif
