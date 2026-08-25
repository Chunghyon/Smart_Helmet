/****************************************************************************
Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_ble_privacy_mode.c

DESCRIPTION
   
    This file contains the implementation of Low Energy privacy mode configuration.

NOTES

*/

/****************************************************************************
    Header files
*/

#include "connection.h"
#include "connection_private.h"
#include "bdaddr.h"
#include "dm_ble_privacy_mode.h"
#include "connection_tdl.h"

#ifndef DISABLE_BLE

/****************************************************************************
NAME    
    ConnectionDmUlpSetPrivacyModeReq

DESCRIPTION
    Set privacy mode for a given LE connection.

RETURNS
    void

*/
void ConnectionDmUlpSetPrivacyModeReq(
        const typed_bdaddr  *peer_taddr,
        const privacy_mode mode
        )
{
    if (mode < privacy_mode_last)
    {
        MAKE_PRIM_C(DM_HCI_ULP_SET_PRIVACY_MODE_REQ);

        prim->peer_identity_address_type = peer_taddr->type;
        BdaddrConvertVmToBluestack(&prim->peer_identity_address, &peer_taddr->addr);

        prim->privacy_mode = (uint8) mode;

        VmSendDmPrim(prim);

        /* This can fail if the IRK (ENC_CENTRAL) key has not been distributed
           but then DM_HCI_ULP_SET_PRIVACY_MODE_CFM will also indicate an error
           with status HCI_ERROR_NO_CONNECTION.
         */
        if ( !ConnectionAuthSetBlePrivacyMode(peer_taddr, mode) )
        {
            CL_DEBUG_INFO("Could not set BLE Privacy Mode in TDL!\n");
        }
    }
    else
    {
        CL_DEBUG_INFO(("Unrecognised privacy mode value %d\n", mode));
    }
}

/****************************************************************************
NAME
    connectionHandleDmUlpSetPrivacyModeCfm

DESCRIPTION
    Send the CL_DM_ULP_SET_PRIVACY_MODE_CFM.

RETURNS
    void

*/
void connectionHandleDmUlpSetPrivacyModeCfm(
        const DM_HCI_ULP_SET_PRIVACY_MODE_CFM_T *ind
        )
{
    MAKE_CL_MESSAGE(CL_DM_ULP_SET_PRIVACY_MODE_CFM);

    message->status = connectionConvertHciStatus(ind->status);

    MessageSend(
            connectionGetAppTask(),
            CL_DM_ULP_SET_PRIVACY_MODE_CFM,
            message);
}
#endif
/* End-of-File */
