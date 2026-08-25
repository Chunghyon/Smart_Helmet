/****************************************************************************
Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_partition_reader.c

DESCRIPTION
    This file handles the partition read request state machine and reads data of requested
    length.

*/

#define DEBUG_LOG_MODULE_NAME upgrade_partition_reader
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <stream.h>
#include <source.h>
#include <imageupgrade.h>
#include <ps.h>

#include "upgrade_partition_reader.h"
#include "upgrade.h"


#define UPGRADE_HEADER_MIN_SECOND_PART_SIZE 22

/** Header image section. */
/* NOTE: this image section is for internal use cannot be upgraded
 * using image upgrade traps.
 * This image section is used to store all headers of DFU file
 */
#define UPGRADE_IMAGE_SECTION_DFU_HEADERS                  (0xd)

#define UPGRADE_RSA_2048

#define FIRST_WORD_SIZE 4

#if defined (UPGRADE_RSA_2048)
/*
 * EXPECTED_SIGNATURE_SIZE is the size of an RSA-2048 signature.
 * 2048 bits / 8 bits per byte is 256 bytes.
 */
#define EXPECTED_SIGNATURE_SIZE 256
#elif defined (UPGRADE_RSA_1024)
/*
 * EXPECTED_SIGNATURE_SIZE is the size of an RSA-1024 signature.
 * 1024 bits / 8 bits per byte is 128 bytes.
 */
#define EXPECTED_SIGNATURE_SIZE 128
#else
#error "Neither UPGRADE_RSA_2048 nor UPGRADE_RSA_1024 defined."
#endif

typedef struct {
    Source handle;
    uint16 pskey;
    uint16 read_offset;
    uint16 app_hdr_length;
    uint32 part_data_length;
    uint16 footer_length;
    uint16 partition_number;
    uint16 sqif_number;
    UpgradePartitionDataState partition_reader_state;
    uint32 partition_offset;
} upgrade_partition_reader_data_ctx_t;

/******************************************************************************
NAME
        upgradePartitionReader_OpenSource
DESCRIPTION
        Open a read handle to a physical partition on the external flash.

PARAMS
        physPartition Physical partition number in external flash.
RETURNS
        A valid handle or NULL if the open failed.
*/

static Source upgradePartitionReader_OpenSource(uint16 physPartition)
{
    uint16 QSPINum = 0;
    Source src = NULL;

    DEBUG_LOG("upgradePartitionReader_OpenSource opening partition %u", physPartition);

    src = ImageUpgradeStreamGetSource(QSPINum, physPartition);

    if (src == NULL)
    {
        DEBUG_LOG_WARN("upgradePartitionReader_OpenSource open failed %u", physPartition);
        return (Source)NULL;
    }

    return src;
}

/******************************************************************************
NAME
        upgradePartitionReader_OpenSourceAtOffset
DESCRIPTION
        Open a read handle to a physical partition on the external flash from the provided offset.

PARAMS
        physPartition Physical partition number in external flash.
        offset Offset in bytes from start of partition, where the next read will happen.
RETURNS
        A valid handle or NULL if the open failed.
*/

static Source upgradePartitionReader_OpenSourceAtOffset(uint16 partition_number, uint32 offset)
{
    uint16 QSPI = 0;
    Source src = NULL;

    DEBUG_LOG("upgradePartitionReader_OpenSourceAtOffset opening partition %u at offset %lu", partition_number, offset);
    src = ImageUpgradeStreamGetSourceFromOffset(QSPI, partition_number, offset);

    if (src == NULL)
    {
        DEBUG_LOG_WARN("upgradePartitionReader_OpenSourceAtOffset open failed %u",partition_number);
        return (Source)NULL;
    }
    return src;
}

static const char * upgradePartitionReader_GetHeaderID(void)
{
    return "APPUHDR5";
}

static const char * upgradePartitionRead_GetPartitionID(void)
{
    return "PARTDATA";
}

