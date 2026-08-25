/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Management of Tx Power */

#include "tx_power.h"
#include "tx_power_msg_handler.h"
#include "tx_power_advertising.h"

#include <connection.h>
#include "connection_abstraction.h"
#include <panic.h>
#include <string.h>
#include <stdio.h>
#include <logging.h>

/*! \brief Data type for API function return values to indicate success/error status */
typedef enum{
    tx_power_status_success,
    tx_power_status_error_unknown
}tx_power_status_t;

/*! \brief Tx Power instance structure. This structure contains information relating to Tx Power.
*/
typedef struct
{
    /*! Tx Power task */
    TaskData task;
    /*! Tx Power task for BLE */
    int8 ble_value;
    /*! Tx Power for BR/EDR */
    int8 bredr_value;
    /*! Tx Power mandatory flag */
    bool mandatory;
    /*! Tx Power mandatory flag */
    tx_power_status_t status;
    /*! Tx power mandatory flag*/
    uint8 tx_power_mandatory;
    /*! Tx power board path loss */
    uint8 tx_power_path_loss;
} tx_power_data_t;

/*!< Tx Power data states */
typedef enum
{
    tx_power_initialising,
    tx_power_ready
} tx_power_state_t;

/*!< Tx Power data readyness */
typedef struct
{
    tx_power_state_t tx_power_state_le;
    tx_power_state_t tx_power_state_br_edr;
}tx_power_data_state_t;

/*!< Task information for the Tx Power */
static tx_power_data_t tx_power_data;
static tx_power_data_state_t data_state;
static TaskData tx_power_task_data = {TxPowerTaskHandler};

void TxPower_SetTxPowerPathLoss(uint8 path_loss)
{
    tx_power_data.tx_power_path_loss = path_loss;
}

uint8 TxPower_GetTxPowerPathLoss(void)
{
    return tx_power_data.tx_power_path_loss;
}

bool TxPower_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("TxPower_Init");
    data_state.tx_power_state_br_edr = tx_power_initialising;
    data_state.tx_power_state_le = tx_power_initialising;
    tx_power_data.tx_power_mandatory = 0;
    /* Request from Bluestack to get the BLE advertisement's Tx power */
    ConnectionDmBleReadAdvertisingChannelTxPower((Task)&tx_power_task_data);

    return TxPower_SetupLeAdvertisingData();
}


void TxPower_Retrigger_LE_Data(void)
{
    DEBUG_LOG("TxPower_Retrigger_LE_Data");
    ConnectionDmBleReadAdvertisingChannelTxPower((Task)&tx_power_task_data);
}


void TxPower_Mandatory(bool enable_mandatory, tx_power_client_t client_id)
{
    DEBUG_LOG("TxPower_Mandatory: enable_mandatory = %d, client_id = %d", enable_mandatory, client_id);
    DEBUG_LOG("TxPower_Mandatory: Present Mandatory flag is set to %s", tx_power_data.mandatory? "TRUE":"FALSE");

    if(client_id >= le_client_last)
    {
        DEBUG_LOG("Invalid client id");
        Panic();
    }
    if(enable_mandatory)
    {
        tx_power_data.tx_power_mandatory |= (1U << client_id);
        DEBUG_LOG("Tx Power Mandatory set for client_id=%d", client_id);
    }
    else
    {
        tx_power_data.tx_power_mandatory &= ~(1U << client_id);
        DEBUG_LOG("Tx Power not Mandatory for client_id=%d", client_id);
    }
    /* Tx Power is mandatory even if one client has it set it */
    tx_power_data.mandatory = (tx_power_data.tx_power_mandatory != 0);
    DEBUG_LOG("TxPower_Mandatory:Updated Mandatory flag is %s", tx_power_data.mandatory? "TRUE":"FALSE");
}

int8 TxPower_LEGetData(void)
{
    int8 tx_power_level;
    DEBUG_LOG("TxPower_LEGetData");
    if(data_state.tx_power_state_le != tx_power_ready)
    {
        DEBUG_LOG("Tx Power for LE adverts is not available yet");
    }
    /* Add board tx power path loss */
    tx_power_level = tx_power_data.ble_value + TxPower_GetTxPowerPathLoss();
    /* Tx Power level (in dBm) */
    return tx_power_level;
}


bool TxPower_GetMandatoryStatus(void)
{
    return tx_power_data.mandatory;
}

static inline void txPowerHandleTxPower(uint8 status, int8 power)
{
	if(hci_success == status)
	{
        tx_power_data.ble_value = power;
		data_state.tx_power_state_le = tx_power_ready;
	}
	else
	{
		DEBUG_LOG("TxPowerTaskHandler: Error while reading LE Tx Power: hci_error = %d", status);
	}
}

void TxPowerTaskHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("TxPowerTaskHandler: MessageID MESSAGE:0x%x", id); 

    switch(id)
    {
        case CL_DM_BLE_READ_ADVERTISING_CHANNEL_TX_POWER_CFM:
		{
            CL_DM_BLE_READ_ADVERTISING_CHANNEL_TX_POWER_CFM_T* cfm =
                    (CL_DM_BLE_READ_ADVERTISING_CHANNEL_TX_POWER_CFM_T*) message;
			txPowerHandleTxPower(cfm->status, cfm->tx_power);		
		}
        break;

        default:
            DEBUG_LOG("TxPowerTaskHandler: Unhandled");
        break;
    }
}



Task TxPower_GetTaskData(void)
{
    return &tx_power_task_data;
}
