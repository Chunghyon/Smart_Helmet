/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   btdbg_peer_profile BTDBG Peer Profile
\ingroup    profiles
\brief      BTDBG peer profile
 
It creates RFCOMM between peers and then connects it to the ISP streams.
It only creates RFCOMM between peers when RFCOMM from handset is connected.

*/

#ifndef BTDBG_PEER_PROFILE_H_
#define BTDBG_PEER_PROFILE_H_

#include <message.h>
#include <bdaddr.h>
#include <handover_if.h>

#ifdef INCLUDE_BTDBG

/*@{*/

extern const handover_interface btdbg_profile_handover_if;

bool BtdbgPeerProfile_Init(Task init_task);

void BtdbgPeerProfile_Connect(Task task, const bdaddr *peer_addr);

/*@}*/

#endif

#endif /* BTDBG_PEER_PROFILE_H_ */
