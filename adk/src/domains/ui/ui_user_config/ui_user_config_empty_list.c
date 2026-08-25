/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Functionality to maintain the empty gesture configuration list.
*/

#include "ui.h"
#include "ui_protected.h"

#include "ui_user_config_empty_list.h"
#include "ui_user_config_device_property.h"
#include "ui_user_config_gaia_plugin.h"

#include <device_properties.h>
#include <logging.h>
#include <panic.h>

bool UiUserConfig_IsOnEmptyList(device_t device, ui_user_config_gesture_id_t gesture_id)
{
    ui_user_config_gesture_id_t *empty_list = NULL;
    size_t empty_list_size = 0;
    bool is_on_empty_list = FALSE;

    PanicNull(device);

    if (UiUserConfig_GetEmptyList(&empty_list, &empty_list_size))
    {
        uint8 list_length = empty_list_size / sizeof(ui_user_config_gesture_id_t);

        for (int index = 0; index < list_length; index++)
        {
            if (empty_list[index] == gesture_id)
            {
                is_on_empty_list = TRUE;
                break;
            }
        }
    }
    return is_on_empty_list;
}

void UiUserConfig_AddGestureToEmptyList(device_t device, ui_user_config_gesture_id_t gesture_id)
{
    ui_user_config_gesture_id_t *empty_list = NULL;
    size_t empty_list_size = 0;

    PanicNull(device);

    if (UiUserConfig_GetEmptyList(&empty_list, &empty_list_size))
    {
        if (!UiUserConfig_IsOnEmptyList(device, gesture_id))
        {
            /* Assign a new copy with the gesture added. */
            uint8 updated_list_size = empty_list_size + sizeof(ui_user_config_gesture_id_t);
            ui_user_config_gesture_id_t * updated_list = PanicUnlessMalloc(updated_list_size);
            uint8 list_length = empty_list_size / sizeof(ui_user_config_gesture_id_t);
            uint8 write_index = 0;

            for (int index = 0; index < list_length; index++)
            {
                updated_list[write_index++] = empty_list[index];
            }
            updated_list[write_index++] = gesture_id;

            UiUserConfig_SetEmptyList(updated_list, updated_list_size);

            DEBUG_LOG_INFO("uiUserConfig_AddGestureToEmptyList added enum:ui_user_config_gesture_id_t:%d", gesture_id);

            free(updated_list);
        }
        else
        {
            DEBUG_LOG_INFO("uiUserConfig_AddGestureToEmptyList enum:ui_user_config_gesture_id_t:%d already on list", gesture_id);
            Panic();
        }
    }
    else
    {
        /* Create the empty list with just this gesture on it. */
        UiUserConfig_SetEmptyList((void *)&gesture_id, sizeof(ui_user_config_gesture_id_t));

        DEBUG_LOG_INFO("uiUserConfig_AddGestureToEmptyList created with enum:ui_user_config_gesture_id_t:%d", gesture_id);
    }
}

void UiUserConfig_RemoveGestureFromEmptyList(device_t device, ui_user_config_gesture_id_t gesture_id)
{
    ui_user_config_gesture_id_t *empty_list = NULL;
    size_t empty_list_size = 0;

    PanicNull(device);

    DEBUG_LOG_INFO("UiUserConfig_RemoveGestureFromEmptyList enum:ui_user_config_gesture_id_t:%d", gesture_id);

    if (UiUserConfig_GetEmptyList(&empty_list, &empty_list_size))
    {
        DEBUG_LOG_INFO("UiUserConfig_RemoveGestureFromEmptyList empty_list=%p, size=%d", empty_list, empty_list_size);

        if (UiUserConfig_IsOnEmptyList(device, gesture_id))
        {
            if (empty_list_size > sizeof(ui_user_config_gesture_id_t))
            {
                /* Need to remove the gesture and adjust the stored list. */
                uint8 updated_list_size = empty_list_size - sizeof(ui_user_config_gesture_id_t);
                ui_user_config_gesture_id_t * updated_list = PanicUnlessMalloc(updated_list_size);

                uint8 list_length = empty_list_size / sizeof(ui_user_config_gesture_id_t);
                uint8 write_index = 0;

                DEBUG_LOG_INFO("UiUserConfig_RemoveGestureFromEmptyList updating empty list as list_length=%d",
                               list_length);

                bool found = FALSE;
                for (int index = 0; index < list_length; index++)
                {
                    if (empty_list[index] != gesture_id)
                    {
                        updated_list[write_index++] = empty_list[index];
                    }
                    else
                    {
                        if (found)
                        {
                            DEBUG_LOG_INFO("UiUserConfig_RemoveGestureFromEmptyList gesture found again at index=%d",
                                           index);
                            Panic();
                        }

                        found=TRUE;

                        DEBUG_LOG_INFO("UiUserConfig_RemoveGestureFromEmptyList gesture found at index=%d",
                                       index);
                    }
                }

                UiUserConfig_SetEmptyList(updated_list, updated_list_size);

                free(updated_list);
            }
            else
            {
                DEBUG_LOG_INFO("UiUserConfig_RemoveGestureFromEmptyList deleting empty list as only one gesture present");

                /* Can simply remove the device property. */
                UiUserConfig_SetEmptyList(NULL, 0);
            }
        }
        else
        {
            DEBUG_LOG_INFO("UiUserConfig_RemoveGestureFromEmptyList wasn't on the list, nothing to remove");
        }
    }
}
