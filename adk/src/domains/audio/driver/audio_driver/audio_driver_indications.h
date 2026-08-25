/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      APIs related to sending indications

*/

#ifndef AUDIO_DRIVER_INDICATIONS_H_
#define AUDIO_DRIVER_INDICATIONS_H_

#include <audio_use_case_types.h>

void AudioDriver_PreparingInd(audio_use_case_instance_t instance);
void AudioDriver_PreparedInd(audio_use_case_instance_t instance);

void AudioDriver_StartingInd(audio_use_case_instance_t instance);
void AudioDriver_StartedInd(audio_use_case_instance_t instance);

void AudioDriver_StoppingInd(audio_use_case_instance_t instance);
void AudioDriver_StoppedInd(audio_use_case_instance_t instance);

void AudioDriver_BeginIoctlInd(audio_use_case_instance_t instance, unsigned ioctl_id);
void AudioDriver_EndIoctlInd(audio_use_case_instance_t instance, unsigned ioctl_id);

#endif /* AUDIO_DRIVER_INDICATIONS_H_ */
