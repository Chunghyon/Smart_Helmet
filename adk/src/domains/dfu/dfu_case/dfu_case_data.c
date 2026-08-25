/*!
\copyright  Copyright (c) 2019-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_case_data.c
\brief      dfu file parser of the dfu_case

    Sequentially parses the dfu file containing case fw image
*/

#ifdef INCLUDE_DFU_CASE

#include "dfu_case_data.h"
#include "dfu_case_data_private.h"
#include "dfu_case_private.h"
#include "dfu_case_host.h"
#include "dfu_case_fw.h"
#include <upgrade.h>

#include <util.h>
#include <service.h>
#include <stdlib.h>
#include <string.h> /* for memset */
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stream.h>
#include <source.h>
#include <sink.h>
#include <byte_utils.h>

static dfu_case_data_context_t* dfu_case_data_context = {0};

static UpgradeHostErrorCode dfuCase_DataParseCompleteData(const uint8 *data, uint16 len, bool reqComplete);
static void dfuCase_DataRequestData(uint32 size, uint32 offset);

/*! \brief Get the private context of dfu_case_data
*/
static dfu_case_data_context_t* dfuCase_DataGetContext(void)
{
    return dfu_case_data_context;
}

/*! \brief Set the private context of dfu_case_data
*/
static void dfuCase_DataSetContext(dfu_case_data_context_t *context)
{
    dfu_case_data_context = context;
}

/*! \brief Get the dfu_case_data FSM state
*/
static dfu_case_data_state_t dfuCase_DataGetState(void)
{
    return dfuCase_DataGetContext()->state;
}

/*! \brief Set the dfu_case_data FSM state
*/
static void dfuCase_DataSetState(dfu_case_data_state_t new_state)
{
    dfuCase_DataGetContext()->state = new_state;
}

/*! \brief parse the incomplete data

        data is incomplete if its less then what we requested. this fn verifies the size and 
        stores it in a buffer.

    \param data buffer containing the data
    \param data_len size of the data to parse
    \return error code if any error occurs during the parsing
*/
static UpgradeHostErrorCode dfuCase_DataParseIncomplete(const uint8 *data, uint16 data_len)
{
    dfu_case_data_context_t* context = dfuCase_DataGetContext();

    UpgradeHostErrorCode status = UPGRADE_HOST_SUCCESS;

    DEBUG_LOG("dfuCase_DataParseIncomplete, data_len %d", data_len);

    /* Calculate space left in buffer */
    uint8 buf_space = sizeof(context->incomplete_data.data) - context->incomplete_data.size;

    if (data_len > buf_space)
    {
        DEBUG_LOG_ERROR("dfuCase_DataParseIncomplete, header too big for buffer");
        return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
    }

    /* Copy into buffer */
    memmove(&context->incomplete_data.data[context->incomplete_data.size], data, data_len);
    context->incomplete_data.size += data_len;

    /* Parse data if request complete or buffer is full */
    const bool req_complete = (context->total_received_size == context->total_req_size);
    DEBUG_LOG("dfuCase_DataParseIncomplete, received %u, requested %u, complete %u",
                   context->total_received_size, context->total_req_size, req_complete);

    /* If request is now complete attempt to parse again */
    if (req_complete)
    {
        status = dfuCase_DataParseCompleteData(context->incomplete_data.data, context->incomplete_data.size,
                                          req_complete);
        context->incomplete_data.size = 0;
    }

    DEBUG_LOG("dfuCase_DataParseIncomplete, status %u", status);
    return status;
}

/*! \brief Determine size of the next data request
*/
static void dfuCase_DataRequestData(uint32 size, uint32 offset)
{
    dfu_case_data_context_t* context = dfuCase_DataGetContext();

    DEBUG_LOG_DEBUG("dfuCase_DataRequestData: size=%u offset=%u", size, offset);
    context->total_req_size = size;
    context->total_received_size = 0;
    context->pending_chunk_size = 0;

    /* We could have added the resume offset if transport got reconnected so, add the requested offset
     * in the existing offset. */
    context->offset += offset;

    /* Reset the incomplete_data buffer to use it for new request. */
    context->incomplete_data.size = 0;
}

