/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Application domain ag profile port component for synergy.
*/

/*! \brief Handle unknown AT commands from the HF.
*/

#include <panic.h>
#include <logging.h>
#include <pmalloc.h>
#include <string.h>
#include <stdio.h>
#include <bdaddr.h>
#include <stream.h>
#include <system_state.h>
#include <device_list.h>
#include <device_properties.h>

#include "aghfp_profile.h"
#include "aghfp_profile_port_protected.h"
#include "aghfp_profile_config.h"
#include "aghfp_profile_private.h"
#include "aghfp_profile_instance.h"
#include "aghfp_profile_sm.h"
#include "aghfp_profile_audio.h"

#include "connection_manager.h"
#include "telephony_messages.h"


static AGHFP        *aghfp_lib_instance;


uint16 aghfpGetSlcDisconnectStatusCode(AGHFP_SLC_DISCONNECT_IND_T *ind)
{
    return ind->status;
}

uint16 aghfpGetAudioConnectStatusCode(AGHFP_AUDIO_CONNECT_CFM_T*ind)
{
    return ind->status;
}

uint16 aghfpGetAudioDisconnectStatusCode(AGHFP_AUDIO_DISCONNECT_IND_T  *ind)
{
    return ind->status;
}

static aghfpInstanceTaskData * aghfpProfileAbstract_GetInstanceForAghfp(AGHFP *aghfp)
{
    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;
    for_all_aghfp_instances(instance, &iterator)
    {
        if (instance->aghfp == aghfp)
        {
            return instance;
        }
    }

    return NULL;
}

static void aghfpProfile_HandleUnrecognisedAtCommand(uint16 id, AGHFP_UNRECOGNISED_AT_CMD_IND_T* message)
{
    AghfpSendError((message)->aghfp);
    UNUSED(id);
}

/*! \brief Handle caller id command from HF.
*/
static void aghfpProfile_HandleCallerIdInd(uint16 id, AGHFP_CALLER_ID_SETUP_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallerIdInd");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleCallerIdInd);
        return;
    }

    instance->bitfields.caller_id_active_remote = ind->enable;
    UNUSED(id);
}

/*! \brief Respond to the HF setting up an audio connection
*/
static void aghfpProfile_HandleCallAudioParamsReqInd(void)
{
    DEBUG_LOG_FN_ENTRY("aghfpProfile_HandleCallAudioParamsReqInd");

    aghfpInstanceTaskData * instance = NULL;
    aghfp_instance_iterator_t iterator;

    for_all_aghfp_instances(instance, &iterator)
    {
        AghfpSetAudioParams(instance->aghfp, instance->sco_supported_packets ^ sync_all_edr_esco, AghfpProfile_GetAudioParams(instance));
    }
}

/*! \brief Handle send call HFP indications confirmation. Note this is
           the HFP indications not a AGHFP library ind
*/
static void aghfpProfile_HandleSendCallIndCfm(uint16 id, AGHFP_SEND_CALL_INDICATOR_CFM_T* cfm)
{
    DEBUG_LOG("AGHFP_SEND_CALL_INDICATOR_CFM enum:aghfp_lib_status:%d", cfm->status);
    UNUSED(id);
}

const aghfp_audio_params * AghfpProfile_GetAudioParams(aghfpInstanceTaskData *instance)
{
    static aghfp_audio_params negotiated_audio_params;
    sync_pkt_type packet_type;
    uint8 wbs_codec;
    
    /* use pre-negotiated audio_params if possible */
    if (AghfpGetNegotiatedAudioParams(instance->aghfp, &packet_type, &negotiated_audio_params) && AghfpCodecHasBeenNegotiated(instance->aghfp, &wbs_codec))
    {
        DEBUG_LOG_INFO("AghfpProfile_GetAudioParams: using negotiated_audio_params");
        return &negotiated_audio_params;
    }
    else
    {
        DEBUG_LOG_INFO("AghfpProfile_GetAudioParams: using library default params" );
        return NULL;
    }
}

