/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    le_advertising_manager
\brief      Utility functions for managing advertising data in the format needed for the extended advertising APIs.

The data packet is structured as a set of up to eight, thrty-two octet buffers.
This is the format expected by ConnectionDmBleExtAdvSetDataReq and
ConnectionDmBleExtAdvSetScanRespDataReq.

When the data packet is created the client sets the maximum size of data
(number of octetst) the packet can contain. This should be one of:

- 31 : for legacy advertising PDUs
- 251 : for extended advertising PDUs
*/

#ifndef LE_ADVERTISING_MANAGER_EXTENDED_DATA_PACKET_H
#define LE_ADVERTISING_MANAGER_EXTENDED_DATA_PACKET_H

#include "le_advertising_manager.h"

#include <stdlib.h>


/*! Maximum data length of an extended advert (as supported by connection library). */
#define MAX_EXT_AD_DATA_SIZE_IN_OCTETS  (251u)

/*! Maximum number of extended advert data buffers (as supported by connection library). */
#define MAX_EXT_AD_DATA_BUFFER_COUNT    (8u)

/*! Maximum data length of a single extended advert buffer (as supported by the connection library). */
#define MAX_EXT_AD_DATA_BUFFER_SIZE_IN_OCTETS   (32u)


typedef struct
{
    uint8 data_size;
    uint8 data_max_size;
    uint8 *data[MAX_EXT_AD_DATA_BUFFER_COUNT];
} le_advertising_manager_data_packet_t;

/*! \brief Create a new empty advertising data packet.

    \param max_size Maximum size of data this packet can hold.
                    Suggested values : 31 for legacy advertising PDUs
                                       251 for extended advertisinf PDUs

    \return Pointer to an empty data packet, or NULL if it could not be created.
*/
le_advertising_manager_data_packet_t *LeAdvertisingManager_DataPacketCreateDataPacket(uint8 max_size);

/*! \brief Destroy an advertising data packet.

    Destroying the packet will free any memory buffers that it points to.

    After this is called the pointer to the data packet will be invalid.

    \param packet Data packet to destroy.

    \return TRUE if the memory was freed ok.
*/
bool LeAdvertisingManager_DataPacketDestroy(le_advertising_manager_data_packet_t *packet);

/*! \brief Reset an advertising data packet.

    This should be used instead of #LeAdvertisingManager_DataPacketDestroy when
    the ownership of the data buffers in the packet is passed onto another
    piece of code. It does not free any of the memory used for the data
    buffer(s); instead it sets the pointers to NULL.

    For example, ConnectionDmBleExtAdvSetDataReq takes ownership of the data
    buffer(s) passed in to it. After that is called use this function to
    reset the data packet to avoid deleting the buffer(s) twice.

    \param packet Data packet to reset.
*/
void LeAdvertisingManager_DataPacketReset(le_advertising_manager_data_packet_t *packet);

/*! \brief Add a data item to a data packet.

    \param packet Data packet to add data item to.
    \param item Data item to add.

    \return TRUE if the data item was added successfully.
            FALSE if there was not enough room in the packet to add the data
            item.
*/
bool LeAdvertisingManager_DataPacketAddDataItem(le_advertising_manager_data_packet_t* packet, const le_adv_item_data_t* item);

/*! \brief Get the current size of an advertising data packet.

    \param packet Data packet to get the size of.

    \return Current size of the data packet in octets.
*/
unsigned LeAdvertisingManager_DataPacketGetSize(le_advertising_manager_data_packet_t *packet);

/*! \brief Log the contents of an advertising data packet to the debug log

    Note: The log is written at the DEBUG_LOG_LEVEL_VERBOSE level.

    \param packet Data packet to log.
*/
void LeAdvertisingManager_DataPacketDebugLogData(le_advertising_manager_data_packet_t *packet);

#endif // LE_ADVERTISING_MANAGER_EXTENDED_DATA_PACKET_H
