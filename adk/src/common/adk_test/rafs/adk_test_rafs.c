/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of specifc application testing functions
*/

#include "adk_test_rafs.h"
#include "vm.h"
#include <ra_partition_api.h>

#include "rafs.h"
#include <stdio.h>
#include <stdlib.h>
#include "logging.h"
#include "panic.h"
#include "rafs_private.h"
#include "rafs_utils.h"

#ifdef INCLUDE_RAFS_TESTS

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

#endif /*INCLUDE_RAFS_TESTS*/


/* RAFS TESTS */
/* ---------- */
static void testRafsTaskHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    switch(id)
    {
    case MESSAGE_RAFS_MOUNT_COMPLETE:
        DEBUG_LOG_ALWAYS("testRafsTaskHandler MOUNT");
        break;
    case MESSAGE_RAFS_COMPACT_COMPLETE:
        DEBUG_LOG_ALWAYS("testRafsTaskHandler COMPACT");
        break;
    case MESSAGE_RAFS_WRITE_COMPLETE:
        DEBUG_LOG_ALWAYS("testRafsTaskHandler WRITE");
        break;
    case MESSAGE_RAFS_REMOVE_COMPLETE:
        DEBUG_LOG_ALWAYS("testRafsTaskHandler REMOVE");
        break;
    }
}
static TaskData test_rafs_task = {testRafsTaskHandler};

#define MOUNT_POINT     RAFS_PATH_SEPARATOR "VMdl"

void appTestRafsInit(void)
{
    if(!Rafs_Init(NULL))
    {
        DEBUG_LOG_ALWAYS("Rafs_Init failed");
    }
}

void appTestRafsFormat(void)
{
    rafs_errors_t result = Rafs_Format(MOUNT_POINT, format_normal);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Format=%d",result);
    }
}

void appTestRafsMount(void)
{
    rafs_errors_t result = Rafs_Mount(&test_rafs_task, MOUNT_POINT);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Mount=%d",result);
    }
}

void appTestRafsUnmount(void)
{
    rafs_errors_t result = Rafs_Unmount(MOUNT_POINT);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Unmount=%d",result);
    }
}

void appTestRafsCompact(void)
{
    rafs_errors_t result = Rafs_Compact(&test_rafs_task, MOUNT_POINT);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Compact=%d",result);
    }
}

static rafs_file_t test_rafs_file_id;

void appTestRafsOpenRead(char id)
{
    char filename[RAFS_MOUNTPOINT_LEN+RAFS_MAX_FILELEN];
    snprintf(filename, sizeof(filename), MOUNT_POINT RAFS_PATH_SEPARATOR "%c_file", id);
    rafs_errors_t result = Rafs_Open(&test_rafs_task, filename, RAFS_RDONLY, &test_rafs_file_id);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Open=%d",result);
    }
}

void appTestRafsOpenWrite(char id)
{
    char filename[RAFS_MOUNTPOINT_LEN+RAFS_MAX_FILELEN];
    snprintf(filename, sizeof(filename), MOUNT_POINT RAFS_PATH_SEPARATOR "%c_file", id);
    rafs_errors_t result = Rafs_Open(&test_rafs_task, filename, RAFS_WRONLY, &test_rafs_file_id);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Open=%d",result);
    }
}

void appTestRafsClose(void)
{
    rafs_errors_t result = Rafs_Close(test_rafs_file_id);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Close=%d",result);
    }
}

size_t appTestRafsRead(char *buffer, size_t len)
{
    rafs_size_t actual;
    rafs_errors_t result = Rafs_Read(test_rafs_file_id, buffer, len, &actual);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Read=%d",result);
    }
    return actual;
}

size_t appTestRafsWrite(const char *buffer, size_t len)
{
    rafs_size_t actual;
    rafs_errors_t result = Rafs_Write(test_rafs_file_id, buffer, len, &actual);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Write=%d",result);
    }
    return actual;
}

void appTestRafsRemove(char id)
{
    char filename[RAFS_MOUNTPOINT_LEN+RAFS_MAX_FILELEN];
    snprintf(filename, sizeof(filename), MOUNT_POINT RAFS_PATH_SEPARATOR "%c_file", id);
    rafs_errors_t result = Rafs_Remove(&test_rafs_task, filename);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Remove=%d",result);
    }
}

