/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain AGHFP component abstraction layer that creates a common api for 
*           both the legacy api and synergy APIs.
*/
#include <panic.h>
#include <logging.h>
#include "aghfp_profile_config.h"
#include "aghfp_profile_private.h"
#include "aghfp_profile_abstraction.h"
#include "aghfp_profile_instance.h"


/***************************** Macros ****************************************/


/***************************** function defines ****************************************/
void AghfpProfileAbstract_Activate(uint16 supported_qce_codecs, uint16 supported_features)
{
    AghfpInitQCE(&aghfp_profile_task_data.task, aghfp_handsfree_18_profile, supported_features,supported_qce_codecs);
}


void AghfpProfileAbstract_SendOk(aghfpInstanceTaskData *ins, bool ignore)
{
    UNUSED(ignore);
    AghfpSendOk(ins->aghfp);
}

void AghfpProfileAbstract_SendError(aghfpInstanceTaskData *ins)
{
    AghfpSendError(ins->aghfp);
}

void AghfpProfileAbstract_ConfigureAudioSettings(aghfpInstanceTaskData * instance)
{
    UNUSED(instance);
}

void AghfpProfileAbstract_EstablishSlcConnect(aghfpInstanceTaskData * instance)
{
    AghfpSlcConnect(instance->aghfp, &instance->hf_bd_addr);
}

void AghfpProfileAbstract_SlcDisconnect(aghfpInstanceTaskData * instance)
{
    AghfpSlcDisconnect(instance->aghfp);
}

void AghfpProfileAbstract_AudioConnect(aghfpInstanceTaskData * instance, const aghfp_audio_params *audio_param, uint16 qceCodecId)
{
    UNUSED(qceCodecId);
    AghfpAudioConnect(instance->aghfp, instance->sco_supported_packets ^ sync_all_edr_esco, audio_param);
}


void AghfpProfileAbstract_AudioDisconnect(aghfpInstanceTaskData * instance)
{
    AghfpAudioDisconnect(instance->aghfp);
}


void AghfpProfileAbstract_AudioConnectResponse(aghfpInstanceTaskData * instance, bool accept, sync_pkt_type packet_type, const aghfp_audio_params *audio_param)
{
    AghfpAudioConnectResponse(instance->aghfp, accept, packet_type, audio_param);
}


void AghfpProfileAbstract_SetRemoteMicrophoneGain(aghfpInstanceTaskData * instance, uint8 gain)
{
    AghfpSetRemoteMicrophoneGain(instance->aghfp, gain);
}


void AghfpProfileAbstract_SetRemoteSpeakerVolume(aghfpInstanceTaskData * instance, int gain)
{
    AghfpSetRemoteSpeakerVolume(instance->aghfp, gain);
}

void AghfpProfileAbstract_SendCallHeldIndication(aghfpInstanceTaskData * instance)
{
    AghfpSendCallHeldIndicator(instance->aghfp, instance->bitfields.call_hold);
}



void AghfpProfileAbstract_SendCallSetupIndication(aghfpInstanceTaskData * instance)
{
    AghfpSendCallSetupIndicator(instance->aghfp, instance->bitfields.call_setup);
}


void AghfpProfileAbstract_SendCallStatusIndication(aghfpInstanceTaskData * instance)
{
    AghfpSendCallIndicator(instance->aghfp, instance->bitfields.call_status);
}

void AghfpProfileAbstract_EnableInbandRingTone(aghfpInstanceTaskData * instance)
{
    AghfpInBandRingToneEnable(instance->aghfp, instance->bitfields.in_band_ring);
}


void AghfpProfileAbstract_SendRingAlert(aghfpInstanceTaskData * instance, bool caller_id_host, bool caller_id_remote)
{
    UNUSED(caller_id_host);
    UNUSED(caller_id_remote);

    AghfpSendRingAlert(instance->aghfp);
}

void AghfpProfileAbstract_SendCallerId(aghfpInstanceTaskData * instance)
{
    AghfpSendCallerId(instance->aghfp,
                          instance->clip.clip_type,
                          instance->clip.size_clip_number,
                          instance->clip.clip_number,
                          0,
                          NULL);
}


void AghfpProfileAbstract_SendServiceIndication(aghfpInstanceTaskData * instance, aghfp_service_availability avail)
{
    AghfpSendServiceIndicator(instance->aghfp, avail);
}

void AghfpProfileAbstract_SendSignalStrengthIndication(aghfpInstanceTaskData * instance, uint16 level)
{
    AghfpSendSignalIndicator(instance->aghfp, level);
}

void AghfpProfileAbstract_SendDialRes(aghfpInstanceTaskData * instance, bool resp, bool ignore)
{
    if(!ignore)
    {
        if(resp == TRUE)
        {
            AghfpSendOk(instance->aghfp);
        }
        else
        {
            AghfpSendError(instance->aghfp);
        }
    }
}

void AghfpProfileAbstract_SendNetworkOperator(aghfpInstanceTaskData * instance)
{
    AghfpSendNetworkOperator(instance->aghfp, 0, strlen((char*)instance->network_operator), instance->network_operator);
}


void AghfpProfileAbstract_SendSubscriberNumber(aghfpInstanceTaskData * instance)
{
    AghfpSendSubscriberNumbersComplete(instance->aghfp);
}


void AghfpProfileAbstract_SendCallWaiting(aghfpInstanceTaskData * instance, bool caller_id_host, bool caller_id_remote)
{
    if (caller_id_host && caller_id_remote)
    {
        AghfpSendCallWaitingNotification(instance->aghfp,
                                         instance->clip.clip_type,
                                         instance->clip.size_clip_number,
                                         instance->clip.clip_number,
                                         0,
                                         NULL);
    }
    else
    {
        AghfpSendCallWaitingNotification(instance->aghfp,
                                         0,
                                         0,
                                         0,
                                         0,
                                         NULL);
    }
}

void AghfpProfileAbstract_SendCallList(aghfpInstanceTaskData * instance, uint16 index)
{
    call_list_element_t *call;

    if (index == 0)
    {
        for_each_call(instance->call_list, call)
        {
            DEBUG_LOG_ALWAYS("call->idx %d", call->call.idx);
            AghfpSendCurrentCall(instance->aghfp, &call->call);
        }
        AghfpSendCurrentCallsComplete(instance->aghfp);
    }
}


void AghfpProfileAbstract_SendBattChgIndicator(aghfpInstanceTaskData * instance, uint16 level)
{
    AghfpSendBattChgIndicator(instance->aghfp, level);
}

void AghfpProfileAbstract_IndicatorsStatusResponse(aghfpInstanceTaskData * instance, 
                                                                aghfp_service_availability availability,
                                                                uint16 signal, 
                                                                aghfp_roam_status roam_status, 
                                                                uint16 batt)
{
    AghfpCallIndicatorsStatusResponse(instance->aghfp, 
                                      availability,                     /* aghfp_service_availability */
                                      instance->bitfields.call_status,  /* call status active/not active */
                                      instance->bitfields.call_setup,   /* not in setup, incoming, outgoing... etc */
                                      instance->bitfields.call_hold,    /* aghfp_call_held_status */
                                      signal,                           /* Signal level */
                                      roam_status,                      /* aghfp_roam_status */
                                      batt);                            /* Battery level */
}
