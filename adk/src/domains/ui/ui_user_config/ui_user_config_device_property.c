/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      The Device Property abstraction implementation for the UI User
            Gesture feature.
*/
#include "ui_user_config_device_property.h"
#include "ui_user_config.h"

#include <csrtypes.h>
#include <bt_device.h>
#include <device.h>
#include <device_properties.h>
#include <logging.h>
#include <panic.h>

typedef bool (* GetDevicePropertyDataHelper_t)(size_t user_table_size, int read_index, uint8 * user_config_data, void ** list, size_t * size);

static bool uiUserConfig_GetList(size_t user_table_size, int read_index, uint8 * user_config_data, void ** list, size_t * size)
{
    bool empty_list_exists = FALSE;

    /* Offset passed the User Table in the device property. */
    if (user_table_size > 0)
    {
        read_index += user_table_size;
    }

    /* Read the empty list size and data from the device property. */
    size_t empty_list_size = user_config_data[read_index++] * sizeof(ui_user_config_gesture_id_t);
    if (empty_list_size > 0)
    {
        empty_list_exists = TRUE;
        *list = (ui_user_config_gesture_id_t *)&(user_config_data[read_index]);
    }
    else
    {
        *list = NULL;
    }
    *size = empty_list_size;

    return empty_list_exists;
}

static bool uiUserConfig_GetTable(size_t user_table_size, int read_index, uint8 * user_config_data, void ** table, size_t * size)
{
    bool user_table_exists = FALSE;

    /* No need to consider the empty list, as this is after the user table in the device property. */
    if (user_table_size > 0)
    {
        user_table_exists = TRUE;
        *table = (ui_user_gesture_table_content_t *)&(user_config_data[read_index]);
    }
    else
    {
        *table = NULL;
    }
    *size = user_table_size;

    return user_table_exists;
}

static bool uiUserConfig_GetDevicePropertyDataHelper(GetDevicePropertyDataHelper_t fn_ptr, void ** data, size_t * size)
{
    bool exists = FALSE;

    device_t device = BtDevice_GetSelfDevice();

    uint8 *user_config_data = NULL;
    size_t user_config_size = 0;

    if (Device_GetProperty(device, device_property_ui_user_gesture, (void *)&user_config_data, &user_config_size))
    {
        PanicNull(user_config_data);

        int read_index = 0;
        size_t user_table_size = user_config_data[read_index++] * sizeof(ui_user_gesture_table_content_t);

        PanicFalse(user_table_size < user_config_size);

        exists = fn_ptr(user_table_size, read_index, user_config_data, data, size);
    }
    else
    {
        *data = NULL;
        *size = 0;
    }

    return exists;
}

bool UiUserConfig_GetUserTable(ui_user_gesture_table_content_t ** table, size_t * size)
{            
    bool user_table_exists = uiUserConfig_GetDevicePropertyDataHelper(uiUserConfig_GetTable, (void **)table, size);

    DEBUG_LOG_INFO("UiUserConfig_GetUserTable table=%p, size=%d", *table, *size);

    return user_table_exists;
}

bool UiUserConfig_GetEmptyList(ui_user_config_gesture_id_t ** list, size_t * size)
{
    bool empty_list_exists = uiUserConfig_GetDevicePropertyDataHelper(uiUserConfig_GetList, (void **)list, size);

    DEBUG_LOG_INFO("UiUserConfig_GetEmptyList list=%p, size=%d", *list, *size);

    return empty_list_exists;
}

