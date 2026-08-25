/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      User-defined touchpad gesture to UI Input mapping configuration.
*/

#include "ui.h"
#include "ui_protected.h"
#include "ui_user_config.h"
#include "ui_user_config_context_id_to_ui_provider_map.h"
#include "ui_user_config_device_property.h"
#include "ui_user_config_empty_list.h"
#include "ui_user_config_gaia_plugin.h"
#include "ui_user_config_pddu.h"

#include <bt_device.h>
#include <device_list.h>
#include <device_db_serialiser.h>
#include <device_properties.h>
#include <device_types.h>
#include <logging.h>
#include <panic.h>
#include <pddu_map.h>
#include <peer_signalling.h>

static const ui_user_config_gesture_id_map_t * gesture_id_map = NULL;
static uint8 gesture_id_map_length = 0;

static const ui_user_config_composite_gesture_t * composite_gesture_map = NULL;
static uint8 composite_gesture_map_length = 0;

static void uiUserConfig_SetBitArrayAtIndex(uint8 bit_array[16], uint8 index)
{
    PanicNull(bit_array);

    if (index < MIN(ui_gesture_end_sentinel, ui_action_end_sentinel))
    {
        bit_array[index/8] |= (0x1 << index%8);
    }
}

static void uiUserConfig_ClearBitArrayAtIndex(uint8 bit_array[16], uint8 index)
{
    PanicNull(bit_array);

    if (index < MIN(ui_gesture_end_sentinel, ui_action_end_sentinel))
    {
        bit_array[index/8] &= ~(0x1 << index%8);
    }
}

static void uiUserConfig_GetSupportedGesturesBitArray(uint8 bit_array[16])
{
    /* For each registered gesture, set the bit at the corresponding index. */
    for (uint8 gesture_index = 0; gesture_index < gesture_id_map_length; gesture_index++)
    {
        uint8 curr_gesture_id = gesture_id_map[gesture_index].gesture_id;
        uiUserConfig_SetBitArrayAtIndex(bit_array, curr_gesture_id);
    }
}

static void uiUserConfig_GetSupportedActionsBitArray(uint8 bit_array[16])
{
    /* Set a bit for all the actions present in the build. */
    for (uint8 action_index = ui_action_first_value; action_index < (ui_action_last_value-1); action_index++)
    {
        uiUserConfig_SetBitArrayAtIndex(bit_array, action_index);
    }

    /* Clear the bits for all VA functions but fetch/query, as they are untested. */
    uiUserConfig_ClearBitArrayAtIndex(bit_array, ui_action_va_privacy_toggle);
    uiUserConfig_ClearBitArrayAtIndex(bit_array, ui_action_va_ptt);
    uiUserConfig_ClearBitArrayAtIndex(bit_array, ui_action_va_cancel);
    uiUserConfig_ClearBitArrayAtIndex(bit_array, ui_action_va_fetch);
    uiUserConfig_ClearBitArrayAtIndex(bit_array, ui_action_va_query);
}

static bool uiUserConfig_GetGestureId(unsigned logical_input, ui_user_config_gesture_id_t * gesture_id)
{
    bool gesture_id_found_in_map = FALSE;
    int i=0;

    for (i=0; i < gesture_id_map_length; i++)
    {
        if (gesture_id_map[i].logical_input == logical_input)
        {
            *gesture_id = gesture_id_map[i].gesture_id;
            gesture_id_found_in_map = TRUE;
            break;
        }
    }

    DEBUG_LOG_VERBOSE("uiUserConfig_GetGestureId found=%d enum:ui_user_config_gesture_id_t:%d",
                      gesture_id_found_in_map, *gesture_id);

    return gesture_id_found_in_map;
}

