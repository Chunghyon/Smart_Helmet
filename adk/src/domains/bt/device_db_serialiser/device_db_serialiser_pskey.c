/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Low-level access to the PDL data.

*/

#include "device_db_serialiser_pskey.h"

#include <device_properties.h>

#include <device_list.h>
#include <bdaddr.h>
#include <ps.h>
#include <panic.h>
#include <logging.h>
#include <vmtypes.h>


#define BDADDR_KEY_1 142
#define BDADDR_KEY_2 143

#define ATTR_KEY_1 100
#define ATTR_KEY_2 101

inline static uint16 deviceDbSerialiser_GetBdAddrPsKey(device_order_t device_order)
{
    uint16 key = 0;
    switch(device_order)
    {
        case device_order_first:
            key = BDADDR_KEY_1;
            break;

        case device_order_second:
            key = BDADDR_KEY_2;
            break;

        default:
            Panic();
    }

    return key;
}

static bool deviceDbSerialiser_GetAddrFromPsKey(device_order_t device_order, bdaddr *addr)
{
    uint16 key = deviceDbSerialiser_GetBdAddrPsKey(device_order);

    uint16 num_of_words = PsRetrieve(key, NULL, 0);

    DEBUG_LOG_VERBOSE("deviceDbSerialiser_GetAddrFromPsKey key 0x%x, num_of_words %d", key, num_of_words);

    if(num_of_words > 2)
    {
        uint16 *buffer = PanicUnlessMalloc(num_of_words*sizeof(uint16));

        PsRetrieve(key, buffer, num_of_words);

        addr->nap = buffer[0];
        addr->uap = buffer[1] >> 8;
        addr->lap = buffer[2] | (buffer[1] & 0xff) << 16;

        DEBUG_LOG_VERBOSE("deviceDbSerialiser_GetAddrFromPsKey addr %x:%x:%x",
                        addr->nap, addr->uap, addr->lap);

        free(buffer);

        return TRUE;
    }

    return FALSE;
}


device_t DeviceDbSerialiser_GetDeviceFromPsKey(device_order_t device_order)
{
    bdaddr addr = {0};

    if(deviceDbSerialiser_GetAddrFromPsKey(device_order, &addr))
    {
        return DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, &addr, sizeof(addr));
    }

    return 0;
}

uint16 DeviceDbSerialiser_GetAttributePsKey(device_order_t device_order)
{
    uint16 ps_key = 0;

    switch(device_order)
    {
        case device_order_first:
            ps_key = ATTR_KEY_1;
            break;

        case device_order_second:
            ps_key = ATTR_KEY_2;
            break;

        default:
            Panic();
    }

    return ps_key;
}
