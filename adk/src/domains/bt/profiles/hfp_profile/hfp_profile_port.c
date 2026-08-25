/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      VM libraries specific HFP functionality port.
*/

#include "hfp_profile_port.h"

#include "hfp_profile_battery_level.h"
#include "hfp_profile.h"
#include "hfp_profile_config.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_private.h"
#include "hfp_profile_sm.h"
#include "hfp_profile_states.h"

#include <device_properties.h>
#include <kymera.h>
#include <logging.h>
#include <message.h>
#include <mirror_profile.h>
#include <panic.h>
#include <ps.h>
#include <ps_key_map.h>
#include <sink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <telephony_messages.h>
#include <volume_messages.h>

static hfp_link_priority hfpProfileInstance_GetLinkForInstance(hfpInstanceTaskData * instance)
{
    hfp_link_priority link = hfp_invalid_link;

    PanicNull(instance);

    device_t device = HfpProfileInstance_FindDeviceFromInstance(instance);
    if (device != NULL)
    {
        bdaddr addr = DeviceProperties_GetBdAddr(device);
        link = HfpLinkPriorityFromBdaddr(&addr);
    }

    DEBUG_LOG_VERBOSE(
        "hfpProfileInstance_GetLinkForInstance instance:%p enum:hfp_link_priority:%d",
        instance, link);

    return link;
}

static voice_source_t HfpProfile_GetVoiceSourceForHfpLinkPrio(hfp_link_priority priority)
{
    voice_source_t source = voice_source_none;

    PanicFalse(priority <= hfp_secondary_link);

    bdaddr bd_addr = {0};
    if (HfpLinkGetBdaddr(priority, &bd_addr))
    {
        hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr);
        if (instance != NULL)
        {
            source = HfpProfileInstance_GetVoiceSourceForInstance(instance);
        }
    }

    DEBUG_LOG_VERBOSE(
        "HfpProfile_GetVoiceSourceForHfpLinkPrio enum:hfp_link_priority:%d enum:voice_source_t:%d",
        priority, source);

    return source;
}

/*! \brief Handle remote support features confirmation
*/
static void hfpProfilePort_HandleClDmRemoteFeaturesConfirm(const CL_DM_REMOTE_FEATURES_CFM_T *cfm)
{
    bool handle_confirmation = FALSE;
    tp_bdaddr bd_addr;
    hfpInstanceTaskData * instance = NULL;

    if (SinkGetBdAddr(cfm->sink, &bd_addr))
    {
        instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr.taddr.addr);

        if (instance != NULL)
        {
            handle_confirmation = TRUE;
        }
    }

    if (handle_confirmation)
    {
        hfpState state = appHfpGetState(instance);

        DEBUG_LOG("appHfpHandleClDmRemoteFeaturesConfirm(%p) enum:hfpState:%d", instance, state);

        if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcDisconnectedOrDisconnecting(state))
        {
            if (cfm->status == hci_success)
            {
                uint16 features[PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE] = PSKEY_LOCAL_SUPPORTED_FEATURES_DEFAULTS;
                uint16 packets;
                uint16 index;

                /* Read local supported features to determine SCO packet types */
                PsFullRetrieve(PSKEY_LOCAL_SUPPORTED_FEATURES, &features, PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE);

                /* Get supported features that both HS & AG support */
                for (index = 0; index < PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE; index++)
                {
                    printf("%04x ", features[index]);
                    features[index] &= cfm->features[index];
                }
                printf("");

                /* Calculate SCO packets we should use */
                packets = sync_hv1;
                if (features[0] & 0x2000)
                    packets |= sync_hv3;
                if (features[0] & 0x1000)
                    packets |= sync_hv2;

                /* Only use eSCO for HFP 1.5+ */
                if (instance->profile == hfp_handsfree_profile)
                {
                    if (features[1] & 0x8000)
                        packets |= sync_ev3;
                    if (features[2] & 0x0001)
                        packets |= sync_ev4;
                    if (features[2] & 0x0002)
                        packets |= sync_ev5;
                    if (features[2] & 0x2000)
                    {
                        packets |= sync_2ev3;
                        if (features[2] & 0x8000)
                            packets |= sync_2ev5;
                    }
                    if (features[2] & 0x4000)
                    {
                        packets |= sync_3ev3;
                        if (features[2] & 0x8000)
                            packets |= sync_3ev5;
                    }
                }

                /* Update supported SCO packet types */
                instance->sco_supported_packets = packets;

                DEBUG_LOG("appHfpHandleClDmRemoteFeaturesConfirm(%p), SCO packets %x", instance, packets);
            }
        }
        else
        {
            HfpProfile_HandleError(instance, CL_DM_REMOTE_FEATURES_CFM, cfm);
        }
    }
    else
    {
        DEBUG_LOG_WARN("appHfpHandleClDmRemoteFeaturesConfirm cannot handle cfm=%p, ignored", cfm);
    }
}