/*! \brief handle the data possibly containing the generic first part of a dfu file

    \param data buffer containing the data
    \param len size of the data to parse
    \param req_complete TRUE if we received the complete size of requested data
    \return error code if any error occurs during the parsing
*/
static UpgradeHostErrorCode dfuCase_DataHandleGeneric1stPartState(const uint8 *data, uint16 len, bool req_complete)
{
    dfu_case_data_context_t* context = dfuCase_DataGetContext();
    uint32 length;

    if (!req_complete)
    {
        /* Handle the case where packet size is smaller than header size */
        DEBUG_LOG("dfuCase_DataHandleHeaderState, header not complete");
        return dfuCase_DataParseIncomplete(data, len);
    }

    if (len < DFU_CASE_HEADER_FIRST_PART_SIZE)
    {
        return UPGRADE_HOST_ERROR_BAD_LENGTH_TOO_SHORT;
    }
    length = ByteUtilsGet4BytesFromStream(&data[DFU_CASE_ID_FIELD_SIZE]);

    DEBUG_LOG("dfuCase_DataHandleGeneric1stPartState, id '%c%c%c%c%c%c%c%c', length 0x%lx",
                      data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], length);

    /* We have received a generic first part from the DFU file so, increase the overall file offset. */
    dfuCase_DataGetContext()->dfu_file_offset += len;

    /* SRECDATA */
    if(0 == strncmp((char *)data, DFU_CASE_SREC_PARTITION_ID, DFU_CASE_ID_FIELD_SIZE))
    {
        if (length < DFU_CASE_SREC_PARTITION_SECOND_HEADER_SIZE)
        {
            return UPGRADE_HOST_ERROR_BAD_LENGTH_PARTITION_HEADER;
        }

        context->partition_length = length - DFU_CASE_SREC_PARTITION_SECOND_HEADER_SIZE;

        dfuCase_DataRequestData(DFU_CASE_SREC_PARTITION_SECOND_HEADER_SIZE, DFU_CASE_DATA_REQUEST_NO_OFFSET);
        dfuCase_DataSetState(DFU_CASE_DATA_STATE_DATA_HEADER);
        MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_REQUEST_MORE_DATA, NULL);
    }
    /* APPUPFTR */
    else if (0 == strncmp((char *)data, DFU_CASE_FOOTER_ID, DFU_CASE_ID_FIELD_SIZE))
    {
        if(length)
        {
            /* dfu file containing the case image shouldn't have any footer signature. */
            return UPGRADE_HOST_ERROR_BAD_LENGTH_SIGNATURE;
        }
        return UPGRADE_HOST_DATA_TRANSFER_COMPLETE;
    }
    else
    {
        return UPGRADE_HOST_ERROR_UNKNOWN_ID;
    }

    return UPGRADE_HOST_SUCCESS;
}

/*! \brief handle the data possibly containing the second part of the header of a dfu file

    \param data buffer containing the data
    \param len size of the data to parse
    \param req_complete TRUE if we received the complete size of requested data
    \return error code if any error occurs during the parsing
*/
static UpgradeHostErrorCode dfuCase_DataHandleHeaderState(const uint8 *data, uint16 len, bool req_complete)
{
    if (!req_complete)
    {
        /* Handle the case where packet size is smaller than header size */
        DEBUG_LOG("dfuCase_DataHandleHeaderState, header not complete");
        return dfuCase_DataParseIncomplete(data, len);
    }

    /* Length must contain at least ID FIELD, major, minor and compatibleVersions */
    if (len < (DFU_CASE_ID_FIELD_SIZE + DFU_CASE_VERSION_SIZE + DFU_CASE_NO_OF_COMPATIBLE_UPGRADES_SIZE))
    {
        DEBUG_LOG_ERROR("dfuCase_DataHandleHeaderState, packet size incorrect");
        return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
    }

    //To do : use variant and version numbers to check compatibility

    /* We have received a DFU file header from the host DFU file so, increase the overall file offset. */
    dfuCase_DataGetContext()->dfu_file_offset += len;

    MessageSend(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_DFU_COMPATIBLE, NULL);
    dfuCase_DataRequestData(DFU_CASE_HEADER_FIRST_PART_SIZE, DFU_CASE_DATA_REQUEST_NO_OFFSET);
    dfuCase_DataSetState(DFU_CASE_DATA_STATE_GENERIC_1ST_PART);
    MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_REQUEST_MORE_DATA, NULL);
    return UPGRADE_HOST_SUCCESS;
}