static ui_input_t uiUserConfig_ConvertActionIdToUiInput(ui_user_config_action_id_t action_id)
{
    switch (action_id)
    {
    case ui_action_media_play_pause_toggle:
        return ui_input_toggle_play_pause;

    case ui_action_media_stop:
        return ui_input_stop_av_connection;

    case ui_action_media_next_track:
        return ui_input_av_forward;

    case ui_action_media_previous_track:
        return ui_input_av_backward;

    case ui_action_media_seek_forward:
        return ui_input_av_fast_forward_start;

    case ui_action_media_seek_backward:
        return ui_input_av_rewind_start;

    case ui_action_voice_accept_call:
        return ui_input_voice_call_accept;

    case ui_action_voice_reject_call:
        return ui_input_voice_call_reject;

    case ui_action_voice_hang_up_call:
        return ui_input_voice_call_hang_up;

    case ui_action_voice_transfer_call:
        return ui_input_voice_transfer;

    case ui_action_voice_call_cycle:
        return ui_input_voice_call_cycle;

    case ui_action_voice_join_calls:
        return ui_input_voice_call_join_calls;

    case ui_action_voice_mic_mute_toggle:
        return ui_input_mic_mute_toggle;

    case ui_action_gaming_mode_toggle:
        return ui_input_gaming_mode_toggle;

    case ui_action_anc_enable_toggle:
        return ui_input_anc_toggle_on_off;

    case ui_action_anc_next_mode:
        return ui_input_anc_toggle_way;

    case ui_action_volume_up:
        return ui_input_volume_up;

    case ui_action_volume_down:
        return ui_input_volume_down;

    case ui_action_reconnect_mru_handset:
        return ui_input_connect_handset;

    case ui_action_disconnect_lru_handset:
        return ui_input_disconnect_lru_handset;

    case ui_action_voice_join_calls_hang_up:
        return ui_input_voice_call_join_calls_and_hang_up;

        /* Not currently implemented for UI reconfiguration -  start
    case ui_input_anc_on:
    case ui_input_anc_off:
    case ui_input_anc_set_mode_1:
    case ui_input_anc_set_mode_2:
    case ui_input_anc_set_mode_3:
    case ui_input_anc_set_mode_4:
    case ui_input_anc_set_mode_5:
    case ui_input_anc_set_mode_6:
    case ui_input_anc_set_mode_7:
    case ui_input_anc_set_mode_8:
    case ui_input_anc_set_mode_9:
    case ui_input_anc_set_mode_10:
    case ui_input_anc_set_world_vol_up:
    case ui_input_anc_set_world_vol_down:
    case ui_input_anc_wind_detected:
    case ui_input_anc_wind_released:
    case ui_input_anc_wind_enable:
    case ui_input_anc_wind_disable:
    case ui_input_anc_anti_howling_enable:
    case ui_input_anc_anti_howling_disable:
    case ui_input_anc_enter_tuning_mode:
    case ui_input_anc_exit_tuning_mode:
    case ui_input_anc_enter_adaptive_anc_tuning_mode:
    case ui_input_anc_exit_adaptive_anc_tuning_mode:
    case ui_input_anc_set_leakthrough_gain:
    case ui_input_anc_adaptivity_toggle_on_off:
    case ui_input_anc_toggle_diagnostic:
    case ui_input_leakthrough_on:
    case ui_input_leakthrough_off:
    case ui_input_leakthrough_toggle_on_off:
    case ui_input_leakthrough_set_mode_1:
    case ui_input_leakthrough_set_mode_2:
    case ui_input_leakthrough_set_mode_3:
    case ui_input_leakthrough_set_next_mode:
    case ui_input_fit_test_prepare_test:
    case ui_input_fit_test_start:
    case ui_input_fit_test_remote_result_ready:
    case ui_input_fit_test_abort:
    case ui_input_fit_test_disable:
    case ui_input_fit_test_enter_tuning_mode:
    case ui_input_fit_test_exit_tuning_mode:
    case ui_input_le_audio_disable_anc:
    case ui_input_le_audio_enable_anc:
         Not currently implemented for UI reconfiguration - end */

    default:
        return ui_input_invalid;
    }
}

static ui_user_config_action_id_t uiUserConfig_LookupActionId(ui_input_t ui_input)
{
    switch (ui_input)
    {
    case ui_input_toggle_play_pause:
        return ui_action_media_play_pause_toggle;

    case ui_input_stop_av_connection:
        return ui_action_media_stop;

    case ui_input_av_forward:
        return ui_action_media_next_track;

    case ui_input_av_backward:
        return ui_action_media_previous_track;

    case ui_input_av_fast_forward_start:
        return ui_action_media_seek_forward;

    case ui_input_av_rewind_start:
        return ui_action_media_seek_backward;

    case ui_input_voice_call_accept:
        return ui_action_voice_accept_call;

    case ui_input_voice_call_reject:
        return ui_action_voice_reject_call;

    case ui_input_voice_call_hang_up:
        return ui_action_voice_hang_up_call;

    case ui_input_voice_transfer:
        return ui_action_voice_transfer_call;

    case ui_input_voice_call_cycle:
        return ui_action_voice_call_cycle;

    case ui_input_voice_call_join_calls:
        return ui_action_voice_join_calls;

    case ui_input_mic_mute_toggle:
        return ui_action_voice_mic_mute_toggle;

    case ui_input_gaming_mode_toggle:
        return ui_action_gaming_mode_toggle;

    case ui_input_anc_toggle_way:
        return ui_action_anc_next_mode;

    case ui_input_anc_toggle_on_off:
        return ui_action_anc_enable_toggle;

    case ui_input_volume_up:
    case ui_input_volume_up_start:
        return ui_action_volume_up;

    case ui_input_volume_down:
    case ui_input_volume_down_start:
        return ui_action_volume_down;

    case ui_input_connect_handset:
        return ui_action_reconnect_mru_handset;

    case ui_input_disconnect_lru_handset:
        return ui_action_disconnect_lru_handset;

    case ui_input_voice_call_join_calls_and_hang_up:
        return ui_action_voice_join_calls_hang_up;

    case ui_input_va_1:
    case ui_input_va_5:
    case ui_input_va_6:
        return ui_action_va_fetch_query;

    default:
        return ui_action_end_sentinel;
    }
}