/*! \brief Handle encrypt confirmation
*/
static void hfpProfilePort_HandleClDmEncryptConfirmation(const CL_SM_ENCRYPT_CFM_T *cfm)
{
    bool handle_confirmation = FALSE;
    tp_bdaddr bd_addr;
    hfpInstanceTaskData * instance = NULL;

    if (SinkGetBdAddr(cfm->sink, &bd_addr))
    {
        instance = HfpProfileInstance_GetInstanceForBdaddr(&bd_addr.taddr.addr);

        if (instance != NULL)
        {
            handle_confirmation = TRUE;
        }
    }

    if (handle_confirmation)
    {
        hfpState state = appHfpGetState(instance);

        DEBUG_LOG("appHfpHandleClDmEncryptConfirmation(%p) enum:hfpState:%d encypted=%d", instance, state, cfm->encrypted);

        if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
        {
            /* Store encrypted status */
            instance->bitfields.encrypted = cfm->encrypted;

            /* Check if SCO is now encrypted (or not) */
            HfpProfile_CheckEncryptedSco(instance);
        }
        else
        {
            HfpProfile_HandleError(instance, CL_SM_ENCRYPT_CFM, cfm);
        }
    }
    else
    {
        DEBUG_LOG_WARN("appHfpHandleClDmEncryptConfirmation cannot handle cfm=%p, ignored", cfm);
    }
}

static bool hfpProfilePort_HandleClMessage(Task task, MessageId id, Message message)
{
    bool handled;
    UNUSED(task);
    
    /* Handle connection library messages, specific to VM libs port. */
    switch (id)
    {
        case CL_DM_REMOTE_FEATURES_CFM:
            hfpProfilePort_HandleClDmRemoteFeaturesConfirm((CL_DM_REMOTE_FEATURES_CFM_T *)message);
            handled = TRUE;
        break;
        
        case CL_SM_ENCRYPT_CFM:
            hfpProfilePort_HandleClDmEncryptConfirmation((CL_SM_ENCRYPT_CFM_T *)message);
            handled = TRUE;
        break;
        
        default:
            handled = FALSE;
        break;
    }
    
    return handled;
}

static hfpInstanceTaskData* hfpProfile_GetInstanceFromPriority(hfp_link_priority priority)
{
    bdaddr bd_addr = {0};
    
    if (HfpLinkGetBdaddr(priority, &bd_addr))
    {
        return HfpProfileInstance_GetInstanceForBdaddr(&bd_addr);
    }
    
    return NULL;
}

/*! \brief Entering `Initialising HFP` state

    This function is called when the HFP state machine enters
    the 'Initialising HFP' state, it calls the HfpInit() function
    to initialise the profile library for HFP.
*/
void hfpProfile_InitHfpLibrary(void)
{
    hfp_init_params hfp_params = {0};
    uint16 supp_features = (HFP_VOICE_RECOGNITION |
                            HFP_NREC_FUNCTION |
                            HFP_REMOTE_VOL_CONTROL |
                            HFP_CODEC_NEGOTIATION |
                            HFP_HF_INDICATORS |
                            HFP_ESCO_S4_SUPPORTED |
                            HFP_THREE_WAY_CALLING |
                            HFP_ENHANCED_CALL_STATUS);

    /* Initialise an HFP profile instance */
    hfp_params.supported_profile = hfp_handsfree_profile;
    hfp_params.supported_features = supp_features;
    hfp_params.disable_nrec = TRUE;
    hfp_params.extended_errors = FALSE;
    hfp_params.optional_indicators.service = hfp_indicator_off;
    hfp_params.optional_indicators.signal_strength = hfp_indicator_off;
    hfp_params.optional_indicators.roaming_status = hfp_indicator_off;
    hfp_params.optional_indicators.battery_charge = hfp_indicator_off;
    hfp_params.multipoint = TRUE;
    hfp_params.supported_wbs_codecs = hfp_wbs_codec_mask_cvsd | hfp_wbs_codec_mask_msbc;
    hfp_params.link_loss_time = 1;
    hfp_params.link_loss_interval = 5;
    if (appConfigHfpBatteryIndicatorEnabled())
    {
        hfp_params.hf_indicators = hfp_battery_level_mask;
    }
    else
    {
        hfp_params.hf_indicators = hfp_indicator_mask_none;
    }

#ifdef INCLUDE_SWB
    if (appConfigScoSwbEnabled())
    {
        hfp_params.hf_codec_modes = CODEC_64_2_EV3;
    }
    else
    {
        hfp_params.hf_codec_modes = 0;
    }
#endif

#ifdef TEST_HFP_CODEC_PSKEY

    uint16 hfp_codec_pskey = 0;
    PsRetrieve(PS_KEY_TEST_HFP_CODEC, &hfp_codec_pskey, sizeof(hfp_codec_pskey));

    DEBUG_LOG_ALWAYS("hfpProfile_InitHfpLibrary 0x%x", hfp_codec_pskey);

    hfp_params.supported_wbs_codecs =  (hfp_codec_pskey & HFP_CODEC_PS_BIT_NB) ? hfp_wbs_codec_mask_cvsd: 0;
    hfp_params.supported_wbs_codecs |= (hfp_codec_pskey & HFP_CODEC_PS_BIT_WB) ? hfp_wbs_codec_mask_msbc  : 0;

    if (appConfigScoSwbEnabled())
    {
        hfp_params.hf_codec_modes = (hfp_codec_pskey & HFP_CODEC_PS_BIT_SWB) ? CODEC_64_2_EV3 : 0;
    }

    /* Disable codec negotiation if we ONLY support narrow band */
    if (hfp_codec_pskey == HFP_CODEC_PS_BIT_NB)
        hfp_params.supported_features &= ~(HFP_CODEC_NEGOTIATION);

#endif

    HfpInit(&hfp_profile_task_data.task, &hfp_params, NULL);
}

