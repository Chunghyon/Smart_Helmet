/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_types.h
\brief      Types relevant the ama_transport module
*/

#ifndef AMA_TRANSPORT_TYPES_H
#define AMA_TRANSPORT_TYPES_H

#include "transport.pb-c.h"

typedef enum {
    ama_transport_ble=TRANSPORT__BLUETOOTH_LOW_ENERGY,
    ama_transport_rfcomm=TRANSPORT__BLUETOOTH_RFCOMM,
    ama_transport_iap=TRANSPORT__BLUETOOTH_IAP,
    ama_transport_max,
    ama_transport_none
}ama_transport_type_t;

/*! Reason for local disconnection request */
typedef enum
{
    ama_local_disconnect_reason_normal,
    ama_local_disconnect_reason_forced,
    ama_local_disconnect_reason_last
} ama_local_disconnect_reason_t;

typedef bool(*data_received_callback_t)(const uint8 * data, uint16 length);

#endif // AMA_TRANSPORT_TYPES_H
