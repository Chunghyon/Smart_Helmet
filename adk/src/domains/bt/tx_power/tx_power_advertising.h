/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef TX_POWER_ADVERTISING_H_
#define TX_POWER_ADVERTISING_H_

/*! \brief Setup the LE advertising data for TX Power

    \returns TRUE if the advertising data was setup successfully, else FALSE
 */
bool TxPower_SetupLeAdvertisingData(void);

/*! \brief Trigger an update the LE advertising data for TX power

    \returns TRUE if the update was successful, else FALSE
 */
bool TxPower_UpdateLeAdvertisingData(void);

#endif /* TX_POWER_ADVERTISING_H_ */
