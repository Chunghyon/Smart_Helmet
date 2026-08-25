/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_message_handler.c
\brief      Implemetation of the message handler APIs for the dfu_protocol module
*/

#include "dfu_protocol_log.h"

#include "dfu_protocol_message_handler.h"

#include "dfu_protocol_client_config.h"
#include "dfu_protocol_client_notifier.h"
#include "dfu_protocol_data.h"
#include "dfu_protocol_message_dispatcher.h"
#include "dfu_protocol_sm.h"
#include "dfu_protocol_sm_types.h"
#include "dfu_protocol_upgrade_config.h"

#include "dfu_peer.h"

#include <bt_device.h>
#include <byte_utils.h>
#include <panic.h>
#include <upgrade_protocol.h>

#define RESUME_POINT_SIZE sizeof(uint8)
#define ERROR_CODE_SIZE sizeof(uint16)
#define NUMBER_OF_BYTES_SIZE sizeof(uint32)
#define START_OFFSET_SIZE sizeof(uint32)
#define SILENT_COMMIT_SUPPORTED_SIZE sizeof(uint8)

static uint8 host_sync_cfm_resume_point;

static void dfuProtocol_MessageHandler(Task task, MessageId id, Message message);
static TaskData dfu_protocol_message_handler_task_data = { .handler = dfuProtocol_MessageHandler };

static uint16 dfuProtocol_Get4BytesIn16BitLittleEndian(uint8 * src, uint16 byte_index, uint32 * dst)
{
    *dst =  ((uint32) src[byte_index] << 24);
    *dst |= ((uint32) src[byte_index + 1] << 16);
    *dst |= ((uint32) src[byte_index + 2] << 8);
    *dst |= (uint32) src[byte_index + 3];
    return 4;
}

static void dfuProtocol_HandleUpgradeHostSyncCfm(uint16 size_data, uint8 * data)
{
    uint16 size_to_read = ID_SIZE + LENGTH_SIZE + RESUME_POINT_SIZE;
    PanicFalse(size_data >= size_to_read);

    uint16 byte_index = 0;
    uint8 id;
    uint16 length;

    byte_index += ByteUtilsGet1Byte(data, byte_index, &id);
    byte_index += ByteUtilsGet2Bytes(data, byte_index, &length);

    /* The resume point is used when handling UPGRADE_HOST_START_CFM to determine whether to respond
     * with UPGRADE_HOST_START_DATA_REQ, UPGRADE_HOST_IS_CSR_VALID_DONE_REQ,
     * UPGRADE_HOST_TRANSFER_COMPLETE_RES, or UPGRADE_HOST_IN_PROGRESS_RES */
    byte_index += ByteUtilsGet1Byte(data, byte_index, &host_sync_cfm_resume_point);

    dfu_protocol_events_t sync_event = dfu_protocol_host_sync_complete_event;

    if(DfuProtocol_DidActiveClientCauseReboot())
    {
        sync_event = dfu_protocol_host_sync_complete_post_reboot_event;
    }

    DEBUG_LOG_DEBUG("dfuProtocol_HandleUpgradeHostSyncCfm enum:UpdateResumePoint:%d", (UpdateResumePoint)host_sync_cfm_resume_point);

    StateMachine_Update(DfuProtocol_GetStateMachine(), sync_event, NULL);
}

static void dfuProtocol_HandleUpgradeHostStartCfm(void)
{
    dfu_protocol_events_t start_event = dfu_protocol_begin_transfer_event;

    if(host_sync_cfm_resume_point == UPGRADE_RESUME_POINT_PRE_REBOOT)
    {
        start_event = dfu_protocol_transfer_complete_event;
    }
    else if(host_sync_cfm_resume_point == UPGRADE_RESUME_POINT_PRE_VALIDATE)
    {
        start_event = dfu_protocol_validate_event;
    }
    else if(DfuProtocol_DidActiveClientCauseReboot())
    {
        start_event = dfu_protocol_commit_request_event;
    }
    else
    {
        /* We are not waiting for validation, reboot, or a post-reboot commit.
         * Default to requested for data transfer to start */
    }

    StateMachine_Update(DfuProtocol_GetStateMachine(), start_event, NULL);
}