static void hfpProfile_HandleHfpInitCfm(const HFP_INIT_CFM_T *cfm)
{
    DEBUG_LOG("hfpProfile_HandleHfpInitCfm status enum:hfp_init_status:%d", cfm->status);

    /* Check HFP initialisation was successful */
    hfpProfile_HandleInitComplete(cfm->status == hfp_init_success);
}

/*! \brief Update handset swb support bit field in hfp library
 *         if handset supports swb or not based on handset blocking
 *         status for swb calls.
*/
static void hfpProfile_UpdateHandsetSwbSupportStatus(const bdaddr *bd_addr)
{
    if(HfpProfile_IsHandsetBlockedForSwb(bd_addr))
    {
        DEBUG_LOG("hfpProfile_ResetSwbStatusIfHandsetIsBlockedForSwb");
        HfpUpdateHandsetSwbSupportStatus(bd_addr,FALSE);
    }
    else
    {
        HfpUpdateHandsetSwbSupportStatus(bd_addr,TRUE);
    }
}

static void hfpProfile_HandleHfpSlcConnectInd(const HFP_SLC_CONNECT_IND_T *ind)
{
    DEBUG_LOG_FN_ENTRY("hfpProfile_HandleHfpSlcConnectInd lap=%d accepted=%d", ind->addr.lap, ind->accepted);

    if (!ind->accepted)
    {
        return;
    }

    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(&ind->addr);
    if (instance == NULL)
    {
        instance = HfpProfileInstance_Create(&ind->addr, TRUE);
    }
    if (instance != NULL)
    {
        hfpState state = appHfpGetState(instance);

        DEBUG_LOG("hfpProfile_HandleHfpSlcConnectInd(%p) enum:hfpState:%d", instance, state);

        if (HfpProfile_StateIsSlcDisconnected(state))
        {
            /* Store address of AG */
            instance->ag_bd_addr = ind->addr;
            hfpProfile_UpdateHandsetSwbSupportStatus(&ind->addr);
            appHfpSetState(instance, HFP_STATE_CONNECTING_REMOTE);
        }
    }
}

static void hfpProfile_HandleHfpSlcConnectCfm(const HFP_SLC_CONNECT_CFM_T *cfm)
{
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(&cfm->bd_addr);

    /* The device might have been deleted already, leave the handler in this case */
    if (NULL == instance)
    {
        return;
    }

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpSlcConnectCfm(%p) enum:hfpState:%d enum:hfp_connect_status:%d",
              instance, state, cfm->status);

    if(HfpProfile_StateIsSlcConnecting(state))
    {
        if (cfm->status == hfp_connect_success)
        {
            hfp_call_state call_state = hfp_call_state_idle;

            /* Inform the hfp library if the link is secure */
            if (hfpProfile_IsSecureConnection(&cfm->bd_addr))
            {
                HfpLinkSetLinkMode(cfm->priority, TRUE);
            }

            /* Update the HFP instance members at time of SLC connection. */
            PanicFalse(HfpLinkGetCallState(cfm->priority, &call_state));
            instance->bitfields.call_state = call_state;
            instance->slc_sink = cfm->sink;
            instance->profile = cfm->profile;

            /* Turn off link-loss management. */
            HfpManageLinkLoss(cfm->priority, FALSE);

            /* Ensure the underlying ACL is encrypted. */
            ConnectionSmEncrypt(&hfp_profile_task_data.task, instance->slc_sink, TRUE);
            
            hfpProfile_HandleConnectCompleteSuccess(instance);
        }
        else
        {
            hfpProfile_HandleConnectCompleteFailed(instance, (cfm->status == hfp_connect_sdp_fail));
        }
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_SLC_CONNECT_CFM, cfm);
    }
}

