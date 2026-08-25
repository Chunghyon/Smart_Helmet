/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain AGHFP port component.
*/

#ifndef AGHFP_PROFILE_PORT_PROTECTED_H_
#define AGHFP_PROFILE_PORT_PROTECTED_H_

/* handler functions */
#include <aghfp.h>
#include <aghfp_profile_typedef.h>

extern const aghfpState aghfp_call_setup_table[];
extern const aghfpState aghfp_call_status_table[];

#define DEBUG_LOG_NO_INSTANCE(function_name) DEBUG_LOG_INFO(#function_name ": No aghfp profile instance available")

uint16 aghfpGetAudioDisconnectStatusCode(AGHFP_AUDIO_DISCONNECT_IND_T  *ind);
uint16 aghfpGetSlcDisconnectStatusCode(AGHFP_SLC_DISCONNECT_IND_T *ind);
uint16 aghfpGetAudioConnectStatusCode(AGHFP_AUDIO_CONNECT_CFM_T*ind);


/*! \brief Handle HF answering an incoming call
*/
void aghfpProfile_HandleCallAnswerInd(uint16 id, AGHFP_ANSWER_IND_T *ind);

/*! \brief Handle HF rejecting an incoming call or ending an ongoing call
*/
void aghfpProfile_HandleCallHangUpInd(uint16 id, AGHFP_CALL_HANG_UP_IND_T *ind);

void aghfpProfile_HandleAgHfpAudioConnectCfm(uint16 id, AGHFP_AUDIO_CONNECT_CFM_T *cfm);

void aghfpProfile_HandleHfpAudioDisconnectInd(uint16 id, AGHFP_AUDIO_DISCONNECT_IND_T *ind);

/*! \brief Handle disconnect of the SLC
*/
void aghfpProfile_HandleSlcDisconnectInd(uint16 id, AGHFP_SLC_DISCONNECT_IND_T *ind);

/*! \brief Handle indication to set speaker volume for HF
*/
void aghfpProfile_HandleSpeakerVolumeInd(uint16 id, AGHFP_SYNC_SPEAKER_VOLUME_IND_T* ind);

/*! \brief Handle indication to set microphone gain for HF
*/
void aghfpProfile_HandleSyncMicGain(uint16 id, AGHFP_SYNC_MICROPHONE_GAIN_IND_T* ind);

/*! \brief Handle a request to perform a memory dial from the HF.
*/
void agfhpProfile_HandleRedialLastCall(uint16 id, AGHFP_LAST_NUMBER_REDIAL_IND_T *ind);

/*! \brief Handle a request to perform a memory dial from the HF.
*/
void agfhpProfile_HandleMemoryDialInd(uint16 id, AGHFP_MEMORY_DIAL_IND_T *ind);

/*! \brief Return a list of all current calls
*/
void aghfpProfile_HandleGetCurrentCallsInd(uint16 id, AGHFP_CURRENT_CALLS_IND_T *ind);

/*! \brief Handle AT+CIND message.
*/
void aghfpProfile_HandleCallIndicationsStatusReqInd(uint16 id, AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND_T *ind);

/*! \brief Handle HF requesting subscriber number.
*/
void aghfpProfile_HandleSubscriberNumberInd(uint16 id, AGHFP_SUBSCRIBER_NUMBER_IND_T *ind);

/*! \brief Handle HF requesting network operator ind
*/
void aghfpProfile_HandleNetworkOperatorInd(uint16 id, AGHFP_NETWORK_OPERATOR_IND_T *ind);

/*! \brief Handle dial command from HF.
*/
void aghfpProfile_HandleDialInd(uint16 id, AGHFP_DIAL_IND_T* ind);

/*! \brief Handle unknown NREC command from HF.
           Send ERROR unconditionally since we don't support NR/EC at the moment.
*/
void aghfpProfile_HandleNrecSetupInd(uint16 id, AGHFP_NREC_SETUP_IND_T* ind);

/*! \brief Handle dial command from HF.
*/
void aghfpProfile_HandleDialInd(uint16 id, AGHFP_DIAL_IND_T* ind);

/*! \brief Handle HF requesting network operator ind
*/
void aghfpProfile_HandleNetworkOperatorInd(uint16 id, AGHFP_NETWORK_OPERATOR_IND_T *ind);

/*! \brief Handle HF requesting subscriber number.
*/
void aghfpProfile_HandleSubscriberNumberInd(uint16 id, AGHFP_SUBSCRIBER_NUMBER_IND_T *ind);

/*! \brief Handle AT+CIND message.
*/
void aghfpProfile_HandleCallIndicationsStatusReqInd(uint16 id, AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND_T *ind);

/*! \brief Return a list of all current calls
*/
void aghfpProfile_HandleGetCurrentCallsInd(uint16 id, AGHFP_CURRENT_CALLS_IND_T *ind);

/*! \brief Handle a request to perform a memory dial from the HF.
*/
void agfhpProfile_HandleMemoryDialInd(uint16 id, AGHFP_MEMORY_DIAL_IND_T *ind);

/*! \brief Handle a request to perform a memory dial from the HF.
*/
void agfhpProfile_HandleRedialLastCall(uint16 id, AGHFP_LAST_NUMBER_REDIAL_IND_T *ind);

/*! \brief Handle indication to set microphone gain for HF
*/
void aghfpProfile_HandleSyncMicGain(uint16 id, AGHFP_SYNC_MICROPHONE_GAIN_IND_T* ind);

/*! \brief Handle indication to set speaker volume for HF
*/
void aghfpProfile_HandleSpeakerVolumeInd(uint16 id, AGHFP_SYNC_SPEAKER_VOLUME_IND_T* ind);

/* Utility function */

/*! \brief Return the audio parameters used for an audio connection
\return Pointer to the audio parameters
*/
aghfpInstanceTaskData* AghfpProfilePort_GetInstance(uint16 id, void* message);
void    AghfpProfilePort_HandleAgHfpAudioConnectCfm(aghfpInstanceTaskData *instance, AGHFP_AUDIO_CONNECT_CFM_T *cfm);
void    AghfpProfilePort_HandleHfgMessages(Task task, MessageId id, Message message);
uint16  AghfpProfilePort_GetSupportedFeatures(void);
uint16  AghfpProfilePort_GetSupportedQceCodec(void);
void    AghfpProfilePort_SetLastDialledNumber(AGHFP_DIAL_IND_T *ind);
uint16  AghfpProfilePort_GetMemDialNumber(AGHFP_MEMORY_DIAL_IND_T *ind);
uint16  AghfpProfilePort_GetCurCallIndex(AGHFP_CURRENT_CALLS_IND_T *ind);
uint8   AghfpProfilePort_GetSpeakerVolume(AGHFP_SYNC_SPEAKER_VOLUME_IND_T *ind);

void    AghfpProfilePort_InitLibrary(uint16 supported_qce_codecs, uint16 supported_features);
void    AghfpProfilePort_InitInstanceData(aghfpInstanceTaskData* instance);
void    AghfpProfilePort_DeinitInstanceData(aghfpInstanceTaskData* instance);
void    AghfpProfile_SendSlcStatus(bool connected, const bdaddr* bd_addr);


#endif /* AGHFP_PROFILE_PORT_PROTECTED_H_ */