static const char * upgradePartitionReader_GetFooterID(void)
{
    return "APPUPFTR";
}

/******************************************************************************
NAME
    upgradePartitionReader_IsValidPartitionNum

DESCRIPTION
    Checks if the provided partition is accessible or not.

*/
static bool upgradePartitionReader_IsValidPartitionNum(uint16 partition_number)
{
    switch (partition_number)
    {
    case IMAGE_SECTION_NONCE:
    case IMAGE_SECTION_APPS_P0_HEADER:
    case IMAGE_SECTION_APPS_P1_HEADER:
    case IMAGE_SECTION_AUDIO_HEADER:
    case IMAGE_SECTION_CURATOR_FILESYSTEM:
    case IMAGE_SECTION_APPS_P0_IMAGE:
    case IMAGE_SECTION_APPS_RO_CONFIG_FILESYSTEM:
    case IMAGE_SECTION_APPS_RO_FILESYSTEM:
    case IMAGE_SECTION_APPS_P1_IMAGE:
    case IMAGE_SECTION_APPS_DEVICE_RO_FILESYSTEM:
    case IMAGE_SECTION_AUDIO_IMAGE:
    case IMAGE_SECTION_APPS_RW_CONFIG:
        return TRUE;

    case IMAGE_SECTION_APPS_RW_FILESYSTEM:
    case UPGRADE_IMAGE_SECTION_DFU_HEADERS:
    default:
        return FALSE;
    }
}

static bool upgradePartitionReader_IsValidsqif_number(uint16 sqif_number, uint16 partition_number)
{
    UNUSED(partition_number);
    /* Until audio is supported, only SQIF zero is valid.*/
    return (sqif_number == 0);
}

/******************************************************************************
NAME
    upgradePartitionReader_ReadNextPartitionDataChunk

DESCRIPTION
    Fetch the next chunk of upgrade data from required partition.

RETURNS
    Boolean value.
*/
static bool upgradePartitionReader_ReadNextPartitionDataChunk(Source src,
                                                              uint8 *data_to_populate,
                                                              uint32 req_len,
                                                              uint32 * length_populated)
{
    uint32 available_bytes;
    uint8 *data;

    available_bytes = SourceSize(src);

    *length_populated = MIN(available_bytes, req_len);
    data = (uint8 *) SourceMap(src);

    if(data == NULL)
    {
        return FALSE;
    }

    memmove(data_to_populate, data, *length_populated);
    SourceDrop(src, *length_populated);

    return TRUE;
}

