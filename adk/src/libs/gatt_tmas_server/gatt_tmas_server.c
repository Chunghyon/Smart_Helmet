/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/

#include "gatt_tmas_server_msg_handler.h"
#include "gatt_tmas_server_common.h"
#include "gatt_tmas_server.h"
/******************************************************************************/
ServiceHandle tmasServiceHandle;

ServiceHandle GattTmasServerInit(
                     Task    theAppTask,
                     uint16  startHandle,
                     uint16  endHandle,
                     GattTmasInitData* initData)
{
    GTMAS *gattTmasServerInst = NULL;
    gatt_manager_server_registration_params_t regParams;

    if (theAppTask == NULL)
    {
        GATT_TMAS_SERVER_PANIC(("Application Task NULL\n"));
    }

    tmasServiceHandle = ServiceHandleNewInstance((void **) &gattTmasServerInst, sizeof(GTMAS));

    if (gattTmasServerInst)
    {
        /* Reset all the service library memory */
        memset(gattTmasServerInst, 0, sizeof(GTMAS));

        /* Set up library handler for external messages */
        gattTmasServerInst->libTask.handler = tmasServerGattMsgHandler;
        /* Store the Task function parameter.
         * All library messages need to be sent here */
        gattTmasServerInst->appTask = theAppTask;
        gattTmasServerInst->srvcHndl = tmasServiceHandle;

       /* Initiliasation of the TMAS Charateristics  and parameters*/
        gattTmasServerInst->data.role = initData->role;
        /* Reset the client data memory */
        memset(gattTmasServerInst->data.connectedClients, 0, (sizeof(gattTmasClientData) * GATT_TMAS_MAX_CONNECTIONS));

        /* Setup data required for TMAS to be registered with the GATT Manager */
        regParams.start_handle = startHandle;
        regParams.end_handle = endHandle;
        regParams.task = &gattTmasServerInst->libTask;

        /* Register with the GATT Manager and verify the result */
        if (GattManagerRegisterServer(&regParams) == gatt_manager_status_success)
        {
            return tmasServiceHandle;
        }
        else
        {
            GATT_TMAS_SERVER_PANIC(("Register with the GATT Manager failed!\n"));
            /* If the registration with GATT Manager fails and we have allocated memory
             * for the new instance successfully (service handle not zero), we have to free
             * the memnory of that instance.
             */
            if (tmasServiceHandle)
            {
                ServiceHandleFreeInstanceData(tmasServiceHandle);
            }
            return 0;
        }
    }
    else
    {
        GATT_TMAS_SERVER_PANIC(("Memory alllocation of TMAS Server instance failed!\n"));
    }
    return 0;
}

/******************************************************************************/
gatt_status_t GattTmasServerAddConfig(ServiceHandle srvcHndl,
                                 connection_id_t  cid)
{
    uint8 i;
    GTMAS* gattTmasServerInst = (GTMAS*)ServiceHandleGetInstanceData(srvcHndl);

    if (gattTmasServerInst == NULL)
    {
        GATT_TMAS_SERVER_DEBUG_INFO(("/nTMAS: is NULL/n"));
        return gatt_status_invalid_handle;
    }

    for (i = 0; i < GATT_TMAS_MAX_CONNECTIONS; i++)
    {
        if (gattTmasServerInst->data.connectedClients[i].cid == 0)
        {
            /* Add client btConnId to the server data */
            gattTmasServerInst->data.connectedClients[i].cid = cid;

            return gatt_status_success;
        }
    }

    return gatt_status_insufficient_resources;
}

/******************************************************************************/
void GattTmasServerRemoveConfig(ServiceHandle srvcHndl,
                               connection_id_t  cid)
{
    uint8 i;
    GTMAS* gattTmasServerInst = (GTMAS*)ServiceHandleGetInstanceData(srvcHndl);

    if (gattTmasServerInst == NULL)
    {
        GATT_TMAS_SERVER_DEBUG_INFO(("\n TMAS: NULL instance \n"));
        return;
    }

    for (i = 0; i < GATT_TMAS_MAX_CONNECTIONS; i++)
    {
        /* Check the saved CID to find the peeer device */
        if (gattTmasServerInst->data.connectedClients[i].cid == cid)
        {
            if ((i == (GATT_TMAS_MAX_CONNECTIONS - 1)) || (i == 0 && gattTmasServerInst->data.connectedClients[i + 1].cid == 0))
            {
                /* The peer device is the only or the last element of the array */
                memset(&(gattTmasServerInst->data.connectedClients[i]), 0, sizeof(gattTmasClientData));
            }
            else
            {
                /* The peer device is in the middle of the array */
                uint8 j;

                for (j = i; j < (GATT_TMAS_MAX_CONNECTIONS - 1) && gattTmasServerInst->data.connectedClients[j + 1].cid != 0; j++)
                {
                    /* Shift all the elements of the array of one position behind */
                    memmove(&(gattTmasServerInst->data.connectedClients[j]),
                        &(gattTmasServerInst->data.connectedClients[j + 1]),
                             sizeof(gattTmasClientData));
                }

                /* Remove the last element of the array, already shifted behind */
                memset(&(gattTmasServerInst->data.connectedClients[j]), 0, sizeof(gattTmasClientData));
            }
        }
    }
}


/******************************************************************************/
bool GattTmasServerSetRole(ServiceHandle srvcHndl, uint16 role)
{
    GTMAS *tmasServer = (GTMAS *) ServiceHandleGetInstanceData(srvcHndl);

    if (tmasServer && (role != tmasServer->data.role))
    {
        /* The role value to set is valid and different from the actual one*/
        tmasServer->data.role = role;

        return TRUE;
    }
    return FALSE;
}

uint16 GattTmasServerGetRole(ServiceHandle srvcHndl)
{
    GTMAS *tmasServer = (GTMAS *) ServiceHandleGetInstanceData(srvcHndl);
    uint16 role = tmasServer ? tmasServer->data.role : 0;
    return role;
}
