/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_sm_actions.h
\brief      Definition of the state machine action APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_SM_ACTIONS_H
#define DFU_PROTOCOL_SM_ACTIONS_H

/*! \brief Handle an abort event
    \param params Pointer to state transition action parameters */
void DfuProtocol_CommonAbortEventAction(const void * params);

/*! \brief Handle transport connected event during the abort procedure
    \param params Pointer to state transition action parameters */
void DfuProtocol_TransportConnectedAbortEventAction(const void * params);

/*! \brief Handle an abort event when Upgrade transport is not connected
    \param params Pointer to state transition action parameters */
void DfuProtocol_ConnectTransportForAbortEvent(const void * params);

/*! \brief Handle the complete event
    \param params Pointer to state transition action parameters */
void DfuProtocol_GenericCompleteEventAction(const void * params);

/*! \brief Handle the start event
    \param params Pointer to state transition action parameters */
void DfuProtocol_StartEventAction(const void * params);

/*! \brief Handle the start event after a DFU reboot
    \param params Pointer to state transition action parameters */
void DfuProtocol_StartPostRebootEventAction(const void * params);

/*! \brief Handle the transport connected event
    \param params Pointer to state transition action parameters */
void DfuProtocol_TransportConnectedEventAction(const void * params);

/*! \brief Handle the transport connect event after a DFU reboot
    \param params Pointer to state transition action parameters */
void DfuProtocol_TransportConnectedPostRebootEventAction(const void * params);

/*! \brief Handle the sync complete event
    \param params Pointer to state transition action parameters */
void DfuProtocol_SyncCompleteEventAction(const void * params);

/*! \brief Handle the sync complete event after a DFU reboot
    \param params Pointer to state transition action parameters */
void DfuProtocol_SyncCompletePostRebootEventAction(const void * params);

/*! \brief Handle the transfer begin event
    \param params Pointer to state transition action parameters */
void DfuProtocol_BeginTransferEventAction(const void * params);

/*! \brief Handle the commit request event
    \param params Pointer to state transition action parameters */
void DfuProtocol_CommitRequestEventAction(const void * params);

/*! \brief Handle the validate event
    \param params Pointer to state transition action parameters */
void DfuProtocol_ValidateEventAction(const void * params);

/*! \brief Handle the apply event
    \param params Pointer to state transition action parameters */
void DfuProtocol_ApplyEventAction(const void * params);

/*! \brief Handle the commit event
    \param params Pointer to state transition action parameters */
void DfuProtocol_CommitEventAction(const void * params);

/*! \brief Handle the transport disconnect event
    \param params Pointer to state transition action parameters */
void DfuProtocol_TransportDisconnectEventAction(const void * params);

/*! \brief Handle the transport disconnected event
    \param params Pointer to state transition action parameters */
void DfuProtocol_TransportDisconnectedEventAction(const void * params);

#endif // DFU_PROTOCOL_SM_ACTIONS_H
