/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Backup of earbud properties.

*/

#include "device_db_serialiser_backup.h"
#include "device_db_serialiser_pddu.h"
#include "device_db_serialiser_pskey.h"
#include "device_properties.h"

#include <device_types.h>
#include <ps_key_map.h>

#include <vmtypes.h>
#include <device_list.h>
#include <ps.h>
#include <connection.h>
#include <panic.h>
#include <logging.h>

static uint16 deviceDbSerialiser_GetFrameSizeForDevice(device_t device)
{
    uint16 pdd_frame_payload_len = 0;
    uint8 *pddus_payload_lengths = NULL;

    pddus_payload_lengths = DeviceDbSerialiser_GetAllPddusPayloadLengths(device);
    pdd_frame_payload_len = DeviceDbSerialiser_SumAllPdduPayloads(pddus_payload_lengths);
    pdd_frame_payload_len += SIZE_OF_TYPE_AND_LEN;

    free(pddus_payload_lengths);

    return pdd_frame_payload_len;
}

static void deviceDbSerialiser_PopulateBuffer(device_t device, uint8 *buffer, uint16 size)
{
    uint8 *pddus_payload_lengths = NULL;

    pddus_payload_lengths = DeviceDbSerialiser_GetAllPddusPayloadLengths(device);

    buffer[TYPE_OFFSET_IN_FRAME] = DBS_PDD_FRAME_TYPE;
    buffer[LEN_OFFSET_IN_FRAME] = size;

    DeviceDbSerialiser_PopulatePddFrame(device, buffer, pddus_payload_lengths);

    free(pddus_payload_lengths);
}

void DeviceDbSerialiser_MakeBackup(void)
{
    DEBUG_LOG_VERBOSE("DeviceDbSerialiser_MakeBackup");

    if (DeviceDbSerialiser_GetNumOfRegisteredPddu() == 0)
    {
        return;
    }

    device_t first_device = DeviceDbSerialiser_GetDeviceFromPsKey(device_order_first);

    if(!first_device)
    {
        return;
    }

    uint16 first_size = deviceDbSerialiser_GetFrameSizeForDevice(first_device);


    device_t second_device = DeviceDbSerialiser_GetDeviceFromPsKey(device_order_second);

    if(!second_device)
    {
        return;
    }

    uint16 second_size = deviceDbSerialiser_GetFrameSizeForDevice(second_device);

    DEBUG_LOG_VERBOSE("DeviceDbSerialiser_MakeBackup self_size %d, earbud_size %d", first_size, second_size);

    uint16 num_of_words = PS_SIZE_ADJ(first_size) + PS_SIZE_ADJ(second_size);
    uint16 offset_to_second = PS_SIZE_ADJ(first_size)*sizeof(uint16);

    uint8 *buffer = (uint8 *)PanicUnlessMalloc(num_of_words * sizeof(uint16));
    memset(buffer, 0, num_of_words * sizeof(uint16));

    deviceDbSerialiser_PopulateBuffer(first_device, buffer, first_size);
    deviceDbSerialiser_PopulateBuffer(second_device, &buffer[offset_to_second], second_size);

    uint16 num_of_words_written = PsStore(PS_KEY_EARBUD_DEVICES_BACKUP, buffer, num_of_words);

    DEBUG_LOG_VERBOSE("DeviceDbSerialiser_MakeBackup num_of_words %d, num_of_words_written %d", num_of_words, num_of_words_written);

    free(buffer);
}

void DeviceDbSerialiser_RestoreBackup(void)
{
    uint16 num_of_words = PsRetrieve(PS_KEY_EARBUD_DEVICES_BACKUP, NULL, 0);

    DEBUG_LOG_ALWAYS("DeviceDbSerialiser_RestoreBackup num_of_words %d", num_of_words);

    if(num_of_words > 2)
    {
        uint8 *buffer = PanicUnlessMalloc(num_of_words*sizeof(uint16));

        PsRetrieve(PS_KEY_EARBUD_DEVICES_BACKUP, buffer, num_of_words);

        uint16 first_size = buffer[LEN_OFFSET_IN_FRAME];
        uint16 offset_to_second = PS_SIZE_ADJ(first_size)*sizeof(uint16);
        uint16 second_size = buffer[offset_to_second + LEN_OFFSET_IN_FRAME];

        PsStore(DeviceDbSerialiser_GetAttributePsKey(device_order_first), buffer, PS_SIZE_ADJ(first_size));
        PsStore(DeviceDbSerialiser_GetAttributePsKey(device_order_second), &buffer[offset_to_second], PS_SIZE_ADJ(second_size));

        DEBUG_LOG_ALWAYS("DeviceDbSerialiser_RestoreBackup first buffer");
        DEBUG_LOG_DATA_ERROR(buffer, PS_SIZE_ADJ(first_size)*sizeof(uint16));

        DEBUG_LOG_ALWAYS("DeviceDbSerialiser_RestoreBackup second buffer");
        DEBUG_LOG_DATA_ERROR(&buffer[offset_to_second], PS_SIZE_ADJ(second_size)*sizeof(uint16));

        free(buffer);
    }
}
