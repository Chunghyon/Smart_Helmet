/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       tx_power.h
\brief      Header file for Tx Power module.

Provides information relating to Tx power of LE and BR/EDR.

Support for BR/EDR Tx power is yet to be implemented.
*/

#ifndef TX_POWER_H_
#define TX_POWER_H_

/*@{*/

/*! \brief Clients requesting tx power */
typedef enum{
    le_client_fast_pair,
    le_client_last
}tx_power_client_t;

/*! \brief Set board tx power path loss */
void TxPower_SetTxPowerPathLoss(uint8 path_loss);

/*! \brief Get board tx power path loss */
uint8 TxPower_GetTxPowerPathLoss(void);

/*! \brief Initialise the Tx Power module.*/
bool TxPower_Init(Task init_task);

/*! \brief Indicate that Tx Power is mandatory to be present in advertising field.

    \param enable_mandatory TRUE indicates Tx power is mandatory.
                            FALSE indicates Tx power is optional.
    \param Client ID of the requester.
*/
void TxPower_Mandatory(bool enable_mandatory, tx_power_client_t client);

/*! \brief Get the Tx Power Mandatory status.

    \return TRUE indicates atleast one client has set Tx Power Mandatory.
            FASLSE indicates that Tx Power is not mandatory.
*/
bool TxPower_GetMandatoryStatus(void);

/*! \brief Re-trigger read of LE power data.
*/
void TxPower_Retrigger_LE_Data(void);

/*! \brief Get the LE advertisement Tx Power.

    \return returns the Tx Power level (in dBm).
*/
int8 TxPower_LEGetData(void);

/*@}*/

#endif /* TX_POWER_H_ */

