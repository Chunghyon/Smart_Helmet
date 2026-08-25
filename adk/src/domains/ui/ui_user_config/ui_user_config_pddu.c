/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      The Persistent Device Data User (PDDU) implemenatation for the UI User
            Gesture feature.
*/
#include "ui_user_config_pddu.h"
#include "ui_user_config.h"

#include <csrtypes.h>
#include <device.h>
#include <device_db_serialiser.h>
#include <device_properties.h>
#include <logging.h>
#include <pddu_map.h>

static uint8 uiUserConfig_GetDeviceDataLen(device_t device)
{
    uint8 size_needed = 0;
    void *user_config_data = NULL;
    size_t user_config_size = 0;

    if (device != NULL)
    {
        bool table_exists = FALSE;

        table_exists = Device_GetProperty(device,
                           device_property_ui_user_gesture,
                           &user_config_data,
                           &user_config_size);

        if (table_exists)
        {
            size_needed = user_config_size;
        }

        DEBUG_LOG_INFO("uiUserConfig_GetDeviceDataLen exists=%d,size=%d",
                       table_exists, user_config_size);
    }
    return size_needed;
}

static void uiUserConfig_SerialisePersistentDeviceData(device_t device, void *buf, uint8 offset)
{
    UNUSED(offset);

    ui_user_gesture_table_content_t *user_config_table = NULL;
    size_t user_config_table_size = 0;
    uint8 * buffer = buf;

    if (Device_GetProperty(device, device_property_ui_user_gesture, (void *)&user_config_table, &user_config_table_size))
    {
        DEBUG_LOG_INFO("uiUserConfig_SerialisePersistentDeviceData serialise table size=%d", user_config_table_size);

        // store the buffer
        memcpy(buffer, user_config_table, user_config_table_size);
    }
}

static void uiUserConfig_DeserialisePersistentDeviceData(device_t device, void *buf, uint8 data_length, uint8 offset)
{
    UNUSED(offset);

    uint8 * buffer = buf;

    DEBUG_LOG_VERBOSE("uiUserConfig_DeserialisePersistentDeviceData data_length=%d", data_length);

    Device_SetProperty(device, device_property_ui_user_gesture, buffer, data_length);
}

void UiUserConfig_RegisterPdduInternal(void)
{
    DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        PDDU_ID_UI_USER_CONFIG,
        uiUserConfig_GetDeviceDataLen,
        uiUserConfig_SerialisePersistentDeviceData,
        uiUserConfig_DeserialisePersistentDeviceData);
}
