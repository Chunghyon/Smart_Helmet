/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_msg_types.h
\brief      Message types used by the AMA modules
*/

#ifndef AMA_MSG_TYPES_H
#define AMA_MSG_TYPES_H

#include <csrtypes.h>
#include <library.h>
#include "ama_transport_types.h"
#include "voice_sources_telephony_control_interface.h"

#include "accessories.pb-c.h"
#include "state.pb-c.h"

#define MAKE_AMA_MESSAGE(TYPE) MESSAGE_MAKE(message,TYPE##_T);
#define MAKE_AMA_MESSAGE_WITH_LEN(TYPE, LEN) TYPE##_T *message = (TYPE##_T *) PanicUnlessMalloc(sizeof(TYPE##_T) + sizeof(uint8)*LEN);

typedef enum
{
    AMA_GET_DEVICE_INFORMATION_IND = AMA_MSG_BASE,
    AMA_GET_DEVICE_CONFIGURATION_IND,
    AMA_NOTIFY_DEVICE_CONFIG_IND,
    AMA_NOTIFY_SPEECH_STATE_IND,
    AMA_SPEECH_STOP_IND,
    AMA_SPEECH_PROVIDE_IND,
    AMA_SPEECH_ENDPOINT_IND,
    AMA_SYNCHRONIZE_SETTING_IND,
    AMA_UPGRADE_TRANSPORT_IND,
    AMA_SWITCH_TRANSPORT_IND,
    AMA_ENABLE_CLASSIC_PAIRING_IND,
    AMA_STOP_ADVERTISING_AMA_IND,
    AMA_START_ADVERTISING_AMA_IND,
    AMA_SEND_AT_COMMAND_IND,
    AMA_SEND_TRANSPORT_VERSION_ID,
    AMA_OVERRIDE_ASSISTANT_IND,
    AMA_START_SETUP_IND,
    AMA_COMPLETE_SETUP_IND,
    AMA_LOCAL_DISCONNECT_COMPLETE_IND,
    AMA_ISSUE_MEDIA_CONTROL_IND,
    AMA_GET_DEVICE_FEATURES_IND,
    AMA_GET_STATE_IND,
    AMA_SET_STATE_IND,
    AMA_SYNCHRONIZE_STATE_IND,
    AMA_KEEP_ALIVE_IND,
    AMA_LAUNCH_APP_IND,
    AMA_GET_LOCALES_IND,
    AMA_SET_LOCALE_IND,
    AMA_UNHANDLED_COMMAND_IND,
    AMA_MESSAGE_TOP
}ama_message_type_t;

typedef enum{
    ama_at_cmd_ata_ind,
    ama_at_cmd_at_plus_chup_ind,
    ama_at_cmd_at_plus_bldn_ind,
    ama_at_cmd_at_plus_chld_eq_0_ind,
    ama_at_cmd_at_plus_chld_eq_1_ind,
    ama_at_cmd_at_plus_chld_eq_2_ind,
    ama_at_cmd_at_plus_chld_eq_3_ind,
    ama_at_cmd_atd_ind,
    ama_at_cmd_unknown
}ama_at_cmd_t;

typedef struct
{
    uint32 device_id;
} AMA_GET_DEVICE_INFORMATION_IND_T;

typedef struct
{
    phone_number_t telephony_number;
    ama_at_cmd_t at_command;
} AMA_SEND_AT_COMMAND_IND_T;

typedef struct
{
    ama_transport_type_t transport;
} AMA_SWITCH_TRANSPORT_IND_T;

typedef struct
{
    uint16 pkt_size;
    uint8 packet[1];
} AMA_SEND_TRANSPORT_VERSION_ID_T;

typedef struct
{
    SpeechState state;
} AMA_NOTIFY_SPEECH_STATE_IND_T;

typedef struct
{
    uint32 dialog_id;
} AMA_SPEECH_PROVIDE_IND_T;

typedef struct
{
    uint32 dialog_id;
} AMA_SPEECH_STOP_IND_T;

typedef struct
{
    uint32 feature_id;
} AMA_GET_STATE_IND_T;

typedef struct
{
    uint32 feature_id;
    State__ValueCase value_case;
    uint32 value;
} AMA_SET_STATE_IND_T;

typedef struct
{
    uint32 feature_id;
    State__ValueCase value_case;
    uint32 value;
} AMA_SYNCHRONIZE_STATE_IND_T;

typedef struct
{
    Command command;
} AMA_UNHANDLED_COMMAND_IND_T;

typedef struct
{
    /*! Must be freed explicity by the message handler */
    char * app_id;
} AMA_LAUNCH_APP_IND_T;

typedef struct
{
    /*! Must be freed explicity by the message handler */
    char * locale;
} AMA_SET_LOCALE_IND_T;

#endif // AMA_MSG_TYPES_H
