/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_data.c
\brief      Implemetation of the data caching APIs for the dfu_protocol module
*/

#include "dfu_protocol_log.h"

#include "dfu_protocol_data.h"
#include "dfu_protocol_client_notifier.h"

#include <message.h>
#include <panic.h>
#include <stdlib.h>

static dfu_protocol_data_t dfu_protocol_data;

static bool dfuProtocol_WillWriteWrap(uint16 length_to_write)
{
    uint16 space_up_to_end_of_buffer = BUFFER_SIZE_IN_BYTES - dfu_protocol_data.write_index;

    return (dfu_protocol_data.write_index >= dfu_protocol_data.read_index) &&
           (dfu_protocol_data.read_index != 0) &&
           (length_to_write > space_up_to_end_of_buffer);
}

static bool dfuProtocol_WillReadWrap(uint16 length_to_read)
{
    uint16 data_up_to_end_of_buffer = BUFFER_SIZE_IN_BYTES - dfu_protocol_data.read_index;

    return (dfu_protocol_data.read_index > dfu_protocol_data.write_index) &&
           (dfu_protocol_data.write_index != 0) &&
           (length_to_read > data_up_to_end_of_buffer);
}

static void dfuProtocol_IncreaseBufferIndex(uint16 * index, uint16 length_to_increase)
{
    *index += length_to_increase;

    if(*index >= BUFFER_SIZE_IN_BYTES)
    {
        *index -= BUFFER_SIZE_IN_BYTES;
    }
}

static bool dfuProtocol_ExpectingFirstChunkOfData(void)
{
    return (dfu_protocol_data.read_offset == 0) && (dfu_protocol_data.write_offset == 0);
}

static bool dfuProtocol_IsRequestedDataBeyondTheDataWrite(uint16 length_to_write, uint32 offset_of_data_start)
{
    uint16 offset_from_start_of_data_to_write_offset = offset_of_data_start >= dfu_protocol_data.write_offset ? 0 : dfu_protocol_data.write_offset - offset_of_data_start;

    return (offset_of_data_start + length_to_write < dfu_protocol_data.requested_read_offset) ||
           (length_to_write < offset_from_start_of_data_to_write_offset);
}

static bool dfuProtocol_PopulateCacheFromBuffer(uint8 * buffer, uint16 * length_to_write, uint32 * offset_of_start_of_data)
{
    bool status = FALSE;
    uint16 free_space_in_bytes = BUFFER_SIZE_IN_BYTES - DfuProtocol_GetLengthOfAvailableDataInBytes();
    int16 offset_from_start_of_data_to_write_offset = *offset_of_start_of_data >= dfu_protocol_data.write_offset ? 0 : dfu_protocol_data.write_offset - *offset_of_start_of_data;

    PanicNull(buffer);
    PanicFalse(*offset_of_start_of_data <= dfu_protocol_data.write_offset);
    PanicFalse(dfu_protocol_data.write_offset + *length_to_write <= dfu_protocol_data.total_dfu_file_size);
    PanicNull(dfu_protocol_data.buffer);

    uint8 * head_of_data = buffer;

    if(*offset_of_start_of_data < dfu_protocol_data.write_offset)
    {
        /* Skip the data which has already been written */
        head_of_data += offset_from_start_of_data_to_write_offset;
        *length_to_write -= offset_from_start_of_data_to_write_offset;
        *offset_of_start_of_data = dfu_protocol_data.write_offset;
    }

    if(free_space_in_bytes >= *length_to_write)
    {
        if(dfuProtocol_WillWriteWrap(*length_to_write))
        {
            /* Write up to the end of the buffer, then wrap */
            uint16 space_up_to_end_of_buffer = BUFFER_SIZE_IN_BYTES - dfu_protocol_data.write_index;
            memcpy(&dfu_protocol_data.buffer[dfu_protocol_data.write_index], head_of_data, space_up_to_end_of_buffer);
            memcpy(&dfu_protocol_data.buffer[0], &head_of_data[space_up_to_end_of_buffer], *length_to_write - space_up_to_end_of_buffer);
        }
        else
        {
            /* Can write everything contiguously */
            memcpy(&dfu_protocol_data.buffer[dfu_protocol_data.write_index], head_of_data, *length_to_write);
        }

        dfu_protocol_data.write_offset += *length_to_write;
        dfuProtocol_IncreaseBufferIndex(&dfu_protocol_data.write_index, *length_to_write);

        status = TRUE;
    }

    return status;
}

