/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\addtogroup device_database_serialiser
\brief      PDDU operations.
 
Internal module to be used only inside of device_db_serialiser component.
 
*/

#ifndef DEVICE_DB_SERIALISER_PDDU_H_
#define DEVICE_DB_SERIALISER_PDDU_H_

#include "device_db_serialiser.h"

/*! @{ */

#define SIZE_OF_TYPE            0x1
#define SIZE_OF_LEN             0x1
#define SIZE_OF_TYPE_AND_LEN    (SIZE_OF_TYPE + SIZE_OF_LEN)

#define TYPE_OFFSET_IN_FRAME    0x0
#define LEN_OFFSET_IN_FRAME     SIZE_OF_TYPE
#define PAYLOAD_OFFSET_IN_FRAME SIZE_OF_TYPE_AND_LEN

#define DBS_PDD_FRAME_TYPE      0xDB

#define SIZE_OF_METADATA 2

/*! \brief Stores the function pointers for a registered PDDU */
typedef struct
{
    unsigned id;
    get_persistent_device_data_len get_len;
    serialise_persistent_device_data ser;
    deserialise_persistent_device_data deser;

} device_db_serialiser_registered_pddu_t;

/*! \brief  Init the PDDU module.
*/
void DeviceDbSerialiser_PdduInit(void);

/*! \brief  Get the number of registered PDDUs.

    \return The number of registered PDDUs.
*/
uint8 DeviceDbSerialiser_GetNumOfRegisteredPddu(void);

/*  \brief Retrieve the PDDU callbacks by id.

    \param id Id of PDDU.

    \return The structure containing PDDU callbacks.
*/
device_db_serialiser_registered_pddu_t *DeviceDbSerialiser_GetRegisteredPddu(uint8 id);

/*  \brief Get a list of PDDU lengths for given device.

    It is to be used with DeviceDbSerialiser_SumAllPdduPayloads() and
    DeviceDbSerialiser_PopulatePddFrame().

    \param device A device for which list of PDDU lengths shall be retrieved.

    \return The list of PDDU lengths.
*/
uint8 *DeviceDbSerialiser_GetAllPddusPayloadLengths(device_t device);

/*  \brief Get total length of listed PDDUs.

    \param pddus_payload_lengths The list of PDDU lengths obtained from
                                 DeviceDbSerialiser_GetRegisteredPddu().

    \return Total length of listed PDDUs.
*/
uint16 DeviceDbSerialiser_SumAllPdduPayloads(uint8 *pddus_payload_lengths);

/*  \brief Populate buffer with PDDU data.

    \param device       The device containing PDDUs data.

    \param pdd_frame    Destination buffer.

    \param pddus_len    The list of PDDU lengths obtained from
                        DeviceDbSerialiser_GetRegisteredPddu().
*/
void DeviceDbSerialiser_PopulatePddFrame(device_t device, uint8 *pdd_frame, uint8 *pddus_len);



/*! @} */

#endif /* DEVICE_DB_SERIALISER_PDDU_H_ */