/******************************************************************************
NAME
    upgradePartitionReader_ReadNextPskeyDataChunk

DESCRIPTION
    Fetch the next chunk of upgrade data from required pskey.

RETURNS
    Boolean value.
*/
static bool upgradePartitionReader_ReadNextPskeyDataChunk(upgrade_partition_reader_data_ctx_t * context, uint8 *data_to_populate, uint32 req_len)
{
    uint32 read_len, available_bytes;
    uint32 data_offset = 0;
    uint16 key_length;
    uint16 *key_data;

    if(context == NULL || context->pskey > UpgradePartitionDataDfuHeaderPskeyEnd() ||
       req_len == 0)
    {
        return FALSE;
    }

    DEBUG_LOG("upgradePartitionReader_ReadNextPskeyDataChunk read header of length %lu", req_len);

    /* Allocate temperory buffer to store PSKEY data */
    key_data = malloc(PSKEY_MAX_STORAGE_LENGTH * sizeof(uint16));

    if(!key_data)
    {
        DEBUG_LOG_WARN("upgradePartitionReader_ReadNextPskeyDataChunk Failed to allocate memory for PSKey data");
        return FALSE;
    }
    
    /* Fetch PSKEY length */
    key_length = PsRetrieve(context->pskey, NULL, 0);

    DEBUG_LOG("upgradePartitionReader_ReadNextPskeyDataChunk PSKEY %u, length %u", context->pskey, key_length);

    while(req_len)
    {
        /* First get available data size in current PSKEY and then take
         * least value of requested data length or available data.
         */
        available_bytes = key_length - context->read_offset;

        DEBUG_LOG("upgradePartitionReader_ReadNextPskeyDataChunk available_bytes %lu", available_bytes);
        DEBUG_LOG("upgradePartitionReader_ReadNextPskeyDataChunk upgrade_partition_reader_context->read_offset %d", context->read_offset);

        read_len = MIN(available_bytes, MIN(req_len, key_length));

        DEBUG_LOG("upgradePartitionReader_ReadNextPskeyDataChunk read pskey and len %d %lu", context->pskey, read_len);

        if(PsRetrieve(context->pskey, key_data, key_length) != 0)
        {
            memcpy(data_to_populate + data_offset,
                   key_data + context->read_offset,
                   read_len* sizeof(uint16));

            /* Increment offset by read length */
            context->read_offset += read_len;
            data_offset += read_len* sizeof(uint16);
            req_len -= read_len;
        }
        else
        {
            DEBUG_LOG_WARN("upgradePartitionReader_ReadNextPskeyDataChunk PSRetrieve Failed");
            return FALSE;
        }

        if(context->read_offset == PSKEY_MAX_STORAGE_LENGTH)
        {
            /* If length reaches to PSKEY size then move to next PSKEY */
            context->read_offset = 0;
            context->pskey += 1;
            key_length = PsRetrieve(context->pskey, NULL, 0);
        }
    }

    free(key_data);

    return TRUE;
}
/******************************************************************************
NAME
    upgradePartitionReader_IsDataAvailableInPskeys

DESCRIPTION
    check if the header data size is available in the pskeys.

PARAMS
        size Size requested in words    

RETURNS
    Boolean value.
*/
static bool upgradePartitionReader_IsDataAvailableInPskeys(upgrade_partition_reader_data_ctx_t * context, uint32 size)
{
    uint16 key_length;
    uint8 diff_len;
    uint16 readable_size;


    DEBUG_LOG("upgradePartitionReader_IsDataAvailableInPskeys : UpgradeCtxGetPartitionData()->dfuHeaderPskey %d", UpgardeCtxDfuHeaderPskey());
    DEBUG_LOG("upgradePartitionReader_IsDataAvailableInPskeys : partition_reader_context->pskey %d", context->pskey);
    DEBUG_LOG("upgradePartitionReader_IsDataAvailableInPskeys : UpgradeCtxGetPartitionData()->dfuHeaderPskeyOffset %d", UpgardeCtxDfuHeaderPskeyOffset());
    DEBUG_LOG("upgradePartitionReader_IsDataAvailableInPskeys : partition_reader_context->read_offset %d",context->read_offset);
    
    if(UpgardeCtxDfuHeaderPskey() < context->pskey)
    {
        DEBUG_LOG("checkPskeyDataAvailable : write is slower"); 
        return FALSE;
    }
    else
    {
        diff_len = UpgardeCtxDfuHeaderPskey() - context->pskey;
    }

    /*check if same pskey is used, ensure write offset not crossed*/
    if (diff_len == 0)
    {
        /* dfuHeaderPskeyOffset is in bytes, so convert to words with rounding down */
        if ((UpgardeCtxDfuHeaderPskeyOffset() / 2) < context->read_offset)
        {
            DEBUG_LOG("checkPskeyDataAvailable : write offset is lower"); 
            return FALSE;
        }
    }

    key_length = PsRetrieve(context->pskey, NULL, 0);

    /*check if requested size goes beyond the write pskey offset*/
    readable_size = ((diff_len * PSKEY_MAX_STORAGE_LENGTH) + (UpgardeCtxDfuHeaderPskeyOffset() / 2)) - context->read_offset;

    DEBUG_LOG("upgradePartitionReader_IsDataAvailableInPskeys : readable_size  %d size requested %lu",readable_size, size);
    
    if((readable_size < size ) ||(context->read_offset == key_length ))
    {
        DEBUG_LOG("upgradePartitionReader_IsDataAvailableInPskeys : PSKEY len insufficient");
        return FALSE;
    }

    DEBUG_LOG("upgradePartitionReader_IsDataAvailableInPskeys : data is fine");

    return TRUE;
}

