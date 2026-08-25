/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    le_advertising_manager
\brief      Utility functions for managing advertising data in the format needed for the extended advertising APIs.
*/

#include "le_advertising_manager_data_packet.h"

#include <logging.h>
#include <panic.h>






le_advertising_manager_data_packet_t *LeAdvertisingManager_DataPacketCreateDataPacket(uint8 max_size)
{
    le_advertising_manager_data_packet_t* new_packet = NULL;

    new_packet = PanicUnlessMalloc(sizeof(*new_packet));
    memset(new_packet, 0, sizeof(*new_packet));
    new_packet->data_max_size = max_size;

    /* TBD: check max_size is either 31, for legacy adverts, or 251, for extended adverts? */

    DEBUG_LOG_VERBOSE("leAdvertisingManager_createNewExtendedDataPacket new_packet %p", new_packet);

    return new_packet;
}

bool LeAdvertisingManager_DataPacketDestroy(le_advertising_manager_data_packet_t *packet)
{
    DEBUG_LOG_VERBOSE("leAdvertisingManager_destroyExtendedDataPacket packet %p", packet);

    if (packet)
    {
        for (uint8 i = 0; i < ARRAY_DIM(packet->data); i++)
        {
            free(packet->data[i]);
        }

        free(packet);
    }

    return TRUE;
}

void LeAdvertisingManager_DataPacketReset(le_advertising_manager_data_packet_t *packet)
{
    packet->data_size = 0;
    memset(packet->data, 0, sizeof(packet->data));
}

bool LeAdvertisingManager_DataPacketAddDataItem(le_advertising_manager_data_packet_t* packet, const le_adv_item_data_t* item)
{
    PanicNull(packet);
    PanicNull((le_adv_item_data_t*)item);

    DEBUG_LOG("leAdvertisingManager_AddDataItemToExtendedPacket packet_size %u item_size %u",
              packet->data_size, item->size);

    if (((unsigned)packet->data_max_size - packet->data_size) < item->size)
    {
        return FALSE;
    }

    if(item->size)
    {
        uint8 data_to_copy = item->size;
        const uint8 *data_ptr = item->data;

        while (data_to_copy)
        {
            int current_buffer_idx = (packet->data_size / MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
            size_t current_buffer_pos = (packet->data_size % MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
            size_t current_buffer_space = (MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS - current_buffer_pos);

            if (!packet->data[current_buffer_idx])
            {
                packet->data[current_buffer_idx] = PanicUnlessMalloc(MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
                memset(packet->data[current_buffer_idx], 0, MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);
            }

            uint8 *current_buffer_ptr = &packet->data[current_buffer_idx][current_buffer_pos];
            uint8 size = (data_to_copy > current_buffer_space) ? current_buffer_space : data_to_copy;

            memmove(current_buffer_ptr, data_ptr, size);
            packet->data_size += size;

            data_ptr += size;
            data_to_copy -= size;
        }
    }

    return TRUE;
}

unsigned LeAdvertisingManager_DataPacketGetSize(le_advertising_manager_data_packet_t *packet)
{
    return (packet ? packet->data_size : 0);
}

void LeAdvertisingManager_DataPacketDebugLogData(le_advertising_manager_data_packet_t *packet)
{
    if (packet && packet->data_size)
    {
        for (uint8 i = 0; i < ARRAY_DIM(packet->data); i++)
        {
            if (packet->data[i])
            {
                size_t current_buffer_size = (packet->data_size >= ((i + 1) * MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS )) ?
                            MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS : (packet->data_size % MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS);

                DEBUG_LOG_DATA_VERBOSE(packet->data[i], current_buffer_size);
            }
            else
            {
                break;
            }
        }
    }
}
