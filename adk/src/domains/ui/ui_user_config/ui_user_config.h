/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      User-defined touchpad gesture to UI Input mapping configuration header file.
*/
#ifndef UI_GESTURE_CONFIG_H_
#define UI_GESTURE_CONFIG_H_

#include <csrtypes.h>

#include "ui_inputs.h"

/*! \brief  Type defining the touchpad gesture identifiers, exposed to the end-user via the
            mobile application. These allow the end-user to reconfigure certain gestures to
            perform their chosen action, rather than the application's default UI input for
            the specified gesture.

    \warning The values assigned to the symbolic identifiers of this type MUST NOT be changed,
             new gestures may be appended to the list, the list MUST NOT exceed 128 gestures.

    \note   The above constraints apply because the type is used by a GAIA plug-in for
            communication with the mobile application, where the command payloads are of
            fixed width, designed to hold 7 bit values.
*/
typedef enum
{
    ui_gesture_tap                          = 0,
    ui_gesture_swipe_up                     = 1,
    ui_gesture_swipe_down                   = 2,
    ui_gesture_tap_and_swipe_up             = 3,
    ui_gesture_tap_and_swipe_down           = 4,
    ui_gesture_double_tap                   = 5,
    ui_gesture_long_press                   = 6,
    ui_gesture_press_and_hold               = 7,

    ui_gesture_end_sentinel                 = 128
} ui_user_config_gesture_id_t;

/*! \brief  Type defining the originating touchpad for a gesture

    \warning This type definition MUST NOT be modified
*/
typedef enum
{
    touchpad_single                         = 0,
    touchpad_right                          = 1,
    touchpad_left                           = 2,
    touchpad_left_and_right                 = 3

} ui_user_config_touchpad_t;

/*! \brief  Type defining context identifiers, exposed to the end-user via the mobile
            application. These context IDs allow the end-user to multiplex several actions
            onto a single gesture, dependent on the device state.


    \warning The values assigned to the symbolic identifiers of this type MUST NOT be changed,
             new gestures may be appended to the list, the list MUST NOT exceed 128 gestures.

    \note   The above constraints apply because the type is used by a GAIA plug-in for
            communication with the mobile application, where the command payloads are of
            fixed width, designed to hold 7 bit values.
*/
typedef enum
{
    ui_context_passthrough                  = 0,
    ui_context_media_streaming              = 1,
    ui_context_media_idle                   = 2,
    ui_context_voice_in_call                = 3,
    ui_context_voice_incoming               = 4,
    ui_context_voice_outgoing               = 5,
    ui_context_voice_held_call              = 6,
    ui_context_handset_disconnected         = 7,
    ui_context_handset_connected            = 8,

    ui_context_end_sentinel                 = 128
} ui_user_config_context_id_t;

/*! \brief  Type defining action identifiers, exposed to the end-user via the mobile
            application. These allow the end-user to reconfigure certain gestures to
            perform their chosen action, rather than the application's default UI input for
            the specified gesture.


    \warning The values assigned to the symbolic identifiers of this type MUST NOT be changed,
             new gestures may be appended to the list, the list MUST NOT exceed 128 gestures.

    \note   The above constraints apply because the type is used by a GAIA plug-in for
            communication with the mobile application, where the command payloads are of
            fixed width, designed to hold 7 bit values.
*/
typedef enum
{
    /* ui_action_first_value is used only to find the search space of legal Action Ids. */
    ui_action_first_value                   = 0,

    ui_action_media_play_pause_toggle       = 0,
    ui_action_media_stop                    = 1,
    ui_action_media_next_track              = 2,
    ui_action_media_previous_track          = 3,
    ui_action_media_seek_forward            = 4,
    ui_action_media_seek_backward           = 5,
    ui_action_voice_accept_call             = 6,
    ui_action_voice_reject_call             = 7,
    ui_action_voice_hang_up_call            = 8,
    ui_action_voice_transfer_call           = 9,
    ui_action_voice_call_cycle              = 10,
    ui_action_voice_join_calls              = 11,
    ui_action_voice_mic_mute_toggle         = 12,
    ui_action_gaming_mode_toggle            = 13,
    ui_action_anc_enable_toggle             = 14,
    ui_action_anc_next_mode                 = 15,
    ui_action_volume_up                     = 16,
    ui_action_volume_down                   = 17,
    ui_action_reconnect_mru_handset         = 18,
    ui_action_va_privacy_toggle             = 19,
    ui_action_va_fetch_query                = 20,
    ui_action_va_ptt                        = 21,
    ui_action_va_cancel                     = 22,
    ui_action_va_fetch                      = 23,
    ui_action_va_query                      = 24,
    ui_action_disconnect_lru_handset        = 25,
    ui_action_voice_join_calls_hang_up      = 26,

    /* ui_action_last_value is used only to find the search space of legal Action Ids. */
    ui_action_last_value,

    ui_action_end_sentinel                  = 128
} ui_user_config_action_id_t;