static bool uiUserConfig_MatchesOriginatingTouchpad(ui_user_config_touchpad_t touchpad, bool is_right_device)
{
    if (touchpad == touchpad_single ||
        touchpad == touchpad_left_and_right ||
        (( is_right_device && touchpad == touchpad_right) ||
         (!is_right_device && touchpad == touchpad_left)))
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static bool uiUserConfig_MatchesContextId(ui_user_config_context_id_t context_id)
{
    bool context_id_matched = FALSE;

    if (context_id == ui_context_passthrough)
    {
        context_id_matched = TRUE;
    }
    else
    {
        ui_providers_t provider;
        uint8 * context_array = NULL;
        uint8 num_contexts;

        /* Panic, because if a Context ID is used in the RAM table for which a UI Provider
           and provider context map hasn't been registered, this reflects a misconfiguration
           during Development. */
        PanicFalse(UiUserConfig_LookUpUiProviderAndContexts(context_id, &provider, &context_array, &num_contexts));

        unsigned curr_context = Ui_GetUiProviderContext(provider);

        for (int i=0; i < num_contexts; i++)
        {
            if (curr_context == context_array[i])
            {
                context_id_matched = TRUE;
            }
        }

        free(context_array);
    }

    return context_id_matched;
}

static bool uiUserConfig_CheckForActionIdInCompositeGestureTable(
        ui_user_config_action_id_t action,
        unsigned logical_input,
        ui_input_t * ui_input)
{
    /* If the action id is in the gesture table, then raise the ui_input that corresponds
       to the logical input, if any, otherwise raise ui_input_invalid. */
    bool action_id_is_in_composite_gesture_table = FALSE;

    for (int i=0; i < composite_gesture_map_length; i++)
    {
        if (composite_gesture_map[i].action_id == action)
        {
            action_id_is_in_composite_gesture_table = TRUE;

            if (composite_gesture_map[i].logical_input == logical_input)
            {
                *ui_input = composite_gesture_map[i].ui_input;
                break;
            }
        }
    }
    return action_id_is_in_composite_gesture_table;
}

static void uiUserConfig_RaiseConfiguredUiInputOnHandRelease(
        ui_user_config_action_id_t action,
        unsigned logical_input,
        ui_input_t * ui_input)
{
    bool logical_input_found_in_map = FALSE;
    int i=gesture_id_map_length-1;
    unsigned logical_input_terminating_press_and_hold = 0;

    /* For composite gestures (i.e. VA press and hold) match the last logical input, which shall be the hand release. */
    for (; i >= 0; i--)
    {
        if (gesture_id_map[i].gesture_id == ui_gesture_press_and_hold)
        {
            logical_input_terminating_press_and_hold = gesture_id_map[i].logical_input;
            logical_input_found_in_map = TRUE;
            break;
        }
    }
    if (logical_input_found_in_map && logical_input_terminating_press_and_hold == logical_input)
    {
        /* Action the terminating logical input of the compost gesture. */
        *ui_input = uiUserConfig_ConvertActionIdToUiInput(action);
    }
    else
    {
        /* Ignore all other logical inputs */
        *ui_input = ui_input_invalid;
    }
}

static bool uiUserConfig_HandlePressAndHoldGesture(
        ui_user_gesture_table_content_t * table,
        size_t table_size,
        unsigned logical_input,
        bool is_right_device,
        ui_input_t * ui_input)
{
    bool gesture_id_present_in_RAM_table = FALSE;
    uint8 index = 0;
    uint8 table_length = table_size / sizeof(ui_user_gesture_table_content_t);

    for (index = 0; index < table_length; index++)
    {
        ui_user_gesture_table_content_t table_entry = table[index];

        if (table_entry.gesture_id == ui_gesture_press_and_hold)
        {
            /* If the Gesture is present in the RAM table, then that means the RAM table supercedes the
               const configuration for this gesture, so set a flag to report this to the caller. */
            gesture_id_present_in_RAM_table = TRUE;

            if (uiUserConfig_MatchesOriginatingTouchpad(table_entry.originating_touchpad, is_right_device) &&
                uiUserConfig_MatchesContextId(table_entry.context_id))
            {
                /* Some events, such as VA PTT, Seek and Volume Ramps require special handling, this is encoded into
                   the composite gesture table passed from the application. */
                bool action_id_is_in_composite_gesture_table = uiUserConfig_CheckForActionIdInCompositeGestureTable(
                            table_entry.action_id,
                            logical_input,
                            ui_input);

                if (!action_id_is_in_composite_gesture_table)
                {
                    /* If the action id is not contained in the composite gesture table, then raise the configured
                       ui_input on the trailing edge of the extended press. This represents a kind of default case,
                       for when we do not have special behaviour coded for the event type. */
                    uiUserConfig_RaiseConfiguredUiInputOnHandRelease(
                                table_entry.action_id,
                                logical_input,
                                ui_input);
                }
            }
        }
    }
    return gesture_id_present_in_RAM_table;
}

static bool uiUserConfig_LookUpUiInput(
        ui_user_gesture_table_content_t * table,
        size_t table_size,
        ui_user_config_gesture_id_t gesture_id,
        bool is_right_device,
        ui_input_t * ui_input)
{
    bool gesture_id_present_in_RAM_table = FALSE;
    uint8 index = 0;
    uint8 table_length = table_size / sizeof(ui_user_gesture_table_content_t);

    for (index = 0; index < table_length; index++)
    {
        ui_user_gesture_table_content_t table_entry = table[index];

        if (table_entry.gesture_id == gesture_id)
        {
            /* If the Gesture is present in the RAM table, then that means the RAM table supercedes the
               const configuration for this gesture, so set a flag to report this to the caller. */
            gesture_id_present_in_RAM_table = TRUE;

            if (uiUserConfig_MatchesOriginatingTouchpad(table_entry.originating_touchpad, is_right_device) &&
                uiUserConfig_MatchesContextId(table_entry.context_id))
            {
                *ui_input = uiUserConfig_ConvertActionIdToUiInput(table_entry.action_id);

                /* The first matching RAM table entry causes the UI Input to be raised. */
                break;
            }
        }
    }

    return gesture_id_present_in_RAM_table;
}

static bool uiUserConfig_GetSubTableForGesture(
        ui_user_config_gesture_id_t gesture_id,
        ui_user_gesture_table_content_t ** table,
        size_t * size)
{
    bool gesture_id_present_in_RAM_table = FALSE;
    uint8 table_length = *size / sizeof(ui_user_gesture_table_content_t);

    size_t length_gesture_data = 0;
    ui_user_gesture_table_content_t * start_requested_gesture = NULL;

    for (int index = 0; index < table_length; index++)
    {
        ui_user_gesture_table_content_t * table_entry = &((*table)[index]);

        if (table_entry->gesture_id == gesture_id)
        {
            if (!gesture_id_present_in_RAM_table)
            {
                /* Found first entry relating to the requested gesture ID. */
                gesture_id_present_in_RAM_table = TRUE;
                start_requested_gesture = table_entry;
                length_gesture_data = sizeof(ui_user_gesture_table_content_t);
            }
            else
            {
                length_gesture_data += sizeof(ui_user_gesture_table_content_t);
            }
        }
        else
        {
            if (gesture_id_present_in_RAM_table)
            {
                /* The Gestures must be entered in the table as contiguous blocks, so we can stop
                   looking if we found another gesture. */
                break;
            }
        }
    }

    if (gesture_id_present_in_RAM_table)
    {
        ui_user_gesture_table_content_t * ptr = (ui_user_gesture_table_content_t *)PanicUnlessMalloc(length_gesture_data);
        memcpy(ptr, start_requested_gesture, length_gesture_data);
        *table = ptr;
        *size = length_gesture_data;
    }
    else
    {
        *table = NULL;
        *size = 0;
    }

    return gesture_id_present_in_RAM_table;
}

static bool uiUserConfig_GetLogicalInputFromGestureId(
        ui_user_config_gesture_id_t gesture_id,
        uint16 * logical_input)
{
    bool logical_input_found_in_map = FALSE;
    int i=0;

    /* Basic case is for non-composite gesture. For composite gestures (i.e. VA press and hold)
       it is enough to match the first logical input, which shall be the hand cover. */
    for (i=0; i < gesture_id_map_length; i++)
    {
        if (gesture_id_map[i].gesture_id == gesture_id)
        {
            *logical_input = gesture_id_map[i].logical_input;
            logical_input_found_in_map = TRUE;
            break;
        }
    }
    return logical_input_found_in_map;
}

static bool uiUserConfig_GetSubTableForGestureFromConstConfig(
        ui_user_config_gesture_id_t gesture_id,
        ui_user_gesture_table_content_t ** table,
        size_t * size)
{
    bool gesture_id_present_in_const_table = FALSE;
    uint16 logical_input = 0x0;

    if (!uiUserConfig_GetLogicalInputFromGestureId(gesture_id, &logical_input))
    {
        /* If the logical input is not present in the Gesture Id map, then return an empty table. */
        DEBUG_LOG_VERBOSE("uiUserConfig_GetSubTableForGestureFromConstConfig no gesture %d", gesture_id);
        *table = NULL;
        *size = 0;
        return gesture_id_present_in_const_table;
    }

    /* Search the const config table for entries for this logical input. */
    unsigned length_in_elements = 0;
    unsigned count = 0;
    const ui_config_table_content_t *const_table = Ui_GetConstUiConfigTable(&length_in_elements);

    /* first pass to get number of elements for this gesture in the const table*/
    for (int i=0; i < length_in_elements; i++)
    {
        if (const_table[i].logical_input == logical_input)
        {
            count++;
        }
    }

    /* second pass to build the sub table for the Gesture ID and parse it into the GAIA format. */
    if (count != 0)
    {
        gesture_id_present_in_const_table = TRUE;

        ui_user_gesture_table_content_t * gaia_form_table = (ui_user_gesture_table_content_t *)PanicUnlessMalloc(count*sizeof(ui_user_gesture_table_content_t));
        ui_user_gesture_table_content_t * write_ptr = gaia_form_table;
        unsigned write_index = 0;

        for (int i=0; i < length_in_elements; i++)
        {
            if (const_table[i].logical_input == logical_input)
            {
                /* Convert and assign the data into GAIA format */
                write_ptr[write_index].gesture_id = gesture_id;
                write_ptr[write_index].originating_touchpad = touchpad_left_and_right;
                write_ptr[write_index].context_id = UiUserConfig_LookupContextId(
                            const_table[i].ui_provider_id,
                            const_table[i].ui_provider_context);
                write_ptr[write_index].action_id = uiUserConfig_LookupActionId(const_table[i].ui_input);
                write_index++;
            }
        }

        *table = gaia_form_table;
        *size = count*sizeof(ui_user_gesture_table_content_t);
    }
    else
    {
        DEBUG_LOG_VERBOSE("uiUserConfig_GetSubTableForGestureFromConstConfig gesture %d not present", gesture_id);

        /* If the logical input is not present in the Config Table, then return an empty table. */
        *table = NULL;
        *size = 0;
    }

    return gesture_id_present_in_const_table;
}

static bool uiUserConfig_IsGesturePresentInTable(
        ui_user_config_gesture_id_t gesture_id,
        ui_user_gesture_table_content_t * table,
        size_t table_size,
        uint8 *index)
{
    bool gesture_id_present_in_RAM_table = FALSE;

    PanicNull(index);

    uint8 table_length = table_size / sizeof(ui_user_gesture_table_content_t);
    for (*index = 0; *index < table_length; (*index)++)
    {
        ui_user_gesture_table_content_t table_entry = table[*index];

        if (table_entry.gesture_id == gesture_id)
        {
            gesture_id_present_in_RAM_table = TRUE;
            break;
        }
    }
    return gesture_id_present_in_RAM_table;
}

static bool uiUserConfig_PeerContextDoesntPreventDeletion(void)
{
    /* A reset can be performed if we aren't in peer-enabled topology, or; if we are,
       and the peer link is connected/ */
    bool is_peer_enabled = Ui_IsPeerEnabled();
    return !is_peer_enabled || (is_peer_enabled && appPeerSigIsConnected());
}

/******************************************************************************
 * External API functions
 ******************************************************************************/
bool UiUserConfig_Init(Task init_task)
{
    UNUSED(init_task);

    return TRUE;
}

void UiUserConfig_RegisterPddu(void)
{
    UiUserConfig_RegisterPdduInternal();
}

void UiUserConfig_RegisterGestureIdMap(
        const ui_user_config_gesture_id_map_t *map,
        uint8 map_length)
{
    PanicNull((ui_user_config_context_id_map_t *)map);
    PanicFalse(map_length != 0);

    gesture_id_map = map;
    gesture_id_map_length = map_length;
}

void UiUserConfig_RegisterCompositeGestureMap(
        const ui_user_config_composite_gesture_t *map,
        uint8 map_length)
{
    PanicNull((ui_user_config_composite_gesture_t *)map);
    PanicFalse(map_length != 0);

    composite_gesture_map = map;
    composite_gesture_map_length = map_length;
}

void UiUserConfig_RegisterContextIdMap(
        ui_providers_t provider,
        const ui_user_config_context_id_map_t * map,
        uint8 map_length)
{
    PanicFalse(provider < ui_providers_max);
    PanicNull((ui_user_config_context_id_map_t *)map);
    PanicFalse(map_length != 0);

    UiUserConfig_AddProviderMap(provider, map, map_length);
}

void UiUserConfig_SetUserGestureConfiguration(ui_user_gesture_table_content_t * table, size_t size)
{
    UiUserConfig_SetUserTable(table, size);
}

void UiUserConfig_GetConfigurationForGesture(
        ui_user_config_gesture_id_t gesture_id,
        ui_user_gesture_table_content_t ** table,
        size_t * size)
{
    bool ram_table_exists = FALSE;
    bool is_gesture_present_in_table = FALSE;
    bool is_gesture_on_empty_list = FALSE;
    device_t device = BtDevice_GetSelfDevice();

    if (device != NULL)
    {
        ram_table_exists = UiUserConfig_GetUserTable(table, size);

        if (ram_table_exists)
        {
            uint8 index = 0;
            is_gesture_present_in_table = uiUserConfig_IsGesturePresentInTable(
                        gesture_id, *table, *size, &index);

            if (is_gesture_present_in_table)
            {
                /* Look up requested gesture ID and return to caller. */
                uiUserConfig_GetSubTableForGesture(gesture_id, table, size);
            }
        }

        if (!is_gesture_present_in_table)
        {
            is_gesture_on_empty_list = UiUserConfig_IsOnEmptyList(device, gesture_id);
        }
    }

    if (is_gesture_on_empty_list)
    {
        /* Gesture is overridden by user and has empty configuration, clear the table. */
        *table = NULL;
        *size = 0;
    }
    else if (!is_gesture_present_in_table)
    {
        /* Convert sub table of const UI configuration table to GAIA form and return that to caller. */
        uiUserConfig_GetSubTableForGestureFromConstConfig(gesture_id, table, size);
    }
    else
    {
        /* Not on empty list and present in table. The table is valid entry. Do nothing here. */
    }

    DEBUG_LOG_VERBOSE("UiUserConfig_GetConfigurationForGesture enum:ui_user_config_gesture_id_t:%d"
                      " ram?=%d present=%d empty=%d table=%p, size=%d)", gesture_id,
                      ram_table_exists, is_gesture_present_in_table, is_gesture_on_empty_list, *table, *size);
}

void UiUserConfig_SetConfigurationForGesture(
        ui_user_config_gesture_id_t gesture_id,
        uint8 write_offset,
        ui_user_gesture_table_content_t * gesture_sub_table,
        size_t gesture_sub_table_size)
{
    bool ram_table_exists = FALSE;
    device_t device = BtDevice_GetSelfDevice();

    if (device != NULL)
    {
        DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture enum:ui_user_config_gesture_id_t:%d"
                    " write_offset=%d gesture_sub_table=%p gesture_sub_table_size=%d",
                    gesture_id,
                    write_offset,
                    gesture_sub_table,
                    gesture_sub_table_size);

        if (gesture_sub_table_size == 0)
        {
            DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture adding gesture to empty list");
            /* Indicates an empty gesture. */
            UiUserConfig_AddGestureToEmptyList(device, gesture_id);
        }
        else
        {
            DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture removing gesture from empty list");
            /* The gesture can no longer be empty, if we have recieved non-zero length configuration for it. */
            UiUserConfig_RemoveGestureFromEmptyList(device, gesture_id);
        }

        ui_user_gesture_table_content_t * existing_table = NULL;
        size_t existing_size = 0;
        ram_table_exists = UiUserConfig_GetUserTable(
                    &existing_table,
                    &existing_size);

        if (ram_table_exists)
        {
            /* Read-modify-write approach to updating the UI configuration. */
            /* Merged table will be written to the Self device. */
            ui_user_gesture_table_content_t * merged_table = NULL;
            size_t merged_table_size = 0;
            int write_index = 0;

            uint8 existing_table_length = existing_size / sizeof(ui_user_gesture_table_content_t);
            uint8 start_index_in_existing_table = 0;

            bool is_gesture_present = uiUserConfig_IsGesturePresentInTable(
                        gesture_id,
                        existing_table,
                        existing_size,
                        &start_index_in_existing_table);

            DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture"
                        " enum:ui_user_config_gesture_id_t:%d present=%d existing_table_length=%d"
                        " start_index_in_existing_table=%d",
                        gesture_id,
                        is_gesture_present,
                        existing_table_length,
                        start_index_in_existing_table);

            if (is_gesture_present)
            {
                /* Determine if there are any records to remove. */
                uint8 num_records_to_remove = 0;
                uint8 i = start_index_in_existing_table + write_offset;
                for (; i < existing_table_length; i++)
                {
                    if (existing_table[i].gesture_id == gesture_id )
                    {
                        num_records_to_remove += 1;
                    }
                }

                DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture num_records_to_remove=%d",
                                  num_records_to_remove);

                /* Allocate memory for the new, merged Configuration data table. */
                size_t to_remove = num_records_to_remove * sizeof(ui_user_gesture_table_content_t);
                merged_table_size = existing_size - to_remove + gesture_sub_table_size;
                merged_table = (ui_user_gesture_table_content_t *) PanicUnlessMalloc(merged_table_size);

                /* Do the merge.. */
                /* First pass, copy all the other gestures into the merged table. */
                for (i = 0; i < existing_table_length; i++)
                {
                    if (existing_table[i].gesture_id != gesture_id)
                    {
                        merged_table[write_index++] = existing_table[i];
                    }
                }

                /* Second pass, Copy the existing config for the gesture to the end of the array,
                   up to the write index count. */
                if (write_offset != 0)
                {
                    uint8 copy_count = 0;
                    for (i = 0; i < existing_table_length; i++)
                    {
                        if (existing_table[i].gesture_id == gesture_id && copy_count < write_offset)
                        {
                            merged_table[write_index++] = existing_table[i];
                            copy_count += 1;
                        }
                    }
                }

                /* Complete the merge by appending the new gesture configuration at the end of the merged
                   table, i.e. after the write index. */

                DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture write_index=%d appending @%p",
                            write_index, &merged_table[write_index]);

                memcpy(&merged_table[write_index], gesture_sub_table, gesture_sub_table_size);
            }
            else
            {
                PanicFalse(write_offset==0);

                /* Append the new gesture configuration at the end of a resized table. */
                if (gesture_sub_table_size != 0)
                {
                    merged_table_size = existing_size + gesture_sub_table_size;
                    merged_table = (ui_user_gesture_table_content_t *) PanicUnlessMalloc(merged_table_size);

                    memcpy(merged_table, existing_table, existing_size);

                    DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture existing_table_length=%d appending @%p",
                                existing_table_length, &merged_table[existing_table_length]);

                    memcpy(&merged_table[existing_table_length], gesture_sub_table, gesture_sub_table_size);
                }
                else
                {
                    /* When the gesture is not present in the table, and the user is overriding the const
                       configuration (i.e. writing an empty gesture configuration), then we have already
                       entered the gesture onto the empty list. The table is not altered. */
                    merged_table_size = 0;
                    merged_table = NULL;
                }
            }

            DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture merged_table_size=%d merged_table=%p",
                        merged_table_size, merged_table);

            UiUserConfig_SetUserTable(
                        merged_table,
                        merged_table_size);

            if (merged_table != NULL)
            {
                free(merged_table);
            }
        }
        else
        {
            PanicFalse(write_offset==0);

            if (gesture_sub_table_size != 0)
            {
                DEBUG_LOG_INFO("UiUserConfig_SetConfigurationForGesture creating table with"
                          " enum:ui_user_config_gesture_id_t:%d", gesture_id);

                /* Create new RAM table from scratch with just the configration for this gesture. */
                UiUserConfig_SetUserTable(gesture_sub_table, gesture_sub_table_size);
            }
            else
            {
                /* The user is overriding a const configuration (i.e. writing an empty gesture
                   configuration), and the RAM config table doesn't exist, we have already
                   entered the gesture onto the empty list. */
            }
        }

        DeviceDbSerialiser_SerialiseDevice(device);
    }
}