static void hfpProfile_HandleHfpSlcDisconnectInd(const HFP_SLC_DISCONNECT_IND_T *ind)
{
    appHfpDisconnectReason reason;
    hfpInstanceTaskData* instance = HfpProfileInstance_GetInstanceForBdaddr(&ind->bd_addr);
    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpSlcDisconnectInd(%p) enum:hfpState:%d enum:hfp_disconnect_status:%d",
              instance, state, ind->status);
    
    /* The device might have been deleted already, ignore if it has */
    if(instance)
    {
        if(HfpProfile_StateIsSlcConnectedOrConnecting(state))
        {
            /* Check if SCO is still up */
            if (instance && instance->sco_sink)
            {
                /* Disconnect SCO */
                HfpAudioDisconnectRequest(ind->priority);
            }
        }
        
        switch(ind->status)
        {
            case hfp_disconnect_link_loss:
                reason = APP_HFP_DISCONNECT_LINKLOSS;
            break;
            
            case hfp_disconnect_transferred:
                reason = APP_HFP_DISCONNECT_TRANSFERRED;
            break;
            
            default:
                reason = APP_HFP_DISCONNECT_NORMAL;
            break;
        }
        
        hfpProfile_HandleDisconnectComplete(instance, reason);
    }
}

static void hfpProfile_HandleHfpAudioConnectInd(const HFP_AUDIO_CONNECT_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    DEBUG_LOG_FN_ENTRY("hfpProfile_HandleHfpAudioConnectInd enum:hfp_link_priority:%d", ind->priority);

    if (instance == NULL)
    {
        /* Reject SCO connection */
        HfpAudioConnectResponse(ind->priority, FALSE, 0, NULL, FALSE);
    }
    else
    {
        hfpProfile_HandleHfpAudioConnectIncoming(instance, (ind->link_type == sync_link_esco));
    }
}

void hfpProfile_SendAudioConnectResponse(hfpInstanceTaskData * instance, bool is_esco, bool accept)
{
    sync_pkt_type pkt_types = 0;
    hfp_link_priority link_priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);

    if(is_esco && !(instance->sco_supported_packets & sync_all_esco))
    {
        DEBUG_LOG("hfpProfile_SendAudioConnectResponse reject - eSCO not supported");
        accept = FALSE;
    }
    
    if(accept)
    {
        pkt_types = instance->sco_supported_packets ^ sync_all_edr_esco;

        if(!is_esco)
        {
            pkt_types &= ~sync_all_esco;
        }
    }

    HfpAudioConnectResponse(link_priority, accept, pkt_types, NULL, FALSE);
}

/*! \brief Handle SCO Audio connect confirmation
*/
static void hfpProfile_HandleHfpAudioConnectCfm(const HFP_AUDIO_CONNECT_CFM_T *cfm)
{
    hfpInstanceTaskData * instance;
    hfp_profile_audio_connect_status_t status;
    
    if(cfm->priority == hfp_invalid_link)
    {
        DEBUG_LOG("hfpProfile_HandleHfpAudioConnectCfm, cfm but no link, ignoring");
        return;
    }

    instance = hfpProfile_GetInstanceFromPriority(cfm->priority);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpAudioConnectCfm(%p) enum:hfpState:%d enum:hfp_audio_connect_status:%d",
              instance, state, cfm->status);
              
    switch(cfm->status)
    {
        case hfp_audio_connect_success:
            status = hfp_profile_audio_connect_success;
        break;
        
        case hfp_audio_connect_in_progress:
            status = hfp_profile_audio_connect_in_progress;
        break;
        
        default:
            status = hfp_profile_audio_connect_failed;
        break;
    }

    hfpProfile_HandleHfpAudioConnectComplete(instance, status, cfm->audio_sink, cfm->codec, cfm->wesco, cfm->tesco, cfm->qce_codec_mode_id);
}

/*! \brief Handle SCO Audio disconnect indication
*/
static void hfpProfile_HandleHfpAudioDisconnectInd(const HFP_AUDIO_DISCONNECT_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    hfpState state = appHfpGetState(instance);

    if(NULL == instance)
    {
        /* If the instance is null, it is possible that SLC disconnection in the HFP lib has preceded SCO disconnection.
           In that case, fall-back to getting the HFP instance from the focused voice source, assuming that if we have a
           focused HFP voice source with SCO, then this is the source we need to disconnect.*/
        instance = HfpProfile_GetInstanceForVoiceSourceWithAudioFocus();
    }
    
    DEBUG_LOG("hfpProfile_HandleHfpAudioDisconnectInd(%p) enum:hfp_audio_disconnect_status:%d enum:hfpState:%d",
              instance, ind->status, state);

    hfpProfile_HandleAudioDisconnectIndication(instance, (ind->status == hfp_audio_disconnect_transferred));
}

static void hfpProfile_HandleHfpCallAnswerCfm(const HFP_CALL_ANSWER_CFM_T *cfm)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(cfm->priority);
    hfpProfile_HandleHfpCallAnswerComplete(instance, (cfm->status == hfp_success));
}

static void hfpProfile_HandleHfpCallTerminateCfm(const HFP_CALL_TERMINATE_CFM_T *cfm)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(cfm->priority);
    hfpProfile_HandleHfpCallTerminateComplete(instance, (cfm->status == hfp_success));
}

static void hfpProfile_HandleHfpCallHoldActionCfm(const HFP_CALL_HOLD_ACTION_CFM_T* cfm)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(cfm->priority);
    hfpProfile_HandleHfpCallHoldActionComplete(instance, (cfm->status == hfp_success));
}