/*! \brief Handle SLC connect confirmation
*/
static void aghfpProfile_HandleHfpSlcConnectCfm(uint16 id, const AGHFP_SLC_CONNECT_CFM_T *cfm)
{
    aghfpInstanceTaskData* instance = AghfpProfilePort_GetInstance(id, (void*)cfm);
    aghfpState state;
	
    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleHfpSlcConnectCfm);
        return;
    }

	state = AghfpProfile_GetState(instance);
    DEBUG_LOG("aghfpProfile_HandleHfpSlcConnectCfm(%p) enum:aghfpState:%d enum:aghfp_connect_status:%d",
              instance, state, cfm->status);

    switch (state)
    {
        case AGHFP_STATE_CONNECTING_LOCAL:
        case AGHFP_STATE_CONNECTING_REMOTE:
        {
            /* Check if SLC connection was successful */
            if (cfm->status == aghfp_connect_success)
            {
                /* Store SLC sink */
                instance->slc_sink = cfm->rfcomm_sink;


                if (instance->bitfields.call_setup == aghfp_call_setup_none)
                {
                    /* If there are no calls being setup then progress to connected state
                       based on the call status
                    */
                    AghfpProfile_SetState(instance, aghfp_call_status_table[instance->bitfields.call_status]);
                }
                else
                {
                    /* If there are calls being setup then progress to connected state
                       based on the call setup stage.
                    */
                    AghfpProfile_SetState(instance, aghfp_call_setup_table[instance->bitfields.call_setup]);
                }

                AghfpProfile_SendSlcStatus(TRUE, &instance->hf_bd_addr);

                return;
            }
            /* Not a successful connection, store reason and move to disconnected state */
            instance->disconnect_reason = APP_AGHFP_CONNECT_FAILED;
            AghfpProfile_SetState(instance, AGHFP_STATE_DISCONNECTED);

            /* If a call is active or being setup keep the instance to track
               the state in the event of a successful SLC connection.
            */
            if (instance->bitfields.call_status == aghfp_call_none &&
                instance->bitfields.call_setup == aghfp_call_setup_none)
            {
                AghfpProfileInstance_Destroy(instance);
            }
        }
        return;

        default:
        DEBUG_LOG("SLC connect confirmation received in wrong state.");
        return;
    }
}


/*! \brief Handle audio connected indications confirmation
*/
static void aghfpProfile_HandleAudioConnectInd(uint16 id, AGHFP_AUDIO_CONNECT_IND_T * ind)
{
    aghfpInstanceTaskData   *instance = AghfpProfilePort_GetInstance(id, ind);
    sync_pkt_type           packet_types;

    DEBUG_LOG("aghfpProfile_HandleAudioConnectInd(%p)", instance);

    if (!instance)
    {
        DEBUG_LOG_NO_INSTANCE(aghfpProfile_HandleAudioConnectInd);
        /* No instance means a SCO was attempted without an SLC */
        AghfpAudioConnectResponse(ind->aghfp, FALSE, 0, NULL);
        return;
    }

    bool accept = FALSE;

    switch (instance->state)
    {
        case AGHFP_STATE_CONNECTED_OUTGOING:
        case AGHFP_STATE_CONNECTED_INCOMING:
        case AGHFP_STATE_CONNECTED_ACTIVE:
        {
            if (instance->bitfields.sco_status == aghfp_sco_disconnected)
            {
                DEBUG_LOG("aghfpProfile_HandleAudioConnectInd, accepting");
                instance->bitfields.sco_status = aghfp_sco_connecting;
                accept = TRUE;
            }
        }
        break;

        default:
            /* No call so no audio to route, reject incoming SCO request */
            DEBUG_LOG("aghfpProfile_HandleAudioConnectInd in wrong state, rejecting");
        break;
    }

    /* Flip the EDR packet bits, to match the format expected by the AGHFP library. */
    packet_types = instance->sco_supported_packets ^ sync_all_edr_esco;

    /* Disable all eSCO packet types if only trying to form a basic SCO connection. */
    if (ind->link_type == sync_link_sco)
    {
        packet_types &= ~sync_all_esco;
    }

    DEBUG_LOG("aghfpProfile_HandleAudioConnectInd, accept:%d, packet_types:0x%x", accept, packet_types);

    AghfpProfileAbstract_AudioConnectResponse(instance, accept, packet_types, AghfpProfile_GetAudioParams(instance));
}



