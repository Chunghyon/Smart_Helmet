/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module with context framework provider support
*/

#include <panic.h>
#include <stdlib.h>
#include <logging.h>
#include "kymera_statistics.h"
#include "bt_device.h"
#include "context_framework.h"
#include "kymera.h"
#include "kymera_data.h"
#include "av_seids.h"

#define STREAMING_IS_LOSSLESS_INDICATOR 0xaf
#define STREAMING_MICROSECONDS_BEFORE_REFRESH (uint32)100000
#define STREAMING_RTP_HEADER_SIZE (uint32)12
#define STREAMING_MICROSECONDS_IN_SECOND 1000000

#if defined(ENABLE_BITRATE_STATISTIC)
static vm_transform_query_bitrate_t previous_bitrate_info = {0};
static uint32 previous_reported_bitrate = 0;
#endif

static bool kymeraStatistics_IsAdaptive(void)
{
    uint8 seid = Kymera_GetCurrentSeid();
    return seid == AV_SEID_APTX_ADAPTIVE_SNK || seid == AV_SEID_APTX_ADAPTIVE_TWS_SNK;
}

static inline bool kymeraStatistics_GetConnectedHandsetTpAddress(tp_bdaddr* addr)
{
    Source source = KymeraGetTaskData()->media_source;
    if (!source) {
        DEBUG_LOG_VERBOSE("kymeraStatistics_GetConnectedHandsetTpAddress source is 0");
        return FALSE;
    }
    Sink sink = StreamSinkFromSource(source);
    if (!sink) {
        DEBUG_LOG_VERBOSE("kymeraStatistics_GetConnectedHandsetTpAddress sink is 0");
        return FALSE;
    }
    if (!SinkGetBdAddr(sink, addr))
    {
        DEBUG_LOG_VERBOSE("kymeraStatistics_GetConnectedHandsetTpAddress SinkGetBdAddr failed");
        return FALSE;
    }
    return TRUE;
}

static inline Transform kymeraStatistics_getPacketiser(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    Transform packetiser = NULL;
#ifdef INCLUDE_MIRRORING
    packetiser = theKymera->hashu.packetiser;
#elif INCLUDE_STEREO
    packetiser = theKymera->packetiser;
#endif
    return packetiser;
}

static inline bool kymeraStatistics_IsLossless(void)
{
    if (!kymeraStatistics_IsAdaptive())
    {
        return FALSE;
    }

    Transform packetiser = kymeraStatistics_getPacketiser();
    uint32 ssrc=0;

    if (packetiser && TransformQuery(packetiser, VM_TRANSFORM_QUERY_PACKETISE_SSRC ,&ssrc))
    {
        return (ssrc&0xFF) == STREAMING_IS_LOSSLESS_INDICATOR;
    }

    return FALSE;
}

