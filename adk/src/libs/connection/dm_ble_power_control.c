/****************************************************************************
Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_ble_power_control.c

DESCRIPTION
    Handles power control (BLE) prims from bluestack.

NOTES

*/

/****************************************************************************
    Header files
*/

#include "connection.h"
#include "connection_private.h"
#include "common.h"
#include "dm_ble_power_control.h"

#include <bdaddr.h>
#include <vm.h>
#include <string.h>
#include <types.h>
#include <dm_prim.h>

#ifndef DISABLE_BLE

/****************************************************************************
NAME
    ConnectionUlpEnhancedReadTransmitPowerLevelReq

DESCRIPTION
    Internal request for read the current and maximum transmit
    power levels of the local Controller

RETURNS
    void
*/
void ConnectionUlpEnhancedReadTransmitPowerLevelReq(Task theAppTask,tp_bdaddr tp_addr, uint8 phy)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_REQ);
    message->theAppTask = theAppTask;
    message->tp_addr = tp_addr;
    message->phy = phy;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleUlpEnhancedReadTransmitPowerLevelReq

DESCRIPTION
    Handles the request for read the current and maximum transmit
    power levels of the local Controller

RETURNS
    void
*/

void connectionHandleUlpEnhancedReadTransmitPowerLevelReq(connectionGeneralLockState *state, const CL_INTERNAL_DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_REQ_T *req)
{
    if (!state->taskLock)
    {
        /* Store the requesting task in the lock. */
        state->taskLock = req->theAppTask;

        /* Send the request to BlueStack. */
        MAKE_PRIM_T(DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_REQ);
        BdaddrConvertTpVmToBluestack(&(prim->tp_addrt), &(req->tp_addr));
        prim->phy = req->phy;

        VmSendDmPrim(prim);
    }
    else /* There is already an txPwr Request being processed, queue this one */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_REQ,
                    message,
                    &state->taskLock
                    );
    }
}
/****************************************************************************
NAME
    connectionHandleUlpEnhancedReadTransmitPowerLevelCfm

DESCRIPTION
    Handles the CFM for read the current and maximum transmit
    power levels of the local Controller

RETURNS
    void
*/
void connectionHandleUlpEnhancedReadTransmitPowerLevelCfm(
        connectionGeneralLockState *state,
        const DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_CFM_T *cfm)
{

    if (state->taskLock)
    {
        MAKE_CL_MESSAGE(CL_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        message->max_tx_pwr_level = cfm->max_tx_pwr_level;
        message->current_tx_pwr_level = cfm->current_tx_pwr_level;
        message ->phy = cfm->phy;
        BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->tp_addrt.addrt);
        MessageSend(
            connectionGetAppTask(),
            CL_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_CFM,
            message
            );
    }
    /* Reset the lock. */
    state->taskLock = NULL;
}


/****************************************************************************
NAME
    ConnectionUlpReadRemoteTransmitPowerReq

DESCRIPTION
    Handles the request for read the transmit power level
    used by the remote Controller

RETURNS
    void
*/
void ConnectionUlpReadRemoteTransmitPowerReq(Task theAppTask,tp_bdaddr tp_addr, uint8 phy)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_REQ);
    message->theAppTask = theAppTask;
    message->tp_addr = tp_addr;
    message->phy = phy;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_REQ,
                message);
}


