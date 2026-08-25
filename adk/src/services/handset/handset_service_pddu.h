/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    handset_service
\brief      Header for the Persistent Device Data User (PDDU) implemenatation of the Handset Service.
*/
#ifndef HANDSET_SERVCE_PDDU_H
#define HANDSET_SERVCE_PDDU_H

/*! \brief Configuration of handset service. */
typedef struct
{
    unsigned max_bredr_connections : 2;
    unsigned acl_connect_attempt_limit : 3;
    /* Number of Bluetooth Low Energy connections allowed.
       Must be greater than 1. If Bluetooth Low Energy is not
       required, changes should be made in Topology or the
       application. */
    unsigned max_le_connections : 4;
    bool enable_connection_barge_in : 1;
    /* Page interval, this is a power of 2 which is then scaled by 500ms
       to give a reconnection interval range between a minimum value of 500ms
       and a maximum of approximately 4.5hours. */
    unsigned page_interval : 4;
    /* A flag indicating whether the acl_connect_attempt_limit should be
       ignored and instead the Handset Service should perform an unlimited
       number of ACL reconnection attempts. */
    bool enable_unlimited_acl_reconnection : 1;
    bool unused : 1;
    unsigned page_timeout : 16;
} handset_service_config_v1_t;

/*! \brief Register the Handset Service with the Device Database Serialiser

    Called early in the Application start up to register the Handset Service with the
    Device Database Serialiser so that the service can use the device NVRAM.
*/
void HandsetServicePddu_RegisterPdduInternal(void);

/*! \brief Check whether we need to initialise the Handset Service PDD from a previous ADK
           BT Device PDD

    If the device has been upgraded from a software build based on an ADK version between
    ADK21.1 and ADK22.2, we shall need to initialise the handset service PDD from the BT
    Device PDD (where it was stored in those software versions). This function shall
    therefore create the device_property_hadset_service from the device_property_handset_config.
*/
void HandsetServicePddu_CheckToImportBtDeviceHandsetServiceConfig(void);

#endif // HANDSET_SERVCE_PDDU_H