static inline bool kymeraStatistics_GetBitRate(uint32 *bitrate)
{
    *bitrate = 0;
#if defined(ENABLE_BITRATE_STATISTIC)
    /* Check how long since last request */
    bool fetchNew = TRUE;
    if (previous_bitrate_info.timestamp > 0)
    {
        uint32 timestamp = VmGetTimerTime();
        if (timestamp > previous_bitrate_info.timestamp)
        {
            fetchNew = (timestamp - previous_bitrate_info.timestamp) > STREAMING_MICROSECONDS_BEFORE_REFRESH;
        }
    }

    if (!fetchNew)
    {
        DEBUG_LOG_VERBOSE("kymeraStatistics_GetBitRate Not fetching new - returning old bitrate");
        *bitrate = previous_reported_bitrate;
        return *bitrate != 0;
    }

    /* We need to fetch again as suitable time has elapsed */
    Transform packetiser = kymeraStatistics_getPacketiser();
    vm_transform_query_bitrate_t new_bitrate_info = {0};

    if (packetiser && TransformQuery(packetiser, VM_TRANSFORM_QUERY_BITRATE ,(uint32 *)&new_bitrate_info))
    {
       if ((previous_bitrate_info.timestamp == 0) ||
           (previous_bitrate_info.timestamp > new_bitrate_info.timestamp) ||
           (previous_bitrate_info.bytes_processed > new_bitrate_info.bytes_processed) ||
           (previous_bitrate_info.packets_processed > new_bitrate_info.packets_processed))
       {
           /* No previous report or something has wrapped around. Just save the new data and return zero. */
           DEBUG_LOG_VERBOSE("kymeraStatistics_GetBitRate Return 0 Validation error");
           memcpy(&previous_bitrate_info, &new_bitrate_info, sizeof (vm_transform_query_bitrate_t));
           previous_reported_bitrate = 0;
           return FALSE;
       }

       uint32 microseconds = new_bitrate_info.timestamp - previous_bitrate_info.timestamp;
       uint32 bytes = new_bitrate_info.bytes_processed - previous_bitrate_info.bytes_processed;
       uint32 packets = new_bitrate_info.packets_processed - previous_bitrate_info.packets_processed;

       memcpy(&previous_bitrate_info, &new_bitrate_info, sizeof (vm_transform_query_bitrate_t));

       uint32 headerSize = packets * STREAMING_RTP_HEADER_SIZE;
       if (headerSize > bytes)
       {
           DEBUG_LOG_VERBOSE("kymeraStatistics_GetBitRate Return 0 header > bytes");
           previous_reported_bitrate = 0;
           return FALSE;
       }

       uint32 audioBytes = bytes - headerSize;
       double bits = (double)audioBytes * (double)8;
       double bitsPerSec = bits * ((double)STREAMING_MICROSECONDS_IN_SECOND / (double)microseconds);

       if (bitsPerSec > (double)UINT32_MAX)
       {
           previous_reported_bitrate = 0;
           DEBUG_LOG_VERBOSE("kymeraStatistics_GetBitRate Return 0 bitsPerSec too big");
           return FALSE;
       }

       *bitrate =(uint32)bitsPerSec;

       previous_reported_bitrate = *bitrate;
       return TRUE;
    }
    else
    {
        DEBUG_LOG_VERBOSE("kymeraStatistics_GetBitRate couldn't get transform info");
    }
#endif
    return FALSE;
}

static bool kymeraStatistics_GetStreamingInfo(unsigned * context_data, uint8 context_data_size)
{
    DEBUG_LOG_VERBOSE("kymeraStatistics_GetStreamingInfo");
    PanicZero(context_data_size >= sizeof(context_streaming_info_t));
    memset(context_data, 0, sizeof(context_streaming_info_t));

 #if defined(INCLUDE_MIRRORING)
    if (!BtDevice_IsMyAddressPrimary())
    {
        /* We don't return values if secondary earbud. */
        return FALSE;
    }
 #endif

    bool is_adaptive = kymeraStatistics_IsAdaptive();
    ((context_streaming_info_t *)context_data)->seid = Kymera_GetCurrentSeid();
    ((context_streaming_info_t *)context_data)->is_adaptive = is_adaptive;
    ((context_streaming_info_t *)context_data)->is_lossless = kymeraStatistics_IsLossless();

    tp_bdaddr addr = {0};
    int16 rssi = 0;
    uint16 link_quality = 0;

    if (kymeraStatistics_GetConnectedHandsetTpAddress(&addr))
    {
        if (!VmBdAddrGetRssi(&addr, &rssi)) {
            DEBUG_LOG_VERBOSE("kymeraStatistics_GetStreamingInfo Couldn't get RSSI");
        }

        if (!VmGetAclLinkQuality(&addr, &link_quality)) {
            DEBUG_LOG_VERBOSE("kymeraStatistics_GetStreamingInfo Couldn't get Link Quality");
        }
    }
    ((context_streaming_info_t *)context_data)->primary_rssi = rssi;
    ((context_streaming_info_t *)context_data)->primary_link_quality = link_quality;

    uint32 bitrate = 0;
    kymeraStatistics_GetBitRate(&bitrate);

    ((context_streaming_info_t *)context_data)->bitrate = bitrate;

    return TRUE;
}

void KymeraStatistics_Init(void)
{
    ContextFramework_RegisterContextProvider(context_streaming_info,
                                             kymeraStatistics_GetStreamingInfo);
}