/*! \brief Handle HFP Library initialisation confirmation
*/
static void aghfpProfile_HandleAghfpInitCfm(const AGHFP_INIT_CFM_T *cfm)
{
    DEBUG_LOG("aghfpProfile_HandleAghfpInitCfm status enum:aghfp_init_status:%d", cfm->status);

    if (cfm->status == aghfp_init_success)
    {
        PanicNotNull(aghfp_lib_instance);
        aghfp_lib_instance = cfm->aghfp;

        /* Handle the AT+CIND? in the AGHFP profile library.
           Enabling causes AGHFP lib to send a AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND */
        AghfpCindStatusPollClientEnable(cfm->aghfp, TRUE);
        MessageSend(SystemState_GetTransitionTask(), APP_AGHFP_INIT_CFM, 0);
    }
    else
    {
        Panic();
    }
}

/*! \brief Handle SLC connect indication
*/
static void aghfpProfile_HandleHfpSlcConnectInd(uint16 id, const AGHFP_SLC_CONNECT_IND_T *ind)
{
    aghfpInstanceTaskData* instance = AghfpProfileInstance_GetInstanceForBdaddr(&ind->bd_addr);

    if (!instance)
    {
        instance = PanicNull(AghfpProfileInstance_Create(&ind->bd_addr, TRUE));
    }

    aghfpState state = AghfpProfile_GetState(instance);

    bool response = FALSE;

    DEBUG_LOG("aghfpProfile_HandleHfpSlcConnectInd(%p) enum:aghfpState:%d", instance, state);

    switch (state)
    {
        case AGHFP_STATE_DISCONNECTED:
        {
            instance->hf_bd_addr = ind->bd_addr;
            AghfpProfile_SetState(instance, AGHFP_STATE_CONNECTING_REMOTE);
            response = TRUE;
        }
        break;

        default:
        break;
    }

    AghfpSlcConnectResponse(ind->aghfp, response);
    UNUSED(id);
    
}



/*! \brief Handle a request to return the Response and Hold (AT+BTRH?) status.
*/
static void agfhpProfilePort_HandleResponseHoldStatusRequestInd(uint16 id, AGHFP_RESPONSE_HOLD_STATUS_REQUEST_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("agfhpProfile_HandleResponseHoldStatusRequestInd");

    aghfpInstanceTaskData *instance;
    instance = AghfpProfilePort_GetInstance(id, ind);

    AghfpProfileAbstract_SendOk(instance, FALSE);
}

void AghfpProfilePort_DeinitInstanceData(aghfpInstanceTaskData* instance)
{
    UNUSED(instance);
}

