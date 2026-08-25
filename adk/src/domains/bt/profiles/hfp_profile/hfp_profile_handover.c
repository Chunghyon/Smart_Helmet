/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       hfp_profile_handover.c
\brief      HFP Profile Handover related interfaces

*/
#ifdef INCLUDE_MIRRORING

#include "domain_marshal_types.h"
#include "app_handover_if.h"
#include "hfp_profile.h"
#include "hfp_profile_instance.h"
#include "hfp_profile_port.h"
#include "hfp_profile_private.h"
#include "mirror_profile.h"
#include <logging.h>
#include <stdlib.h>
#include <panic.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool hfpProfile_VetoLink(const bdaddr *bd_addr);

static bool hfpProfile_Marshal(const bdaddr *bd_addr, 
                               marshal_type_t type,
                               void **marshal_obj);

static app_unmarshal_status_t hfpProfile_Unmarshal(const bdaddr *bd_addr, 
                                 marshal_type_t type,
                                 void *unmarshal_obj);

static void hfpProfile_Commit(bool is_primary);


/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_info_t hfp_profile_marshal_types[] = {
    MARSHAL_TYPE_INFO(hfpInstanceTaskData, MARSHAL_TYPE_CATEGORY_PER_INSTANCE)
};

const marshal_type_list_t hfp_profile_marshal_types_list = {hfp_profile_marshal_types, ARRAY_DIM(hfp_profile_marshal_types)};

REGISTER_HANDOVER_INTERFACE_VPL(HFP_PROFILE, &hfp_profile_marshal_types_list, NULL, hfpProfile_VetoLink, hfpProfile_Marshal, hfpProfile_Unmarshal, hfpProfile_Commit);

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*!
    \brief Checks if hfp task has any msg pending other than
           HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ.

    \return TRUE to veto handover if any other msg is pending.
            FALSE not to veto handover if HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ
            is only pending during swb call.
*/
static bool hfpProfile_MessageIsDisallowed(Task task, MessageId id, Message message)
{
    bool msg_disallowed = TRUE;

    UNUSED(task);
    UNUSED(message);

    if (id == HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ)
    {
        msg_disallowed = FALSE;
    }

    return msg_disallowed;
}

/*! 
    \brief Handle Veto of link check during handover.

    \param[in] bd_addr Bluetooth address of the link to be checked.

    \return TRUE to veto handover.
*/
static bool hfpProfile_VetoLink(const bdaddr *bd_addr)
{
    bool veto = FALSE;
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);

    if(instance)
    {
        /* Veto if any one of the following conditions is TRUE
        a) In transient state (lock is held). 
        b) Detach is pending.
        c) Pending messages for the task.
        */
        if (MessagePendingMatch(HfpProfile_GetInstanceTask(instance), FALSE, hfpProfile_MessageIsDisallowed))
        {
            DEBUG_LOG_INFO("hfpProfile_VetoLink(%p), Messages pending for HFP task", instance);
            veto = TRUE;
        }
        else
        {
            if (*HfpProfileInstance_GetLock(instance))
            {
                DEBUG_LOG_INFO("hfpProfile_VetoLink(%p), hfp_lock", instance);
                veto = TRUE;
            }
            else if (!HfpProfile_IsDisconnected(instance) && instance->bitfields.detach_pending)
            {
                /* We are not yet disconnected, but we have a "detach pending", i.e. ACL has been disconnected
                    * and now we wait for profile disconnection event from Stack. Veto untill the profile is A2DP_STATE_DISCONNECTED.
                    */
                DEBUG_LOG_INFO("hfpProfile_VetoLink(%p), detach_pending", instance);
                veto = TRUE;
            }
        }
    }
    return veto;
}

