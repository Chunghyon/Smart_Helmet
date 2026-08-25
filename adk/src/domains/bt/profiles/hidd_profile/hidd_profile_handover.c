/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       hidd_profile_handover.c
\brief      Implementation of HIDD Handover interface
*/

#ifndef HIDD_PROFILE_HANDOVER_C
#define HIDD_PROFILE_HANDOVER_C
#ifdef INCLUDE_HIDD_PROFILE
#include "hidd_profile.h"
#include "hidd_profile_private.h"
#include "app_handover_if.h"
#include "hidd_profile_marshal_typedef.h"
#include "domain_marshal_types.h"
#include <panic.h>
#include <logging.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool hidd_Veto(void);

static bool hidd_Marshal(const bdaddr *bd_addr,
                       marshal_type_t type,
                       void **marshal_obj);

static app_unmarshal_status_t hidd_Unmarshal(const bdaddr *bd_addr,
                         marshal_type_t type,
                         void *unmarshal_obj);

static void hidd_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
const marshal_type_descriptor_t marshal_type_descriptor_hidd_profile_state = MAKE_MARSHAL_TYPE_DEFINITION_BASIC(hiddState);

const marshal_type_info_t hidd_marshal_types[] = {
    MARSHAL_TYPE_INFO(hiddInstanceTaskData, MARSHAL_TYPE_CATEGORY_PER_INSTANCE)
};

const marshal_type_list_t hidd_marshal_types_list = {hidd_marshal_types, ARRAY_DIM(hidd_marshal_types)};

REGISTER_HANDOVER_INTERFACE(HIDD_PROFILE, &hidd_marshal_types_list, hidd_Veto, hidd_Marshal, hidd_Unmarshal, hidd_Commit);

static bool hidd_Veto(void)
{
    hiddInstanceTaskData * instance = HiddProfileGetTaskData();
    bool veto = FALSE;

    /* Check if there are any pending messages in queue */
    if (MessagesPendingForTask(&instance->task, NULL))
    {
        DEBUG_LOG_INFO("hidd_Veto, Messages pending for hidd task");
        veto = TRUE;
    }
    else
    {
        if (instance->state == HIDD_STATE_CONNECTING ||
            instance->state == HIDD_STATE_DISCONNECTING ||
            instance->state == HIDD_STATE_CONNECTED_SENDING_DATA)
        {
            DEBUG_LOG_INFO("hidd_Veto, hidd profile in a transient state (%d)", instance->state);
            veto = TRUE;
        }
    }

    return veto;
}

static bool hidd_Marshal(const bdaddr *bd_addr,
                       marshal_type_t type,
                       void **marshal_obj)
{
    bool status = FALSE;
    UNUSED(type);
    UNUSED(marshal_obj);
    hiddInstanceTaskData * instance = HiddProfile_GetInstanceForBdaddr(bd_addr);
    if(NULL != instance)
    {
        switch (type)
        {
            case MARSHAL_TYPE(hiddInstanceTaskData):
                *marshal_obj = instance;
                status = TRUE;
                break;

            default:
                break;
        }
    }
    else
    {
        DEBUG_LOG("hiddProfile_Marshal:Bluetooth Address Mismatch ");
    }

    return status;
}

static app_unmarshal_status_t hidd_Unmarshal(const bdaddr *bd_addr,
                         marshal_type_t type,
                         void *unmarshal_obj)
{

    DEBUG_LOG("hidd_Unmarshal");
    app_unmarshal_status_t result = UNMARSHAL_FAILURE;
    switch (type)
    {
        case MARSHAL_TYPE(hiddInstanceTaskData):
        {
            hiddInstanceTaskData *instance = HiddProfile_CreateInstance();
            instance->connection_id = ((hiddInstanceTaskData*)unmarshal_obj)->connection_id;
            instance->state = ((hiddInstanceTaskData*)unmarshal_obj)->state;
            instance->hidd_bd_addr = *bd_addr;
            result = UNMARSHAL_SUCCESS_FREE_OBJECT;
        }
        break;

        default:
            /* Do nothing */
            break;
    }

    return result;
}

static void hidd_Commit(bool is_primary)
{
    UNUSED(is_primary);
}

#endif /* INCLUDE_HIDD_PROFILE */
#endif /* HIDD_PROFILE_HANDOVER_C */