static bool dfuProtocol_PopulateBufferFromCache(uint8 * buffer_to_populate, uint16 length_to_read)
{
    bool status = FALSE;
    uint32 read_offset_actual_and_requested_difference = dfu_protocol_data.requested_read_offset - dfu_protocol_data.read_offset;
    dfu_protocol_data.read_offset = dfu_protocol_data.requested_read_offset;
    dfuProtocol_IncreaseBufferIndex(&dfu_protocol_data.read_index, read_offset_actual_and_requested_difference);

    PanicNull(buffer_to_populate);
    PanicNull(dfu_protocol_data.buffer);
    PanicFalse(length_to_read <= dfu_protocol_data.requested_number_of_bytes);

    if(length_to_read <= DfuProtocol_GetLengthOfAvailableDataInBytes())
    {
        if(dfuProtocol_WillReadWrap(length_to_read))
        {
            /* Copy from up to the end of the buffer, then wrap */
            uint16 data_up_to_end_of_buffer = BUFFER_SIZE_IN_BYTES - dfu_protocol_data.read_index;
            memcpy(buffer_to_populate, &dfu_protocol_data.buffer[dfu_protocol_data.read_index], data_up_to_end_of_buffer);
            memcpy(&buffer_to_populate[data_up_to_end_of_buffer], &dfu_protocol_data.buffer[0], length_to_read - data_up_to_end_of_buffer);
        }
        else
        {
             /* Can read everything contiguously */
            memcpy(buffer_to_populate, &dfu_protocol_data.buffer[dfu_protocol_data.read_index], length_to_read);
        }

        dfu_protocol_data.read_offset += length_to_read;
        dfu_protocol_data.requested_read_offset += length_to_read;
        dfuProtocol_IncreaseBufferIndex(&dfu_protocol_data.read_index, length_to_read);

        dfu_protocol_data.requested_number_of_bytes -= length_to_read;

        status = TRUE;
    }

    return status;
}


void DfuProtocol_CreateDataInstance(uint32 total_dfu_file_size)
{
    DEBUG_LOG("DfuProtocol_CreateDataInstance");
    dfu_protocol_data.buffer = (uint8 *)PanicUnlessMalloc(BUFFER_SIZE_IN_BYTES);
    memset(dfu_protocol_data.buffer, 0, BUFFER_SIZE_IN_BYTES);
    dfu_protocol_data.total_dfu_file_size = total_dfu_file_size;
}

void DfuProtocol_DestroyDataInstance(void)
{
    DEBUG_LOG("DfuProtocol_DestroyDataInstance");
    free(dfu_protocol_data.buffer);
    memset(&dfu_protocol_data, 0, sizeof dfu_protocol_data);
}

void DfuProtocol_SetRequestedReadOffset(uint32 offset_from_previous_request)
{
    dfu_protocol_data.requested_read_offset += offset_from_previous_request;
    PanicFalse(dfu_protocol_data.requested_read_offset <= dfu_protocol_data.total_dfu_file_size);

    if(dfu_protocol_data.requested_read_offset < dfu_protocol_data.read_offset)
    {
        DfuProtocol_SendErrorIndToActiveClient(UPGRADE_HOST_ERROR_UPDATE_FAILED);
    }
    else if(dfu_protocol_data.requested_read_offset >= dfu_protocol_data.write_offset)
    {
        /* Where the Upgrade library is interested in reading from is beyond any data we have stored */
        dfu_protocol_data.write_offset = dfu_protocol_data.read_offset = dfu_protocol_data.requested_read_offset;
        dfu_protocol_data.write_index = dfu_protocol_data.read_index = 0;
    }
}

