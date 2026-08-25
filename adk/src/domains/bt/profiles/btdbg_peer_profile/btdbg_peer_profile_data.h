/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    btdbg_peer_profile
\brief      Data structures exposed for other parts of the component to access.
 
*/

#ifndef BTDBG_PEER_PROFILE_DATA_H_
#define BTDBG_PEER_PROFILE_DATA_H_

#include <message.h>
#include <sink.h>
#include <stream.h>

#ifdef INCLUDE_BTDBG

/*@{*/

typedef enum {
    btdbg_peer_profile_state_disconnected,
    btdbg_peer_profile_state_connecting,
    btdbg_peer_profile_state_connected,
    btdbg_peer_profile_state_disconnecting
} btdbg_peer_profile_state_t;

typedef struct
{
    Task rfc_task;
    TaskData rfc_task_data;
    btdbg_peer_profile_state_t state;
    void *sdp_search_data;
    Task listener;
    bdaddr peer_addr;
    Sink rfcomm_sink;
    Sink isp_sink;
    uint8 connection_requested;
    uint8 connection_allowed;
    uint8 cfm_sent;
} btdbg_peer_profile_data_t;

extern btdbg_peer_profile_data_t btdbg_peer_profile_data;

/*@}*/

#endif

#endif /* BTDBG_PEER_PROFILE_DATA_H_ */