void UiUserConfig_SetUserTable(ui_user_gesture_table_content_t * table, size_t size)
{
    device_t device = BtDevice_GetSelfDevice();

    uint8 *user_config_data = NULL;
    size_t user_config_size = 0;

    DEBUG_LOG_INFO("UiUserConfig_SetUserTable table=%p, size=%d", table, size);

    if (Device_GetProperty(device, device_property_ui_user_gesture, (void *)&user_config_data, &user_config_size))
    {
        int read_index = 0;

        /* Offset passed the user table to read the existing empty list size. */
        size_t user_table_size = user_config_data[read_index++] * sizeof(ui_user_gesture_table_content_t);
        read_index += user_table_size;

        /* The existing empty list shall be preserved in the device property. */
        uint8 existing_empty_list_length = user_config_data[read_index++];
        size_t existing_empty_list_size = existing_empty_list_length * sizeof(ui_user_config_gesture_id_t);

        size_t size_required = sizeof(uint8) + size + sizeof(uint8) + existing_empty_list_size;
        uint8 * buffer = (uint8 *)PanicUnlessMalloc(size_required);

        /* Copy the specified table into the updated device property buffer. */
        int write_index = 0;
        buffer[write_index++] = size/sizeof(ui_user_gesture_table_content_t);
        if (size != 0)
        {
            PanicNull(table);
            memcpy(&(buffer[write_index]), table, size);
            write_index += size;
        }

        /* Copy the existing empty list into the updated device property. */
        buffer[write_index++] = existing_empty_list_length;
        if (existing_empty_list_size != 0)
        {
            uint8* existing_empty_list = &(user_config_data[read_index]);
            PanicNull(existing_empty_list);
            memcpy(&(buffer[write_index]), existing_empty_list, existing_empty_list_size);
        }

        /* Update the device property. */
        Device_SetProperty(device, device_property_ui_user_gesture, buffer, size_required);

        free(buffer);
    }
    else
    {
        /* Create the UI User Config device property. */
        uint8 write_index = 0;
        size_t size_required = 2*sizeof(uint8) + size;
        uint8 * buffer = (uint8 *)PanicUnlessMalloc(size_required);

        /* Assign the specified user table into the new device property buffer. */
        buffer[write_index++] = size/sizeof(ui_user_gesture_table_content_t);
        if (size != 0)
        {
            memcpy(&(buffer[write_index]), table, size);
            write_index += size;
        }

        /* There is no empty list data, so the length is zero. */
        buffer[write_index++] = 0;

        Device_SetProperty(device, device_property_ui_user_gesture, buffer, size_required);

        free(buffer);
    }
}

void UiUserConfig_SetEmptyList(ui_user_config_gesture_id_t * list, size_t size)
{
    device_t device = BtDevice_GetSelfDevice();

    uint8 *user_config_data = NULL;
    size_t user_config_size = 0;

    DEBUG_LOG_INFO("UiUserConfig_SetEmptyList list=%p, size=%d", list, size);

    if (Device_GetProperty(device, device_property_ui_user_gesture, (void *)&user_config_data, &user_config_size))
    {
        int read_index = 0;
        uint8 existing_num_user_entries = user_config_data[read_index++];
        size_t existing_user_table_size = existing_num_user_entries * sizeof(ui_user_gesture_table_content_t);

        /* Allocate a new buffer for the updated device property. */
        size_t size_required = sizeof(uint8) + existing_user_table_size + sizeof(uint8) + size;
        uint8 * buffer = (uint8 *)PanicUnlessMalloc(size_required);

        /* Preserve existing the existing user table without change in the device property. */
        int write_index = 0;
        buffer[write_index++] = existing_num_user_entries;
        if (existing_user_table_size != 0)
        {
            uint8* existing_user_table = &(user_config_data[read_index]);
            PanicNull(existing_user_table);
            memcpy(&(buffer[write_index]), existing_user_table, existing_user_table_size);
            write_index += existing_user_table_size;
        }

        /* Merge in the specified empty list */
        buffer[write_index++] = size/sizeof(ui_user_config_gesture_id_t);
        if (size != 0)
        {
            PanicNull(list);
            memcpy(&(buffer[write_index]), list, size);
            write_index += size;
        }

        /* Update the device property. */
        Device_SetProperty(device, device_property_ui_user_gesture, buffer, size_required);

        free(buffer);
    }
    else
    {
        /* Create the UI User Config device property. */
        uint8 write_index = 0;
        size_t size_required = 2*sizeof(uint8) + size;
        uint8 * buffer = (uint8 *)PanicUnlessMalloc(size_required);

        /* No user table exists, so the length is zero. */
        buffer[write_index++] = 0;

        /* Assign the specified empty list into the new device property buffer. */
        buffer[write_index++] = size/sizeof(ui_user_config_gesture_id_t);
        if (size != 0)
        {
            memcpy(&(buffer[write_index]), list, size);
            write_index += size;
        }

        /* Create the device property. */
        Device_SetProperty(device, device_property_ui_user_gesture, buffer, size_required);

        free(buffer);
    }
}
