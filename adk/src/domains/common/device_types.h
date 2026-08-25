/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       device.types.h
\addtogroup common_domain
\brief      Header file for types of devices
*/

#ifndef BT_DEVICE_TYPES_H
#define BT_DEVICE_TYPES_H

/*! @{ */

/*! /brief Common devcie types */
typedef enum
{
    DEVICE_TYPE_UNKNOWN = 0, /*!< Remote device type is unknown (default) */
    DEVICE_TYPE_EARBUD,      /*!< Remote device is an earbud */
    DEVICE_TYPE_HANDSET,     /*!< Remote device is a handset */
    DEVICE_TYPE_SELF,        /*!< This device is an earbud - and it's me */
    DEVICE_TYPE_SINK,        /*!< Remote device is a Sink device */
    DEVICE_TYPE_HANDSET_LE,  /*!< Remote device is an LE handset */
    DEVICE_TYPE_MAX,        /*!< The maximum number of device types */

} deviceType;

/*! @} */

#endif // BT_DEVICE_TYPES_H
