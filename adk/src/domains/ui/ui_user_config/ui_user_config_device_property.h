/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Header for the Device Property API implementation of the UI User
            Gesture configuration feature.
*/
#ifndef UI_USER_CONFIG_DEVICE_PROPERTY_H
#define UI_USER_CONFIG_DEVICE_PROPERTY_H

#include <csrtypes.h>
#include <ui_user_config.h>

/*! \brief Get the user gesture configuration table from the device property.

    \param table - A pointer to a table which represents the configuration for this gesture,
                   in a format suitable for direct transimssion by the GAIA plug-in.
    \param size - The size of the table.
    \return a bool indicating whether the user table exists or not in the device property

    \note If there is no UI configuration table, the table shall be NULL and the size zero.
*/
bool UiUserConfig_GetUserTable(ui_user_gesture_table_content_t ** table, size_t * size);

/*! \brief Set the user gesture configuration table in the device property.

    \param table - A table which contains the configuration to write for this gesture.
    \param size - The size of the table.
*/
void UiUserConfig_SetUserTable(ui_user_gesture_table_content_t * table, size_t size);

/*! \brief Get the list of 'empty' gestures in the device property. These are the gestures for
           which the user has overridden the const configuration, but for which no actions
           are configured.

    \param list - A pointer to a list which contains the gesture ids that are empty.
    \param size - The size of the table.
    \return a bool indicating whether the user table exists or not in the device property

    \note If there is no empty list, the list shall be NULL and the size zero.
*/
bool UiUserConfig_GetEmptyList(ui_user_config_gesture_id_t ** table, size_t * size);

/*! \brief Set the list of 'empty' gestures in the device property. These are the gestures for
           which the user has overridden the const configuration, but for which no actions
           are configured.

    \param list - The list which contains the gesture ids that are empty of configuration.
    \param size - The size of the list.
*/
void UiUserConfig_SetEmptyList(ui_user_config_gesture_id_t * table, size_t size);

#endif // UI_USER_CONFIG_DEVICE_PROPERTY_H