static void dfuProtocol_HandleUpgradeHostErrorwarnInd(uint16 size_data, uint8 * data)
{
    uint16 size_to_read = ID_SIZE + LENGTH_SIZE + ERROR_CODE_SIZE;
    PanicFalse(size_data >= size_to_read);

    uint16 byte_index = 0;
    uint8 id;
    uint16 length;
    uint16 error_code;

    byte_index += ByteUtilsGet1Byte(data, byte_index, &id);
    byte_index += ByteUtilsGet2Bytes(data, byte_index, &length);
    byte_index += ByteUtilsGet2Bytes(data, byte_index, &error_code);

    DEBUG_LOG_VERBOSE("dfuProtocol_HandleUpgradeHostErrorwarnInd enum:UpgradeHostErrorCode:%d", (UpgradeHostErrorCode)error_code);

    DfuProtocol_SendErrorIndToActiveClient((UpgradeHostErrorCode)error_code);
    DfuProtocol_ErrorWarnResponse(error_code);
}

static void dfuProtocol_HandleUpgradeHostDataBytesReq(uint16 size_data, uint8 * data)
{
    uint16 size_to_read = ID_SIZE + LENGTH_SIZE + NUMBER_OF_BYTES_SIZE + START_OFFSET_SIZE;
    PanicFalse(size_data >= size_to_read);

    /* We do not care about the ID or length */
    uint16 byte_index = ID_SIZE + LENGTH_SIZE;
    uint32 number_of_bytes;
    uint32 start_offset;

    byte_index += dfuProtocol_Get4BytesIn16BitLittleEndian(data, byte_index, &number_of_bytes);
    byte_index += dfuProtocol_Get4BytesIn16BitLittleEndian(data, byte_index, &start_offset);

    DEBUG_LOG("dfuProtocol_HandleUpgradeHostDataBytesReq number_of_bytes 0x%X start_offset 0x%X", number_of_bytes, start_offset);

    DfuProtocol_SetRequestedNumberOfBytes(number_of_bytes);
    DfuProtocol_SetRequestedReadOffset(start_offset);

    DfuProtocol_SendUpdateHostData();
}

static void dfuProtocol_HandleUpgradeHostSilentCommitSupportedCfm(uint16 size_data, uint8 *data)
{
    uint16 size_to_read = ID_SIZE + LENGTH_SIZE + SILENT_COMMIT_SUPPORTED_SIZE;
    PanicFalse(size_data >= size_to_read);

    uint16 byte_index = 0;
    uint8 id;
    uint16 length;
    uint8 is_silent_commit_supported;
    UNUSED(size_data);
    byte_index += ByteUtilsGet1Byte(data, byte_index, &id);
    byte_index += ByteUtilsGet2Bytes(data, byte_index, &length);
    byte_index += ByteUtilsGet1Byte(data, byte_index, &is_silent_commit_supported);
    DfuProtocol_SetUpgradeSupportsSilentCommit(is_silent_commit_supported);
    DEBUG_LOG_VERBOSE("dfuProtocol_HandleUpgradeHostSilentCommitSupportedCfm is_silent_commit_supported %d", is_silent_commit_supported);
}

static void dfuProtocol_HandleUpgradeHostTransferCompleteInd(void)
{
    DfuProtocol_SendTransferCompleteIndToActiveClient();
    StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_transfer_complete_event, NULL);
}

static void dfuProtocol_HandleUpgradeTransportDataInd(UPGRADE_TRANSPORT_DATA_IND_T *msg)
{
    DEBUG_LOG_DEBUG("dfuProtocol_HandleUpgradeTransportDataInd: enum:UpgradeMsgHost:%d", msg->data[0] + UPGRADE_HOST_MSG_BASE);
    switch(msg->data[0] + UPGRADE_HOST_MSG_BASE)
    {
        case UPGRADE_HOST_SYNC_CFM:
            dfuProtocol_HandleUpgradeHostSyncCfm(msg->size_data, msg->data);
            break;

        case UPGRADE_HOST_START_CFM:
            dfuProtocol_HandleUpgradeHostStartCfm();
            break;

        case UPGRADE_HOST_COMMIT_REQ:
            DfuProtocol_SendCommitReqToActiveClient();
            break;

        case UPGRADE_HOST_ERRORWARN_IND:
            dfuProtocol_HandleUpgradeHostErrorwarnInd(msg->size_data, msg->data);
            break;

        case UPGRADE_HOST_DATA_BYTES_REQ:
            dfuProtocol_HandleUpgradeHostDataBytesReq(msg->size_data, msg->data);
            break;

        case UPGRADE_HOST_IS_CSR_VALID_DONE_CFM:
            StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_validate_event, NULL);
            break;

        case UPGRADE_HOST_TRANSFER_COMPLETE_IND:
            dfuProtocol_HandleUpgradeHostTransferCompleteInd();
            break;

        case UPGRADE_HOST_ABORT_CFM:
            StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_complete_event, NULL);
            break;

        case UPGRADE_HOST_SILENT_COMMIT_CFM:
            StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_silent_commit_event, NULL);
            break;

        case UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_CFM:
            dfuProtocol_HandleUpgradeHostSilentCommitSupportedCfm(msg->size_data, msg->data);
            break;

        default:
            DEBUG_LOG_ERROR("dfuProtocol_HandleUpgradeTransportDataInd unexpected host msg");
            /* Ignore the unexpected message. */
            break;
    }
}

