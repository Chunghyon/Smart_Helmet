/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Provides the command, response, notification GAIA message functionality
            for the UI User Config feature.
*/

#include "ui_user_config_gaia_plugin.h"

#include "gaia_framework_feature.h"
#include "gaia_framework_data_channel.h"

#include <gaia_features.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#define REPSONSE_PAYLOAD_LENGTH                 16
#define GET_CONFIG_FOR_GESTURE_CMD_MAX_ELEMENTS 7

static uint8 ui_num_touchpads = 0;

static size_t uiUserConfigGaiaPlugin_InsertToGaiaPayload(
        ui_user_config_gesture_id_t gesture_id,
        const ui_user_gesture_table_content_t *read_ptr,
        uint16 read_length,
        uint8 *rsp_payload)
{
    uint8 more_data_to_send = 0x0;
    int write_index = 0;

    if (read_length > GET_CONFIG_FOR_GESTURE_CMD_MAX_ELEMENTS)
    {
        more_data_to_send = 0x1 << 7;
    }

    /* Assign the response payload */
    rsp_payload[write_index++] = more_data_to_send | gesture_id;

    for (int i=0; i < MIN(GET_CONFIG_FOR_GESTURE_CMD_MAX_ELEMENTS, read_length); i++)
    {
        rsp_payload[write_index++] = ((read_ptr[i].originating_touchpad & 0x03) << 6) | ((read_ptr[i].context_id >> 1) & 0x3F);
        rsp_payload[write_index++] = ((read_ptr[i].context_id & 0x1) << 7) | (read_ptr[i].action_id & 0x7F);
    }
    return write_index*sizeof(uint8);
}

static void uiUserConfigGaiaPlugin_ExtractFromGaiaPayload(
        ui_user_config_gesture_id_t gesture_id,
        const uint8 *payload,
        uint16 payload_length,
        ui_user_gesture_table_content_t *extracted_data)
{
    int write_index = 0;

    /* Loop two bytes at a time extracting the data received in the GAIA command. */
    for (int i = number_of_ui_user_config_plugin_set_config_cmd_bytes; i < payload_length; i += 2)
    {
        extracted_data[write_index].gesture_id = gesture_id;
        extracted_data[write_index].originating_touchpad = payload[i] >> 6;
        extracted_data[write_index].context_id = (((payload[i] & 0x3f) << 1) | ((payload[i+1] & 0x80) >> 7));
        extracted_data[write_index].action_id = payload[i+1] & 0x7f;

        DEBUG_LOG_VERBOSE(
                  "uiUserConfigGaiaPlugin_ExtractFromGaiaPayload %d enum:ui_user_config_touchpad_t:%d"
                  " enum:ui_user_config_context_id_t:%d enum:ui_user_config_action_id_t:%d",
                  write_index,
                  extracted_data[write_index].originating_touchpad,
                  extracted_data[write_index].context_id,
                  extracted_data[write_index].action_id);

        write_index += 1;
    }
}

static void uiUserConfigGaiaPlugin_GetNumTouchpads(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload);
    UNUSED(payload_length);

    uint8 rsp_payload[number_of_ui_user_config_plugin_get_number_of_touchpads_rsp_bytes];

    DEBUG_LOG_INFO("uiUserConfigGaiaPlugin_GetNumTouchpads %d", ui_num_touchpads);
    rsp_payload[ui_user_config_plugin_get_number_of_touchpads] = ui_num_touchpads;

    GaiaFramework_SendResponse(
                t,
                GAIA_UI_USER_CONFIG_FEATURE_ID,
                get_number_of_touchpads,
                sizeof(rsp_payload),
                rsp_payload);
}

static ui_user_config_get_supported_bit_array_t uiUserConfigGaiaPlugin_GetSupportedBitArrayType(
        ui_user_config_plugin_pdu_ids_t command_id)
{
    switch(command_id)
    {
    case get_supported_gestures:
        return get_supported_gestures_bit_array;

    case get_supported_contexts:
        return get_supported_contexts_bit_array;

    case get_supported_actions:
        return get_supported_actions_bit_array;

    default:
        Panic();
        return 0;
    }
}

