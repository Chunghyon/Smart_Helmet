/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    dm_ble_power_control.h

DESCRIPTION
    Handles power control (BLE) prims from bluestack.

NOTES

*/


void connectionHandleUlpEnhancedReadTransmitPowerLevelCfm(
        connectionGeneralLockState *state,
        const DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_CFM_T *cfm);

void connectionHandleUlpReadRemoteTransmitPowerLevelCfm(
        connectionGeneralLockState *state,
        const DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_CFM_T *cfm);

void connectionHandleUlpSetPathLossReportingParametersCfm(
        connectionGeneralLockState *state,
        const DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_CFM_T *cfm);

void connectionHandleUlpSetPathLossReportingEnableCfm(
        connectionGeneralLockState *state,
        const DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_CFM_T *cfm);

void connectionHandleSetTransmitPowerReportingEnableCfm(
        connectionGeneralLockState *state,
        const DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_CFM_T *cfm);

void connectionHandleUlpEnhancedReadTransmitPowerLevelReq(
        connectionGeneralLockState *state,
        const CL_INTERNAL_DM_ULP_ENHANCED_READ_TRANSMIT_POWER_LEVEL_REQ_T *req);

void connectionHandleUlpReadRemoteTransmitPowerLevelReq(
            connectionGeneralLockState *state,
            const CL_INTERNAL_DM_ULP_READ_REMOTE_TRANSMIT_POWER_LEVEL_REQ_T *req);

void connectionHandleUlpSetPathLossReportingParemetersReq(
        connectionGeneralLockState *state,
        const CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_PARAMETERS_REQ_T *req);

void connectionHandleUlpSetPathLossReportingReq(
        connectionGeneralLockState *state,
        const CL_INTERNAL_DM_ULP_SET_PATH_LOSS_REPORTING_ENABLE_REQ_T *req);


void connectionHandleUlpSetTransmitPowerReportingEnableReq(
            connectionGeneralLockState *state,
            const CL_INTERNAL_DM_ULP_SET_TRANSMIT_POWER_REPORTING_ENABLE_REQ_T *req);

void connectionHandleUlpPathLossThresholdInd(const DM_ULP_PATH_LOSS_THRESHOLD_IND_T *ind);

void connectionHandleUlpTransmitPowerReportingInd(const DM_ULP_TRANSMIT_POWER_REPORTING_IND_T*ind);