static void dfuProtocol_HandleUpgradeTransportConnectCfm(UPGRADE_TRANSPORT_CONNECT_CFM_T *msg)
{
    UNUSED(msg);
    dfu_protocol_events_t event = DfuProtocol_DidActiveClientCauseReboot() ? dfu_protocol_transport_connected_post_reboot_event : dfu_protocol_transport_connected_event;
    StateMachine_Update(DfuProtocol_GetStateMachine(), event, NULL);
}

static void dfuProtocol_HandleUpgradeTransportDisconnectCfm(UPGRADE_TRANSPORT_DISCONNECT_CFM_T *msg)
{
    DEBUG_LOG_VERBOSE("dfuProtocol_HandleUpgradeTransportDisconnectCfm: msg->status %d", msg->status);
    dfu_protocol_transport_disconnected_params_t params = { .is_upgrade_successful = msg->status };
    StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_transport_disconnected_event, (void *)&params);
}

static void dfuProtocol_HandleUpgradeTransportDataCfm(UPGRADE_TRANSPORT_DATA_CFM_T *msg)
{
    if(msg && msg->data)
    {
        free((void *)msg->data);
    }

    if ((DfuProtocol_GetRemainingNumberOfBytesRequested() > 0) && (DfuProtocol_GetLengthOfAvailableDataInBytes() > 0))
    {
        DfuProtocol_SendUpdateHostData();
    }
}

static void dfuProtocol_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        /* Handle an external message from the upgrade library. */
        case UPGRADE_TRANSPORT_DATA_IND:
            dfuProtocol_HandleUpgradeTransportDataInd((UPGRADE_TRANSPORT_DATA_IND_T *) message);
            break;

        case UPGRADE_TRANSPORT_CONNECT_CFM:
            DEBUG_LOG_DEBUG("dfuProtocol_MessageHandler UPGRADE_TRANSPORT_CONNECT_CFM");
            dfuProtocol_HandleUpgradeTransportConnectCfm((UPGRADE_TRANSPORT_CONNECT_CFM_T *) message);
            break;

        case UPGRADE_TRANSPORT_DISCONNECT_CFM:
            DEBUG_LOG_DEBUG("dfuProtocol_MessageHandler UPGRADE_TRANSPORT_DISCONNECT_CFM");
            dfuProtocol_HandleUpgradeTransportDisconnectCfm((UPGRADE_TRANSPORT_DISCONNECT_CFM_T *) message);
            break;

        case UPGRADE_TRANSPORT_DATA_CFM:
            DEBUG_LOG_DEBUG("dfuProtocol_MessageHandler UPGRADE_TRANSPORT_DATA_CFM");
            dfuProtocol_HandleUpgradeTransportDataCfm((UPGRADE_TRANSPORT_DATA_CFM_T *) message);
            break;

#ifdef INCLUDE_DFU_PEER
        /* Handle an external message from the upgrade peer. */
        case DFU_PEER_INIT_CFM:
            /* Ignore */
            break;

        case DFU_PEER_STARTED:
            DEBUG_LOG_DEBUG("dfuProtocol_MessageHandler DFU_PEER_STARTED");

            if(DfuProtocol_DidActiveClientCauseReboot() && BtDevice_IsMyAddressPrimary())
            {
                dfu_protocol_start_params_t start_params = { 0, Upgrade_GetContext() };
                StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_start_post_reboot_event, &start_params);
            }
            break;
#endif /* INCLUDE_DFU_PEER */

        default:
            DEBUG_LOG_VERBOSE("dfuProtocol_MessageHandler unexpected msg 0x%04x", id);
            /* Ignore the unexpected message. */
            break;
    }
}

void DfuProtocol_MessageHandlerInit(void)
{
#ifdef INCLUDE_DFU_PEER
    DfuPeer_ClientRegister((Task)&dfu_protocol_message_handler_task_data);
#endif
}

Task DfuProtocol_GetMessageHandlerTask(void)
{
    return (Task)&dfu_protocol_message_handler_task_data;
}