static void uiUserConfigGaiaPlugin_GetSupportedCommand(
        GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload, ui_user_config_plugin_pdu_ids_t command_id)
{
    UNUSED(payload);
    UNUSED(payload_length);

    uint8 rsp_payload[REPSONSE_PAYLOAD_LENGTH];
    ui_user_config_get_supported_bit_array_t type = uiUserConfigGaiaPlugin_GetSupportedBitArrayType(command_id);
    UiUserConfig_GetSupportedBitArray(type, rsp_payload);

    DEBUG_LOG_INFO("uiUserConfigGaiaPlugin_GetSupported enum:ui_user_config_get_supported_bit_array_t:%d", type);

    GaiaFramework_SendResponse(
                t,
                GAIA_UI_USER_CONFIG_FEATURE_ID,
                command_id,
                sizeof(rsp_payload),
                rsp_payload);
}

static void uiUserConfigGaiaPlugin_GetConfigForGesture(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool is_successful = FALSE;

    if (payload_length >= number_of_ui_user_config_plugin_get_config_cmd_bytes)
    {
        uint8 rsp_payload[REPSONSE_PAYLOAD_LENGTH];

        /* Read command parameters */
        ui_user_config_gesture_id_t gesture_id =
                (ui_user_config_gesture_id_t) payload[ui_user_config_plugin_get_config_cmd_gesture_id];
        uint8 read_offset = payload[ui_user_config_plugin_get_config_cmd_read_offset];

        /* Get data for that gesture */
        void * table = NULL;
        size_t size = 0;
        UiUserConfig_GetConfigurationForGesture(gesture_id, (ui_user_gesture_table_content_t **)&table, &size);

        size_t rsp_size = 0;
        if (table != NULL)
        {
            uint8 table_length = size / sizeof(ui_user_gesture_table_content_t);
            ui_user_gesture_table_content_t * read_ptr = (ui_user_gesture_table_content_t *)table;

            int8 data_len_to_send_in_this_response = table_length - read_offset;
            if (data_len_to_send_in_this_response >= 0)
            {
                /* Offset to the read address in the data. Note, if the read offset is zero,
                   this shall not change the pointer */
                read_ptr = &(read_ptr[read_offset]);

                rsp_size = uiUserConfigGaiaPlugin_InsertToGaiaPayload(
                            gesture_id,
                            read_ptr,
                            data_len_to_send_in_this_response,
                            rsp_payload);

                is_successful = TRUE;
            }
            else
            {
                DEBUG_LOG_ERROR("uiUserConfigGaiaPlugin_GetConfigForGesture read_offset=%d > len=%d",
                                read_offset, table_length);
            }

            free(table);
        }
        else
        {
            /* There is no configuration present for this gesture. */
            rsp_payload[0] = gesture_id;
            rsp_size = sizeof(uint8);

            is_successful = TRUE;
        }

        if (is_successful)
        {
            GaiaFramework_SendResponse(
                        t,
                        GAIA_UI_USER_CONFIG_FEATURE_ID,
                        get_configuration_for_gesture,
                        rsp_size,
                        rsp_payload);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("uiUserConfigGaiaPlugin_GetConfigForGesture bad PDU Size:%d",
                        payload_length);
    }

    if (!is_successful)
    {
        GaiaFramework_SendError(
                    t,
                    GAIA_UI_USER_CONFIG_FEATURE_ID,
                    get_configuration_for_gesture,
                    invalid_parameter);
    }
}

static void uiUserConfigGaiaPlugin_SetConfigForGesture(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    /* The payload is variable length, but must be greater than two bytes. */
    if (payload_length >= number_of_ui_user_config_plugin_set_config_cmd_bytes)
    {
        /* Read command parameters */
        uint8 more_data = (payload[ui_user_config_plugin_set_config_cmd_gesture_id] & 0x80) >> 7;
        UNUSED(more_data);
        ui_user_config_gesture_id_t gesture_id =
                (ui_user_config_gesture_id_t) (payload[ui_user_config_plugin_set_config_cmd_gesture_id] & 0x7F);
        uint8 write_offset = payload[ui_user_config_plugin_set_config_cmd_write_offset];

        DEBUG_LOG_INFO("uiUserConfigGaiaPlugin_SetConfigForGesture: "
                          "enum:ui_user_config_gesture_id_t:%d PDU Size:%d", gesture_id, payload_length);

        /* Allocate a buffer and extract the configuration data to the internal data representation. */
        uint8 number_of_entries_in_payload = (payload_length - number_of_ui_user_config_plugin_set_config_cmd_bytes)/2;
        size_t extracted_data_size = number_of_entries_in_payload * sizeof(ui_user_gesture_table_content_t);
        ui_user_gesture_table_content_t *extracted_data = (ui_user_gesture_table_content_t *)PanicUnlessMalloc(extracted_data_size);
        uiUserConfigGaiaPlugin_ExtractFromGaiaPayload(
                    gesture_id,
                    payload,
                    payload_length,
                    extracted_data);

        /* Write the extracted configuration data for the gesture to the feature. */
        UiUserConfig_SetConfigurationForGesture(
                    gesture_id,
                    write_offset,
                    extracted_data,
                    extracted_data_size);

        free(extracted_data);

        GaiaFramework_SendResponse(
                    t,
                    GAIA_UI_USER_CONFIG_FEATURE_ID,
                    set_configuration_for_gesture,
                    0,
                    NULL);

        GaiaFramework_SendNotification(
                    GAIA_UI_USER_CONFIG_FEATURE_ID,
                    gesture_configuration_changed_notification,
                    sizeof(uint8),
                    (void *)&gesture_id);
    }
    else
    {
        DEBUG_LOG_ERROR("uiUserConfigGaiaPlugin_SetConfigForGesture: Invalid PDU Size:%d", payload_length);

        GaiaFramework_SendError(
                    t,
                    GAIA_UI_USER_CONFIG_FEATURE_ID,
                    set_configuration_for_gesture,
                    invalid_parameter);
    }
}

static void uiUserConfigGaiaPlugin_ResetToDefaults(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload);
    UNUSED(payload_length);

    if (UiUserConfig_DeleteUserConfiguration(FALSE))
    {
        GaiaFramework_SendResponse(
                    t,
                    GAIA_UI_USER_CONFIG_FEATURE_ID,
                    reset_configuration_to_defaults,
                    0,
                    NULL);

        GaiaFramework_SendNotification(
                    GAIA_UI_USER_CONFIG_FEATURE_ID,
                    configuration_reset_to_defaults_notification,
                    0,
                    NULL);
    }
    else
    {
        GaiaFramework_SendError(
                    t,
                    GAIA_UI_USER_CONFIG_FEATURE_ID,
                    reset_configuration_to_defaults,
                    ui_user_config_plugin_peer_link_down_failure);
    }
}