/******************************************************************************
NAME
    upgradePartitionReader_HandleGeneric1stPartState  -  Parser for ID and
    length part of a header.

DESCRIPTION
    Parses common beginning of any header and determines which header it is.
    All headers have the same first two fields which are 'header id' and
    length.
*/
static UpgradeHostErrorCode upgradePartitionReader_HandleGeneric1stPartState(upgrade_partition_reader_data_ctx_t * context,
                                                                             uint8* data_ptr,
                                                                             uint32 req_len)
{
    uint32 length;
    uint8 index;

    if(req_len < UpgradePartitionDataHeaderFirstPartSize())
    {
        return UPGRADE_HOST_ERROR_BAD_LENGTH_TOO_SHORT;
    }

    /*ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!upgradePartitionReader_IsDataAvailableInPskeys(context, BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("upgradePartitionReader_HandleGeneric1stPartState : PSKEY len insufficient");
        return UPGRADE_HOST_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }

    if(upgradePartitionReader_ReadNextPskeyDataChunk(context, data_ptr, BYTES_TO_WORDS(req_len)) == FALSE)
    {
        DEBUG_LOG("upgradePartitionReader_HandleGeneric1stPartState: Generic1stPartHeaderState Error\n");
        return UPGRADE_HOST_ERROR_PARTITION_OPEN_FAILED;
    }

    length = ByteUtilsGet4BytesFromStream(&data_ptr[UpgradePartitionDataIdFieldSize()]);

    DEBUG_LOG("upgradePartitionReader_HandleGeneric1stPartState: len, data 0x%luX", length);
    for(index =0; index < UpgradePartitionDataIdFieldSize(); index++)
        DEBUG_LOG("%c\n", data_ptr[index]);

    /*If Header is "APPUHDR5"*/
    if(0 == strncmp((char *)data_ptr, upgradePartitionReader_GetHeaderID(), UpgradePartitionDataIdFieldSize()))
    {
        context->app_hdr_length = length;
        if(context->app_hdr_length < UPGRADE_HEADER_MIN_SECOND_PART_SIZE)
        {
            return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
        }
        context->partition_reader_state = UPGRADE_PARTITION_DATA_STATE_HEADER;

        return UPGRADE_HOST_SUCCESS;
    }
    /*If Header is "PARTDATA"*/
    else if(0 == strncmp((char *)data_ptr, upgradePartitionRead_GetPartitionID(),
                                           UpgradePartitionDataIdFieldSize()))
    {
        context->part_data_length = length;
        if(context->part_data_length < UpgradePartitionDataPartitionSecondHeaderSize())
        {
            return UPGRADE_HOST_ERROR_BAD_LENGTH_UPGRADE_HEADER;
        }
        context->partition_reader_state = UPGRADE_PARTITION_DATA_STATE_DATA_HEADER;

        return UPGRADE_HOST_SUCCESS;
    }
    /*If Header is "APPUPFTR"*/
    else if(0 == strncmp((char *)data_ptr, upgradePartitionReader_GetFooterID(),
                                           UpgradePartitionDataIdFieldSize()))
    {
        context->footer_length = length;
        if(context->footer_length != EXPECTED_SIGNATURE_SIZE)
        {
            /* The length of signature must match expected length.
             * Otherwise OEM signature checking could be omitted by just
             * setting length to 0 and not sending signature.
             */
            return UPGRADE_HOST_ERROR_BAD_LENGTH_SIGNATURE;
        }
        context->partition_reader_state = UPGRADE_PARTITION_DATA_STATE_FOOTER;
        return UPGRADE_HOST_SUCCESS;
    }
    else
    {
        return UPGRADE_HOST_ERROR_UNKNOWN_ID;
    }
}