bool UiUserConfig_GetUiInput(unsigned logical_input, bool is_right_device, ui_input_t * ui_input)
{
    bool logical_input_has_user_config = FALSE;
    ui_user_gesture_table_content_t * user_config_table = NULL;
    size_t user_config_table_size = 0;
    ui_user_config_gesture_id_t gesture_id = ui_gesture_end_sentinel;
    bool is_on_empty_list = FALSE;

    device_t device = BtDevice_GetSelfDevice();

    bool is_logical_input_a_gesture = uiUserConfig_GetGestureId(logical_input, &gesture_id);
    if (!is_logical_input_a_gesture)
    {
        return FALSE;
    }

    if (device != NULL)
    {
        UiUserConfig_GetUserTable(&user_config_table, &user_config_table_size);
        is_on_empty_list = UiUserConfig_IsOnEmptyList(device, gesture_id);
    }

    if (is_on_empty_list)
    {
        *ui_input = ui_input_invalid;

        DEBUG_LOG_ALWAYS("UiUserConfig_GetUiInput enum:ui_user_config_gesture_id_t:%d is on empty list.",
                          gesture_id);
    }
    else if (user_config_table != NULL)
    {
        if (gesture_id == ui_gesture_press_and_hold)
        {
            /* Press and hold is a special case, it is a composite button press. It can be configured to
               either the left or the right touchpad. We have different UI Inputs to raise based on the
               button_down, held_1_sec, button_up checkpoints, using a preset mapping provided by the App. */
            logical_input_has_user_config = uiUserConfig_HandlePressAndHoldGesture(
                        (ui_user_gesture_table_content_t *)user_config_table,
                        user_config_table_size,
                        logical_input,
                        is_right_device,
                        ui_input);
        }
        else
        {
            /* Check the end-user gesture configuration. Note, the const configuration is now overridden
               by the end-users configuration and shall not be checked, even if a ui_input is not raised. */
            logical_input_has_user_config = uiUserConfig_LookUpUiInput(
                        (ui_user_gesture_table_content_t *)user_config_table,
                        user_config_table_size,
                        gesture_id,
                        is_right_device,
                        ui_input);
        }

        if (logical_input_has_user_config)
        {
            DEBUG_LOG_ALWAYS("UiUserConfig_GetUiInput enum:ui_user_config_gesture_id_t:%d enum:ui_input_t:%d",
                             gesture_id, *ui_input);
        }
    }
    else
    {
        /* If not on empty list and there is no config user table, then ignore. */
    }

    return logical_input_has_user_config || is_on_empty_list;
}

