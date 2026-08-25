/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   btdbg_profile BTDBG Profile
\ingroup    profiles
\brief      BTDBG profile
 
It registers RFCOMM for the purpose of BTDBG.
When handset connects RFCOMM then this component connects it to ISP streams.

*/

#ifndef BTDBG_PROFILE_H_
#define BTDBG_PROFILE_H_

#include <message.h>
#include <domain_message.h>

#ifdef INCLUDE_BTDBG

/*@{*/

typedef enum
{
    BTDBG_PROFILE_PEER_CONNECT_CFM = BTDBG_PROFILE_MESSAGE_BASE,
    BTDBG_PROFILE_HANDSET_CONNECT_IND,
    BTDBG_PROFILE_HANDSET_DISCONNECT_IND
} btdbg_profile_msg_t;

bool BtdbgProfile_Init(Task init_task);

void BtdbgProfile_RegisterListener(Task listener);

/*@}*/

#endif

#endif /* BTDBG_PROFILE_H_ */