/*! \brief handle the data possibly containing the second part of a partition header

    \param data buffer containing the data
    \param len size of the data to parse
    \param req_complete TRUE if we received the complete size of requested data
    \return error code if any error occurs during the parsing
*/
static UpgradeHostErrorCode dfuCase_DataHandleDataHeaderState(const uint8 *data, uint16 len, bool req_complete)
{
    dfu_case_data_context_t* context = dfuCase_DataGetContext();

    if (!req_complete)
    {
        /* Handle the case where packet size is smaller than header size */
        DEBUG_LOG("dfuCase_DataHandleHeaderState, header not complete");
        return dfuCase_DataParseIncomplete(data, len);
    }

    if (len < DFU_CASE_SREC_PARTITION_SECOND_HEADER_SIZE)
    {
        return UPGRADE_HOST_ERROR_BAD_LENGTH_DATAHDR_RESUME;
    }

    /* We have received a SREC partition header from the host DFU file so, increase the overall file offset. */
    dfuCase_DataGetContext()->dfu_file_offset += len;

    context->bank_b_offset = ByteUtilsGet4BytesFromStream(data);
    if(context->bank_b_offset > context->partition_length)
    {
        DEBUG_LOG("dfuCase_DataHandleDataHeaderState bank_b_offset %u", context->bank_b_offset);
        return UPGRADE_HOST_ERROR_PARTITION_SIZE_MISMATCH;
    }

    dfuCase_DataSetState(DFU_CASE_DATA_STATE_DATA);
    if(context->is_case_ready_for_data)
    {
        DfuCase_DataHandleCaseReady();
    }
    return UPGRADE_HOST_SUCCESS;
}

/*! \brief handle the data possibly containing the data part of a partition

    \param data buffer containing the data
    \param len size of the data to parse
    \param req_complete TRUE if we received the complete size of requested data
    \return error code if any error occurs during the parsing
*/
static UpgradeHostErrorCode dfuCase_DataHandleDataState(const uint8 *data, uint16 len, bool req_complete)
{
    UNUSED(data);
    UNUSED(req_complete);

    DEBUG_LOG("dfuCase_DataHandleDataState data received %u", len);
    MESSAGE_MAKE(msg, DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE_T);
    msg->data = data;
    msg->len = len;

    /* We have received len bytes of data from the host DFU file so, increase the overall file offset. */
    dfuCase_DataGetContext()->dfu_file_offset += len;

    MessageSend(dfuCase_FWGetTask(), DFU_CASE_INTERNAL_MORE_DATA_AVAILABLE, msg);

    if (req_complete)
    {
        uint32 offset=0;
        DEBUG_LOG_INFO("dfuCase_DataHandleDataState, SREC partition has been received");

        if(DfuCase_FWGetBankToUpgrade() == DFU_CASE_FW_BANK_A)
        {
            offset = dfuCase_DataGetContext()->partition_length - dfuCase_DataGetContext()->bank_b_offset;
        }

        /* Request the first header of the footer. */
        dfuCase_DataRequestData(DFU_CASE_HEADER_FIRST_PART_SIZE, offset);

    /* We might be skipping some part of the partition data depending on the case Bank getting updated so,
     * increase the overall offset accordingly. */
        dfuCase_DataGetContext()->dfu_file_offset += offset;

        dfuCase_DataSetState(DFU_CASE_DATA_STATE_GENERIC_1ST_PART);
    }

    return UPGRADE_HOST_SUCCESS;
}

