/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   device_database_serialiser Device Database Serialiser
\ingroup    bt_domain
\brief	    Interface for the Device Database Serialiser module.
*/

#ifndef DEVICE_DATABASE_SERIALISER_H_
#define DEVICE_DATABASE_SERIALISER_H_

#include <csrtypes.h>
#include <device.h>

/*! @{ */

typedef uint8 (*get_persistent_device_data_len)(device_t device);

typedef void (*serialise_persistent_device_data)(device_t device, void *buf, uint8 offset);

typedef void (*deserialise_persistent_device_data)(device_t device, void *buf, uint8 data_length, uint8 offset);

/*! \brief Initialise the Device Database Serialiser.
*/
void DeviceDbSerialiser_Init(void);

/*! \brief This function is used to register a Persistent Device Data User module
         with the Device Database Serialiser.

    \param pddu_id  Identifier for the PDDU being registered

    \param get_len  Pointer to a function that the Device Database Serialiser shall use
                    to get the length (in bytes) of this PDDU's Persistent Device Data.

    \param ser      Pointer to a function that the Device Database Serialiser shall call
                    to cause the PDDU to serialise its Persistent Device Data into a
                    provided buffer.

    \param deser    Pointer to a function that the Device Database Serialiser shall call
                    to cause the PDDU to deserialise its Persistent Device Data from a
                    provided buffer and offset bit index.
*/
void DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        unsigned pddu_id,
        get_persistent_device_data_len get_len,
        serialise_persistent_device_data ser,
        deserialise_persistent_device_data deser);

/*! \brief Serialise the set of Persistent Device Data.
*/
void DeviceDbSerialiser_Serialise(void);

/*! \brief Serialise the set of Persistent Device Data for specified device only.

    \param device   Device for which PDD should be serialised.

    \return TRUE if serialisation was successful.
*/
bool DeviceDbSerialiser_SerialiseDevice(device_t device);

/*! \brief Deserialise the set of Persistent Device Data.
*/
void DeviceDbSerialiser_Deserialise(void);

/*! \brief Serialise the device after a delay.
    \param device Device to serialise.
    \param delay The delay after which to serialise. A positive delay
    sets a lazy deadline. A negative delay sets a strict deadline.

    A lazy deadline means the serialisation will, at the earliest, occur after
    the defined delay. But if the function is called again before the deadline
    the serialisation will be deferred if the second deadline is later.
    For example, consider the following sequence:
    1. Function called at t=0 with delay=5
    2. Function called at t=4 with delay=5
    The serialisation will occur at t=9.

    A strict deadline means the serialisation will, at the latest, occur after
    the defined delay even if the function is called again.
    For example, consider the following sequence:
    1. Function called at t=0 with delay=-5
    2. Function called at t=4 with delay=-5
    The serialisation will occur at t=5. It will not occur again at t=9.

    If a strict deadline has been set, and a lazy deadline is then set,
    the strict deadline will be honoured.

    If the device is serialised using other functions in this component
    the delayed serialise will be cancelled.
*/
void DeviceDbSerialiser_SerialiseDeviceLater(device_t device, int32 delay);

/*! @} */

#endif /* DEVICE_DATABASE_SERIALISER_H_ */
