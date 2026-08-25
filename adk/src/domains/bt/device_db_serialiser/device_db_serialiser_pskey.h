/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\addtogroup device_database_serialiser
\brief      Low-level access to the PDL data.
 
This module is only to be used by the backup functionality on device_db_serialiser.
Functionality of this module is based on knowledge of internal implementation of
the connection and connection manager libraries.
It introduces concept of the first and the second device as an abstraction for
referencing to ps key that are storing BT address and attributes of the first
two devices in the PDL. The assumption is that those two first devices are earbuds.

Note that order here is not priority or anything else exposed by the available APIs,
instead it corresponds to how devices BT address and attributes are stored in the ps keys.
This order is determined by adding (pairing) devices to PDL.
In case of earbuds, earbuds are always the first two devices that are added to PDL.
 
*/

#ifndef DEVICE_DB_SERIALISER_PSKEY_H_
#define DEVICE_DB_SERIALISER_PSKEY_H_

#include <device.h>

/*! @{ */

typedef enum
{
    device_order_first = 0,
    device_order_second
} device_order_t;


/*! \brief  Get device that is the first or second in the PDL.

    \param  device_order First or second device.

    \return Device that is stored in the first or second PDL entry.
*/
device_t DeviceDbSerialiser_GetDeviceFromPsKey(device_order_t device_order);

/*! \brief  Get ps key number where attributes of the first or second device are stored.

    \param  device_order First or second device.

    \return ps key number where attributes of the first or second device are stored.
*/
uint16 DeviceDbSerialiser_GetAttributePsKey(device_order_t device_order);

/*! @} */

#endif /* DEVICE_DB_SERIALISER_PSKEY_H_ */
