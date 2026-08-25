/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      .
*/
#ifndef UI_USER_CONFIG_GAIA_PLUGIN_H_
#define UI_USER_CONFIG_GAIA_PLUGIN_H_

#include "ui_user_config.h"

#include <gaia_features.h>

/*! \brief UI User Configuration GAIA plug-in version.
*/
#define UI_USER_CONFIG_GAIA_PLUGIN_VERSION (1)

/*! \brief UI User Configuration GAIA plug-in commands.
*/
typedef enum
{
    /*! Get Number of Touchpads Command */
    get_number_of_touchpads = 0,
    /*! Get Supported Gestures Command */
    get_supported_gestures,
    /*! Get Supported Contexts Command */
    get_supported_contexts,
    /*! Get Supported Actions Command */
    get_supported_actions,
    /*! Get Configuration For Gesture Command */
    get_configuration_for_gesture,
    /*! Set Configuration For Gesture Command */
    set_configuration_for_gesture,
    /*! Reset Configuration To Defaults Command */
    reset_configuration_to_defaults,

    /*! Total number of commands */
    number_of_ui_user_config_commands
} ui_user_config_plugin_pdu_ids_t;


/*! \brief UI User Configuration GAIA plug-in notifications.
*/
typedef enum
{
    /*! Gesture Configuration Changed Notification */
    gesture_configuration_changed_notification = 0,
    /*! Total number of notifications */
    configuration_reset_to_defaults_notification,

    /*! Total number of notifications */
    number_of_ui_user_config_notifications
} ui_user_config_plugin_notifications_t;


/*! \brief UI User Configuration GAIA plug-in Error Status Codes.
    \note  Valid range is 128 to 255 for the Feature.
           (Values 0~127 are reserved for Gaia Framework.)
*/
typedef enum
{
    /*! 'Reset to Defaults' command failed because the Peer Link is not connected. */
    ui_user_config_plugin_peer_link_down_failure             = 0x80,

} ui_user_config_plugin_status_code_t;

/*! \brief 'Get Number Of Touchpads' response parameter positions. */
typedef enum
{
    ui_user_config_plugin_get_number_of_touchpads            = 0,

    number_of_ui_user_config_plugin_get_number_of_touchpads_rsp_bytes,
} ui_user_config_plugin_get_num_touchpads_rsp_t;

/*! \brief 'Get Configuration For Gesture' command parameter positions. */
typedef enum
{
    ui_user_config_plugin_get_config_cmd_gesture_id              = 0,
    /* The Gesture ID for which the Mobile application is querying the configuration */
    ui_user_config_plugin_get_config_cmd_read_offset,
    /* The record offset to read the configuration data from */

    number_of_ui_user_config_plugin_get_config_cmd_bytes,
} ui_user_config_plugin_get_config_cmd_t;

/*! \brief 'Set Configuration For Gesture' command parameter positions. */
typedef enum
{
    ui_user_config_plugin_set_config_cmd_gesture_id              = 0,
    /* The Gesture ID for which the Mobile application is writing the configuration */
    ui_user_config_plugin_set_config_cmd_write_offset,
    /* The record offset to write the configuration data at */

    number_of_ui_user_config_plugin_set_config_cmd_bytes,
} ui_user_config_plugin_set_config_cmd_t;

/*! \brief Initialise the UI User Config GAIA Plug-in.

    This function registers the GAIA command/notification handler of the End-User UI
    configuration feature. The Application that uses this Feature must call this function
    before calling any other functions of the Feature.

    \return TRUE indicating success
*/
bool UiUserConfigGaiaPlugin_Init(Task init_task);

/*! \brief Register a Composite Gesture ID to Logical Input mapping.

    This API is expected to be called by the Application to inform the GAIA plug-in of the
    number of touchpads available to the end-user. Setting 1 means the device has a single
    originating touchpad. Setting 2	could be a device in an Earbud pair or possibly a Headset
    application, where there are discrete Left and Right touchpads.

    \param num_touchpads - The number of touchpads
*/
void UiUserConfigGaiaPlugin_SetNumberOfTouchpads(uint8 num_touchpads);

#endif /* UI_USER_CONFIG_GAIA_PLUGIN_H_ */
