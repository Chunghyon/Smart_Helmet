/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       ama_ble.h
\brief      Implementation for AMA BLE.
*/
    
/*! \brief Setup the LE advertising data for AMA

    \returns TRUE if the advertising data was setup successfully, else FALSE
 */
bool AmaBle_SetupLeAdvertisingData(void);

/*! \brief Trigger an update the LE advertising data for AMA

    \returns TRUE if the update was successful, else FALSE
 */
bool AmaBle_UpdateLeAdvertisingData(void);