/*! \brief parse the new data

        we have received a new block of data which needs to be handled and there is no
        incomplete data in the buffer.

    \param data buffer containing the data
    \param len size of the data to parse
    \param req_complete TRUE if we received the complete size of requested data
    \return error code if any error occurs during the parsing
*/
static UpgradeHostErrorCode dfuCase_DataParseCompleteData(const uint8 *data, uint16 len, bool req_complete)
{
    UpgradeHostErrorCode rc = UPGRADE_HOST_ERROR_INTERNAL_ERROR_1;

    DEBUG_LOG_VERBOSE("dfuCase_DataHandleMessage, state enum:dfu_case_data_state_t:%u, length %d, complete %u",
                      dfuCase_DataGetState(), len, req_complete);

    dfu_case_data_state_t state = dfuCase_DataGetState();
    switch (dfuCase_DataGetState())
    {
    case DFU_CASE_DATA_STATE_GENERIC_1ST_PART:
        DEBUG_LOG_VERBOSE("dfuCase_DataHandleMessage, DFU_CASE_DATA_STATE_GENERIC_1ST_PART");
        rc = dfuCase_DataHandleGeneric1stPartState(data, len, req_complete);
        break;

    case DFU_CASE_DATA_STATE_HEADER:
        DEBUG_LOG_VERBOSE("dfuCase_DataHandleMessage, DFU_CASE_DATA_STATE_HEADER");
        rc = dfuCase_DataHandleHeaderState(data, len, req_complete);
        break;

    case DFU_CASE_DATA_STATE_DATA_HEADER:
        DEBUG_LOG_VERBOSE("dfuCase_DataHandleMessage, DFU_CASE_DATA_STATE_DATA_HEADER");
        rc = dfuCase_DataHandleDataHeaderState(data, len, req_complete);
        break;

    case DFU_CASE_DATA_STATE_DATA:
        DEBUG_LOG_VERBOSE("dfuCase_DataHandleMessage, DFU_CASE_DATA_STATE_DATA");
        rc = dfuCase_DataHandleDataState(data, len, req_complete);
        break;
    }

    if (state != dfuCase_DataGetState())
    {
        DEBUG_LOG_INFO("dfuCase_DataHandleMessage, new state enum:dfu_case_data_state_t:%u", dfuCase_DataGetState());
    }
    if (rc != UPGRADE_HOST_SUCCESS)
    {
        DEBUG_LOG_INFO("dfuCase_DataHandleMessage, status %u", rc);
    }
    return rc;
}

/*! \brief Initialise dfu_case_host
*/
bool DfuCase_DataInit(void)
{
    dfu_case_data_context_t *context = PanicUnlessMalloc(sizeof(dfu_case_data_context_t));
    memset(context, 0, sizeof(dfu_case_data_context_t));
    dfuCase_DataSetContext(context);

    /* Upgrade library has already requested first generic part so add it to overall offset. */
    dfuCase_DataGetContext()->dfu_file_offset = DFU_CASE_HEADER_FIRST_PART_SIZE;

    return TRUE;
}

/*! \brief start data transfer for new DFU
*/
void dfuCase_DataStartDataTransfer(void)
{
        DEBUG_LOG("dfuCase_DataStartDataTransfer");
        /* Request DFU file header second part for a new DFU. */
        dfuCase_DataRequestData(UpgradeGetCaseDfuHeaderLength(), DFU_CASE_DATA_REQUEST_NO_OFFSET);
        dfuCase_DataSetState(DFU_CASE_DATA_STATE_HEADER);
}

/*! \brief calculate dfu file offset to resume data transfer. 

        Also, carry forward the size of unreceived requested data
*/
void dfuCase_DataCalculateResumeOffset(void)
{
    dfu_case_data_context_t* context = dfuCase_DataGetContext();

    context->offset = context->dfu_file_offset;
    context->total_req_size = context->total_req_size - context->total_received_size;
    context->pending_chunk_size = context->total_received_size = 0;
    DEBUG_LOG("dfuCase_DataCalculateResumeOffset offset %d, remaining data to request %d", context->dfu_file_offset, context->total_req_size);
}