/*! \brief Handle Ring indication
*/
static void hfpProfile_HandleHfpRingInd(const HFP_RING_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    hfpProfile_HandleRingIndication(instance, ind->in_band);
}

/*! \brief Handle service indication
*/
static void hfpProfile_HandleHfpServiceInd(const HFP_SERVICE_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    hfpProfile_HandleServiceIndication(instance, ind->service);
}

/*! \brief Handle call state indication
*/
static void hfpProfile_HandleHfpCallStateInd(const HFP_CALL_STATE_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);

    PanicNull(instance);
    
    /* Always update the call state when it changes */
    instance->bitfields.call_state = ind->call_state;
    
    hfpProfile_HandleHfpCallStateIndication(instance);
}

/*! \brief Handle voice recognition indication
*/
static void hfpProfile_HandleHfpVoiceRecognitionInd(const HFP_VOICE_RECOGNITION_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    hfpProfile_HandleHfpVoiceRecognitionIndication(instance, ind->enable);
}

/*! \brief Handle voice recognition enable confirmation
*/
static void hfpProfile_HandleHfpVoiceRecognitionEnableCfm(const HFP_VOICE_RECOGNITION_ENABLE_CFM_T *cfm)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(cfm->priority);

    PanicNull(instance);

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpVoiceRecognitionEnableCfm(%p) enum:hfpState:%d enum:hfp_lib_status:%d ",
              instance, state, cfm->status);
    
    hfpProfile_HandleHfpVoiceRecognitionEnableComplete(instance, (cfm->status == hfp_success));
}

static phone_number_type_t hfpProfile_ConvertCallerIdNumberType(hfp_number_type type)
{
    switch(type)
    {
    case hfp_number_unknown:
    default:
        return number_unknown;
    case hfp_number_international:
        return number_international;
    case hfp_number_national:
        return number_national;
    case hfp_number_network:
        return number_network;
    case hfp_number_dedicated:
        return number_dedicated;
    }
}

/*! \brief Handle caller ID indication
*/
static void hfpProfile_HandleHfpCallerIdInd(const HFP_CALLER_ID_IND_T *ind)
{
    phone_number_type_t type = hfpProfile_ConvertCallerIdNumberType(ind->number_type);
    
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    
    hfpProfile_HandleHfpCallerIdIndication(instance, (char *)ind->caller_number, type, (char *)ind->caller_name);

    if (ind->caller_name != NULL)
    {
        free(ind->caller_name);
    }
}

/*! \brief Handle caller ID enable confirmation
*/
static void hfpProfile_HandleHfpCallerIdEnableCfm(const HFP_CALLER_ID_ENABLE_CFM_T *cfm)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(cfm->priority);

    /* Ignore this confirmation if the instance no longer exists */
    if (instance == NULL)
    {
        return;
    }

    hfpState state = appHfpGetState(instance);

    DEBUG_LOG("hfpProfile_HandleHfpCallerIdEnableCfm(%p) enum:hfpState:%d enum:hfp_lib_status:%d ",
              instance, state, cfm->status);

    if(HfpProfile_StateIsSlcConnected(state) || HfpProfile_StateIsSlcTransition(state))
    {
        if (cfm->status == hfp_success)
            instance->bitfields.caller_id_active = TRUE;
    }
    else
    {
        HfpProfile_HandleError(instance, HFP_CALLER_ID_ENABLE_CFM, cfm);
    }
}

/*! \brief Handle volume indication
*/
static void hfpProfile_HandleHfpVolumeSyncSpeakerGainInd(const HFP_VOLUME_SYNC_SPEAKER_GAIN_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    hfpProfile_HandleHfpVolumeSyncSpeakerGainIndication(instance, ind->volume_gain);
}

/*! \brief Handle microphone volume indication
*/
static void hfpProfile_HandleHfpVolumeSyncMicGainInd(const HFP_VOLUME_SYNC_MICROPHONE_GAIN_IND_T *ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    hfpProfile_HandleHfpVolumeSyncMicGainIndication(instance, ind->mic_gain);
}

/*! \brief Handle unrecognised AT commands as TWS+ custom commands.
 */
static void hfpProfile_HandleHfpUnrecognisedAtCmdInd(HFP_UNRECOGNISED_AT_CMD_IND_T* ind)
{
    hfpInstanceTaskData * instance = hfpProfile_GetInstanceFromPriority(ind->priority);
    hfpProfile_HandleHfpUnrecognisedAtCmdIndication(instance, ind->data, ind->size_data);
}

static void hfpProfile_HandleHfpHfIndicatorsReportInd(const HFP_HF_INDICATORS_REPORT_IND_T *ind)
{
    DEBUG_LOG("hfpProfile_HandleHfpHfIndicatorsReportInd, num=%u, mask=%04x", ind->num_hf_indicators, ind->hf_indicators_mask);
}

