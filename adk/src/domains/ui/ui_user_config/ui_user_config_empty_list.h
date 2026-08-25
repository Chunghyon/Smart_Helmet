/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      API for adding and removing gestures from the empty list.
*/
#ifndef UI_USER_CONFIG_EMPTY_LIST_H
#define UI_USER_CONFIG_EMPTY_LIST_H

#include <ui_user_config.h>

#include <device.h>

/*! \brief Check to see if the specified gesture is on the empty list.

    This API is called to determine if the specified gesture is on the empty list

    \param device - The device to check the empty list for
    \param gesture_id - The Gesture ID to look for on the empty list
    \return TRUE if the empty list exists for that device and the Gesture Id is on
            the list, FALSE if not.
*/
bool UiUserConfig_IsOnEmptyList(device_t device, ui_user_config_gesture_id_t gesture_id);

/*! \brief Add a gesture to the empty list.

    This API is called to add a gesture to the empty list, for the specified device. If
    the list doesn't exist it will be created.

    \param device - The device for which to modify/create the empty list
    \param gesture_id - The Gesture ID to add to the empty list
*/
void UiUserConfig_AddGestureToEmptyList(device_t device, ui_user_config_gesture_id_t gesture_id);

/*! \brief Remove a gesture from the empty list.

    This API is called to remove a gesture from the empty list, for the specified device. If
    the list becomes empty the device property will be destroyed

    \param device - The device for which to modify/create the empty list
    \param gesture_id - The Gesture ID to add to the empty list
*/
void UiUserConfig_RemoveGestureFromEmptyList(device_t device, ui_user_config_gesture_id_t gesture_id);

#endif // UI_USER_CONFIG_EMPTY_LIST_H
