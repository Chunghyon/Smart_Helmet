/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain AGHFP component abstraction layer that creates a common api for 
*           both the legacy api and synergy APIs.
*/

#ifndef AGHFP_PROFILE_ABSTRACTION_H_
#define AGHFP_PROFILE_ABSTRACTION_H_

#include "aghfp_profile_typedef.h"

void    AghfpProfileAbstract_Activate(uint16 supported_qce_codecs, uint16 supported_features);
void    AghfpProfileAbstract_SendOk(aghfpInstanceTaskData *ins, bool ignore); 
void    AghfpProfileAbstract_SendError(aghfpInstanceTaskData *ins);

void    AghfpProfileAbstract_EstablishSlcConnect(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_SlcDisconnect(aghfpInstanceTaskData * instance);

void    AghfpProfileAbstract_AudioConnect(aghfpInstanceTaskData * instance, 
                                                   const aghfp_audio_params *audio_param, 
                                                   uint16 qceCodecId);
void    AghfpProfileAbstract_AudioDisconnect(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_AudioConnectResponse(aghfpInstanceTaskData * instance, 
                                                              bool accept, 
                                                              sync_pkt_type packet_type, 
                                                              const aghfp_audio_params *audio_param);
void    AghfpProfileAbstract_ConfigureAudioSettings(aghfpInstanceTaskData * instance);

void    AghfpProfileAbstract_SetRemoteMicrophoneGain(aghfpInstanceTaskData * instance, uint8 gain);
void    AghfpProfileAbstract_SetRemoteSpeakerVolume(aghfpInstanceTaskData * instance, int gain);
void    AghfpProfileAbstract_SendCallHeldIndication(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_SendRingAlert(aghfpInstanceTaskData * instance, bool caller_id_host, bool caller_id_remote);
void    AghfpProfileAbstract_SendCallerId(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_EnableInbandRingTone(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_SendCallStatusIndication(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_SendCallSetupIndication(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_SendServiceIndication(aghfpInstanceTaskData * instance, 
                                                               aghfp_service_availability avail);
void    AghfpProfileAbstract_SendSignalStrengthIndication(aghfpInstanceTaskData * instance, uint16 level);
void    AghfpProfileAbstract_SendBattChgIndicator(aghfpInstanceTaskData * instance, uint16 level);
void    AghfpProfileAbstract_SendDialRes(aghfpInstanceTaskData * instance, bool resp, bool ignore);
void    AghfpProfileAbstract_SendNetworkOperator(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_SendSubscriberNumber(aghfpInstanceTaskData * instance);
void    AghfpProfileAbstract_SendCallWaiting(aghfpInstanceTaskData * instance, bool caller_id_host, bool caller_id_remote);
void    AghfpProfileAbstract_SendCallList(aghfpInstanceTaskData * instance, uint16 index);
void    AghfpProfileAbstract_IndicatorsStatusResponse(aghfpInstanceTaskData * instance, 
                                                      aghfp_service_availability availability,
                                                      uint16 signal, 
                                                      aghfp_roam_status roam_status, 
                                                      uint16 batt);
#endif /* AGHFP_PROFILE_ABSTRACTION_H_ */

