/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   HID Device profile
\ingroup    profiles
\brief      HID Device profile
*/
#ifndef HIDDEVICE_PROFILE_H
#define HIDDEVICE_PROFILE_H
#include "domain_message.h"
#include "task_list.h"
#include "device.h"
/* Report Types */
typedef enum
{
    hidd_profile_report_type_other,
    hidd_profile_report_type_input,
    hidd_profile_report_type_output,
    hidd_profile_report_type_feature,
}hidd_report_type_t;

typedef enum
{
    hidd_profile_handshake_success,
    hidd_profile_handshake_not_ready,
    hidd_profile_handshake_invalid_report_id,
    hidd_profile_handshake_unsupported,
    hidd_profile_handshake_invalid_parameter,
    hidd_profile_handshake_unknown,
    hidd_profile_handshake_fatal
}hidd_handshake_result_t;

typedef enum
{
    hidd_profile_status_connected,
    hidd_profile_status_connect_failed,
    hidd_profile_status_disconnected,
    hidd_profile_status_conn_loss,
    hidd_profile_status_reconnecting,
    hidd_profile_status_unacceptable_parameters,
    hidd_profile_status_sdp_register_failed,
    hidd_profile_status_timeout,
    hidd_profile_status_conn_term_by_remote,
    hidd_profile_status_conn_term_by_local
} hidd_profile_status_t;

typedef struct {
    hidd_profile_status_t status;
} HIDD_PROFILE_ACTIVATE_CFM_T;

typedef struct {
    hidd_profile_status_t status;
    bdaddr addr;
    uint32 connid;
} HIDD_PROFILE_CONNECT_IND_T;

typedef struct {
    bdaddr addr;
    bool successful;
} HIDD_PROFILE_CONNECT_CFM_T;

typedef struct {
    hidd_profile_status_t status;
    bdaddr addr;
} HIDD_PROFILE_DISCONNECT_IND_T;

typedef struct {
    bdaddr addr;
    bool successful;
} HIDD_PROFILE_DISCONNECT_CFM_T;

typedef struct {
    hidd_report_type_t report_type;
    uint16 size;
    uint8 reportid;
} HIDD_PROFILE_GET_REPORT_IND_T;

typedef struct {
    hidd_report_type_t type;
    uint8 reportid;
    uint16 reportLen;
    uint8* data;
} HIDD_PROFILE_SET_REPORT_IND_T;

typedef HIDD_PROFILE_SET_REPORT_IND_T HIDD_PROFILE_DATA_IND_T;

typedef struct {
    hidd_profile_status_t status;
} HIDD_PROFILE_DATA_CFM_T;

typedef struct {
    hidd_profile_status_t status;
} HIDD_PROFILE_DEACTIVATE_CFM_T;

/*! \brief Confirmation Messages to be sent to clients */
typedef enum hidd_profile_messages
{
    /*! HIDD SDP registered */
    HIDD_PROFILE_ACTIVATE_CFM = HIDD_PROFILE_MESSAGE_BASE,
    /*! Connect indication */
    HIDD_PROFILE_CONNECT_IND,
    /*! Connect Confirmation for locally initiated connection request */
    HIDD_PROFILE_CONNECT_CFM,
    /*! Disconnect indication */
    HIDD_PROFILE_DISCONNECT_IND,
    /*! Disconnect Confirmation for locally initiated disconnection request */
    HIDD_PROFILE_DISCONNECT_CFM,
    /*! Data Indication (usually output reports on interrupt channel) */
    HIDD_PROFILE_DATA_IND,
    /*! Set Report Indication */
    HIDD_PROFILE_SET_REPORT_IND,
    /*! Get Report Indication */
    HIDD_PROFILE_GET_REPORT_IND,
    /*! Confirmation in response to HiddProfile_DataReq */
    HIDD_PROFILE_DATA_CFM,
    /*! Deactivation (and disconnection if required) complete. */
    HIDD_PROFILE_DEACTIVATE_CFM,
    HIDD_PROFILE_MESSAGE_END
}hidd_profile_messages_t;

#endif /*HIDDEVICE_PROFILE_H*/