void DfuProtocol_SetRequestedNumberOfBytes(uint32 number_of_bytes)
{
    /* Should not overwrite an incomplete request */
    PanicFalse(dfu_protocol_data.requested_number_of_bytes == 0);
    dfu_protocol_data.requested_number_of_bytes = number_of_bytes;
}

uint32 DfuProtocol_GetRemainingNumberOfBytesRequested(void)
{
    return dfu_protocol_data.requested_number_of_bytes;
}

uint16 DfuProtocol_GetLengthOfAvailableDataInBytes(void)
{
    uint16 length_of_data_in_bytes = dfu_protocol_data.write_offset - dfu_protocol_data.read_offset;
    DEBUG_LOG_V_VERBOSE("DfuProtocol_GetLengthOfAvailableDataInBytes %u", length_of_data_in_bytes);

    return length_of_data_in_bytes;
}

bool DfuProtocol_WriteData(uint8 * data, uint16 length_to_write, uint32 offset_of_start_of_data)
{
    bool status = FALSE;

    if(dfuProtocol_ExpectingFirstChunkOfData() && (dfu_protocol_data.requested_read_offset < offset_of_start_of_data))
    {
        /* Client is trying to resume, allow this and panic if there is a mismatch when Upgrade library sets up its request in DfuProtocol_SetRequestedReadOffset() */
        dfu_protocol_data.write_offset = dfu_protocol_data.read_offset = offset_of_start_of_data;
    }

    if(dfuProtocol_IsRequestedDataBeyondTheDataWrite(length_to_write, offset_of_start_of_data))
    {
        DEBUG_LOG("DfuProtocol_WriteData requested data is beyond the data we are writing, ignore this data");
        status = TRUE;
    }
    else
    {
        status = dfuProtocol_PopulateCacheFromBuffer(data, &length_to_write, &offset_of_start_of_data);
    }

    DEBUG_LOG("DfuProtocol_WriteData length_to_write %u offset_in_dfu_file %u status %u", length_to_write, offset_of_start_of_data, status);

    return status;
}

bool DfuProtocol_ReadData(uint8 * buffer_to_populate, uint16 length_to_read)
{
    bool status = FALSE;

    PanicFalse(dfu_protocol_data.requested_read_offset + length_to_read <= dfu_protocol_data.total_dfu_file_size);

    if(dfu_protocol_data.requested_read_offset < dfu_protocol_data.read_offset)
    {
        DEBUG_LOG_WARN("DfuProtocol_ReadData requested data is before the cached data");
    }
    else
    {
        if((dfu_protocol_data.requested_read_offset != dfu_protocol_data.read_offset) && (dfu_protocol_data.requested_read_offset + length_to_read >= dfu_protocol_data.write_offset))
        {
            DEBUG_LOG_WARN("DfuProtocol_ReadData requested data has not yet been written");
        }
        else
        {
            status = dfuProtocol_PopulateBufferFromCache(buffer_to_populate, length_to_read);
        }
    }

    DEBUG_LOG("DfuProtocol_ReadData length_to_read %u status %u", length_to_read, status);

    if(DfuProtocol_GetLengthOfAvailableDataInBytes() == 0)
    {
        DfuProtocol_SendCacheClearedIndToActiveClient();
    }

    return status;
}

bool DfuProtocol_IsReadingLastPacket(uint16 length_to_read)
{
    return dfu_protocol_data.read_offset + length_to_read == dfu_protocol_data.total_dfu_file_size;
}

#ifdef HOSTED_TEST_ENVIRONMENT
dfu_protocol_data_t * DfuProtocol_GetInternalData(void)
{
    return &dfu_protocol_data;
}
#endif /* HOSTED_TEST_ENVIRONMENT */
