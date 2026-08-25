/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private Header file for USB Audio class 1.0 driver
*/

#ifndef USB_AUDIO_CLASS_10_H_
#define USB_AUDIO_CLASS_10_H_


#include "usb_audio_class.h"

#define USB_AUDIO_PACKET_RATE_HZ                    1000

/* Round up _value to the nearest _multiple. */
#define ROUNDUP(_value, _multiple) (((_value) + (_multiple) - 1) / (_multiple))

#define UAC_MAX_PACKET_SIZE(sample_rate_hz, channels, sample_size)         \
    (ROUNDUP(sample_rate_hz, USB_AUDIO_PACKET_RATE_HZ) *   \
    (sample_size) * (channels))

/*! \brief Interface to get function pointers for class driver.
    
    \return Return function table for usb_audio_class_10 driver interface .
*/
usb_fn_tbl_uac_if *UsbAudioClass10_GetFnTbl(void);


#endif /* USB_AUDIO_CLASS_10_H_ */
