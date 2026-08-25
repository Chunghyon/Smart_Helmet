/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    btdbg_peer_profile
\brief      Handover handling for both BTDBG Profile and BTDBG Peer Profile

*/
#ifdef INCLUDE_BTDBG

#include "btdbg_profile_data.h"
#include "btdbg_peer_profile_data.h"
#include "btdbg_profile_streams.h"

#include <handover_if.h>
#include <app_handover_if.h>
#include <multidevice.h>

#include <bdaddr.h>
#include <logging.h>

#include <stream.h>
#include <sink.h>
#include <source.h>

#include <stdlib.h>

typedef struct
{
    tp_bdaddr tp_bd_addr;
    uint8 dlci;
} btdbg_profile_link_t;

typedef struct
{
    btdbg_profile_link_t handset;
    btdbg_profile_link_t peer;
} btdbg_profile_marshalled_t;

static btdbg_profile_marshalled_t rfcomm_data;

static bool btdbgProfile_Veto(void)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_Veto enum:btdbg_profile_state_t:%d", btdbg_profile_data.state);

    if(btdbg_profile_data.state == btdbg_profile_state_disconnecting
            || btdbg_peer_profile_data.state == btdbg_peer_profile_state_connecting
            || btdbg_peer_profile_data.state == btdbg_peer_profile_state_disconnecting)
    {
        return TRUE;
    }

    return FALSE;
}

static bool btdbgProfile_Marshal(const tp_bdaddr *tp_bd_addr, uint8 *buf, uint16 buf_length, uint16 *written)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_Marshal buf len %d, lap 0x%x", buf_length, tp_bd_addr->taddr.addr.lap);

    bool marshalled = TRUE;

    if(buf_length >= sizeof(btdbg_profile_marshalled_t))
    {
        btdbg_profile_marshalled_t *md = PanicUnlessNew(btdbg_profile_marshalled_t);
        SinkGetBdAddr(btdbg_profile_data.rfcomm_sink, &md->handset.tp_bd_addr);
        md->handset.dlci = SinkGetRfcommDlci(btdbg_profile_data.rfcomm_sink);
        SinkGetBdAddr(btdbg_peer_profile_data.rfcomm_sink, &md->peer.tp_bd_addr);
        md->peer.dlci = SinkGetRfcommDlci(btdbg_peer_profile_data.rfcomm_sink);
        memcpy(buf, md, sizeof(btdbg_profile_marshalled_t));
        free(md);

        *written = sizeof(btdbg_profile_marshalled_t);

        DEBUG_LOG_VERBOSE("btdbgProfile_Marshal marshalled lap 0x%x, dlci %d", md->handset.tp_bd_addr.taddr.addr.lap, md->handset.dlci);
    }
    else
    {
        marshalled = FALSE;
    }

    return marshalled;
}

static bool btdbgProfile_Unmarshal(const tp_bdaddr *tp_bd_addr, const uint8 *buf, uint16 buf_length, uint16 *consumed)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_Unmarshal buf len %d, lap 0x%x", buf_length, tp_bd_addr->taddr.addr.lap);

    if(buf_length >= sizeof(btdbg_profile_marshalled_t))
    {
        memcpy(&rfcomm_data, buf, sizeof(btdbg_profile_marshalled_t));

        DEBUG_LOG_VERBOSE("btdbgProfile_Unmarshal unmarshalled lap 0x%x, dlci %d", rfcomm_data.handset.tp_bd_addr.taddr.addr.lap, rfcomm_data.handset.dlci);

        *consumed = sizeof(btdbg_profile_marshalled_t);
        return TRUE;
    }

    return FALSE;
}

static void btdbgProfile_Commit(const tp_bdaddr *tp_bd_addr, bool is_primary)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_Commit enum:btdbg_profile_state_t:%d, is primary %d lap 0x%x", btdbg_profile_data.state, is_primary, tp_bd_addr->taddr.addr.lap);

    btdbg_peer_profile_data.rfcomm_sink = StreamRfcommSinkFromDlci(&rfcomm_data.peer.tp_bd_addr, rfcomm_data.peer.dlci);

    if(is_primary)
    {
        btdbg_profile_data.rfcomm_sink = StreamRfcommSinkFromDlci(&rfcomm_data.handset.tp_bd_addr, rfcomm_data.handset.dlci);

        Source src = StreamSourceFromSink(btdbg_profile_data.rfcomm_sink);
        SourceConfigure(src, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);

        btdbg_profile_data.isp_role = is_primary ? ISP_ROLE_PRIMARY : ISP_ROLE_SECONDARY;
        btdbg_profile_data.isp_role |= Multidevice_IsLeft() ? ISP_ROLE_LEFT : ISP_ROLE_RIGHT;

        btdbg_profile_data.isp_sink = BtdbgProfile_ConnectStreams(btdbg_profile_data.rfcomm_sink, btdbg_profile_data.isp_role);

        btdbg_peer_profile_data.isp_sink = BtdbgProfile_ConnectStreams(btdbg_peer_profile_data.rfcomm_sink, ISP_ROLE_FORWARDING);

    }
    else
    {
        stream_isp_role isp_role = ISP_ROLE_FORWARDING;
        BtdbgProfile_DisconnectStreams(btdbg_peer_profile_data.rfcomm_sink, btdbg_peer_profile_data.isp_sink);

        isp_role = ISP_ROLE_SECONDARY;
        isp_role |= Multidevice_IsLeft() ? ISP_ROLE_LEFT : ISP_ROLE_RIGHT;

        btdbg_peer_profile_data.isp_sink = BtdbgProfile_ConnectStreams(btdbg_peer_profile_data.rfcomm_sink, isp_role);
    }
}

static void btdbgProfile_Complete(bool is_primary)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_Complete enum:btdbg_profile_state_t:%d, is primary %d", btdbg_profile_data.state, is_primary);
}

static void btdbgProfile_Abort(void)
{
    DEBUG_LOG_VERBOSE("btdbgProfile_Abort");
}

const handover_interface btdbg_profile_handover_if =
        MAKE_HANDOVER_IF(&btdbgProfile_Veto,
                         &btdbgProfile_Marshal,
                         &btdbgProfile_Unmarshal,
                         &btdbgProfile_Commit,
                         &btdbgProfile_Complete,
                         &btdbgProfile_Abort);



#endif