static void hfpProfile_HandleHfpHfIndicatorsInd(const HFP_HF_INDICATORS_IND_T *ind)
{
    voice_source_t source = HfpProfile_GetVoiceSourceForHfpLinkPrio(ind->priority);
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForSource(source);

    DEBUG_LOG("hfpProfile_HandleHfpHfIndicatorsInd, num %u, status %u", ind->hf_indicator_assigned_num, ind->hf_indicator_status);

    PanicNull(instance);

    if (ind->hf_indicator_assigned_num == hf_battery_level)
    {
        HfpProfile_EnableBatteryHfInd(instance, ind->hf_indicator_status);
    }
}

static void hfpProfile_HandleHfpAtCmdCfm(HFP_AT_CMD_CFM_T *cfm)
{
    DEBUG_LOG("hfpProfile_HandleHfpAtCmdCfm status enum:hfp_lib_status:%d", cfm->status);
    hfpProfile_HandleHfpAtCmdComplete(cfm->status == hfp_success);
}

static bool hfpProfilePort_HandleHfpMessage(Task task, MessageId id, Message message)
{
    bool handled = TRUE;
    UNUSED(task);
    
    switch(id)
    {
        case HFP_INIT_CFM:
            hfpProfile_HandleHfpInitCfm((HFP_INIT_CFM_T *)message);
        break;

        case HFP_SLC_CONNECT_IND:
            hfpProfile_HandleHfpSlcConnectInd((HFP_SLC_CONNECT_IND_T *)message);
        break;

        case HFP_SLC_CONNECT_CFM:
            hfpProfile_HandleHfpSlcConnectCfm((HFP_SLC_CONNECT_CFM_T *)message);
        break;

        case HFP_SLC_DISCONNECT_IND:
            hfpProfile_HandleHfpSlcDisconnectInd((HFP_SLC_DISCONNECT_IND_T *)message);
        break;

        case HFP_AUDIO_CONNECT_IND:
             hfpProfile_HandleHfpAudioConnectInd((HFP_AUDIO_CONNECT_IND_T *)message);
        break;

        case HFP_AUDIO_CONNECT_CFM:
             hfpProfile_HandleHfpAudioConnectCfm((HFP_AUDIO_CONNECT_CFM_T *)message);
        break;
        
        case HFP_AUDIO_DISCONNECT_IND:
            hfpProfile_HandleHfpAudioDisconnectInd((HFP_AUDIO_DISCONNECT_IND_T *)message);
        break;
        
        case HFP_CALL_ANSWER_CFM:
            hfpProfile_HandleHfpCallAnswerCfm((HFP_CALL_ANSWER_CFM_T*)message);
        break;
        
        case HFP_CALL_TERMINATE_CFM:
            hfpProfile_HandleHfpCallTerminateCfm((HFP_CALL_TERMINATE_CFM_T*)message);
        break;
        
        case HFP_CALL_HOLD_ACTION_CFM:
            hfpProfile_HandleHfpCallHoldActionCfm((HFP_CALL_HOLD_ACTION_CFM_T*)message);
        break;

        case HFP_RING_IND:
             hfpProfile_HandleHfpRingInd((HFP_RING_IND_T *)message);
        break;

        case HFP_SERVICE_IND:
             hfpProfile_HandleHfpServiceInd((HFP_SERVICE_IND_T *)message);
        break;

        case HFP_CALL_STATE_IND:
             hfpProfile_HandleHfpCallStateInd((HFP_CALL_STATE_IND_T *)message);
        break;

        case HFP_VOICE_RECOGNITION_IND:
             hfpProfile_HandleHfpVoiceRecognitionInd((HFP_VOICE_RECOGNITION_IND_T *)message);
        break;

        case HFP_VOICE_RECOGNITION_ENABLE_CFM:
             hfpProfile_HandleHfpVoiceRecognitionEnableCfm((HFP_VOICE_RECOGNITION_ENABLE_CFM_T *)message);
        break;

        case HFP_CALLER_ID_IND:
             hfpProfile_HandleHfpCallerIdInd((HFP_CALLER_ID_IND_T *)message);
        break;

        case HFP_CALLER_ID_ENABLE_CFM:
             hfpProfile_HandleHfpCallerIdEnableCfm((HFP_CALLER_ID_ENABLE_CFM_T *)message);
        break;

        case HFP_VOLUME_SYNC_SPEAKER_GAIN_IND:
             hfpProfile_HandleHfpVolumeSyncSpeakerGainInd((HFP_VOLUME_SYNC_SPEAKER_GAIN_IND_T *)message);
        break;

        case HFP_VOLUME_SYNC_MICROPHONE_GAIN_IND:
             hfpProfile_HandleHfpVolumeSyncMicGainInd((HFP_VOLUME_SYNC_MICROPHONE_GAIN_IND_T *)message);
        break;

        case HFP_AT_CMD_CFM:
             hfpProfile_HandleHfpAtCmdCfm((HFP_AT_CMD_CFM_T*)message);
        break;

        case HFP_UNRECOGNISED_AT_CMD_IND:
             hfpProfile_HandleHfpUnrecognisedAtCmdInd((HFP_UNRECOGNISED_AT_CMD_IND_T*)message);
        break;

        case HFP_HF_INDICATORS_REPORT_IND:
            hfpProfile_HandleHfpHfIndicatorsReportInd((HFP_HF_INDICATORS_REPORT_IND_T *)message);
        break;

        case HFP_HF_INDICATORS_IND:
            hfpProfile_HandleHfpHfIndicatorsInd((HFP_HF_INDICATORS_IND_T *)message);
        break;

        /* Handle additional messages */
        case HFP_HS_BUTTON_PRESS_CFM:
        case HFP_DIAL_LAST_NUMBER_CFM:
        case HFP_SIGNAL_IND:
        case HFP_ROAM_IND:
        case HFP_BATTCHG_IND:
        case HFP_CALL_WAITING_IND:
        case HFP_EXTRA_INDICATOR_INDEX_IND:
        case HFP_EXTRA_INDICATOR_UPDATE_IND:
        case HFP_NETWORK_OPERATOR_IND:
        case HFP_CURRENT_CALLS_CFM:
        case HFP_DIAL_NUMBER_CFM:
        break;
        
        default:
            handled = FALSE;
        break;
    }
    
    return handled;
}

