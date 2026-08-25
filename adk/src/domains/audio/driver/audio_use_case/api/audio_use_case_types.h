/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_use_case
\ingroup    audio_domain
\brief      List of supported audio use cases.
*/

#ifndef AUDIO_USE_CASE_TYPES_H_
#define AUDIO_USE_CASE_TYPES_H_

/*! @{ */

typedef enum
{
    audio_use_case_none,
    audio_use_case_a2dp,
    audio_use_case_le_audio,
    audio_use_case_le_voice,
    audio_use_case_loopback,
    audio_use_case_usb_audio,
    audio_use_case_usb_voice,
    audio_use_case_all,
} audio_use_case_t;

typedef void* audio_use_case_instance_t;

/*! @} */

#endif /* AUDIO_USE_CASE_TYPES_H_ */
