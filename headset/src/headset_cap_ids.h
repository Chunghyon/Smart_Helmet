/*!
\copyright  Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Definitions of audio capability IDs
*/

#ifndef HEADSET_CAP_IDS_H
#define HEADSET_CAP_IDS_H

#ifdef INCLUDE_GAA
#include <gaa_cap_ids.h>
#endif

#ifdef INCLUDE_AMA
#include <ama_cap_ids.h>
#endif

#ifdef INCLUDE_WUW
#define DOWNLOAD_VA_GRAPH_MANAGER
#endif

#if (defined(__QCC304X__) || defined(__QCC514X__))
#define DOWNLOAD_APTX_ADAPTIVE_DECODE
#endif

#if (defined(__QCC304X__) || defined(__QCC514X__) || defined(__QCC302X_APPS__) || defined(__QCC512X_APPS__)) && !defined(HAVE_STP_ROM_1_2)
#define DOWNLOAD_CVC_FBC
#endif


#if defined(__QCC307X__) || defined(__QCC517X__)

#ifndef AUDIO_IN_SQIF



#endif /* AUDIO_IN_SQIF */

#endif /* defined(__QCC307X__) || defined(__QCC517X__) */

#ifdef DOWNLOAD_AEC_REF
#define CAP_ID_AEC_REF                CAP_ID_DOWNLOAD_AEC_REFERENCE
#else
#define CAP_ID_AEC_REF                CAP_ID_AEC_REFERENCE
#endif

#ifdef DOWNLOAD_VOLUME_CONTROL
#define CAP_ID_OUTPUT_VOL_CTRL           CAP_ID_DOWNLOAD_VOL_CTRL_VOL
#else
#define CAP_ID_OUTPUT_VOL_CTRL           CAP_ID_VOL_CTRL_VOL
#endif

#ifdef DOWNLOAD_APTX_ADAPTIVE_DECODE
#define HS_CAP_ID_APTX_ADAPTIVE_DECODE   CAP_ID_DOWNLOAD_APTX_ADAPTIVE_DECODE
#else
#define HS_CAP_ID_APTX_ADAPTIVE_DECODE   CAP_ID_APTX_ADAPTIVE_DECODE
#endif

#ifdef DOWNLOAD_SWITCHED_PASSTHROUGH
#define HS_CAP_ID_SWITCHED_PASSTHROUGH   CAP_ID_DOWNLOAD_SWITCHED_PASSTHROUGH_CONSUMER
#else
#define HS_CAP_ID_SWITCHED_PASSTHROUGH   CAP_ID_SWITCHED_PASSTHROUGH_CONSUMER
#endif

#ifdef DOWNLOAD_AAC_DECODER
#define HS_CAP_ID_AAC_DECODER            CAP_ID_DOWNLOAD_AAC_DECODER
#else
#define HS_CAP_ID_AAC_DECODER            CAP_ID_AAC_DECODER
#endif

#ifdef DOWNLOAD_VA_GRAPH_MANAGER
#define HS_CAP_ID_VA_GRAPH_MANAGER CAP_ID_DOWNLOAD_VA_GRAPH_MANAGER
#else
#define HS_CAP_ID_VA_GRAPH_MANAGER CAP_ID_VA_GRAPH_MANAGER
#endif

#ifdef DOWNLOAD_CVC_FBC
#define HS_CAP_ID_CVC_FBC CAP_ID_DOWNLOAD_CVC_FBC
#else
#define HS_CAP_ID_CVC_FBC CAP_ID_CVC_FBC
#endif

#define CAP_ID_VA_CVC_1MIC HS_CAP_ID_CVC_FBC
#define CAP_ID_VA_CVC_2MIC CAP_ID_CVCHS2MIC_BARGEIN_WB

#ifdef DOWNLOAD_GVA
#define CAP_ID_VA_GVA CAP_ID_DOWNLOAD_GVA
#else
    #if defined(INCLUDE_GAA) && defined(INCLUDE_GAA_WUW)
        #error GAA with Wake-up word functionality needs the DOWNLOAD_GVA define and download_gva capability
    #endif
/* This capability, if present, is always a downloadable */
#define CAP_ID_VA_GVA CAP_ID_NONE
#endif

#ifdef DOWNLOAD_APVA
#define CAP_ID_VA_APVA CAP_ID_DOWNLOAD_APVA
#else
    #if defined(INCLUDE_AMA) && defined(INCLUDE_AMA_WUW)
        #error AMA with Wake-up word functionality needs the DOWNLOAD_APVA define and download_apva capability
    #endif
/* This capability, if present, is always a downloadable */
#define CAP_ID_VA_APVA CAP_ID_NONE
#endif

#ifdef DOWNLOAD_SWBS
    #define CAP_ID_SCO_SWBS_ENC CAP_ID_DOWNLOAD_SWBS_ENC
    #define CAP_ID_SCO_SWBS_DEC CAP_ID_DOWNLOAD_SWBS_DEC
#else
        #define CAP_ID_SCO_SWBS_ENC CAP_ID_SWBS_ENC
        #define CAP_ID_SCO_SWBS_DEC CAP_ID_SWBS_DEC
#endif

#ifdef DOWNLOAD_LC3_ENCODE_SCO_ISO
#define CAP_ID_LC3_SCO_ISO_ENC    0x409A //CAP_ID_DOWNLOAD_LC3_ENCODE_SCO_ISO
#else
#define CAP_ID_LC3_SCO_ISO_ENC    0X00C4 //CAP_ID_LC3_ENCODE_SCO_ISO
#endif

#ifdef DOWNLOAD_LC3_DECODE_SCO_ISO
#define CAP_ID_LC3_SCO_ISO_DEC 0x4098 /*CAP_ID_DOWNLOAD_LC3_DECODE_SCO_ISO*/
#else
#define CAP_ID_LC3_SCO_ISO_DEC 0x00C2 /*CAP_ID_LC3_DECODE_SCO_ISO*/
#endif

#endif // HEADSET_CAP_IDS_H
