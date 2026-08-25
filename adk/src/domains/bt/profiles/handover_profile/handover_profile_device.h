/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       handover_profile_private.h
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Handover Profile device declaration
*/

#ifndef HANDOVER_PROFILE_DEVICE_H_
#define HANDOVER_PROFILE_DEVICE_H_

#include <bdaddr_.h>
#include <source.h>

/*! Per-device handover state */
typedef struct handover_device
{
    /*! The next device to handover */
    struct handover_device *next;
    /*! The device address */
    tp_bdaddr addr;
    /*! The device is a BR/EDR mirrored device */
    bool is_mirrored;
    /*! ACL Handle */
    uint16 handle;
    union
    {
        /* Handover state specific to primary/initiator device */
        struct primary_state
        {
            /*! Source of appsP1 marshal data */
            Source p1_source;
            /*! p0 source state */
            Source btstack_source;
            /*! Length of P0 source data */
            uint16 btstack_data_len;
        } p;
        /* Handover state specific to secondary/acceptor device  */
        struct secondary_state
        {
            /*! Sink for bluetooth stack marshal data */
            Sink btstack_sink;
            /*! Length of P0 source data */
            uint16 btstack_data_len;
            /* Set when appsP1 unmarshalling is complete for this device */
            bool appsp1_unmarshal_complete;
            /*! Set when the btstack_sink has been filled with marshal data */
            bool btstack_unmarshal_complete;
        } s;
    } u;

} handover_device_t;

#endif