/****************************************************************************
NAME
    connectionHandleUlpReadRemoteTransmitPowerLevelReq

DESCRIPTION
    Handles the request for read the transmit power level
    used by the remote Controller

RETURNS
    void
*/
void connectionHandleUlpReadRemoteTransmitPowerLevelReq(
            connectionGeneralLockState *state,
            const CL_INTERNAL_DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_REQ_T *req)
{
    if (!state->taskLock)
    {
        /* Store the requesting task in the lock. */
        state->taskLock = req->theAppTask;

        /* Send the request to BlueStack. */
        MAKE_PRIM_T(DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_REQ);
        BdaddrConvertTpVmToBluestack(&(prim->tp_addrt), &(req->tp_addr));
        prim->phy = req->phy;

        VmSendDmPrim(prim);
    }
    else /* There is already an txPwr Request being processed, queue this one */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_REQ,
                    message,
                    &state->taskLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleUlpReadRemoteTransmitPowerLevelCfm

DESCRIPTION
    Handles the CFM for read the transmit power level
    used by the remote Controller.

 The Reason parameter indicates why the event was sent and the device
 * whose transmit power level is being reported:
 * 0x00 - Local transmit power changed
 * 0x01 - Remote transmit power changed
 * 0x02 - HCI_LE_Read_Transmit_Power_Level command completed

RETURNS
    void
*/

void connectionHandleUlpReadRemoteTransmitPowerLevelCfm(
              connectionGeneralLockState *state,
              const DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_CFM_T *cfm)
{
    if (state->taskLock)
    {
        MAKE_CL_MESSAGE(CL_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        message->delta = cfm->delta;
        message->tx_pwr_level_flag = cfm->tx_pwr_level_flag;
        message->tx_pwr_level = cfm->tx_pwr_level;
        message->phy = cfm->phy;
        message->reason = cfm->reason;
        BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->tp_addrt.addrt);
        MessageSend(
            connectionGetAppTask(),
            CL_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_CFM,
            message
            );
    }
    /* Reset the lock. */
    state->taskLock = NULL;
}


/****************************************************************************
NAME
    ConnectionUlpSetPathLossReportingParemetersReq

DESCRIPTION
    Internal request for set the path loss threshold
    reporting parameters.

RETURNS
    void
*/
void ConnectionUlpSetPathLossReportingParemetersReq(
                                Task theAppTask,
                                tp_bdaddr tp_addr,
                                uint8 high_threshold,
                                uint8 high_hysteresis,
                                uint8 low_threshold,
                                uint8 low_hysteresis,
                                uint16 min_time_spent)


{
    MAKE_CL_MESSAGE( CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_REQ);
    message->theAppTask = theAppTask;
    message->tp_addr = tp_addr;
    message->high_threshold = high_threshold;
    message->high_hysteresis = high_hysteresis;
    message->low_threshold = low_threshold;
    message->low_hysteresis = low_hysteresis;
    message->min_time_spent = min_time_spent;
    MessageSend(
                connectionGetCmTask(),
                 CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleUlpSetPathLossReportingParemetersReq

DESCRIPTION
    handle request for set the path loss threshold
    reporting parameters.

RETURNS
    void
*/
void connectionHandleUlpSetPathLossReportingParemetersReq(
        connectionGeneralLockState *state,
        const CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_REQ_T *req)
{
    if (!state->taskLock)
    {
        /* Store the requesting task in the lock. */
        state->taskLock = req->theAppTask;

        /* Send the request to BlueStack. */
        MAKE_PRIM_T(DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_REQ);
        BdaddrConvertTpVmToBluestack(&(prim->tp_addrt), &(req->tp_addr));
        prim->high_threshold = req->high_threshold;
        prim->high_hysteresis = req->high_hysteresis;
        prim->low_threshold = req->low_threshold;
        prim->low_hysteresis = req->low_hysteresis;
        prim->min_time_spent = req->min_time_spent;

        VmSendDmPrim(prim);
    }
    else /* There is already an tasklock Request being processed, queue this one */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_REQ,
                    message,
                    &state->taskLock
                    );
    }
}

/****************************************************************************
NAME
    connectionHandleUlpSetPathLossReportingParametersCfm

DESCRIPTION
    Handles the CFM for set the path loss threshold
    reporting parameters.

RETURNS
    void
*/

void connectionHandleUlpSetPathLossReportingParametersCfm(
            connectionGeneralLockState *state,
        const DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_CFM_T *cfm)
{
    if (state->taskLock)
    {
        MAKE_CL_MESSAGE(CL_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_CFM);
        message->status = connectionConvertHciStatus(cfm->status);
        BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->tp_addrt.addrt);
        MessageSend(
            connectionGetAppTask(),
            CL_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_CFM,
            message
            );
    }
    /* Reset the lock. */
    state->taskLock = NULL;
}



/****************************************************************************
NAME
    ConnectionUlpSetPathLossReportingEnablelReq

DESCRIPTION
    Internal request to enable or disable path loss reporting.


RETURNS
    void
*/
void ConnectionUlpSetPathLossReportingEnablelReq(Task theAppTask,tp_bdaddr tp_addr, bool enable)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_REQ);
    message->theAppTask = theAppTask;
    message->tp_addr = tp_addr;
    message->enable = enable;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_REQ,
                message);
}

/****************************************************************************
NAME
    connectionHandleUlpSetPathLossReportingReq

DESCRIPTION
    request to enable or disable path loss reporting.

RETURNS
    void
*/
void connectionHandleUlpSetPathLossReportingReq(
        connectionGeneralLockState *state,
        const CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_REQ_T *req)
{
    if (!state->taskLock)
    {
        /* Store the requesting task in the lock. */
        state->taskLock = req->theAppTask;

        /* Send the request to BlueStack. */
        MAKE_PRIM_T(DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_REQ);
        BdaddrConvertTpVmToBluestack(&(prim->tp_addrt), &(req->tp_addr));
        prim->enable = req->enable;

        VmSendDmPrim(prim);
    }
    else /* There is already an txPwr Request being processed, queue this one */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_REQ,
                    message,
                    &state->taskLock
                    );
    }
}

/****************************************************************************
    NAME
        connectionHandleUlpSetPathLossReportingEnableCfm

    DESCRIPTION
        Handles the CFM for used to enable or disable path loss reporting.

    RETURNS
        void
    */
