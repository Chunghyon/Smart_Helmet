/****************************************************************************
Copyright (c) 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_reboot_handler.c

DESCRIPTION
    Provides functionality to calculate Upgrade partition state variable from give offset.
*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <string.h>
#include <stdlib.h>
#include <message.h>
#include <byte_utils.h>
#include <panic.h>

#include "upgrade_partition_data.h"
#include "upgrade_partition_data_priv.h"
#include "upgrade.h"
#include "upgrade_fw_if.h"
#include "upgrade_ctx.h"
#include <system_clock.h>
#include "upgrade_reboot_handler.h"

/* An arbitrary high number which sets the upper limit for DFU file components */
#define MAX_DFU_FILE_COMPONENTS 1000

typedef struct
{
    uint8 key_cache[PSKEY_MAX_STORAGE_LENGTH_IN_BYTES];
    UpgradeFWIFPartitionHdl partition_hdl;
    uint32 total_file_offset;
    uint32 expected_offset;
    uint32 partition_len;
    uint32 first_word;
    uint32 part_offset;
    bool is_upgrade_hdr_available;
    uint16 pskey;
    uint16 pskey_offset;
    uint16 pskey_len;
    uint16 part_num;
    UpgradePartitionDataState state;
    uint8 signing_mode;
} upgrade_reboot_handler_context_t;

static bool loadNextPskey(upgrade_reboot_handler_context_t* context)
{
    if(!context->pskey)
    {
        /* load first pskey */
        context->pskey = DFU_HEADER_PSKEY_START;
    }
    else if(context->pskey_len < PSKEY_MAX_STORAGE_LENGTH_IN_BYTES /* current pskey is not fully written so, it would be the last written key */
            || context->pskey > DFU_HEADER_PSKEY_END /* pskey overflow. An unlikely scenario. */)
    {
        return FALSE;
    }

    context->pskey_len = PsRetrieve(context->pskey, NULL, 0);
    if (context->pskey_len)
    {
        PsRetrieve(context->pskey, context->key_cache, context->pskey_len);
        /* pskey var maintains the value of (current open pskey+1). */
        context->pskey++;
        context->pskey_len <<= 1;
        context->pskey_offset=0;
        return TRUE;
    }
    else
    {
        /* next pskey is empty */
        return FALSE;
    }
}

static bool readHeaderPskey(upgrade_reboot_handler_context_t* context, uint8* buff, uint16 size)
{
    uint16 buff_offset = 0;

    /* local copies to restore if we are not able to read full data. */
    uint16 header_pskey = context->pskey;
    uint16 pskey_offset = context->pskey_offset;


    if(!context->pskey && !loadNextPskey(context))
    {
            DEBUG_LOG_ERROR("readHeaderPskey error loading first key");
            return FALSE;
    }

    while(size)
    {
        uint16 diff;
        if(context->pskey_offset==context->pskey_len && !loadNextPskey(context))
        {
            /* requested component is not fully written so, restore the pskey details and we will rewrite
             * this component again. */
            context->pskey = header_pskey;
            context->pskey_offset = pskey_offset;

            DEBUG_LOG_ERROR("readHeaderPskey error loading key %d for size %d", context->pskey, size);
            return FALSE;
        }

        diff = MIN(size, (context->pskey_len - context->pskey_offset));
        memcpy(&buff[buff_offset], &context->key_cache[context->pskey_offset], diff);
        context->pskey_offset+=diff;
        buff_offset+=diff;
        size-=diff;
    }

    return TRUE;
}

static bool upgradeRebootHandlerHandleGeneric1stPart(upgrade_reboot_handler_context_t* context)
{
    uint8 buff[HEADER_FIRST_PART_SIZE];
    if(!readHeaderPskey(context, buff, HEADER_FIRST_PART_SIZE))
    {
        return FALSE;
    }
    context->total_file_offset += HEADER_FIRST_PART_SIZE;
    context->partition_len = ByteUtilsGet4BytesFromStream(&buff[ID_FIELD_SIZE]);

    /* APPUHDR5 */
    if(0 == strncmp((char *)buff, UpgradeFWIFGetHeaderID(), ID_FIELD_SIZE))
    {
        context->state = UPGRADE_PARTITION_DATA_STATE_HEADER;
    }
    /* PARTDATA */
    else if (0 == strncmp((char *)buff, UpgradeFWIFGetPartitionID(), ID_FIELD_SIZE))
    {
        context->state = UPGRADE_PARTITION_DATA_STATE_DATA_HEADER;
    }
    /* APPUPFTR */
    else if (0 == strncmp((char *)buff, UpgradeFWIFGetFooterID(), ID_FIELD_SIZE))
    {
       DEBUG_LOG("upgradeRebootHandlerHandleGeneric1stPart reached footer");
       context->state = UPGRADE_PARTITION_DATA_STATE_FOOTER;
       return FALSE;
    }
    DEBUG_LOG("upgradeRebootHandlerHandleGeneric1stPart next state %d offset %lu",context->state, context->total_file_offset);
    return TRUE;
}

