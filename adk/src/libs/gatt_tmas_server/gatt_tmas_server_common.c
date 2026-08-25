/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/

#include "gatt_tmas_server_common.h"
#include "gatt_tmas_server_debug.h"


void tmasServerSendAccessRsp(
        Task task,
        connection_id_t cid,
        uint16 handle,
        uint16 result,
        uint16 sizeValue,
        uint8 *const value
        )
{
    if (!GattManagerServerAccessResponse(
                task,
                cid,
                handle,
                result,
                sizeValue,
                value
                )
       )
    {
        GATT_TMAS_SERVER_DEBUG_PANIC((
                    "Couldn't send GattManagerServerAccessRsp\n"
                    ));

    }
}