static gaia_framework_command_status_t uiUserConfigGaiaPlugin_CommandHandler(
        GAIA_TRANSPORT *t,
        uint8 pdu_id,
        uint16 payload_length,
        const uint8 *payload)
{
    DEBUG_LOG_INFO("uiUserConfigGaiaPlugin_CommandHandler enum:ui_user_config_plugin_pdu_ids_t:%d", pdu_id);

    switch (pdu_id)
    {
    case get_number_of_touchpads:
        uiUserConfigGaiaPlugin_GetNumTouchpads(t, payload_length, payload);
        break;

    case get_supported_gestures:
        uiUserConfigGaiaPlugin_GetSupportedCommand(t, payload_length, payload, pdu_id);
        break;

    case get_supported_contexts:
        uiUserConfigGaiaPlugin_GetSupportedCommand(t, payload_length, payload, pdu_id);
        break;

    case get_supported_actions:
        uiUserConfigGaiaPlugin_GetSupportedCommand(t, payload_length, payload, pdu_id);
        break;

    case get_configuration_for_gesture:
        uiUserConfigGaiaPlugin_GetConfigForGesture(t, payload_length, payload);
        break;

    case set_configuration_for_gesture:
        uiUserConfigGaiaPlugin_SetConfigForGesture(t, payload_length, payload);
        break;

    case reset_configuration_to_defaults:
        uiUserConfigGaiaPlugin_ResetToDefaults(t, payload_length, payload);
        break;

    default:
        DEBUG_LOG_WARN("gaiaDebugPlugin Invalid command ID enum:ui_user_config_plugin_pdu_ids_t:%d", pdu_id);
        return command_not_handled;
    }

    return command_handled;
}

bool UiUserConfigGaiaPlugin_Init(Task init_task)
{
    UNUSED(init_task);

    static const gaia_framework_plugin_functions_t functions_gaia =
    {
        .command_handler        = uiUserConfigGaiaPlugin_CommandHandler,
        .send_all_notifications = NULL,
        .transport_connect      = NULL,
        .transport_disconnect   = NULL,
        .handover_veto          = NULL,
        .handover_abort         = NULL,
        .handover_complete      = NULL
    };

    DEBUG_LOG_DEBUG("UiUserConfigGaiaPlugin_Init");

    GaiaFramework_RegisterFeature(GAIA_UI_USER_CONFIG_FEATURE_ID, UI_USER_CONFIG_GAIA_PLUGIN_VERSION, &functions_gaia);

    return TRUE;
}

void UiUserConfigGaiaPlugin_SetNumberOfTouchpads(uint8 num_touchpads)
{
    ui_num_touchpads = num_touchpads;
}
