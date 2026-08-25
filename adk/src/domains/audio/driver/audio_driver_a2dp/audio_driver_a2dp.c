/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Audio Driver A2DP handler implementation

*/

#include "audio_driver_a2dp.h"
#include <audio_use_case_handler.h>
#include <kymera_a2dp.h>
#include <volume_utils.h>
#include <logging.h>

static audio_use_case_request_status_t audioDriverA2dp_Prepare(audio_use_case_instance_t instance, const data_blob_t *params);
static audio_use_case_request_status_t audioDriverA2dp_Start(audio_use_case_instance_t instance, const data_blob_t *params);
static audio_use_case_request_status_t audioDriverA2dp_Stop(audio_use_case_instance_t instance, const data_blob_t *params);
static audio_use_case_request_status_t audioDriverA2dp_Ioctl(audio_use_case_instance_t instance, unsigned ioctl_id, const data_blob_t *params);

static const audio_use_case_handler_if use_case_handler_if =
{
    .Prepare = audioDriverA2dp_Prepare,
    .Start = audioDriverA2dp_Start,
    .Stop = audioDriverA2dp_Stop,
    .Ioctl = audioDriverA2dp_Ioctl,
};

static audio_use_case_request_status_t audioDriverA2dp_Prepare(audio_use_case_instance_t instance, const data_blob_t *params)
{
    UNUSED(instance);
    const audio_driver_a2dp_prepare_t *prepare_params;
    PanicFalse(params->data_length == sizeof(*prepare_params));
    prepare_params = params->data;
    Kymera_A2dpHandlePrepareReq(prepare_params);
    return audio_use_case_request_success;
}

static audio_use_case_request_status_t audioDriverA2dp_Start(audio_use_case_instance_t instance, const data_blob_t *params)
{
    UNUSED(instance);
    const audio_driver_a2dp_start_t *start_params;
    PanicFalse(params->data_length == sizeof(*start_params));
    start_params = params->data;
    Kymera_A2dpHandleStartReq(start_params);
    return audio_use_case_request_success;
}

static audio_use_case_request_status_t audioDriverA2dp_Stop(audio_use_case_instance_t instance, const data_blob_t *params)
{
    UNUSED(instance);
    const audio_driver_a2dp_stop_t *stop_params;
    PanicFalse(params->data_length == sizeof(*stop_params));
    stop_params = params->data;
    Kymera_A2dpHandleInternalStop(stop_params);
    return audio_use_case_request_success;
}

static void audioDriverA2dp_SetVolume(const data_blob_t *params)
{
    const audio_driver_a2dp_set_volume_t *volume_params;
    PanicFalse(params->data_length == sizeof(*volume_params));
    volume_params = params->data;
    Kymera_A2dpHandleInternalSetVolume(VolumeUtils_GetVolumeInDb(*volume_params));
}

static audio_use_case_request_status_t audioDriverA2dp_Ioctl(audio_use_case_instance_t instance, unsigned ioctl_id, const data_blob_t *params)
{
    UNUSED(instance);

    switch(ioctl_id)
    {
        case audio_driver_a2dp_set_volume:
            audioDriverA2dp_SetVolume(params);
            break;
        default:
            DEBUG_LOG_PANIC("audioDriverA2dp_Ioctl: Unknown ID enum:audio_driver_a2dp_ioctl_ids_t:%d", ioctl_id);
            break;
    }

    return audio_use_case_request_success;
}

void AudioDriverA2dp_Init(void)
{
    AudioUseCase_RegisterHandler(audio_use_case_a2dp, &use_case_handler_if);
}
