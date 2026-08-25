/*!
\copyright  Copyright (c) 2015 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Implementation of TWS Topology use of peer signalling marshalled message channel.
*/

#include <peer_signalling.h>
#include "tws_topology_peer_sig.h"

void TwsTopology_HandleMarshalledMsgChannelRxInd(PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T* ind)
{
    switch (ind->type)
    {
        default:
            break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

void TwsTopology_HandleMarshalledMsgChannelTxCfm(PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    UNUSED(cfm);
}
