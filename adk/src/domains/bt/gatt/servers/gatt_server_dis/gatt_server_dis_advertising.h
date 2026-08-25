/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef GATT_SERVER_DIS_ADVERTISING_H_
#define GATT_SERVER_DIS_ADVERTISING_H_

/*! \brief Setup the LE advertising data for the GATT DIS server

    \returns TRUE if the advertising data was setup successfully, else FALSE
 */
bool GattServerDis_SetupLeAdvertisingData(void);

/*! \brief Trigger an update the LE advertising data for the GATT DIS server

    \returns TRUE if the update was successful, else FALSE
 */
bool GattServerDis_UpdateLeAdvertisingData(void);

#endif /* GATT_SERVER_DIS_ADVERTISING_H_ */
