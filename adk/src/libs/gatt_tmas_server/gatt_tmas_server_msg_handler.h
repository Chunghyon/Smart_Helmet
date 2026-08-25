/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/

#ifndef GATT_TMAS_SERVER_MSG_HANDLER_H_
#define GATT_TMAS_SERVER_MSG_HANDLER_H_


/***************************************************************************
NAME
    tmasServerGattMsgHandler

DESCRIPTION
    Handler for external messages sent to the library in the server role.
*/
void tmasServerGattMsgHandler(Task task, MessageId id, Message msg);

#endif /* GATT_TMAS_SERVER_MSG_HANDLER_H_ */

