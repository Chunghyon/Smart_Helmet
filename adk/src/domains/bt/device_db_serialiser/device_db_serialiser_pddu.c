/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      PDDU operations.

Internal module to be used only inside of device_db_serialiser component.

*/

#include "device_db_serialiser_pddu.h"

#include <panic.h>

static device_db_serialiser_registered_pddu_t *registered_pddu_list = NULL;

static uint8 num_registered_pddus = 0;

void DeviceDbSerialiser_PdduInit(void)
{
    registered_pddu_list = NULL;
    num_registered_pddus = 0;
}

uint8 DeviceDbSerialiser_GetNumOfRegisteredPddu(void)
{
    return num_registered_pddus;
}

void DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        unsigned pddu_id,
        get_persistent_device_data_len get_len,
        serialise_persistent_device_data ser,
        deserialise_persistent_device_data deser)
{
    PanicFalse(get_len);
    PanicFalse(ser);
    PanicFalse(deser);

    if (!registered_pddu_list)
    {
       registered_pddu_list = (device_db_serialiser_registered_pddu_t*)PanicUnlessMalloc(sizeof(device_db_serialiser_registered_pddu_t));
    }
    else
    {
        registered_pddu_list = realloc(registered_pddu_list, sizeof(device_db_serialiser_registered_pddu_t)*(num_registered_pddus + 1));
        PanicNull(registered_pddu_list);
    }

    registered_pddu_list[num_registered_pddus].id = pddu_id;
    registered_pddu_list[num_registered_pddus].get_len = get_len;
    registered_pddu_list[num_registered_pddus].ser = ser;
    registered_pddu_list[num_registered_pddus].deser = deser;

    num_registered_pddus++;
}

uint8 * DeviceDbSerialiser_GetAllPddusPayloadLengths(device_t device)
{
    uint8 *pddus_len = (uint8 *)PanicUnlessMalloc(num_registered_pddus*sizeof(uint8));

    for (int i=0; i<num_registered_pddus; i++)
    {
        uint8 pdd_len = registered_pddu_list[i].get_len(device);
        if (pdd_len)
            pdd_len += SIZE_OF_TYPE_AND_LEN;
        pddus_len[i] = pdd_len;
    }

    return pddus_len;
}

uint16 DeviceDbSerialiser_SumAllPdduPayloads(uint8 *pddus_payload_lengths)
{
    uint8 sum_of_individual_pddu_frames = 0;
    for (int pddu_index=0; pddu_index<num_registered_pddus; pddu_index++)
    {
        sum_of_individual_pddu_frames += pddus_payload_lengths[pddu_index];
    }
    return sum_of_individual_pddu_frames;
}

inline static void deviceDbSerialiser_addPdduData(device_t device, device_db_serialiser_registered_pddu_t *pddu, uint8 *payload, uint8 len)
{
    payload[TYPE_OFFSET_IN_FRAME] = pddu->id;
    payload[LEN_OFFSET_IN_FRAME] = len;
    pddu->ser(device, &payload[PAYLOAD_OFFSET_IN_FRAME], 0);
}

void DeviceDbSerialiser_PopulatePddFrame(device_t device, uint8 * pdd_frame, uint8 *pddus_len)
{
    uint8 offset = 0;
    uint8 * payload = pdd_frame + PAYLOAD_OFFSET_IN_FRAME;

    for (int i=0; i<num_registered_pddus; i++)
    {
        device_db_serialiser_registered_pddu_t *curr_pddu = &registered_pddu_list[i];

        if (pddus_len[i])
        {
            deviceDbSerialiser_addPdduData(device, curr_pddu, &payload[offset], pddus_len[i]);
            offset += pddus_len[i];
        }
    }
}

device_db_serialiser_registered_pddu_t *DeviceDbSerialiser_GetRegisteredPddu(uint8 id)
{
    device_db_serialiser_registered_pddu_t * pddu = NULL;
    if (registered_pddu_list && num_registered_pddus)
    {
        for (int i=0; i<num_registered_pddus; i++)
        {
            if (registered_pddu_list[i].id == id)
            {
                pddu = &registered_pddu_list[i];
                break;
            }
        }
    }
    return pddu;
}