bool HfpProfilePort_HandleMessage(Task task, MessageId id, Message message)
{
    bool handled = hfpProfilePort_HandleClMessage(task, id, message);
    
    if(!handled)
    {
        handled = hfpProfilePort_HandleHfpMessage(task, id, message);
    }

    return handled;
}

void hfpProfile_InitialiseInstancePortSpecificData(hfpInstanceTaskData * instance)
{
    UNUSED(instance);
}

void hfpProfile_CopyInstancePortSpecificData(hfpInstanceTaskData * target_instance, hfpInstanceTaskData * source_instance)
{
    UNUSED(source_instance);
    UNUSED(target_instance);
}

void hfpProfile_ConnectSlc(hfpInstanceTaskData* instance)
{
    /* Start HFP connection
     * Previous version was using profile as hfp_handsfree_107_profile so check
     * here is done as ">=" to retain the compatibility. */
    if (instance->profile >= hfp_handsfree_profile)
    {
        DEBUG_LOG("appHfpEnterConnectingLocal:Connecting HFP to AG (%x,%x,%lx)", instance->ag_bd_addr.nap, instance->ag_bd_addr.uap, instance->ag_bd_addr.lap);
        HfpSlcConnectRequestEx(&instance->ag_bd_addr, hfp_handsfree_and_headset, hfp_handsfree_all,
                               HfpProfile_IsHandsetBlockedForSwb(&instance->ag_bd_addr) ? hfp_connect_ex_no_swb:hfp_connect_ex_none);
    }
    else
    {
        Panic();
    }
}

void hfpProfile_DisconnectSlc(hfpInstanceTaskData* instance)
{
    hfp_link_priority link_priority = hfpProfileInstance_GetLinkForInstance(instance);
    HfpSlcDisconnectRequest(link_priority);
}

void hfpProfile_ReadRemoteSupportedFeatures(hfpInstanceTaskData* instance)
{
    /* Read the remote supported features of the AG */
    ConnectionReadRemoteSuppFeatures(&hfp_profile_task_data.task, instance->slc_sink);
}

void hfpProfile_SendBievCommandToInstance(hfpInstanceTaskData * instance, uint8 percent)
{
    hfp_link_priority link_priority = hfpProfileInstance_GetLinkForInstance(instance);
    DEBUG_LOG_VERBOSE("hfpProfile_SendBievCommandToInstance sending %d percent to link enum:hfp_link_priority:0x%x",
            percent, link_priority);
    HfpBievIndStatusRequest(link_priority, hf_battery_level, percent);
}

void hfpProfile_RefreshCallStateRequest(hfpInstanceTaskData* instance)
{
    hfp_link_priority link_priority = hfpProfileInstance_GetLinkForInstance(instance);
    HfpCurrentCallsRequest(link_priority);
}

void hfpProfile_CallerIdEnable(hfpInstanceTaskData* instance, bool enable)
{
    hfp_link_priority link_priority = hfpProfileInstance_GetLinkForInstance(instance);

    if (link_priority != hfp_invalid_link)
    {
        HfpCallerIdEnableRequest(link_priority, enable);
    }
}

