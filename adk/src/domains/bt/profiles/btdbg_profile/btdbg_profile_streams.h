/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    btdbg_profile
\brief      Common actions for connecting and disconnecting RFCOMM and ISP streams.
 
*/

#ifndef BTDBG_PROFILE_STREAMS_H_
#define BTDBG_PROFILE_STREAMS_H_

#include <sink.h>
#include <stream.h>

#ifdef INCLUDE_BTDBG

/*@{*/

Sink BtdbgProfile_ConnectStreams(Sink rfcomm_sink, stream_isp_role isp_role);

void BtdbgProfile_DisconnectStreams(Sink rfcomm_sink, Sink isp_sink);

/*@}*/

#endif

#endif /* BTDBG_PROFILE_STREAMS_H_ */