/******************************************************************************
NAME
    upgradePartitionReader_HandleHeaderState  -  Parser for the main header.

DESCRIPTION
    Handles partiton header state.

RETURNS
    Upgrade library error code.
*/
static UpgradeHostErrorCode upgradePartitionReader_HandleHeaderState(upgrade_partition_reader_data_ctx_t * context,
                                                                     uint8* data_ptr,
                                                                     uint32 req_len)

{

    /* ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!upgradePartitionReader_IsDataAvailableInPskeys(context, BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("upgradePartitionReader_HandleHeaderState : PSKEY len insufficient");
        return UPGRADE_HOST_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }
    
    if(upgradePartitionReader_ReadNextPskeyDataChunk(context, data_ptr, BYTES_TO_WORDS(req_len)) == FALSE)
    {
        DEBUG_LOG("upgradePartitionReader_HandleHeaderState: DataHandleHeaderState Error\n");
        return UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    context->partition_reader_state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;

    return UPGRADE_HOST_SUCCESS;
}

/******************************************************************************
NAME
    upgradePartitionReader_HandleDataHeaderState  -  Parser for the partition
    data header.


DESCRIPTION
    Validates content of the partition data header and saves the required
    partion open handle in the context.

RETURNS
    Upgrade library error code.
*/
static UpgradeHostErrorCode upgradePartitionReader_HandleDataHeaderState(upgrade_partition_reader_data_ctx_t * context,
                                                                         uint8* data_ptr,
                                                                         uint32 req_len)
{
    if(req_len < UpgradePartitionDataPartitionSecondHeaderSize() + FIRST_WORD_SIZE)
    {
        return UPGRADE_HOST_ERROR_BAD_LENGTH_DATAHDR_RESUME;
    }

    /*ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!upgradePartitionReader_IsDataAvailableInPskeys(context, BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("upgradePartitionReader_HandleDataHeaderState : PSKEY len insufficient");
        return UPGRADE_HOST_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }

    if(upgradePartitionReader_ReadNextPskeyDataChunk(context, data_ptr,
                BYTES_TO_WORDS(UpgradePartitionDataPartitionSecondHeaderSize() + FIRST_WORD_SIZE)) == FALSE)
    {
        DEBUG_LOG("upgradePartitionReader_HandleDataHeaderState: header read error");
        return UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    context->part_data_length -= UpgradePartitionDataPartitionSecondHeaderSize();
    context->part_data_length -= FIRST_WORD_SIZE;

    context->sqif_number = ByteUtilsGet2BytesFromStream(data_ptr);
    DEBUG_LOG("upgradePartitionReader_HandleDataHeaderState: SQIF number %u\n",
                                   context->sqif_number);

    context->partition_number = ByteUtilsGet2BytesFromStream(&data_ptr[2]);
    DEBUG_LOG("upgradePartitionReader_HandleDataHeaderState: partition number %u\n",
                                   context->partition_number);

    if(!upgradePartitionReader_IsValidPartitionNum(context->partition_number))
    {
        return UPGRADE_HOST_ERROR_WRONG_PARTITION_NUMBER;
    }

    if(!upgradePartitionReader_IsValidsqif_number(context->sqif_number, context->partition_number))
    {
        return UPGRADE_HOST_ERROR_PARTITION_TYPE_NOT_MATCHING;
    }

    context->partition_reader_state = UPGRADE_PARTITION_DATA_STATE_DATA;
    return UPGRADE_HOST_SUCCESS;
}

/******************************************************************************
NAME
    upgradePartitionReader_HandleDataState  -  Partition data handling.

DESCRIPTION
    Reads data from the required partition for requested length.

RETURNS
    Upgrade library error code.
*/
static UpgradeHostErrorCode upgradePartitionReader_HandleDataState(upgrade_partition_reader_data_ctx_t * context,
                                                                   uint8* data_ptr,
                                                                   uint32 req_len,
                                                                   uint32 *sent_len,
                                                                   uint32 part_offset)
{
    uint32 partition_data_len;

    DEBUG_LOG_VERBOSE("upgradePartitionReader_HandleDataState: partition length %lu, partition offset %lu",
                      context->part_data_length, part_offset);

    /* Handling of resume of data transfer */
    if(part_offset != 0)
    {
        /* If the offset size is equal to the part_data_length, it indicates 
         * that the partition read need to be skipped and moved to generic
         * 1st part
         */
        if(part_offset == context->part_data_length)
        {
            UpgradeHostErrorCode ret = UPGRADE_HOST_SUCCESS;

            if(context->handle)
            {
                SourceClose(context->handle);
                /*Reset the partition handle for next iteration*/
                context->handle = NULL;
            }

            ret = upgradePartitionReader_HandleGeneric1stPartState(context, data_ptr, req_len);
            *sent_len = req_len;
            return ret;
        }
        /* Resumption of data handling from the partition where it was last read*/
        else
        {
            context->handle = upgradePartitionReader_OpenSourceAtOffset(context->partition_number, part_offset+FIRST_WORD_SIZE);
            if(!context->handle)
            {
                DEBUG_LOG_ERROR("upgradePartitionReader_HandleDataState: Partition open = %u failed",context->partition_number);
                return UPGRADE_HOST_ERROR_PARTITION_OPEN_FAILED;
            }
            context->part_data_length -= part_offset;
            part_offset = 0;
        }
    }

    if(!context->handle)
    {
        uint8 first_word[FIRST_WORD_SIZE];
        uint32 dummy_length_populated;
        context->handle = upgradePartitionReader_OpenSource(context->partition_number);
        DEBUG_LOG("upgradePartitionReader_HandleDataState partition open : %u",
                                                     context->partition_number);
        if(!context->handle)
        {
            return UPGRADE_HOST_ERROR_PARTITION_OPEN_FAILED;
        }

        /*Read and skip the first word from partition source, it is
         *already copied from pskey*/
        if(upgradePartitionReader_ReadNextPartitionDataChunk(context->handle,
                                                             &first_word[0], FIRST_WORD_SIZE, &dummy_length_populated) == FALSE)
        {
            /*Close stream source during failure condition.*/
            SourceClose(context->handle);
            /*Reset the partition handle for next iteration*/
            context->handle = NULL;
            DEBUG_LOG_ERROR("upgradePartitionReader_HandleDataState: DataHandleDataState Error");
            return UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_DATA;
        }
    }

    /* Resumption of data handling is completed now. Now we read the data from
     * the offset requested
     */
    partition_data_len = MIN(context->part_data_length, req_len);

    if(upgradePartitionReader_ReadNextPartitionDataChunk(context->handle,
                                                         data_ptr, partition_data_len, sent_len) == FALSE)
    {
        SourceClose(context->handle);
        /*Reset the partition handle for next iteration*/
        context->handle = NULL;
        DEBUG_LOG_ERROR("upgradePartitionReader_HandleDataState: DataHandleDataState Error");
        return UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    context->part_data_length -= *sent_len;

    if(context->part_data_length == 0)
    {
        context->partition_reader_state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
        SourceClose(context->handle);
        /* Reset the partition handle for next iteration*/
        context->handle = NULL;
        DEBUG_LOG("upgradePartitionReader_HandleDataState Partition Handle reset");
    }

    return UPGRADE_HOST_SUCCESS;
}

/******************************************************************************
NAME
    upgradePartitionReader_HandleFooterState  -  get footer data.

DESCRIPTION
    Handles footer state and indicates about the last packet.

RETURNS
    Upgrade library error code.
*/
static UpgradeHostErrorCode upgradePartitionReader_HandleFooterState(upgrade_partition_reader_data_ctx_t * context,
                                                                     uint8* data_ptr,
                                                                     uint32 req_len,
                                                                     bool *last_packet)
{

    DEBUG_LOG("upgradePartitionReader_HandleFooterState : req_len bytes  %lu",req_len);
        
    /*ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!upgradePartitionReader_IsDataAvailableInPskeys(context, BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("upgradePartitionReader_HandleFooterState : PSKEY len insufficient offset %d req_len %lu",context->read_offset, req_len);
        return UPGRADE_HOST_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }
    else
    {
        DEBUG_LOG("upgradePartitionReader_HandleFooterState : Success offset %d req_len %lu",context->read_offset, req_len);
    }

    if(upgradePartitionReader_ReadNextPskeyDataChunk(context, data_ptr, BYTES_TO_WORDS(req_len)) == FALSE)
    {
        DEBUG_LOG("upgradePartitionReader_HandleFooterState: Error");
        return UPGRADE_HOST_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    context->footer_length -= req_len;

    if(context->footer_length == 0)
    {
        *last_packet = TRUE;
    }

    return UPGRADE_HOST_SUCCESS;
}

/******************************************************************************
NAME
    upgradePartitionReader_ReadRequest

DESCRIPTION
    Calls state handlers depending on current state.
    All state handlers are setting size of next data request.

RETURNS
    Upgrade library error code.
*/
static UpgradeHostErrorCode upgradePartitionReader_ReadRequest(upgrade_partition_reader_data_ctx_t * context,
                                                               uint8* data_ptr,
                                                               uint32 req_len,
                                                               bool *last_packet,
                                                               uint32 *sentLength,
                                                               uint32 offset)
{
    UpgradeHostErrorCode ret = UPGRADE_HOST_SUCCESS;
    *last_packet = FALSE;

    switch(context->partition_reader_state)
    {
    case UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART:
        DEBUG_LOG_INFO("upgradePartitionReader_ReadRequest Generic1stPartState");
        ret= upgradePartitionReader_HandleGeneric1stPartState(context, data_ptr, req_len);
        *sentLength = req_len;
        break;

    case UPGRADE_PARTITION_DATA_STATE_HEADER:
        DEBUG_LOG_INFO("upgradePartitionReader_ReadRequest DataHandleHeaderState");
        ret = upgradePartitionReader_HandleHeaderState(context, data_ptr, req_len);
        *sentLength = req_len;
        break;

    case UPGRADE_PARTITION_DATA_STATE_DATA_HEADER:
        DEBUG_LOG_INFO("upgradePartitionReader_ReadRequest DataHandleDataHeaderState");
        ret = upgradePartitionReader_HandleDataHeaderState(context, data_ptr, req_len);
        *sentLength = req_len;
        break;

    case UPGRADE_PARTITION_DATA_STATE_DATA:
        DEBUG_LOG_VERBOSE("upgradePartitionReader_ReadRequest DataHandleDataState len %lu", req_len);
        ret = upgradePartitionReader_HandleDataState(context, data_ptr, req_len, sentLength, offset);
        break;

    case UPGRADE_PARTITION_DATA_STATE_FOOTER:
        DEBUG_LOG_INFO("upgradePartitionReader_ReadRequest HandleFooterState");
        ret = upgradePartitionReader_HandleFooterState(context, data_ptr, req_len, last_packet);
        *sentLength = req_len;
        break;

    default:
        DEBUG_LOG("upgradePartitionReader_ReadRequest DefaultState");
        ret = UPGRADE_HOST_ERROR_UNKNOWN_ID;
        break;
    }
    return ret;
}

UpgradeHostErrorCode UpgradePartitionReader_ReadData(upgrade_partition_reader_handle_t handle,
                                                     uint8 *dataPtr,
                                                     bool *last_packet,
                                                     uint32 mBytesReq,
                                                     uint32 *mBytesSent,
                                                     uint32 offset)
{
    UpgradeHostErrorCode ret = UPGRADE_HOST_SUCCESS;

    DEBUG_LOG_VERBOSE("UpgradePartitionReader_ReadData, data_size %lu", mBytesReq);

    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    if (dataPtr == NULL)
        ret = UPGRADE_HOST_ERROR_NO_MEMORY;
    else
        ret = upgradePartitionReader_ReadRequest(context, dataPtr, mBytesReq, last_packet, mBytesSent, offset);

    DEBUG_LOG_VERBOSE("UpgradePartitionReader_ReadData, status %u", ret);

    return ret;
}

upgrade_partition_reader_handle_t UpgradePartitionReader_CreateInstance(void)
{
    DEBUG_LOG("UpgradePartitionReader_Init");

    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)PanicUnlessMalloc(sizeof(upgrade_partition_reader_data_ctx_t));
    memset(context, 0, sizeof(upgrade_partition_reader_data_ctx_t));
    context->pskey = UpgradePartitionDataDfuHeaderPskeyStart();

    return (upgrade_partition_reader_handle_t)context;
}

void UpgradePartitionReader_DestroyInstance(upgrade_partition_reader_handle_t handle)
{
    DEBUG_LOG("UpgradePartitionReader_DeInit");

    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    if(context && context->handle != NULL)
    {
        DEBUG_LOG("UpgradePartitionReader_DeInit SourceClose(%d)", context->partition_number);
        SourceClose(context->handle);
    }

    if(context)
    {
        free(context);
        context = NULL;
    }
}

uint16 UpgradePartitionReader_GetPartitionNumber(upgrade_partition_reader_handle_t handle)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    return context->partition_number;
}

uint32 UpgradePartitionReader_GetPartitionOffset(upgrade_partition_reader_handle_t handle)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    return context->partition_offset;
}

UpgradePartitionDataState UpgradePartitionReader_GetState(upgrade_partition_reader_handle_t handle)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    return context->partition_reader_state;
}