void hfpProfile_LastNumberRedial(hfpInstanceTaskData* instance)
{
    if (instance->profile == hfp_headset_profile)
    {
        HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
    else
    {
        HfpDialLastNumberRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
}

void hfpProfile_VoiceDialEnable(hfpInstanceTaskData* instance)
{
    if (instance->profile == hfp_headset_profile)
    {
        HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
    else
    {
        HfpVoiceRecognitionEnableRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                         instance->bitfields.voice_recognition_request = TRUE);
    }
}

void hfpProfile_VoiceDialDisable(hfpInstanceTaskData* instance)
{
    if (instance->profile == hfp_headset_profile)
    {
        HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
    else
    {
       HfpVoiceRecognitionEnableRequest(hfpProfileInstance_GetLinkForInstance(instance),
                                        instance->bitfields.voice_recognition_request = FALSE);
    }
}

void hfpProfile_DialNumber(hfpInstanceTaskData* instance, uint8* number, unsigned length_number)
{
    if (instance->profile == hfp_headset_profile)
    {
        HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
    else
    {
        HfpDialNumberRequest(hfpProfileInstance_GetLinkForInstance(instance),
                             length_number, number);
    }
}

void hfpProfile_AnswerCall(hfpInstanceTaskData* instance)
{
    if (instance->profile == hfp_headset_profile)
    {
        HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
    else
    {
        HfpCallAnswerRequest(hfpProfileInstance_GetLinkForInstance(instance), TRUE);
    }
}

void hfpProfile_RejectCall(hfpInstanceTaskData* instance)
{
    HfpCallAnswerRequest(hfpProfileInstance_GetLinkForInstance(instance), FALSE);
}

void hfpProfile_TerminateCall(hfpInstanceTaskData* instance)
{
    if (instance->profile == hfp_headset_profile)
    {
        HfpHsButtonPressRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
    else
    {
        HfpCallTerminateRequest(hfpProfileInstance_GetLinkForInstance(instance));
    }
}

void hfpProfile_SetMicrophoneGain(hfpInstanceTaskData *instance, uint8 gain)
{
    HfpVolumeSyncMicrophoneGainRequest(hfpProfileInstance_GetLinkForInstance(instance), &gain);
}

void hfpProfile_SetSpeakerGain(hfpInstanceTaskData *instance, uint8 gain)
{
    HfpVolumeSyncSpeakerGainRequest(hfpProfileInstance_GetLinkForInstance(instance), &gain);
}

static hfp_audio_transfer_direction hfpProfile_GetVoiceSourceHfpDirection(voice_source_audio_transfer_direction_t direction)
{
    if ((direction < voice_source_audio_transfer_to_hfp) || (direction > voice_source_audio_transfer_toggle))
    {
        DEBUG_LOG_ERROR("hfpProfile_GetVoiceSourceHfpDirection Invalid direction");
        Panic();
    }

    if(direction == voice_source_audio_transfer_to_hfp)
    {
        return hfp_audio_to_hfp;
    }
    else if(direction == voice_source_audio_transfer_to_ag)
    {
        return hfp_audio_to_ag;
    }
    else
    {
        return hfp_audio_transfer;
    }
}

void hfpProfile_HandleAudioTransferRequest(hfpInstanceTaskData *instance, voice_source_audio_transfer_direction_t direction)
{
    HfpAudioTransferRequest(hfpProfileInstance_GetLinkForInstance(instance), 
                            hfpProfile_GetVoiceSourceHfpDirection(direction),
                            instance->sco_supported_packets  ^ sync_all_edr_esco, 
                            NULL);
}

void hfpProfile_ReleaseWaitingRejectIncoming(hfpInstanceTaskData* instance)
{
    hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
    HfpCallHoldActionRequest(link, hfp_chld_release_held_reject_waiting, 0);
}

void hfpProfile_AcceptWaitingReleaseActive(hfpInstanceTaskData* instance)
{
    hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
    HfpCallHoldActionRequest(link, hfp_chld_release_active_accept_other, 0);
}

void hfpProfile_AcceptWaitingHoldActive(hfpInstanceTaskData* instance)
{
    hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
    HfpCallHoldActionRequest(link, hfp_chld_hold_active_accept_other, 0);
}

void hfpProfile_AddHeldToMultiparty(hfpInstanceTaskData* instance)
{
    hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
    HfpCallHoldActionRequest(link, hfp_chld_add_held_to_multiparty, 0);
}

void hfpProfile_JoinCallsAndHangUp(hfpInstanceTaskData* instance)
{
    hfp_link_priority link = hfpProfileInstance_GetLinkForInstance(instance);
    HfpCallHoldActionRequest(link, hfp_chld_join_calls_and_hang_up, 0);
}

#ifdef INCLUDE_MIRRORING
void hfpProfile_GetSinks(hfpInstanceTaskData *instance)
{
    Sink sink;
    hfp_link_priority priority;

    DEBUG_LOG("hfpProfile_GetSinks");

    if (HfpIsAudioConnected(&instance->ag_bd_addr))
    {
        instance->sco_sink = MirrorProfile_GetScoSink();

        if (instance->sco_sink)
        {
            DEBUG_LOG("hfpProfile_GetSinks:: Override SCO sink");
            /* Set the HFP Sink in the HFP profile library for the handset connection */
            PanicFalse(HfpOverideSinkBdaddr(&instance->ag_bd_addr, instance->sco_sink));
        }
    }

    /* Derive slc_sink using link priority over which handset is connected*/
    priority = HfpLinkPriorityFromBdaddr(&instance->ag_bd_addr);
    PanicFalse(priority != hfp_invalid_link);

    if (HfpLinkGetSlcSink(priority, &sink))
    {
        instance->slc_sink = sink;
    }
    else
    {
        DEBUG_LOG("hfpProfile_GetSinks:: Deriving slc_link failed for device[0x%06x], enum:hfp_link_priority:%d", instance->ag_bd_addr.lap, priority);
    }
}
#endif