/*! \brief User defined mapping for gestures to UI Inputs table row instance structure */
typedef struct
{
    ui_user_config_gesture_id_t gesture_id;
    ui_user_config_touchpad_t   originating_touchpad;
    ui_user_config_context_id_t context_id;
    ui_user_config_action_id_t  action_id;

} ui_user_gesture_table_content_t;

typedef struct
{
    ui_user_config_context_id_t context_id;
    uint8 context;
} ui_user_config_context_id_map_t;

typedef struct
{
    ui_user_config_gesture_id_t gesture_id;
    uint16 logical_input;
} ui_user_config_gesture_id_map_t;

typedef struct
{
    ui_user_config_action_id_t action_id;
    ui_user_config_gesture_id_t gesture_id;
    uint16 logical_input;
    ui_input_t ui_input;
} ui_user_config_composite_gesture_t;

typedef enum
{
    get_supported_gestures_bit_array,
    get_supported_contexts_bit_array,
    get_supported_actions_bit_array
} ui_user_config_get_supported_bit_array_t;

/*! \brief Initialise the UI User Config component

    Called during Application start up to initialise the UI User Config component.

    \return TRUE indicating success
*/
bool UiUserConfig_Init(Task init_task);

/*! \brief Register the UI User Config component with the Device Database Serialiser

    Called early in the Application start up to register the UI User Config component with the DBS.
*/
void UiUserConfig_RegisterPddu(void);

/*! \brief Register a Context ID to context mapping for a specific UI Provider.

    This API is called by the various UI Providers in the system that may interact with
    the End-User reconfiguration of the touchpad UI feature.

    A UI Provider that provides state (the context information) for a certain Context ID
    used by the GAIA mobile application, needs to register itself so that the UI domain
    can check this state when the configured gesture is performed on the touchpad. If the
    current context of this UI provider matches that of the configuration for the gesture,
    the End-Users action shall be performed.

    \warning The Context ID to context mapping passed is used directly by this module, it
             is not copied. It is expected that it be located in a const linker section.

    \param provider - The UI Provider which is registering its get context ID mapping.
    \param map - The Context ID to context map for this provider.
    \param map_length - The length of the map
*/
void UiUserConfig_RegisterContextIdMap(
        ui_providers_t provider,
        const ui_user_config_context_id_map_t * map,
        uint8 map_length);

/*! \brief Register a Gesture ID to Logical Input mapping.

    This API is expected to be called by the Application to configure the mapping betwen the
    GAIA Gesture IDs and the specific Logical Inputs used by the Application.

    \warning The Gesture ID to Logical Input mapping passed is used directly by this module, it
             is not copied. It is expected that it be located in a const linker section.

    \param map - The Gesture ID to Logical Input map.
    \param map_length - The length of the map
*/
void UiUserConfig_RegisterGestureIdMap(
        const ui_user_config_gesture_id_map_t *map,
        uint8 map_length);

/*! \brief Register a Composite Gesture ID to Logical Input mapping.

    This API is expected to be called by the Application to configure the mapping between a
    GAIA Gesture IDs and the specific Logical Inputs it is compounded from.

    \warning The Composite Gesture data passed is used directly by this module, it
             is not copied. It is expected that it be located in a const linker section.

    \param map - The breakdown of the composite gesture into individual logical inputs and the associated UI Inputs.
    \param map_length - The length of the map
*/
void UiUserConfig_RegisterCompositeGestureMap(
        const ui_user_config_composite_gesture_t *map,
        uint8 map_length);