static rafs_dir_t test_rafs_dir_id;

void appTestRafsReadFs(void)
{
    uint16 num_dir = appTestRafsOpenDir();
    for(uint8 i=0; i< num_dir;i++)
    {
        appTestRafsReadDir();
    }
    appTestRafsCloseDir();
}

uint16 appTestRafsOpenDir(void)
{
    uint16 num_entries = 0;
    rafs_errors_t result = Rafs_OpenDirectory(MOUNT_POINT RAFS_PATH_SEPARATOR, &test_rafs_dir_id, &num_entries);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_OpenDirectory=%d",result);
    }
    else
    {
        DEBUG_LOG_ALWAYS("Rafs dir contains %hu entries", num_entries);
    }
    return num_entries;
}

static void print_stat(const rafs_stat_t *stat)
{
    const char *s = stat->filename;
    char temp[RAFS_MAX_FILELEN] = { 0 };
    memset(temp,' ',sizeof(temp)-1);
    size_t len = strlen(s) < sizeof(temp) ? strlen(s) : sizeof(temp);
    strncpy(temp,s,len);
    DEBUG_LOG_ALWAYS("Rafs_stat: size=%lu, times=%hu:%hu",
        stat->file_size,
        stat->stat_counters.sequence_count,
        stat->stat_counters.access_count);
    DEBUG_LOG_ALWAYS("Rafs_stat: filename=%c%c%c%c%c%c%c%c%c%c",
        temp[0],temp[1],temp[2],temp[3],temp[4],
        temp[5],temp[6],temp[7],temp[8],temp[9]);
}

void appTestRafsReadDir(void)
{
    rafs_stat_t stat;
    rafs_errors_t result = Rafs_ReadDirectory(test_rafs_dir_id, &stat);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_ReadDirectory=%d",result);
    }
    else
    {
        print_stat(&stat);
    }
}

void appTestRafsCloseDir(void)
{
    rafs_errors_t result = Rafs_CloseDirectory(test_rafs_dir_id);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_CloseDirectory=%d",result);
    }
}

void appTestRafsStatFs(void *buffer)
{
    rafs_statfs_t statfs;
    rafs_errors_t result = Rafs_Statfs(MOUNT_POINT RAFS_PATH_SEPARATOR, &statfs);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Statfs=%d",result);
    }
    else
    {
        DEBUG_LOG_ALWAYS("RAFS: Sector_size=%lu Partition_size=%lu Block_size=%lu", statfs.sector_size, statfs.partition_size, statfs.block_size );
        DEBUG_LOG_ALWAYS("RAFS: Available_space=%lu Free_space=%lu File_space=%lu", statfs.available_space, statfs.free_space, statfs.file_space );
        DEBUG_LOG_ALWAYS("RAFS: Maxdir=%u Numdir=%u", statfs.max_dir_entries, statfs.num_dir_entries );
        if( buffer )
        {
            memcpy(buffer, &statfs, sizeof(statfs));
        }
    }
}

void appTestRafsMakeLargeFile(char id, int nsectors, int nrows)
{
    char filename[RAFS_MOUNTPOINT_LEN+RAFS_MAX_FILELEN];
    snprintf(filename, sizeof(filename), MOUNT_POINT RAFS_PATH_SEPARATOR "%c_file", id);
    rafs_errors_t result = Rafs_Open(&test_rafs_task, filename, RAFS_WRONLY, &test_rafs_file_id);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Open=%d",result);
    }

    uint32 t1 = VmGetTimerTime();

    int sector, row;
    char buff[17];
    for( sector = 0 ; sector < nsectors ; sector++)
    {
        for( row = 0 ; row < 256 ; row++)
        {
            snprintf(buff,sizeof(buff),"F=%03d S=%02x R=%02x\n",id,sector,row);
            size_t len = strlen(buff);
            rafs_size_t actual;
            result = Rafs_Write(test_rafs_file_id, buff, len, &actual);
            if( result != RAFS_OK )
            {
                DEBUG_LOG_ALWAYS("Rafs_Write=%d",result);
            }
        }
    }
    nrows %= 256;
    for( row = 0 ; row < nrows ; row++)
    {
        snprintf(buff,sizeof(buff),"F=%03d S=%02x R=%02x\n",id,sector,row);
        size_t len = strlen(buff);
        rafs_size_t actual;
        result = Rafs_Write(test_rafs_file_id, buff, len, &actual);
        if( result != RAFS_OK )
        {
            DEBUG_LOG_ALWAYS("Rafs_Write=%d",result);
        }
    }

    uint32 t2 = VmGetTimerTime();
    uint32 elapsed;
    if( t1 < t2 ) elapsed = t2 - t1;
    else elapsed = t1 - t2;
    DEBUG_LOG_ALWAYS("appTestRafsMakeLargeFile(%c,%d,%d) took %lu microseconds",
                     id, nsectors, nrows, elapsed);

    result = Rafs_Close(test_rafs_file_id);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Close=%d",result);
    }
}