uint16 UpgradePartitionReader_GetAppHeaderLength(upgrade_partition_reader_handle_t handle)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    return context->app_hdr_length;
}


uint32 UpgradePartitionReader_GetPartitionDataLength(upgrade_partition_reader_handle_t handle)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    return context->part_data_length;
}

uint16 UpgradePartitionReader_GetFooterLength(upgrade_partition_reader_handle_t handle)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;

    return context->footer_length;
}

void UpgradePartitionReader_SetPartitionNumber(upgrade_partition_reader_handle_t handle, uint16 partition_number)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;
    context->partition_number = partition_number;
}

void UpgradePartitionReader_SetPartitionOffset(upgrade_partition_reader_handle_t handle, uint32 partition_offset)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;
    context->partition_offset = partition_offset;
}

void UpgradePartitionReader_SetReadOffset(upgrade_partition_reader_handle_t handle, uint16 read_offset)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;
    context->read_offset = read_offset;
}

void UpgradePartitionReader_SetPskey(upgrade_partition_reader_handle_t handle, uint16 pskey)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;
    context->pskey = pskey;
}

void UpgradePartitionReader_SetState(upgrade_partition_reader_handle_t handle, UpgradePartitionDataState state)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;
    context->partition_reader_state = state;
}

void UpgradePartitionReader_SetFooterLength(upgrade_partition_reader_handle_t handle, uint16 length)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;
    context->footer_length = length;
}

void UpgradePartitionReader_SetPartitionDataLength(upgrade_partition_reader_handle_t handle, uint32 length)
{
    PanicNull(handle);
    upgrade_partition_reader_data_ctx_t * context = (upgrade_partition_reader_data_ctx_t *)handle;
    context->part_data_length = length;
}
