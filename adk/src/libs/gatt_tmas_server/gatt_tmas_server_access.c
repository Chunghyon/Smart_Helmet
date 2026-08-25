/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/
#include "gatt_tmas_server_access.h"
#include "gatt_tmas_server_private.h"

/***************************************************************************
NAME
    tmassServerHandleRoleAccess

DESCRIPTION
    Deals with access of the HANDLE_ROLE handle.
*/
static void tmassServerHandleRoleAccess(
        GTMAS *const tmasServer,
        GATT_MANAGER_SERVER_ACCESS_IND_T *const accessInd
        )
{
    if (accessInd->flags & ATT_ACCESS_READ)
    {
        uint8* value = NULL;
        value = (uint8*) PanicUnlessMalloc(GATT_TMAS_SERVER_ROLE_SIZE);

        value[0] = (tmasServer->data.role & 0xff);
        value[1] = ((tmasServer->data.role >> 8) & 0xff);

        tmasServerSendAccessRsp(
                (Task)&tmasServer->libTask,
                accessInd->cid,
                accessInd->handle,
                gatt_status_success,
                GATT_TMAS_SERVER_ROLE_SIZE,
                value
                );
    }
    else
    {
        tmasServerSendAccessErrorRsp(
                (Task)&tmasServer->libTask,
                accessInd->cid,
                accessInd->handle,
                gatt_status_request_not_supported
                );
    }
}

/***************************************************************************/
void tmasServerHandleAccessIndication(
        GTMAS *tmasServer,
        GATT_MANAGER_SERVER_ACCESS_IND_T *const accessInd
        )
{
    switch (accessInd->handle)
    {
        case HANDLE_ROLE:
        {
            GATT_TMAS_SERVER_DEBUG_INFO(("HANDLE_ROLE\n"));
            tmassServerHandleRoleAccess(
                    tmasServer,
                    accessInd
                    );
        }
        break;

        default:
        {
            GATT_TMAS_SERVER_DEBUG_INFO(("Unhandled access indication handle:%d\n", accessInd->handle));
        }
        break;
    }
}