void connectionHandleUlpSetPathLossReportingEnableCfm(
                connectionGeneralLockState *state,
            const DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_CFM_T *cfm)
    {
        if (state->taskLock)
        {
            MAKE_CL_MESSAGE(CL_ULP_SET_PATH_LOSS_REPORTING_ENABLE_CFM);
            message->status = connectionConvertHciStatus(cfm->status);
            BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->tp_addrt.addrt);
             MessageSend(
                connectionGetAppTask(),
                CL_ULP_SET_PATH_LOSS_REPORTING_ENABLE_CFM,
                message
                );
        }
        /* Reset the lock. */
        state->taskLock = NULL;
    }


/****************************************************************************
NAME
    ConnectionUlpSetTransmitPowerReportingEnableReq

DESCRIPTION
    Internal request to enable transmit power reporting.

RETURNS
    void
*/
void ConnectionUlpSetTransmitPowerReportingEnableReq(Task theAppTask,tp_bdaddr tp_addr, bool local_enable, bool remote_enable)
{
    MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_REQ);
    message->theAppTask = theAppTask;
    message->tp_addr = tp_addr;
    message->local_enable = local_enable;
    message->remote_enable = remote_enable;
    MessageSend(
                connectionGetCmTask(),
                CL_INTERNAL_DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_REQ,
                message);
}
/****************************************************************************
NAME
    connectionHandleUlpSetTransmitPowerReportingEnableReq

DESCRIPTION
    request to enable transmit power reporting.

RETURNS
    void
*/
void connectionHandleUlpSetTransmitPowerReportingEnableReq(
                connectionGeneralLockState *state,
            const CL_INTERNAL_DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_REQ_T *req)
{
    if (!state->taskLock)
    {
        /* Store the requesting task in the lock. */
        state->taskLock = req->theAppTask;

        /* Send the request to BlueStack. */
        MAKE_PRIM_T(DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_REQ);
        BdaddrConvertTpVmToBluestack(&(prim->tp_addrt), &(req->tp_addr));
        prim->local_enable = req->local_enable;
        prim->remote_enable = req->remote_enable;

        VmSendDmPrim(prim);
    }
    else /* There is already an txPwr Request being processed, queue this one */
    {
        MAKE_CL_MESSAGE(CL_INTERNAL_DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_REQ);
        COPY_CL_MESSAGE(req, message);
        MessageSendConditionallyOnTask(
                    connectionGetCmTask(),
                    CL_INTERNAL_DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_REQ,
                    message,
                    &state->taskLock
                    );
    }
}

/***************************************************************************
NAME
 connectionHandleSetTransmitPowerReportingEnableCfm

DESCRIPTION
   Handles the CFM for used to enable or disable path loss reporting.

RETURNS
    void
    */

void connectionHandleSetTransmitPowerReportingEnableCfm(
                connectionGeneralLockState *state,
            const DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_CFM_T *cfm)
    {
        if (state->taskLock)
        {
            MAKE_CL_MESSAGE(CL_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_CFM);
            message->status = connectionConvertHciStatus(cfm->status);
            BdaddrConvertTypedBluestackToVm(&message->taddr, &cfm->tp_addrt.addrt);
            MessageSend(
                connectionGetAppTask(),
                CL_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_CFM,
                message
                );
        }
        /* Reset the lock. */
        state->taskLock = NULL;
    }

/***************************************************************************
NAME
 ConnectionHandlePathLossThresholdInd

DESCRIPTION
   Handles the event generated by  HCI_LE_Path_Loss_Threshold.

RETURNS
    void
    */
void connectionHandleUlpPathLossThresholdInd(const DM_ULP_PATH_LOSS_THRESHOLD_IND_T *ind)
{
    MAKE_CL_MESSAGE(CL_ULP_PATH_LOSS_THRESHOLD_IND);

    BdaddrConvertTypedBluestackToVm(&message->taddr, &ind->tp_addrt.addrt);
    message->curr_path_loss = ind->curr_path_loss;
    message->zone_entered = ind->zone_entered;

    MessageSend(
        connectionGetCmTask(),
        CL_ULP_PATH_LOSS_THRESHOLD_IND,
        message
        );
}

/***************************************************************************
NAME
 ConnectionHandleTransmitPowerReportingInd

DESCRIPTION
   Handles the event generated by  HCI_LE_Transmit_Power_Reporting.

RETURNS
    void
    */
void connectionHandleUlpTransmitPowerReportingInd(const DM_ULP_TRANSMIT_POWER_REPORTING_IND_T*ind)
{
    MAKE_CL_MESSAGE(CL_ULP_TRANSMIT_POWER_REPORTING_IND);
    BdaddrConvertTypedBluestackToVm(&message->taddr, &ind->tp_addrt.addrt);
    message->reason = ind->reason;
    message->phy = ind->phy;
    message->tx_pwr_level = ind->tx_pwr_level_flag;

    MessageSend(
        connectionGetCmTask(),
        CL_ULP_TRANSMIT_POWER_REPORTING_IND,
        message
        );
}

#endif /* DISABLE_BLE */

/* End-of-File */