static bool upgradeRebootHandlerHandleHeader(upgrade_reboot_handler_context_t* context)
{
    uint8* buff =  PanicUnlessMalloc(context->partition_len);
    if(!readHeaderPskey(context, buff, context->partition_len))
    {
        free(buff);
        return FALSE;
    }
    context->total_file_offset += context->partition_len;
    context->is_upgrade_hdr_available = TRUE;
    context->signing_mode = buff[context->partition_len-1];
    context->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;

    free(buff);
    DEBUG_LOG("upgradeRebootHandlerHandleHeader next state %d offset %lu",context->state, context->total_file_offset);
    return TRUE;
}

static bool upgradeRebootHandlerHandlePartitionHeader(upgrade_reboot_handler_context_t* context)
{
    uint8 buff[PARTITION_SECOND_HEADER_SIZE+PARTITION_FIRST_WORD_SIZE];
    if(!readHeaderPskey(context, buff, PARTITION_SECOND_HEADER_SIZE+PARTITION_FIRST_WORD_SIZE))
    {
        return FALSE;
    }
    context->total_file_offset += PARTITION_SECOND_HEADER_SIZE+PARTITION_FIRST_WORD_SIZE;
    context->partition_len -= PARTITION_SECOND_HEADER_SIZE+PARTITION_FIRST_WORD_SIZE;

    context->part_num = ByteUtilsGet2BytesFromStream(&buff[PARTITION_TYPE_SIZE]);

    /* reset the firstword before using */
    context->first_word  = (uint32)buff[PARTITION_SECOND_HEADER_SIZE + 3] << 24;
    context->first_word |= (uint32)buff[PARTITION_SECOND_HEADER_SIZE + 2] << 16;
    context->first_word |= (uint32)buff[PARTITION_SECOND_HEADER_SIZE + 1] << 8;
    context->first_word |= (uint32)buff[PARTITION_SECOND_HEADER_SIZE];

    context->state = UPGRADE_PARTITION_DATA_STATE_DATA;
    DEBUG_LOG("upgradeRebootHandlerHandlePartitionHeader part_num %d offset %lu",context->part_num, context->total_file_offset);
    return TRUE;
}

static bool upgradeRebootHandlerHandlePartitionData(upgrade_reboot_handler_context_t* context)
{
    if(context->expected_offset)
    {
        if(context->expected_offset < (context->total_file_offset+context->partition_len))
        {
            context->part_offset = context->expected_offset - context->total_file_offset;
            context->total_file_offset = context->expected_offset;
            return TRUE;
        }
    }
    else if(context->part_num >= UpgradeCtxGetPSKeys()->last_closed_partition)
    {
        context->partition_hdl = UpgradePartitionDataPartitionOpen(context->part_num, context->first_word);
        if (!context->partition_hdl)
        {
            DEBUG_LOG_ERROR("upgradeRebootHandlerHandlePartitionData, failed to open partition %u", context->part_num);
            Panic();
        }

        if(ImageUpgradeSinkGetPosition((Sink) context->partition_hdl, &context->part_offset)) /*Trap call to get offset from sink*/
        {
            DEBUG_LOG("upgradeRebootHandlerHandlePartitionData Sink offset of interrupted partiton : %ld", context->part_offset);
        }
        else
        {
            DEBUG_LOG_ERROR("PANIC, upgradeRebootHandlerHandlePartitionData  Could not retrieve partition offset");
            Panic();
        }
        context->total_file_offset += context->part_offset;
        DEBUG_LOG("upgradeRebootHandlerHandlePartitionData opened part_num %d offset %lu",context->part_num, context->total_file_offset);
        return FALSE;
    }

    context->total_file_offset += context->partition_len;
    context->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
    DEBUG_LOG("upgradeRebootHandlerHandlePartitionData skipping part_num %d offset %lu",context->part_num, context->total_file_offset);
    return TRUE;
}

static bool upgradeRebootHandlerProcessNextStep(upgrade_reboot_handler_context_t* context)
{
    DEBUG_LOG("upgradeRebootHandlerProcessNextStep %d", context->state);
    switch (context->state)
    {
        case UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART:
            return upgradeRebootHandlerHandleGeneric1stPart(context);

        case UPGRADE_PARTITION_DATA_STATE_HEADER:
            return upgradeRebootHandlerHandleHeader(context);

        case UPGRADE_PARTITION_DATA_STATE_DATA_HEADER:
            return upgradeRebootHandlerHandlePartitionHeader(context);

        case UPGRADE_PARTITION_DATA_STATE_DATA:
            return upgradeRebootHandlerHandlePartitionData(context);

        case UPGRADE_PARTITION_DATA_STATE_FOOTER:
            return FALSE;
    }
    return FALSE;
}

