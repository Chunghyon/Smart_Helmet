/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gatt_server_dis.c
\brief      Implementation of the GATT Device Information Service Server module.
*/

#ifdef INCLUDE_GATT_DEVICE_INFO_SERVER

#include "gatt_server_dis.h"

#include "gatt_server_dis_advertising.h"
#include "gatt_handler.h"
#include "gatt_connect.h"
#include "gatt_handler_db_if.h"

#include "device_info.h"

#include <hydra_macros.h>
#include <logging.h>
#include <gatt.h>
#include <string.h>
#include <stdlib.h>
#include <panic.h>

/* Contains device information service supported characteristics data */
static gatt_dis_init_params_t device_info_server_params;

/* App passes reference of gatt_server_device_info to library via GattDeviceInfoServerInit.
library reset this struct so why device_info_server_params is required to pass data separately. */
gattServerDeviceInfoData gatt_server_device_info = {0};

static void gattServerDeviceInfo_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(id);
    UNUSED(message);

    DEBUG_LOG_WARN("gattServerDeviceInfo_MessageHandler MESSAGE:0x%x", id);
}

static bool gattServerDeviceInfo_InitialiseDisParams(void)
{
    bool initialised = FALSE;
    DEBUG_LOG_FN_ENTRY("gattServerDeviceInfo_InitialiseDisParams");

    device_info_server_params.dis_strings = calloc (1, sizeof(gatt_dis_strings_t));

    if (device_info_server_params.dis_strings != NULL)
    {
        device_info_server_params.dis_strings->manufacturer_name_string = DeviceInfo_GetManufacturer();
        device_info_server_params.dis_strings->model_num_string = DeviceInfo_GetModelId();
        device_info_server_params.dis_strings->serial_num_string = DeviceInfo_GetSerialNumber();
        device_info_server_params.dis_strings->hw_revision_string = DeviceInfo_GetHardwareVersion();
        device_info_server_params.dis_strings->fw_revision_string = DeviceInfo_GetFirmwareVersion();
        device_info_server_params.dis_strings->sw_revision_string = DeviceInfo_GetSoftwareVersion();

        initialised = TRUE;
    }
    device_info_server_params.ieee_data = NULL;
    device_info_server_params.pnp_id = NULL;
    device_info_server_params.system_id = NULL;

    return initialised;
}

static void gattServerDeviceInfo_init(void)
{
    DEBUG_LOG_FN_ENTRY("gattServerDeviceInfo_init Server Init");

    gatt_server_device_info.gatt_device_info_task.handler = gattServerDeviceInfo_MessageHandler;

    /* Read the Device Information Service server data to be initialised */
    if(gattServerDeviceInfo_InitialiseDisParams())
    {
        if (!GattDeviceInfoServerInit(GetGattServerDeviceInfoTask(),
                                      GetGattServerDeviceInfoGdis(),
                                      &device_info_server_params,
                                      HANDLE_DEVICE_INFORMATION_SERVICE,
                                      HANDLE_DEVICE_INFORMATION_SERVICE_END))
        {
            // Don't free any of the initialisation parameters in case they were related to the failure
            DEBUG_LOG_ERROR("gattServerDeviceInfo_init Server failed");
            Panic();
        }
        GattServerDis_SetupLeAdvertisingData();
    }
}

/*****************************************************************************/
bool GattServerDeviceInfo_Init(Task init_task)
{
    UNUSED(init_task);

    gattServerDeviceInfo_init();

    DEBUG_LOG("GattServerDeviceInfo_Init. Device Info Service Server initialised");

    return TRUE;
}

#endif /* INCLUDE_GATT_DEVICE_INFO_SERVER */
