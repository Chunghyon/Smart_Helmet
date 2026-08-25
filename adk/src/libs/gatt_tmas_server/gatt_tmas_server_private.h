/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/

#ifndef GATT_TMAS_SERVER_PRIVATE_H
#define GATT_TMAS_SERVER_PRIVATE_H

#include "gatt_tmas_server.h"

/* Client data. This structure contains data for each connected client */
typedef struct
{
    connection_id_t      cid;
} gattTmasClientData;

/* Definition of data required for association. */
typedef struct
{
    uint16 role;
    gattTmasClientData  connectedClients[GATT_TMAS_MAX_CONNECTIONS];
} gattTmasData;

/* The TMA service internal structure for the server role. */
typedef struct __GTMAS
{
    TaskData libTask;
    Task     appTask;

    /* Service handle provided by the service_handle lib when the server
     * memory instance is created
     */
    ServiceHandle srvcHndl;

    /* Inizalisation parameters. */
    gattTmasData data;
} GTMAS;

#define MAKE_TMAS_MESSAGE(TYPE) \
    TYPE * message = (TYPE *)CsrPmemZalloc(sizeof(TYPE))

/* Assumes message struct with
 *    uint16 size_value;
 *    uint8 value[1];
 */
#define MAKE_TMAS_MESSAGE_WITH_LEN_U8(TYPE, LEN) MAKE_TMAS_MESSAGE(TYPE)

#define MAKE_TMAS_MESSAGE_WITH_VALUE(TYPE, SIZE, VALUE) \
        MAKE_TMAS_MESSAGE_WITH_LEN_U8(TYPE, SIZE);          \
        CsrPmemCpy(message->value, (VALUE), (SIZE));           \
        message->size_value = (SIZE)

#endif /* GATT_TMAS_SERVER_PRIVATE_H */