static bool upgradeRebootHandlerReinitPartitionCtx(upgrade_reboot_handler_context_t* context)
{
    UpgradePartitionDataCtx *ctx;
    ctx = PanicUnlessMalloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    UpgradeCtxSetPartitionData(ctx);

    if(context->pskey_offset == PSKEY_MAX_STORAGE_LENGTH_IN_BYTES)
    {
        context->pskey_offset = 0;
        context->pskey+=1;
    }
    if(context->pskey)
    {
        context->pskey-=1;
    }
    ctx->dfuHeaderPskey = context->pskey;
    ctx->dfuHeaderPskeyOffset = context->pskey_offset;
    ctx->isUpgradeHdrAvailable = context->is_upgrade_hdr_available;
    UpgradeCtxGet()->dfu_file_offset = context->total_file_offset;
    UpgradeCtxGetFW()->partitionNum = context->part_num;

    switch (context->state)
    {
        case UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART:
        {
            ctx->totalReqSize = HEADER_FIRST_PART_SIZE;
            ctx->state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
        }
        break;

        case UPGRADE_PARTITION_DATA_STATE_HEADER:
        {
            ctx->totalReqSize = context->partition_len;
            ctx->state = UPGRADE_PARTITION_DATA_STATE_HEADER;
        }
        break;

        case UPGRADE_PARTITION_DATA_STATE_DATA_HEADER:
        {
                ctx->totalReqSize = PARTITION_SECOND_HEADER_SIZE + PARTITION_FIRST_WORD_SIZE;
                ctx->state = UPGRADE_PARTITION_DATA_STATE_DATA_HEADER;
        }
        break;

        case UPGRADE_PARTITION_DATA_STATE_DATA:
        {
            ctx->time_start = SystemClockGetTimerTime();
            ctx->totalReqSize = context->partition_len - context->part_offset;
            ctx->state = UPGRADE_PARTITION_DATA_STATE_DATA;
            ctx->partitionHdl = context->partition_hdl;
            ctx->partitionLength = context->partition_len+PARTITION_FIRST_WORD_SIZE;
        }
        break;

        case UPGRADE_PARTITION_DATA_STATE_FOOTER:
        {
            ctx->totalReqSize = context->partition_len;
            ctx->partitionLength = context->partition_len;
            ctx->state = UPGRADE_PARTITION_DATA_STATE_FOOTER;
            ctx->signature = PanicUnlessMalloc(context->partition_len);
            ctx->signatureReceived = 0;
        }
        break;
    }
    DEBUG_LOG_INFO("upgradeRebootHandlerReinitPartitionCtx success pskey %d offset %d state %d",ctx->dfuHeaderPskey, ctx->dfuHeaderPskeyOffset, ctx->state);
    return TRUE;
}

void UpgradeRebootHandlerCalculateDfuOffset(void)
{
    upgrade_reboot_handler_context_t context = {0};
    uint16 cnt = 0;
    context.state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
    DEBUG_LOG("UpgradeRebootHandlerCalculateDfuOffset");

    for(cnt=MAX_DFU_FILE_COMPONENTS; upgradeRebootHandlerProcessNextStep(&context) && cnt; cnt--);
    if(!cnt)
    {
        /* cnt is a fail-safe mechanism. It reaches 0 when upgradeRebootHandlerProcessNextStep() is stuck in an endless loop due to some error. */
        Panic();
    }
    upgradeRebootHandlerReinitPartitionCtx(&context);
}

bool UpgradeRebootHandlerGetPartitionStateFromDfuFileOffset(uint32 req_offset, upgrade_partition_state_t* expected_state)
{
    upgrade_reboot_handler_context_t context = {0};
    context.state = UPGRADE_PARTITION_DATA_STATE_GENERIC_1ST_PART;
    context.expected_offset = req_offset;
    uint16 cnt = 0;

    for(cnt=MAX_DFU_FILE_COMPONENTS; upgradeRebootHandlerProcessNextStep(&context) && context.total_file_offset < req_offset && cnt; cnt--);
    if(!cnt)
    {
        /* cnt is a fail-safe mechanism. It reaches 0 when upgradeRebootHandlerProcessNextStep() is stuck in an endless loop due to some error. */
        Panic();
    }

    if(context.total_file_offset < req_offset)
    {
        DEBUG_LOG_ERROR("UpgradeRebootHandlerGetPartitionStateFromDfuFileOffset requested data %lu not available, total_file_offset %lu", req_offset, context.total_file_offset);
        return FALSE;
    }

    if(context.pskey_offset == PSKEY_MAX_STORAGE_LENGTH_IN_BYTES)
    {
        context.pskey_offset = 0;
        context.pskey+=1;
    }
    if(context.pskey)
    {
        context.pskey-=1;
    }

    expected_state->pskey = context.pskey;
    expected_state->pskey_offset = context.pskey_offset;
    expected_state->state = context.state;
    expected_state->total_file_offset = context.total_file_offset;
    expected_state->partition_len = context.partition_len;
    expected_state->first_word = context.first_word;
    expected_state->part_num = context.part_num;
    expected_state->part_offset = context.part_offset;

    DEBUG_LOG_INFO("UpgradeRebootHandlerGetPartitionStateFromDfuFileOffset state %d pskey %d offset %d", expected_state->state,context.pskey, context.pskey_offset);
    return TRUE;
}