void AghfpProfilePort_HandleHfgMessages(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG_INFO("aghfpProfile_TaskMessageHandler MESSAGE:AghfpMessageId:0x%04X", id);
    switch (id)
    {
        case AGHFP_INIT_CFM:
            aghfpProfile_HandleAghfpInitCfm((AGHFP_INIT_CFM_T *)message);
            break;

        case AGHFP_SLC_CONNECT_IND:
            aghfpProfile_HandleHfpSlcConnectInd(id, (AGHFP_SLC_CONNECT_IND_T *)message);
            break;

        case AGHFP_SLC_CONNECT_CFM:
            aghfpProfile_HandleHfpSlcConnectCfm(id, (AGHFP_SLC_CONNECT_CFM_T *)message);
            break;

        case AGHFP_SEND_CALL_INDICATOR_CFM:
            aghfpProfile_HandleSendCallIndCfm(id, (AGHFP_SEND_CALL_INDICATOR_CFM_T*)message);
            break;

        case AGHFP_ANSWER_IND:
            aghfpProfile_HandleCallAnswerInd(id, (AGHFP_ANSWER_IND_T *)message);
            break;

        case AGHFP_CALL_HANG_UP_IND:
            aghfpProfile_HandleCallHangUpInd(id, (AGHFP_CALL_HANG_UP_IND_T *)message);
            break;

        case AGHFP_SLC_DISCONNECT_IND:
            aghfpProfile_HandleSlcDisconnectInd(id, (AGHFP_SLC_DISCONNECT_IND_T *)message);
            break;

        case AGHFP_AUDIO_CONNECT_IND:
            aghfpProfile_HandleAudioConnectInd(id, (AGHFP_AUDIO_CONNECT_IND_T *)message);
            break;

        case AGHFP_AUDIO_CONNECT_CFM:
            aghfpProfile_HandleAgHfpAudioConnectCfm(id, (AGHFP_AUDIO_CONNECT_CFM_T *)message);
            break;

        case AGHFP_AUDIO_DISCONNECT_IND:
            aghfpProfile_HandleHfpAudioDisconnectInd(id, (AGHFP_AUDIO_DISCONNECT_IND_T*)message);
            break;

        case AGHFP_UNRECOGNISED_AT_CMD_IND:
            aghfpProfile_HandleUnrecognisedAtCommand(id, (AGHFP_UNRECOGNISED_AT_CMD_IND_T*)message);
            break;

         case AGHFP_NREC_SETUP_IND:
            aghfpProfile_HandleNrecSetupInd(id, (AGHFP_NREC_SETUP_IND_T*)message);
            break;

        case AGHFP_CALLER_ID_SETUP_IND:
            aghfpProfile_HandleCallerIdInd(id, (AGHFP_CALLER_ID_SETUP_IND_T*)message);
            break;

        case AGHFP_DIAL_IND:
            aghfpProfile_HandleDialInd(id, (AGHFP_DIAL_IND_T*)message);
            break;

        case AGHFP_NETWORK_OPERATOR_IND:
            aghfpProfile_HandleNetworkOperatorInd(id, (AGHFP_NETWORK_OPERATOR_IND_T*)message);
            break;

        case AGHFP_SUBSCRIBER_NUMBER_IND:
            aghfpProfile_HandleSubscriberNumberInd(id, (AGHFP_SUBSCRIBER_NUMBER_IND_T*)message);
            break;

        case AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND:
            aghfpProfile_HandleCallIndicationsStatusReqInd(id, (AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND_T*)message);
            break;

        case AGHFP_APP_AUDIO_PARAMS_REQUIRED_IND:
            aghfpProfile_HandleCallAudioParamsReqInd();
            break;

        case AGHFP_CURRENT_CALLS_IND:
            aghfpProfile_HandleGetCurrentCallsInd(id, (AGHFP_CURRENT_CALLS_IND_T*)message);
            break;

        case AGHFP_MEMORY_DIAL_IND:
            agfhpProfile_HandleMemoryDialInd(id, (AGHFP_MEMORY_DIAL_IND_T*)message);
            break;

        case AGHFP_LAST_NUMBER_REDIAL_IND:
            agfhpProfile_HandleRedialLastCall(id, (AGHFP_LAST_NUMBER_REDIAL_IND_T*)message);
            break;
            
       case AGHFP_RESPONSE_HOLD_STATUS_REQUEST_IND:
            agfhpProfilePort_HandleResponseHoldStatusRequestInd(id, (AGHFP_RESPONSE_HOLD_STATUS_REQUEST_IND_T*)message);
            break;

        case AGHFP_SYNC_MICROPHONE_GAIN_IND:
            aghfpProfile_HandleSyncMicGain(id, (AGHFP_SYNC_MICROPHONE_GAIN_IND_T*)message);
            break;

        case AGHFP_SYNC_SPEAKER_VOLUME_IND:
            aghfpProfile_HandleSpeakerVolumeInd(id, (AGHFP_SYNC_SPEAKER_VOLUME_IND_T*)message);
            break;

    default:
        DEBUG_LOG("aghfpProfile_TaskMessageHandler default handler MESSAGE:AghfpMessageId:0x%04X", id);
    }
}


uint16 AghfpProfilePort_GetSupportedFeatures(void)
{
    return aghfp_incoming_call_reject | aghfp_esco_s4_supported | aghfp_codec_negotiation | aghfp_enhanced_call_status;

}

uint16 AghfpProfilePort_GetSupportedQceCodec(void)
{
    return CODEC_64_2_EV3;
}

uint16 AghfpProfilePort_GetMemDialNumber(AGHFP_MEMORY_DIAL_IND_T *ind)
{
    return ind->size_number;
}


uint16 AghfpProfilePort_GetCurCallIndex(AGHFP_CURRENT_CALLS_IND_T *ind)
{
    return ind->last_idx;
}

uint8 AghfpProfilePort_GetSpeakerVolume(AGHFP_SYNC_SPEAKER_VOLUME_IND_T *ind)
{
    return ind->volume;
}
void AghfpProfilePort_InitLibrary(uint16 supported_qce_codecs, uint16 supported_features)
{
    AghfpProfileAbstract_Activate(supported_qce_codecs, supported_features);
}

void    AghfpProfilePort_SetLastDialledNumber(AGHFP_DIAL_IND_T *ind)
{
    AghfpProfile_SetLastDialledNumber(ind->size_number, ind->number);
}