void UiUserConfig_GetSupportedBitArray(
        ui_user_config_get_supported_bit_array_t type,
        uint8 supported_bit_array[16])
{
    PanicFalse(supported_bit_array);

    memset(supported_bit_array, 0, sizeof(uint8)*16);

    switch(type)
    {
    case get_supported_gestures_bit_array:
        uiUserConfig_GetSupportedGesturesBitArray(supported_bit_array);
        break;
    case get_supported_contexts_bit_array:
        UiUserConfig_GetSupportedContextsBitArray(supported_bit_array);
        break;
    case get_supported_actions_bit_array:
        uiUserConfig_GetSupportedActionsBitArray(supported_bit_array);
        break;
    default:
        Panic();
        break;
    }
}

bool UiUserConfig_DeleteUserConfiguration(bool local_only)
{
    bool is_configuration_reset = FALSE;
    device_t device = BtDevice_GetSelfDevice();

    DEBUG_LOG_INFO("UiUserConfig_DeleteUserConfiguration local_only=%d", local_only);

    if (device != NULL)
    {
        if (local_only || uiUserConfig_PeerContextDoesntPreventDeletion())
        {
            if (!local_only)
            {
                /* Forward the indication to the Peer if in a peer-enabled topology. */
                Ui_RaiseUiEvent(ui_indication_type_reset_user_config, 0, 0);
            }

            Device_RemoveProperty(device, device_property_ui_user_gesture);

            DeviceDbSerialiser_SerialiseDevice(device);

            is_configuration_reset = TRUE;
        }
        else
        {
            DEBUG_LOG_ERROR("UiUserConfig_DeleteUserConfiguration Peer Link not connected. ");
        }
    }

    return is_configuration_reset;
}
