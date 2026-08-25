/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    le_advertising_manager
\brief      Message handler functions for the advertising set state machine.
*/

#ifndef LE_ADVERTISING_MANAGER_SET_SM_MSG_HANDLER_H
#define LE_ADVERTISING_MANAGER_SET_SM_MSG_HANDLER_H

#include "le_advertising_manager_set_sm.h"

#include <connection.h>
#include <stdlib.h>


/*
    Internal Messages.
*/

/*! \brief Internal messages for the advertising set state machine. */
typedef enum {
    LE_ADVERTISING_MANAGER_SET_INTERNAL_REGISTER = INTERNAL_MESSAGE_BASE,
    LE_ADVERTISING_MANAGER_SET_INTERNAL_UPDATE_PARAMS_REQ,
    LE_ADVERTISING_MANAGER_SET_INTERNAL_UPDATE_DATA_REQ,
    LE_ADVERTISING_MANAGER_SET_INTERNAL_ENABLE,
    LE_ADVERTISING_MANAGER_SET_INTERNAL_DISABLE
} le_advertising_manager_internal_msg_t;

/*! \brief Generic internal message */
typedef struct {
    le_advertising_manager_set_state_machine_t *sm;
} LE_ADVERTISING_MANAGER_SET_STATE_INTERNAL_MSG_T;

typedef LE_ADVERTISING_MANAGER_SET_STATE_INTERNAL_MSG_T LE_ADVERTISING_MANAGER_SET_INTERNAL_REGISTER_T;

typedef struct {
    le_advertising_manager_set_state_machine_t *sm;
    le_advertising_manager_set_params_t params;
} LE_ADVERTISING_MANAGER_SET_INTERNAL_UPDATE_PARAMS_T;

typedef struct {
    le_advertising_manager_set_state_machine_t *sm;
    le_advertising_manager_set_adv_data_t data;
} LE_ADVERTISING_MANAGER_SET_INTERNAL_UPDATE_DATA_T;

typedef LE_ADVERTISING_MANAGER_SET_STATE_INTERNAL_MSG_T LE_ADVERTISING_MANAGER_SET_INTERNAL_ENABLE_T;

typedef LE_ADVERTISING_MANAGER_SET_STATE_INTERNAL_MSG_T LE_ADVERTISING_MANAGER_SET_INTERNAL_DISABLE_T;



void LeAdvertisingManager_SetSmMessageHandler(Task task, MessageId id, Message message);

/*
    Handlers for Connection library messages.
*/
void LeAdvertisingManager_SetSmHandleExtendedAdvertisingRegisterAppAdvSetCfm(const CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T *cfm);

void LeAdvertisingManager_SetSmHandleExtendedAdvertisingSetParamCfm(const CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T *cfm);

void LeAdvertisingManager_SetSmHandleExtendedAdvertisingSetRandomAddressCfm(const CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM_T *cfm);

void LeAdvertisingManager_SetSmHandleExtendedAdvertisingSetDataCfm(const CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T *cfm);

void LeAdvertisingManager_SetSmHandleExtendedAdvertisingSetScanResponseDataCfm(const CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T *cfm);

void LeAdvertisingManager_SetSmHandleExtendedAdvertisingEnableCfm(const CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T *cfm);

void LeAdvertisingManager_SetSmHandleExtendedAdvertisingTerminatedInd(const CL_DM_BLE_EXT_ADV_TERMINATED_IND_T *ind);

/*
    Functions to send messages to the client of a sm.
*/

/*! \brief Send a LE_ADVERTISING_MANAGER_SET_REGISTER_CFM to the client.

    \param sm Advertising Set state machine to send the CFM for.
    \param success TRUE if set was registered successfully; FALSE otherwise.
*/
void LeAdvertisingManager_SetSmSendRegisterCfm(le_advertising_manager_set_state_machine_t *sm, bool success);

void LeAdvertisingManager_SetSmSendUpdateParamsCfm(le_advertising_manager_set_state_machine_t *sm, bool success);

void LeAdvertisingManager_SetSmSendUpdateDataCfm(le_advertising_manager_set_state_machine_t *sm, bool success);

void LeAdvertisingManager_SetSmSendEnableCfm(le_advertising_manager_set_state_machine_t *sm, bool success);

void LeAdvertisingManager_SetSmSendDisableCfm(le_advertising_manager_set_state_machine_t *sm, bool success);

#endif // LE_ADVERTISING_MANAGER_SET_SM_MSG_HANDLER_H