void appTestRafsCopyFile(char fromId, char toId, uint32 buffsize)
{
    rafs_file_t from, to;
    void *buff = malloc(buffsize);

    char filename[RAFS_MOUNTPOINT_LEN+RAFS_MAX_FILELEN];
    snprintf(filename, sizeof(filename), MOUNT_POINT RAFS_PATH_SEPARATOR "%c_file", toId);
    rafs_errors_t result = Rafs_Open(&test_rafs_task, filename, RAFS_WRONLY, &to);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Open=%d",result);
    }
    snprintf(filename, sizeof(filename), MOUNT_POINT RAFS_PATH_SEPARATOR "%c_file", fromId);
    result = Rafs_Open(&test_rafs_task, filename, RAFS_RDONLY, &from);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Open=%d",result);
    }

    uint32 t1 = VmGetTimerTime();

    rafs_size_t actual;
    do {
        result = Rafs_Read(from, buff, buffsize, &actual);
        if( result != RAFS_OK )
        {
            DEBUG_LOG_ALWAYS("Rafs_Read=%d",result);
        }
        result = Rafs_Write(to, buff, actual, &actual);
        if( result != RAFS_OK )
        {
            DEBUG_LOG_ALWAYS("Rafs_Write=%d",result);
        }
    } while( actual == buffsize);

    uint32 t2 = VmGetTimerTime();
    uint32 elapsed;
    if( t1 < t2 ) elapsed = t2 - t1;
    else elapsed = t1 - t2;
    DEBUG_LOG_ALWAYS("appTestRafsCopyFile(%c,%c,%lu) took %lu microseconds",
                     fromId, toId, buffsize, elapsed);

    result = Rafs_Close(from);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Close=%d",result);
    }
    result = Rafs_Close(to);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_Close=%d",result);
    }

    free(buff);
}

void appTestRafsIoControl(char id)
{
    const char *mount = MOUNT_POINT;
    char filename[RAFS_MOUNTPOINT_LEN+RAFS_MAX_FILELEN];
    snprintf(filename, sizeof(filename), MOUNT_POINT RAFS_PATH_SEPARATOR "%c_file", id);

    raPartition_handle  handle = 0;
    rafs_size_t         handle_size = 0;
    rafs_errors_t       result;

    result = Rafs_IoControl(RAFS_IOC_GET_PARTITION_HANDLE, mount, strlen(mount),
                            &handle, sizeof(handle), &handle_size);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_IoControl=%d",result);
    }

    uint32              offset = 0;
    rafs_size_t         offset_size = 0;
    result = Rafs_IoControl(RAFS_IOC_GET_FILE_OFFSET, filename, strlen(filename),
                            &offset, sizeof(offset), &offset_size);
    if( result != RAFS_OK )
    {
        DEBUG_LOG_ALWAYS("Rafs_IoControl=%d",result);
    }
    DEBUG_LOG_ALWAYS("appTestRafsIoControl: handle=%d, offset=%x", handle, offset);
    DEBUG_LOG_ALWAYS("appTestRafsIoControl: h_size=%d, o_size=%d", handle_size, offset_size);
}

#define MAX_NUM_BYTES_TO_READ_FROM_UNNAMED 256
static void validateReadLength(uint32 num_bytes_to_read)
{
    PanicFalse(MAX_NUM_BYTES_TO_READ_FROM_UNNAMED > num_bytes_to_read);
}

