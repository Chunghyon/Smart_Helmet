/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/

#ifndef GATT_TMAS_SERVER_COMMON_H
#define GATT_TMAS_SERVER_COMMON_H


#include <csrtypes.h>
#include <message.h>

#include <gatt_manager.h>

#include "gatt_tmas_server_private.h"
#include "gatt_tmas_server_debug.h"
#include "gatt_tmas_server_db.h"

/* TMAS role characteristic size */
#define GATT_TMAS_SERVER_ROLE_SIZE (2)

/* TMAS Server invalid cid index value */
#define GATT_TMAS_SERVER_INVALID_CID_INDEX  (0xFF)


/**************************************************************************
NAME
    tmasServerSendAccessRsp

DESCRIPTION
    Send an access response to the GATT Manager library.
*/
void tmasServerSendAccessRsp(
        Task task,
        connection_id_t cid,
        uint16 handle,
        uint16 result,
        uint16 sizeValue,
        uint8 *const value
        );

/**************************************************************************
NAME
    tmasServerSendAccessErrorRsp

DESCRIPTION
    Send an access response to the GATT Manager library with an error status.
*/

#define tmasServerSendAccessErrorRsp(task, cid, handle, error) \
    tmasServerSendAccessRsp( \
            task, \
            cid, \
            handle, \
            error, \
            0, \
            NULL \
            )

#endif /* GATT_TMAS_SERVER_COMMON_H */
