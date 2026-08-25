/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_message_dispatcher.c
\brief      Implemetation of the message dispatching APIs for the dfu_protocol module
*/

#include "dfu_protocol_log.h"

#include "dfu_protocol_message_dispatcher.h"
#include "dfu_protocol_data.h"
#include "dfu_protocol_sm.h"

#include <byte_utils.h>
#include <panic.h>
#include <stdlib.h>
#include <upgrade.h>
#include <upgrade_protocol.h>

#define MAX_UPGRADE_HOST_DATA_PAYLOAD_LENGTH 112

/*! Sends an Upgrade message that has no data aside from ID and length */
static void dfuProtocol_SendShortUpgradeMessage(UpgradeMsgHost upgrade_msg_id)
{
    uint16 byte_index = 0;
    uint8 * payload = (uint8 *)PanicUnlessMalloc(ID_SIZE + LENGTH_SIZE);
    byte_index += ByteUtilsSet1Byte(payload, byte_index, upgrade_msg_id - UPGRADE_HOST_MSG_BASE);
    byte_index += ByteUtilsSet2Bytes(payload, byte_index, 0);
    UpgradeProcessDataRequest(byte_index, payload);
}

static void dfuProtocol_SendGenericActionUpgradeMessage(UpgradeMsgHost upgrade_msg_id, UpgradeHostActionCode upgrade_action_code)
{
    uint16 byte_index = 0;
    uint8 * payload = PanicUnlessMalloc(ID_SIZE + LENGTH_SIZE + sizeof(UPGRADE_HOST_GENERIC_ACTION_T));
    byte_index += ByteUtilsSet1Byte(payload, byte_index, upgrade_msg_id - UPGRADE_HOST_MSG_BASE);
    byte_index += ByteUtilsSet2Bytes(payload, byte_index, (uint16) sizeof(UPGRADE_HOST_GENERIC_ACTION_T));
    byte_index += ByteUtilsSet1Byte(payload, byte_index, upgrade_action_code);
    UpgradeProcessDataRequest(byte_index, payload);
}


void DfuProtocol_SyncOtaHostRequest(uint32 in_progress_id)
{
    uint16 byte_index = 0;
    uint8 * payload = (uint8 *)PanicUnlessMalloc(ID_SIZE + LENGTH_SIZE + sizeof(UPGRADE_HOST_SYNC_REQ_T));
    byte_index += ByteUtilsSet1Byte(payload, byte_index, UPGRADE_HOST_SYNC_REQ - UPGRADE_HOST_MSG_BASE);
    byte_index += ByteUtilsSet2Bytes(payload, byte_index, (uint16) sizeof(UPGRADE_HOST_SYNC_REQ_T));
    byte_index += ByteUtilsSet4Bytes(payload, byte_index, in_progress_id);
    UpgradeProcessDataRequest(byte_index, payload);
}

void DfuProtocol_StartOtaHostRequest(void)
{
    dfuProtocol_SendShortUpgradeMessage(UPGRADE_HOST_START_REQ);
}

void DfuProtocol_StartOtaHostDataRequest(void)
{
    dfuProtocol_SendShortUpgradeMessage(UPGRADE_HOST_START_DATA_REQ);
}

void DfuProtocol_IsCsrValidDoneRequest(void)
{
    dfuProtocol_SendShortUpgradeMessage(UPGRADE_HOST_IS_CSR_VALID_DONE_REQ);
}

void DfuProtocol_SilentCommitSupportedRequest(void)
{
    dfuProtocol_SendShortUpgradeMessage(UPGRADE_HOST_SILENT_COMMIT_SUPPORTED_REQ);
}

void DfuProtocol_AbortHostRequest(void)
{
    dfuProtocol_SendShortUpgradeMessage(UPGRADE_HOST_ABORT_REQ);
}
void DfuProtocol_OtaHostInProgressResponse(UpgradeHostActionCode action)
{
    dfuProtocol_SendGenericActionUpgradeMessage(UPGRADE_HOST_IN_PROGRESS_RES, action);
}

void DfuProtocol_OtaHostCommitConfirm(UpgradeHostActionCode action)
{
    dfuProtocol_SendGenericActionUpgradeMessage(UPGRADE_HOST_COMMIT_CFM, action);
}

void DfuProtocol_InteractiveOtaHostTransferCompleteResponse(void)
{
    dfuProtocol_SendGenericActionUpgradeMessage(UPGRADE_HOST_TRANSFER_COMPLETE_RES, (UpgradeHostActionCode)UPGRADE_HOST_TRANSFER_COMPLETE_ACTION_INTERACTIVE);
}

void DfuProtocol_SilentCommitOtaHostTransferCompleteResponse(void)
{
    dfuProtocol_SendGenericActionUpgradeMessage(UPGRADE_HOST_TRANSFER_COMPLETE_RES, (UpgradeHostActionCode)UPGRADE_HOST_TRANSFER_COMPLETE_ACTION_SILENT);
}

void DfuProtocol_SendUpdateHostData(void)
{
    uint16 byte_index = 0;
    uint32 number_of_bytes_in_cache = DfuProtocol_GetLengthOfAvailableDataInBytes();
    uint32 number_of_bytes_requested = DfuProtocol_GetRemainingNumberOfBytesRequested();
    uint32 length = MIN(MIN(MAX_UPGRADE_HOST_DATA_PAYLOAD_LENGTH, number_of_bytes_in_cache), number_of_bytes_requested);

    DEBUG_LOG_FN_ENTRY("DfuProtocol_SendUpdateHostData bytes_in_cache=%u bytes_requested=%u length_to_send=%u",
                        number_of_bytes_in_cache, number_of_bytes_requested, length);
    
    if(length)
    {
        uint8 is_last_packet = (uint8)DfuProtocol_IsReadingLastPacket(length);
        uint8 * payload = PanicUnlessMalloc(ID_SIZE + LENGTH_SIZE + LAST_PACKET_FLAG_SIZE + length);

        byte_index += ByteUtilsSet1Byte(payload, byte_index, UPGRADE_HOST_DATA - UPGRADE_HOST_MSG_BASE);
        /* Set length to one for the is_last_packet flag, plus the length of the data */
        byte_index += ByteUtilsSet2Bytes(payload, byte_index, length + 1);
        /* The is_last_packet is treated as 1 byte on the Upgrade side, despite being of size uint16 in UPGRADE_HOST_DATA_T */
        byte_index += ByteUtilsSet1Byte(payload, byte_index, is_last_packet);

        if(DfuProtocol_ReadData(&payload[byte_index], length))
        {
            byte_index += length;
            UpgradeProcessDataRequest(byte_index, payload);

            if(is_last_packet)
            {
                StateMachine_Update(DfuProtocol_GetStateMachine(), dfu_protocol_validate_event, NULL);
            }

        }
        else
        {
            free(payload);
            payload = NULL;
        }
    }
}

void DfuProtocol_ErrorWarnResponse(uint16 error_code)
{
    uint16 byte_index = 0;
    uint8 * payload = PanicUnlessMalloc(ID_SIZE + LENGTH_SIZE + sizeof(UPGRADE_HOST_ERRORWARN_RES_T));
    byte_index += ByteUtilsSet1Byte(payload, byte_index, UPGRADE_HOST_ERRORWARN_RES - UPGRADE_HOST_MSG_BASE);
    byte_index += ByteUtilsSet2Bytes(payload, byte_index, (uint16) sizeof(UPGRADE_HOST_ERRORWARN_RES_T));
    byte_index += ByteUtilsSet2Bytes(payload, byte_index, error_code);
    UpgradeProcessDataRequest(byte_index, payload);
}