/*! \brief Set an End-User Gesture Configuration table.
*/
void UiUserConfig_SetUserGestureConfiguration(
        ui_user_gesture_table_content_t * table,
        size_t size);

/*! \brief Look-up a Logical Input against the RAM End-User Gesture Configuration, and
           return the resulting UI Input, if any.

    This API is called to determine if the End User has re-assigned the action associated with
    this Logical Input (gesture on the touchpad). If the Logical Input doesn't correspond to a
    Gesture ID, the function will return FALSE.

    The UI Input passed as a pointer will only be updated if the function returns TRUE.

    \param logical_input - The Logical Input to check against the configuration table.
    \param is_right_device - If TRUE, the Logical Input originated on the touchpad of the right
                             device, if FALSE it originated on the touchpad of the left device,
                             or on a single device, e,g, Headset.
    \param ui_input - pointer to the UI Input variable to update with the generated UI Input
    \return TRUE if the Logical Input caused a UI Input to be generated, FALSE if not.
*/
bool UiUserConfig_GetUiInput(
        unsigned logical_input,
        bool is_right_device,
        ui_input_t * ui_input);

/*! \brief Get either the supported gestures, contexts or actions configured with the UI domain
           for this application build.

    This API is called to determine the supported properties of the UI User Configuration
    feature. It is expected to be called by the GAIA plug-in to respond to a connected smartphone
    App that is trying to discover the device's capability

    \param type - The type of the supported property to get (either gestures, contexts or
                  actions).
    \param supported_bit_array - a 128 bit array, populated by this function, where the bit
                                 index indicates the value of the supported property (specified
                                 by the type parameter)
*/
void UiUserConfig_GetSupportedBitArray(
        ui_user_config_get_supported_bit_array_t type,
        uint8 supported_bit_array[16]);

/*! \brief Get the current configuration, in GAIA plug-in format, for the specified Gesture.

    This API is called to determine the current UI configuration for the specified gesture.

    It abstracts from whether the configuration was held in the Self Device as a RAM table
    or is actually from the const defaults for the application. This function shall allocate
    memory to pass the table back to the caller.

    \param gesture_id - The Gesture the caller is requesting configuration for.
    \param table - A pointer to a table which represents the configuration for this gesture,
                   in a format suitable for direct transimssion by the GAIA plug-in.
    \param size - The size of the table.

    \note This function shall allocate the memory that holds the table. It is up to the caller
          to free this allocation when it is no longer needed.

    \note If there is no UI configuration for the specified gesture, the table shall be NULL
          and the size zero. No memory shall be allocated, in this case.
*/
void UiUserConfig_GetConfigurationForGesture(
        ui_user_config_gesture_id_t gesture_id,
        ui_user_gesture_table_content_t ** table,
        size_t * size);

/*! \brief Set the user configuration, in GAIA plug-in format, for the specified Gesture.

    This API is called to write the user's selected UI configuration for the specified gesture.

    \param gesture_id - The Gesture the caller is setting the configuration for.
    \param write_offset - the record index at which to write the configuration data. This is
                          needed for packets which needed to be fragemented over the air
                          interface.
    \param table - A table which contains the configuration to write for this gesture.
    \param size - The size of the table.
*/
void UiUserConfig_SetConfigurationForGesture(
        ui_user_config_gesture_id_t gesture_id,
        uint8 write_offset,
        ui_user_gesture_table_content_t * table,
        size_t size);

/*! \brief Remove and delete the current User configuration

    Delete all the User configuration for this feature and restore the UI domain to using
    the const configuration defaults passed in from the Application.

    \note This API will not succeed if the Application is peer-enabled and the peer link is
          not connected at the time of the call, unless the reset is to be performed only
          localy.

    \note This API will fail if no user configuration exists.

    \param local_only - TRUE if the reset should only performed on this device, FALSE if it
                        may be forwarded to a peer device, in peer-enabled topologys.
    \return TRUE if the configuration was reset, FALSE if it wasn't (see note).
*/
bool UiUserConfig_DeleteUserConfiguration(bool local_only);

#endif /* UI_GESTURE_CONFIG_H_ */
