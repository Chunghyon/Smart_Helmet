/*
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_sig_static_data.h
\brief      Peer signalling static data
*/

#ifndef PEER_SIG_STATIC_DATA_H
#define PEER_SIG_STATIC_DATA_H

#include "marshal.h"
#include "peer_signalling.h"

/*! \brief Registers a task to use the common peer signalling API
 *  \param task - Task that will receive the peer signalling messages
 *  \param channel - Channel to register
*/
void PeerSigStaticData_Register(Task task, peerSigMsgChannel channel);

/*! \brief Transmits data to peer
 *  \param task - Task to send confirmation message to.
 *  \param channel - Channel to transmit on
 *  \param data_addr - The address of data to transmit
 *  \param data_size - The size of data to transmit (Note: Limited to 255 as per Peer Signalling object limit)
*/
void PeerSigStaticData_Transmit(Task task, peerSigMsgChannel channel, void* data_addr, uint16 data_size);

/*! \brief Receives data from peer
 *  \param ind - The indication of incoming marshalled message via the peer signalling channel.
*/
void PeerSigStaticData_Receive(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind);

#endif // PEER_SIG_STATIC_DATA_H