static bool rafs_PartitionDoesNotExist(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    return(!rafs_self->partition);
}

static bool rafs_PartitionMount(void)
{
    Rafs_CreateInstanceStructure();
    Rafs_CreateFilesStructure();
    return rafs_PartitionDoesNotExist();
}

static raPartition_result rafs_PartitionRead(uint32 offset, uint32 length, void * buffer)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    return Rafs_PartRead(&rafs_self->partition->part_handle, offset, length, buffer);
}

static raPartition_result rafs_PartitionWrite(uint32 offset, uint32 length, void * buffer)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    return Rafs_PartWrite(&rafs_self->partition->part_handle, offset, length, buffer);
}

static void rafs_PartitionUnmount(void)
{
    rafs_instance_t *rafs_self = Rafs_GetTaskData();
    Rafs_FreeFilesStructure();
    Rafs_FreePartitionStructure();
    free(rafs_self);
}

static bool rafs_HasErrorOccured(rafs_errors_t result)
{
    return result != RAFS_OK;
}

static void rafs_PrintFileContent(uint32 num_bytes_to_read, char* buffer)
{
    DEBUG_LOG_ALWAYS("rafs_PrintFileContent:");
    for(uint32 i = 0; i < num_bytes_to_read; i++)
    {
        DEBUG_LOG_ALWAYS("  [%2d]: %3d 0x%2x",i, buffer[i], buffer[i]);
    }
}

void appTestRafsReadFromUnnamedPartition(uint32 offset, uint32 num_bytes_to_read)
{
    char buffer[MAX_NUM_BYTES_TO_READ_FROM_UNNAMED] = {0};
    rafs_errors_t result = RAFS_OK;
    raPartition_result ra_result = RA_PARTITION_RESULT_SUCCESS;
    Rafs_SwitchTaskData(UNNAMED_PARTITION);

    validateReadLength(num_bytes_to_read);
    if(rafs_PartitionMount())
    {
        result = Rafs_PartitionOpen(RAFS_PATH_SEPARATOR);
        if(rafs_HasErrorOccured(result))
        {
            DEBUG_LOG_ALWAYS("Rafs_PartitionOpen=%d", result);
        }
        else
        {
            ra_result = rafs_PartitionRead(offset, num_bytes_to_read, buffer);
            if(ra_result != RA_PARTITION_RESULT_SUCCESS)
            {
                DEBUG_LOG_ALWAYS("rafs_PartitionRead=%d", ra_result);
            }
        }
        Rafs_PartitionClose();
    }
    rafs_PartitionUnmount();

    if(rafs_HasErrorOccured((result)))
    {
        DEBUG_LOG_ALWAYS("appTestRafsReadFromUnnamedPartition=%d",result);
    }
    else
    {
        rafs_PrintFileContent(num_bytes_to_read, buffer);
    }
    Rafs_SwitchTaskData(NAMED_PARTITION);
}

void appTestRafsWriteToUnnamedPartition(char* buffer, uint32 offset, uint32 num_bytes_to_write)
{
    rafs_errors_t result = RAFS_OK;
    raPartition_result ra_result = RA_PARTITION_RESULT_SUCCESS;
    Rafs_SwitchTaskData(UNNAMED_PARTITION);

    if(rafs_PartitionMount())
    {
        result = Rafs_PartitionOpen(RAFS_PATH_SEPARATOR);
        if(rafs_HasErrorOccured(result))
        {
            DEBUG_LOG_ALWAYS("Rafs_PartitionOpen=%d", result);
        }
        else
        {
            ra_result = rafs_PartitionWrite(offset, num_bytes_to_write, buffer);
            if(ra_result != RA_PARTITION_RESULT_SUCCESS)
            {
                DEBUG_LOG_ALWAYS("rafs_PartitionWrite=%d", ra_result);
            }
        }
        Rafs_PartitionClose();
    }
    rafs_PartitionUnmount();

    if(rafs_HasErrorOccured((result)))
    {
        DEBUG_LOG_ALWAYS("appTestRafsWriteToUnnamedPartition=%d",result);
    }
    Rafs_SwitchTaskData(NAMED_PARTITION);
}

