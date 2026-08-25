/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Common actions for connecting and disconnecting RFCOMM and ISP streams.

*/

#ifdef INCLUDE_BTDBG

#include "btdbg_profile_streams.h"

#include <logging.h>

#include <source.h>

Sink BtdbgProfile_ConnectStreams(Sink rfcomm_sink, stream_isp_role isp_role)
{

    Source isp_source = StreamIspSource(isp_role);
    Sink isp_sink = StreamIspSink(isp_role);
    Transform read_trans = StreamConnect(isp_source, rfcomm_sink);
    Transform write_trans = StreamConnect(StreamSourceFromSink(rfcomm_sink), isp_sink);

    SourceConfigure(isp_source, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);
    Source rfcomm_source = StreamSourceFromSink(rfcomm_sink);
    SourceConfigure(rfcomm_source, STREAM_SOURCE_HANDOVER_POLICY, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);


    DEBUG_LOG_VERBOSE("BtdbgProfile_ConnectStreams isp_source %p, isp_sink %p, read transform %p, write transform %p",
                        isp_source, isp_sink, read_trans, write_trans);

    return isp_sink;
}

void BtdbgProfile_DisconnectStreams(Sink rfcomm_sink, Sink isp_sink)
{
    Source rfcomm_source = StreamSourceFromSink(rfcomm_sink);
    Source isp_source = StreamSourceFromSink(isp_sink);

    StreamConnectDispose(isp_source);
    StreamConnectDispose(rfcomm_source);

    StreamDisconnect(isp_source, NULL);
    StreamDisconnect(NULL, rfcomm_sink);

    SourceClose(isp_source);

    StreamDisconnect(rfcomm_source, NULL);
    StreamDisconnect(NULL, isp_sink);

    SinkClose(isp_sink);
}

#endif
