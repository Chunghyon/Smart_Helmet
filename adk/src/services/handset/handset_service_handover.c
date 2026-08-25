/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       handset_service_handover.c
\brief      Handset Service handover related interfaces

*/

#include <bdaddr.h>
#include <logging.h>
#include <panic.h>
#include <app_handover_if.h>
#include <device_properties.h>
#include <device_list.h>
#include <focus_device.h>
#include "handset_service_sm.h"
#include "handset_service_protected.h"
#include "handset_service_config.h"

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static void handsetService_Commit(bool is_primary);
static bool handsetService_VetoLink(const bdaddr *bd_addr);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/
REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING_VPL(HANDSET_SERVICE, NULL, handsetService_VetoLink, handsetService_Commit);

#define handsetService_IsSmUnstable(sm, bredrOnly) \
    (HANDSET_SERVICE_STATE_NULL != (sm)->state) && \
    (HANDSET_SERVICE_STATE_CONNECTED_BREDR != (sm)->state) && \
    (HANDSET_SERVICE_STATE_DISCONNECTED != (sm)->state)  && \
    (bredrOnly ||  (HANDSET_SERVICE_STATE_BLE_CONNECTED != (sm)->ble_sm.le_state))

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/
/*! \brief Handover veto check for BREDR links only.
    \return bool TRUE (veto the handover) if handset service SM for the link is not in a stable state.
                 FALSE otherwise.
*/
static bool handsetService_VetoLink(const bdaddr *bd_addr)
{
    bool veto = FALSE;
    handset_service_state_machine_t* sm = handsetService_GetSmForBredrAddr(bd_addr);
        
    /* veto if SM for the BREDR link is not in a stable state */
    if (sm && handsetService_IsSmUnstable(sm, TRUE))
    {
        veto = TRUE;
        DEBUG_LOG_INFO("handsetService_VetoLink, Unstable handset state enum:handset_service_state_t:state[%d]", sm->state);
    }

    return veto;
}


/*!
    \brief Component commits to the specified role

    The component should take any actions necessary to commit to the
    new role.

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void handsetService_Commit(bool is_primary)
{
    if(is_primary)
    {
        cm_connection_iterator_t iterator;
        tp_bdaddr addr;
        handset_service_state_machine_t *sm;
        /* Populate state machine for the active device connections as per connection manager records */
        if (ConManager_IterateFirstActiveConnection(&iterator, &addr))
        {
            do
            {
                {
                    /* Interested in only handset connections */
                    if (appDeviceIsHandset(&addr.taddr.addr))
                    {
                        /* It should either create a new statemachine or retrieve existing SM created for
                         * bonded BLE handset(whose resolved Public address is matching with BREDR handset)
                         * and set the device for the statemachine */
                        sm = handsetService_FindOrCreateSm(&addr);
                        PanicNull((void*)sm);
                        sm->state = HANDSET_SERVICE_STATE_CONNECTED_BREDR;
                    }

                }
            }while (ConManager_IterateNextActiveConnection(&iterator, &addr));
        }
        /* register for connection manager events in new primary role */
        ConManagerRegisterTpConnectionsObserver(cm_transport_all, HandsetService_GetTask());
    }
    else
    {
        FOR_EACH_HANDSET_SM(sm)
        {
            if (DeviceProperties_GetHandsetContext(sm->handset_device) > handset_context_link_loss)
            {
                DeviceProperties_SetHandsetContext(sm->handset_device, handset_context_disconnected);
            }
            HandsetServiceSm_DeInit(sm);
        }
    }
}