/*! \brief fetch and reset the value of the next request size
*/
uint32 DfuCase_DataGetNextReqSize(void)
{
    dfu_case_data_context_t* context = dfuCase_DataGetContext();

    uint32 pending_data = context->total_req_size - context->total_received_size;
    if(pending_data && !context->pending_chunk_size)
    {
        context->pending_chunk_size = (pending_data < DFU_CASE_DATA_MAX_REQ_SIZE? pending_data: DFU_CASE_DATA_MAX_REQ_SIZE);
        return context->pending_chunk_size;
    }
    return 0;
}

/*! \brief get the value of the next request offset (data to skip from current position)
*/
uint32 DfuCase_DataGetNextOffset(void)
{
    return dfuCase_DataGetContext()->offset;
}

/*! \brief dfuCase_DataParse

    If the received upgrade data is larger than the expected size in the next 
    data request 'nextReqSize' then this loops through until the received data 
    is either completely passed to case or copied into the upgrade 
    buffer,'incompleteData'

    \return Upgrade library error code.
*/
UpgradeHostErrorCode dfuCase_DataParse(const uint8 *data, uint16 data_len)
{
    uint16 data_percentage;
    dfu_case_data_context_t* context = dfuCase_DataGetContext();

    UpgradeHostErrorCode status = UPGRADE_HOST_SUCCESS;

    /* Update total received size */
    context->total_received_size += data_len;
    context->offset = 0;

    data_percentage = 100 * context->total_received_size / context->total_req_size;
    DEBUG_LOG_INFO("dfuCase_DataParse, data_len %u, total_size %u, total_received %u (%u%%)",
                   data_len, context->total_req_size, context->total_received_size, data_percentage);

    /* Error if we've received more than we requested */
    if (context->total_received_size > context->total_req_size || data_len > context->pending_chunk_size)
    {
        return UPGRADE_HOST_ERROR_BAD_LENGTH_PARTITION_PARSE;
    }
    context->pending_chunk_size -= data_len;

    /* Handle case of incomplete data */
    if (context->incomplete_data.size)
    {
        status = dfuCase_DataParseIncomplete(data, data_len);
    }
    else
    {
        /* Parse block */
        const bool req_complete = context->total_received_size == context->total_req_size;
        status = dfuCase_DataParseCompleteData(data, data_len, req_complete);
    }

    DEBUG_LOG_VERBOSE("dfuCase_DataParse, status %u", status);
    return status;
}

/*! \brief case has sent the READY message, handle this event

        READY message from case would contain the current running bank.
        Accordingly, we need to calculate the offset in the SREC partition 
        and request more data.
*/
void DfuCase_DataHandleCaseReady(void)
{
    dfu_case_data_context_t* context = dfuCase_DataGetContext();

    context->is_case_ready_for_data = TRUE;

    if(dfuCase_DataGetState() == DFU_CASE_DATA_STATE_DATA)
    {
        uint32 offset, req_size;
        if(DfuCase_FWGetBankToUpgrade() == DFU_CASE_FW_BANK_A)
        {
            offset = 0;
            req_size = context->bank_b_offset;
        }
        else
        {
            offset = context->bank_b_offset;
            req_size = context->partition_length - context->bank_b_offset;
        }
        dfuCase_DataRequestData(req_size, offset);

        /* We might be skipping some part of the partition data depending on the case Bank getting updated so,
        * increase the overall offset accordingly. */
        dfuCase_DataGetContext()->dfu_file_offset += offset;

        MessageSend(dfuCase_HostGetTask(), DFU_CASE_INTERNAL_REQUEST_MORE_DATA, NULL);
    }
}

/*! \brief Clean up after DFU completes or aborts
*/
void DfuCase_DataCleanUp(void)
{
    if(dfuCase_DataGetContext())
    {
        free(dfuCase_DataGetContext());
        dfuCase_DataSetContext(0);
    }
}

#endif /* INCLUDE_DFU_CASE */
