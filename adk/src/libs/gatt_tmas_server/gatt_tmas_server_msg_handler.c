/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/
#include "gatt_tmas_server_common.h"
#include "gatt_tmas_server_access.h"
#include "gatt_tmas_server_msg_handler.h"
#include "gatt_tmas_server_debug.h"


/******************************************************************************/
void tmasServerGattMsgHandler(Task task, MessageId id, Message msg)
{
    GTMAS *tmasServer = (GTMAS *)task;

    switch (id)
    {
        case GATT_MANAGER_SERVER_ACCESS_IND:
        {
            tmasServerHandleAccessIndication(
                                        tmasServer,
                                        (GATT_MANAGER_SERVER_ACCESS_IND_T *)msg
                                        );
        }
        break;

        case GATT_MANAGER_REMOTE_CLIENT_NOTIFICATION_CFM:
        {
            /* Nothing to do */
        }
        break;

        default:
        {
            GATT_TMAS_SERVER_PANIC((
                        "TMAS Server: GATT Manager message 0x%04x not handled\n",
                        id
                        ));
            break;
        }
    } /* switch */
}