void   AghfpProfilePort_InitInstanceData(aghfpInstanceTaskData* instance)
{
    PanicNull(aghfp_lib_instance);
    instance->aghfp = aghfp_lib_instance;
}

void AghfpProfilePort_HandleAgHfpAudioConnectCfm(aghfpInstanceTaskData *instance, AGHFP_AUDIO_CONNECT_CFM_T *cfm)
{
    instance->sco_sink = cfm->audio_sink;
    AghfpProfile_StoreConnectParams(instance, cfm->wbs_codec, cfm->wesco, cfm->tesco, cfm->qce_codec_mode_id, cfm->using_wbs);
}


aghfpInstanceTaskData* AghfpProfilePort_GetInstance(uint16 id, void* message)
{
    aghfpInstanceTaskData* instance = NULL;

    switch (id)
    {
        case AGHFP_SLC_CONNECT_IND:
            instance = AghfpProfileInstance_GetInstanceForBdaddr(&((AGHFP_SLC_CONNECT_IND_T *)message)->bd_addr);
            break;

        case AGHFP_SLC_CONNECT_CFM:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_SLC_CONNECT_CFM_T *)message)->aghfp);
            break;

        case AGHFP_SEND_CALL_INDICATOR_CFM:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_SEND_CALL_INDICATOR_CFM_T *)message)->aghfp);
            break;

        case AGHFP_ANSWER_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_ANSWER_IND_T *)message)->aghfp);
            break;

        case AGHFP_CALL_HANG_UP_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_CALL_HANG_UP_IND_T *)message)->aghfp);
            break;

        case AGHFP_SLC_DISCONNECT_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_SLC_DISCONNECT_IND_T *)message)->aghfp);
            break;

        case AGHFP_AUDIO_CONNECT_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_AUDIO_CONNECT_IND_T *)message)->aghfp);
            break;

        case AGHFP_AUDIO_CONNECT_CFM:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_AUDIO_CONNECT_CFM_T *)message)->aghfp);
            break;

        case AGHFP_AUDIO_DISCONNECT_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_AUDIO_DISCONNECT_IND_T *)message)->aghfp);
            break;
            
        case AGHFP_UNRECOGNISED_AT_CMD_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_UNRECOGNISED_AT_CMD_IND_T *)message)->aghfp);
            break;
            
        case AGHFP_NREC_SETUP_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_NREC_SETUP_IND_T *)message)->aghfp);
            break;

        case AGHFP_CALLER_ID_SETUP_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_CALLER_ID_SETUP_IND_T *)message)->aghfp);
            break;

        case AGHFP_DIAL_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_DIAL_IND_T *)message)->aghfp);
            break;

        case AGHFP_NETWORK_OPERATOR_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_NETWORK_OPERATOR_IND_T *)message)->aghfp);
            break;

        case AGHFP_SUBSCRIBER_NUMBER_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_SUBSCRIBER_NUMBER_IND_T *)message)->aghfp);
            break;

        case AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_CALL_INDICATIONS_STATUS_REQUEST_IND_T *)message)->aghfp);
            break;
        
        case AGHFP_CURRENT_CALLS_IND:
              instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_CURRENT_CALLS_IND_T *)message)->aghfp);
              break;
        
        case AGHFP_MEMORY_DIAL_IND:
             instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_MEMORY_DIAL_IND_T *)message)->aghfp);
             break;
        
        case AGHFP_LAST_NUMBER_REDIAL_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_LAST_NUMBER_REDIAL_IND_T *)message)->aghfp);
            break;
             
        case AGHFP_RESPONSE_HOLD_STATUS_REQUEST_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_RESPONSE_HOLD_STATUS_REQUEST_IND_T *)message)->aghfp);
            break;
        
        case AGHFP_SYNC_MICROPHONE_GAIN_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_SYNC_MICROPHONE_GAIN_IND_T *)message)->aghfp);
            break;
        
        case AGHFP_SYNC_SPEAKER_VOLUME_IND:
            instance = aghfpProfileAbstract_GetInstanceForAghfp(((AGHFP_SYNC_SPEAKER_VOLUME_IND_T*)message)->aghfp);
            break;
         
        default:
            DEBUG_LOG("AghfpGetInstance default handler MESSAGE:AghfpMessageId:0x%04X", id);
            break;
    }
    return  instance;
}



