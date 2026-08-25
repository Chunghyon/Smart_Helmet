/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    btdbg_profile
\brief      Data structures exposed for other parts of the component to access.
 
*/

#ifndef BTDBG_PROFILE_DATA_H_
#define BTDBG_PROFILE_DATA_H_

#include <message.h>
#include <sink.h>
#include <stream.h>

#ifdef INCLUDE_BTDBG

/*@{*/

#define BTDBG_PROFILE_UUID 0x00, 0x00, 0xBD, 0xB9, 0xD1, 0x02, 0x11, 0xE1, 0x9B, 0x23, 0x00, 0x02, 0x5B, 0x00, 0xA5, 0xA5
#define BTDBG_PROFILE_ATTR 0x09, 0x00, 0x04

typedef enum {
    btdbg_profile_state_disconnected,
    btdbg_profile_state_connecting,
    btdbg_profile_state_connected,
    btdbg_profile_state_disconnecting
} btdbg_profile_state_t;

typedef struct
{
    Task my_task;
    TaskData task_data;
    Task listener;
    btdbg_profile_state_t state;
    Sink rfcomm_sink;
    Sink isp_sink;
    stream_isp_role isp_role;
} btdbg_profile_data_t;

extern btdbg_profile_data_t btdbg_profile_data;

/*@}*/

#endif

#endif /* BTDBG_PROFILE_DATA_H_ */