/*!
    \brief The function shall set marshal_obj to the address of the object to 
           be marshalled.

    \param[in] bd_addr      Bluetooth address of the link to be marshalled.
    \param[in] type         Type of the data to be marshalled.
    \param[out] marshal_obj Holds address of data to be marshalled.
    \return TRUE: Required data has been copied to the marshal_obj.
            FALSE: No data is required to be marshalled. marshal_obj is set to NULL.

*/
static bool hfpProfile_Marshal(const bdaddr *bd_addr, 
                               marshal_type_t type, 
                               void **marshal_obj)
{
    bool status = FALSE;
    DEBUG_LOG("hfpProfile_Marshal");
    *marshal_obj = NULL;
    hfpInstanceTaskData * instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);
    if(NULL != instance)
    {
        switch (type)
        {
            case MARSHAL_TYPE(hfpInstanceTaskData):
                *marshal_obj = instance;
                status = TRUE;
                break;

            default:
                break;
        }
    }
    else
    {
        DEBUG_LOG("hfpProfile_Marshal:Bluetooth Address Mismatch");
    }

    return status;
}

/*! 
    \brief The function shall copy the unmarshal_obj associated to specific 
            marshal type

    \param[in] bd_addr      Bluetooth address of the link to be unmarshalled.
    \param[in] type         Type of the unmarshalled data.
    \param[in] unmarshal_obj Address of the unmarshalled object.
    \return unmarshalling result. Based on this, caller decides whether to free
            the marshalling object or not.
*/
static app_unmarshal_status_t hfpProfile_Unmarshal(const bdaddr *bd_addr, 
                                 marshal_type_t type, 
                                 void *unmarshal_obj)
{
    DEBUG_LOG("hfpProfile_Unmarshal");
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;

    switch (type)
    {
        case MARSHAL_TYPE(hfpInstanceTaskData):
            {
                hfpInstanceTaskData *hfpInst = (hfpInstanceTaskData*)unmarshal_obj;
                hfpInstanceTaskData *instance = HfpProfileInstance_GetInstanceForBdaddr(bd_addr);
                if (instance == NULL)
                {
                    instance = HfpProfileInstance_Create(bd_addr, FALSE);
                }

                instance->state = hfpInst->state;
                instance->profile = hfpInst->profile;
                instance->ag_bd_addr = *bd_addr;
                instance->bitfields = hfpInst->bitfields;
                instance->sco_supported_packets = hfpInst->sco_supported_packets;
                instance->codec = hfpInst->codec;
                instance->wesco = hfpInst->wesco;
                instance->tesco = hfpInst->tesco;
                instance->qce_codec_mode_id = hfpInst->qce_codec_mode_id;
                instance->source_state = hfpInst->source_state;
                hfpProfile_CopyInstancePortSpecificData(instance, hfpInst);
                result = UNMARSHAL_SUCCESS_FREE_OBJECT;
            }
            break;

        default:
            /* Do nothing */
            break;
    }

    return result;
}

static void hfpProfile_CommitInstance( hfpInstanceTaskData *instance, bool accept)
{
    voice_source_t voice_source = HfpProfileInstance_GetVoiceSourceForInstance(instance);

    if (accept)
    {
        hfpProfile_GetSinks(instance);

        if(voice_source != voice_source_none)
        {
            HfpProfileInstance_RegisterVoiceSourceInterfaces(voice_source);

            if (hfpProfile_AptxVoicePacketsCounterToBeMonitored(HfpProfile_GetHandsetBdAddr(instance), instance->qce_codec_mode_id))
            {
                HfpProfileInstance_StartCheckingAptxVoicePacketsCounterImmediatelyIfSwbCallActive(instance);
            }
        }
    }
    else
    {
        instance->state = HFP_STATE_DISCONNECTED;
        instance->slc_sink = 0;
        instance->sco_sink = 0;
        appBatteryUnregister(HfpProfile_GetInstanceTask(instance));
        instance->bitfields.hf_indicator_assigned_num = hf_indicators_invalid;
        MessageCancelAll(HfpProfile_GetInstanceTask(instance), HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ);

        if(voice_source != voice_source_none)
        {
            HfpProfileInstance_DeregisterVoiceSourceInterfaces(voice_source);
        }
    }
}

/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if new role is primary, else secondary

*/
static void hfpProfile_Commit(bool is_primary)
{
    DEBUG_LOG_VERBOSE("hfpProfile_Commit");
    hfpInstanceTaskData * instance = NULL;
    hfp_instance_iterator_t iterator;

    for_all_hfp_instances(instance, &iterator)
    {
        hfpProfile_CommitInstance(instance, is_primary);
    }
}


#endif /* INCLUDE_MIRRORING */
