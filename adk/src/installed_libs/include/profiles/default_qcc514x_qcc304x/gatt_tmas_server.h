/******************************************************************************
 Copyright (c) 2021 Qualcomm Technologies International, Ltd.
 All Rights Reserved.
 Qualcomm Technologies International, Ltd. Confidential and Proprietary.

 REVISION:      $Revision: #1 $
******************************************************************************/

/*!
@file    gatt_tmas_server.h
@brief   Header file for the GATT TMAS (Telephony Media Audio Service) Server library.

        This file provides documentation for the GATT TMAS server library
        API (library name: gatt_tmas_server).
*/

#ifndef GATT_TMAS_H_
#define GATT_TMAS_H_

#include <service_handle.h>
#include <library.h>
#include <gatt.h>

/* Maximum number of GATT connections */
#define GATT_TMAS_MAX_CONNECTIONS (3)

/*! @brief Definition of data required for the initialisation
 *         of the TMAS Server Library.
 */
typedef struct
{
    uint16                 role;
} GattTmasInitData;


/*!
   @TMAP role values
*/
#define TMAP_ROLE_CALL_GATEWAY                       0x0001
#define TMAP_ROLE_CALL_TERMINAL                      0x0002
#define TMAP_ROLE_UNICAST_MEDIA_SENDER               0x0004
#define TMAP_ROLE_UNICAST_MEDIA_RECEIVER             0x0008
#define TMAP_ROLE_BROADCAST_MEDIA_SENDER             0x0010
#define TMAP_ROLE_BROADCAST_MEDIA_RECEIVER           0x0020

/*! @brief Instantiate the GATT TMAS Server Service Library.

    The GATT Service Init function is responsible for allocating its instance memory
    and returning a unique service handle for that instance. The Service handle is
    then used for the rest of the API.

    @param theAppTask The client task that will receive messages from this Service.
    @param startHandle The first handle in the ATT database for this Service instance.
    @param endHandle The last handle in the ATT database for this Service instance.
    @param initData The initialisation data to set

    @return ServiceHandle If the service handle returned is 0, this indicates a failure
                             during GATT Service initialisation.
*/
ServiceHandle GattTmasServerInit(
                     Task theAppTask,
                     uint16  startHandle,
                     uint16  endHandle,
                     GattTmasInitData* initData);

/*!
    \brief Add configuration for a paired peer device, identified by its
    Connection ID (CID).

    \param srvcHndl Instance handle for the service.
    \param cid The Connection ID to the peer device.

    \return gatt_status_t status of the Add Configuration operation.
*/
gatt_status_t GattTmasServerAddConfig(
                  ServiceHandle srvcHndl,
                  connection_id_t cid);

/*!
    \brief Remove the configuration for a peer device, identified by its
           Connection ID.

    \param srvcHndl The GATT service instance handle.
    \param cid A Connection ID for the peer device.
*/
void GattTmasServerRemoveConfig(
                   ServiceHandle srvcHndl,
                   connection_id_t  cid);

/*!
    @brief This API is used to set the role value.

    @param srvcHndl Service handle of the TMAS server instance.
    @param mute Value of Role to set

    @return TRUE if successful, FALSE otherwise

*/
bool GattTmasServerSetRole(ServiceHandle srvcHndl, uint16 role);

/*!
    @brief This API is used when the server application needs to get the role value.

    @param handle Service handle of the TMAS server instance.

    @return uint16 Role value.
*/
uint16 GattTmasServerGetRole(ServiceHandle srvcHndl);

#endif /* GATT_TMAS_SERVER_H_ */
