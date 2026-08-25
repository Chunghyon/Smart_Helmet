/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Implementation of the GAP Server module.
*/

#include "gatt_server_gap.h"

#include "gatt_server_gap_advertising.h"
#include "gatt_handler_db_if.h"
#include "local_name.h"

#include <gatt_gap_server.h>
#include <logging.h>

#include <panic.h>

#define GAP_DEVICE_APPEARANCE_UNKNOWN   { 0x00, 0x00 }
#define GAP_DEVICE_APPEARANCE_CHARACTERISTIC_LENGTH 2

/*! Structure holding information for the application handling of GAP Server */
typedef struct
{
    /*! Task for handling GAP related messages */
    TaskData    gap_task;
    /*! GAP server library data */
    GGAPS       gap_server;
    /*! Complete local name flag */
    bool        complete_local_name;
} gattServerGapData;

static gattServerGapData gatt_server_gap = {0};

/*! Get pointer to the main GAP server Task */
#define GetGattServerGapTask() (&gatt_server_gap.gap_task)

/*! Get pointer to the GAP server data passed to the library */
#define GetGattServerGapGgaps() (&gatt_server_gap.gap_server)

/*! Get the complete local name flag */
#define GetGattServerGapCompleteName() (gatt_server_gap.complete_local_name)

static void gattServerGap_HandleReadAppearanceInd(const GATT_GAP_SERVER_READ_APPEARANCE_IND_T *ind)
{
    uint8 appearance[GAP_DEVICE_APPEARANCE_CHARACTERISTIC_LENGTH] = GAP_DEVICE_APPEARANCE_UNKNOWN;
    PanicFalse(GetGattServerGapGgaps() == ind->gap_server);

    GattGapServerReadAppearanceResponse(GetGattServerGapGgaps(), ind->cid,
                                        (GAP_DEVICE_APPEARANCE_CHARACTERISTIC_LENGTH * sizeof(uint8)), appearance);
}

static void gattServerGap_HandleReadDeviceNameInd(const GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T *ind)
{
    uint16 name_len;
    const uint8 *name = LocalName_GetPrefixedName(&name_len);
    PanicNull((void*)name);

    /* The indication can request a portion of our name by specifying the start offset */
    if (ind->name_offset)
    {
        /* Check that we haven't been asked for an entry off the end of the name */

        if (ind->name_offset >= name_len)
        {
            name_len = 0;
            name = NULL;
        /*  \todo return gatt_status_invalid_offset  */
        }
        else
        {
            name_len -= ind->name_offset;
            name += ind->name_offset;
        /*  \todo return gatt_status_success  */
        }
    }

    PanicFalse(GetGattServerGapGgaps() == ind->gap_server);

    GattGapServerReadDeviceNameResponse(GetGattServerGapGgaps(), ind->cid,
                                        name_len, (uint8*)name);
}

static void gattServerGap_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("gattServerGap_MessageHandler MESSAGE:0x%x", id);

    switch (id)
    {
        case GATT_GAP_SERVER_READ_DEVICE_NAME_IND:
            gattServerGap_HandleReadDeviceNameInd((const GATT_GAP_SERVER_READ_DEVICE_NAME_IND_T*)message);
            break;
        case GATT_GAP_SERVER_READ_APPEARANCE_IND:
            gattServerGap_HandleReadAppearanceInd((const GATT_GAP_SERVER_READ_APPEARANCE_IND_T*)message);
            break;

        default:
            DEBUG_LOG("gattServerGap_MessageHandler. Unhandled message MESSAGE:0x%x", id);
            break;
    }
}

/*****************************************************************************/
bool GattServerGap_Init(Task init_task)
{
    UNUSED(init_task);

    memset(&gatt_server_gap, 0, sizeof(gatt_server_gap));

    gatt_server_gap.gap_task.handler = gattServerGap_MessageHandler;

    if (GattGapServerInit(GetGattServerGapGgaps(), GetGattServerGapTask(),
            HANDLE_GAP_SERVICE, HANDLE_GAP_SERVICE_END)
                            != gatt_gap_server_status_success)
    {
        DEBUG_LOG("gattServerGap_init Server failed");
        Panic();
    }
    GattServerGap_SetupLeAdvertisingData();

    DEBUG_LOG("GattServerGap_Init. Server initialised");

    return TRUE;
}

void GattServerGap_UseCompleteLocalName(bool complete)
{
    gatt_server_gap.complete_local_name = complete;
}

bool GattServerGap_IsCompleteLocalNameBeingUsed(void)
{
    return gatt_server_gap.complete_local_name;
}
