/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    handset_service
\brief      Handset service BLE state machine
*/

#ifndef HANDSET_SERVICE_BLE_SM_H_
#define HANDSET_SERVICE_BLE_SM_H_

#include <bdaddr.h>

/*@{*/

typedef enum
{
    HANDSET_SERVICE_STATE_BLE_DISCONNECTED,
    HANDSET_SERVICE_STATE_BLE_CONNECTED,
    HANDSET_SERVICE_STATE_BLE_DISCONNECTING,
} handset_service_ble_state_t;

typedef struct
{
    /*! Address of LE connected device. */
    tp_bdaddr le_addr;
    /*! State of LE connection */
    handset_service_ble_state_t le_state;
} handset_service_ble_state_machine_t;

/*! \brief Reset a BLE state machine to defaults

    \param ble_sm The BLE state machine to reset
*/
void HandsetService_BleSmReset(handset_service_ble_state_machine_t* ble_sm);

/*! \brief Set BLE state

    \param ble_sm The BLE state machine to set state for
    \param state The state to set
*/
void HandsetService_BleSmSetState(handset_service_ble_state_machine_t* ble_sm, handset_service_ble_state_t state);

/*! \brief Check if BLE is connected

    \param ble_sm The BLE state machine to check
    
    \return TRUE if the state is HANDSET_SERVICE_STATE_BLE_CONNECTED and the BLE ACL
            is still connected, otherwise FALSE
*/
bool HandsetService_BleIsConnected(handset_service_ble_state_machine_t* ble_sm);

/*! \brief Disconnect BLE if it is connected

    \param ble_sm The BLE state machine to disconnect
*/
void HandsetService_BleDisconnectIfConnected(handset_service_ble_state_machine_t* ble_sm);

/*! \brief Handle BLE connection from a handset

    \param ble_sm The BLE state machine associated with the connection
    \param tpaddr The BLE address of the handset
*/
void HandsetService_BleHandleConnected(handset_service_ble_state_machine_t* ble_sm, tp_bdaddr* tpaddr);

/*@}*/

#endif /* HANDSET_SERVICE_BLE_SM_H_ */
