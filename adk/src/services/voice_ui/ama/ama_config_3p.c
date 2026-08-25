/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_config_3p.c
\brief  Implementation of AMA configuration APIs for 3P
*/

#include "ama_config.h"
#include "voice_ui_va_client_if.h"

#include <bt_device.h>
#include <device_info.h>
#include <logging.h>
#include <md5.h>
#include <stdio.h>

#define AMA_SURROGATE_SN_SIZE (2 * MD5_DIGEST_SIZE + 1)

uint32 Ama_GetDeviceId(uint32 device_id)
{
    UNUSED(device_id);
    return 0;
}

char * Ama_GetDeviceType(uint32 device_id)
{
    UNUSED(device_id);
    return AMA_CONFIG_DEVICE_TYPE;
}

char * Ama_GetSerialNumber(uint32 device_id)
{
    UNUSED(device_id);
    if(VoiceUi_IsTwsFeatureIncluded())
    {
        /*  Generate surrogate serial number by hashing right-hand device address  */
        static char serial[AMA_SURROGATE_SN_SIZE];

        if (serial[0] == '\0')
        {
            bdaddr rh_addr = {0};
            MD5_CTX md5_context;
            uint8 digest[MD5_DIGEST_SIZE];
            uint16 length;
            uint16 i;
            char *ptr;

            appDeviceGetMyBdAddr(&rh_addr);

            if (rh_addr.lap & 1)
            {
            /* If the device is part of a left-right pair then this has returned the address of the
             * left-hand unit.  Attempt to get the peer address (which will fail if the device is not
             * part of a pair).
             */
                appDeviceGetPeerBdAddr(&rh_addr);
            }

            /* Convert to string for portability */
            length = sprintf(serial, "%04x %02x %06x", rh_addr.nap, rh_addr.uap, (unsigned int)rh_addr.lap);
            DEBUG_LOG("AMA rh_addr %s", serial);

            /* Hash for privacy */
            MD5Init(&md5_context);
            MD5Update(&md5_context, (uint8 *) serial, length);
            MD5Final(digest, &md5_context);

            /* Convert digest to hex string */
            ptr = serial;
            for (i = 0; i < MD5_DIGEST_SIZE; ++i)
            {
                ptr += sprintf(ptr, "%02X", (unsigned)digest[i]);
            }

            DEBUG_LOG("AMA serial %s", serial);
        }

        return serial;
    }
    else
    {
        return (char *) DeviceInfo_GetSerialNumber();
    }
}

size_t Ama_GetNumberAssociatedDevices(void)
{
    return 0;
}

uint32_t* Ama_GetAssociatedDevices(uint32 device_id)
{
    UNUSED(device_id);
    return NULL;
}

void Ama_PopulateDeviceFeatures(DeviceFeatures *device_features)
{
    Ama_PopulateCommonDeviceFeatures(device_features);
    DEBUG_LOG_DEBUG("Ama_PopulateDeviceFeatures: features 0x%08lx", device_features->features);
}
