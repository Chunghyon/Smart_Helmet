/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_battery.c
\brief  Implementation of the battery handling for Amazon AVS
*/

#ifdef INCLUDE_AMA

#include <logging.h>
#include "ama_battery.h"
#include "ama_config.h"
#include "charger_monitor.h"
#include "ama_send_command.h"
#include "ama_transport.h"
#include "state_of_charge.h"


static DeviceBattery device_battery = DEVICE_BATTERY__INIT;

#define SELF_DEVICE_ID  0

void AmaBattery_Update(uint8 battery_level)
{
    if (device_battery.scale == 100)
    {
        /* This means that AmaBattery_Init has been called */
        device_battery.level = (uint32_t) battery_level;

        if (battery_level == 100)
        {
            device_battery.status = DEVICE_BATTERY__STATUS__FULL;
        }
        else 
        {
            device_battery.status = Charger_IsCharging() ?
                    DEVICE_BATTERY__STATUS__CHARGING: 
                    DEVICE_BATTERY__STATUS__DISCHARGING;
        }

        if (AmaTransport_IsConnected())
        {
            DeviceInformation device_information = DEVICE_INFORMATION__INIT;

            Ama_PopulateDeviceInformation(&device_information, SELF_DEVICE_ID);
            AmaSendCommand_NotifyDeviceInformation(&device_information);
        }
        else 
        {
            DEBUG_LOG_WARN("AmaBattery_Update AMA not connected");
        }

        DEBUG_LOG_INFO("AmaBattery_Update: level %d, status %d", device_battery.level, device_battery.status);
    }
    else 
    {
        DEBUG_LOG_WARN("AmaBattery_Update not initialised");
    }
}

/***************************************************************************/

bool AmaBattery_Init(void)
{
    DEBUG_LOG("AmaBattery_Init");
    device_battery.scale = 100;
	AmaBattery_Update(Soc_GetBatterySoc());
    return TRUE;
}

DeviceBattery *AmaBattery_GetDeviceBattery(void)
{
    DEBUG_LOG("AmaBattery_GetDeviceBattery");
    return &device_battery;
}


#endif

