/**
 * \file
 * \brief
 *      This file contains functions for testing on a live device
 * \copyright
 *  Copyright (c) 2022 Qualcomm Technologies, Inc.
 *  All Rights Reserved.
 *  Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#ifdef INCLUDE_AUDIOREACH
#ifndef DISABLE_TEST_API

#include <logging.h>
#include <vm.h>
#include <rtime.h>

#include "ar_osal_file_io.h"
#include "ar_osal_heap.h"
#include "acdb.h"
#include "gsl_intf.h"

#include "ar_test.h"

#include "kvh2xml_hana_pal.h"
#include <time_probe.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

static uint32_t *acdb_buf;

#define ACDB_FILENAME       "acdb_cal.acdb"
#define ACDB_FILENAME_LEN   (sizeof(ACDB_FILENAME) - 1)

static ar_heap_info heap_info = {
    .align_bytes = AR_HEAP_ALIGN_DEFAULT,
    .pool_type = AR_HEAP_POOL_DEFAULT,
    .heap_id = AR_HEAP_ID_DEFAULT,
    .tag = AR_HEAP_TAG_DEFAULT
};

uint32_t ArOsalTestOpenAcdbFile(const char *filename)
{
    ar_fhandle acdb_handle;
    uint32_t result = 0;
    size_t bytes_read = 0;
    size_t acdb_size = 0;

    result |= ar_fopen(&acdb_handle, filename, AR_FOPEN_READ_ONLY);

    acdb_size = ar_fsize(acdb_handle);
    DEBUG_LOG_ALWAYS("ar_fsize(%p) = %u", acdb_handle, acdb_size);

    acdb_buf = ar_heap_malloc(acdb_size, &heap_info);

    result |= ar_fread(acdb_handle, acdb_buf, acdb_size, &bytes_read);
    DEBUG_LOG_ALWAYS("ar_fread(%p, %p, %u, &bytes_read): bytes_read = %u", acdb_handle, acdb_buf, acdb_size, bytes_read);

    return result;
}

const AcdbFile acdb_file = {
    .fileNameLen = ACDB_FILENAME_LEN,
    .fileName = ACDB_FILENAME,
};

AcdbDataFiles acdb_files = {
    .num_files = 1,
    .acdbFiles = {0}
};

int32_t AcdbTestLoad(void)
{
    acdb_files.acdbFiles[0] = acdb_file;
    return acdb_init(&acdb_files, NULL);
}

enum TestKeyIds{
    USECASEID           = (int)0xFF000000,    /**< @h2xmle_name{UseCaseId} */
};

enum Key_UseCaseId {
    QCC_BT_A2DP = (int)0xFF000001, /**< @h2xmle_name {QCC_BT_A2DP}*/
    QCC_BT_SCO  = (int)0xFF000002, /**< @h2xmle_name {QCC_BT_SCO}*/
};

static struct gsl_acdb_data_files gsl_acdb_files = {
    .num_files = 1,
    .acdbFiles = {
        {.fileNameLen = ACDB_FILENAME_LEN, .fileName = ACDB_FILENAME}
    }
};

static struct gsl_init_data gsl_init_data = {
    .acdb_files = &gsl_acdb_files,
    .acdb_delta_file = NULL,
    .acdb_addr = NULL,
    .max_num_ready_checks = 0,
    .ready_check_interval_ms = 0
};

static struct gsl_key_value_pair kvp = {
    .key = USECASEID,
    .value = QCC_BT_A2DP
};

static struct gsl_key_vector gkv = {
    .num_kvps = 1,
    .kvp = &kvp
};

static bool gsl_init_done = FALSE;

int32_t GslTestInit(void)
{
    int32_t result = 0;
    if (!gsl_init_done)
    {
        result = gsl_init(&gsl_init_data);
        gsl_init_done = !result;
    }
    return result;
}

int32_t GslTestOpen(void)
{
    int32_t result = 0;

    result = GslTestInit();

    gsl_handle_t *handle = ar_heap_malloc(sizeof(gsl_handle_t), &heap_info);

    result = gsl_open(&gkv, NULL, handle);

    return result;
}

#define GSL_OPEN_START (-1)
#define GSL_OPEN_DONE (-2)

time_probe_t probe;

int32_t GslTestBenchmark(void)
{
struct gsl_key_value_pair bench_kvp[] = {
    {.key = DEVICETX, .value = HANDSETMIC},
    {.key = DEVICEPP_TX, .value = DEVICEPP_TX_VOICE_UI_FLUENCE_FFECNS},
    {.key = STREAMTX, .value = VOICE_UI},
};

struct gsl_key_vector bench_gkv = {
    .num_kvps = 3,
    .kvp = bench_kvp
};

    int32_t result = 0;

    result = GslTestInit();

    gsl_handle_t *handle = ar_heap_malloc(sizeof(gsl_handle_t), &heap_info);

    result = gsl_open(&bench_gkv, NULL, handle);
    PanicFalse(result == 0);

    result = gsl_close(*handle);
    PanicFalse(result == 0);

    return result;
}

#define READ_4K (4096)
#define READ_8K (2*(READ_4K))

static uint8_t read_buffer[READ_8K];

void FlashTestBenchmarkReadAll(void)
{
    ar_fhandle fhandle = NULL;
    size_t fsize = 2;
    size_t bytes_read = 1;
    size_t offset = 0;

    ar_fopen(&fhandle, ACDB_FILENAME, AR_FOPEN_READ_ONLY);
    memset(read_buffer, 0, READ_8K);

    fsize = ar_fsize(fhandle);

    TIME_PROBE_EVENT(flash_read_all);
    while(offset < fsize)
    {
        size_t bytes_to_read = fsize - offset > READ_4K ? READ_4K : fsize - offset;
        ar_fseek(fhandle, offset, AR_FSEEK_BEGIN);
        ar_fread(fhandle, (void*)read_buffer, bytes_to_read, &bytes_read);
        PanicFalse(bytes_to_read == bytes_read);
        offset += bytes_read;
    };

    TIME_PROBE_EVENT(flash_event_done);
    ar_fclose(fhandle);
}

void FlashTestBenchmarkRead8k_non_seq(void)
{
    ar_fhandle fhandle = NULL;
    size_t bytes_read = 1;

    ar_fopen(&fhandle, ACDB_FILENAME, AR_FOPEN_READ_ONLY);
    memset(read_buffer, 0, READ_8K);

    TIME_PROBE_EVENT(flash_read_8k_non_seq);
    ar_fread(fhandle, (void*)read_buffer, READ_4K, &bytes_read);
    ar_fseek(fhandle, 8*READ_8K+READ_4K/4, AR_FSEEK_CURRENT);
    ar_fread(fhandle, (void*)read_buffer, READ_4K, &bytes_read);
    TIME_PROBE_EVENT(flash_event_done);

    PanicFalse(READ_4K == bytes_read);
    ar_fclose(fhandle);
}

void FlashTestBenchmarkRead(size_t bytes)
{
    ar_fhandle fhandle = NULL;
    size_t bytes_read = 1;

    ar_fopen(&fhandle, ACDB_FILENAME, AR_FOPEN_READ_ONLY);
    memset(read_buffer, 0, READ_8K);
    PanicFalse(bytes <= READ_8K);
    
    TIME_PROBE_EVENT(flash_read_bytes);
    ar_fread(fhandle, (void*)read_buffer, bytes, &bytes_read);
    TIME_PROBE_EVENT(flash_event_done);

    PanicFalse(bytes == bytes_read);
    ar_fclose(fhandle);
}

#endif /*DISABLE_TEST_API*/
#endif /*INCLUDE_AUDIOREACH*/
