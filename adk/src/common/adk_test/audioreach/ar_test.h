/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\brief      Interface for common testing functions.
*/

/*! @{ */

#ifndef AR_TEST_H
#define AR_TEST_H

#include <stdint.h>

typedef struct _ar_ts_t {
    int id;
    uint32_t timestamp;
} ar_ts_t;

uint32_t ArOsalTestOpenAcdbFile(const char *filename);

int32_t AcdbTestLoad(void);

int32_t GslTestInit(void);

int32_t GslTestOpen(void);

int32_t GslTestBenchmark(void);

void FlashTestBenchmarkReadAll(void);

void FlashTestBenchmarkRead4k(void);

void FlashTestBenchmarkRead8k(void);

void FlashTestBenchmarkRead8k_non_seq(void);

void FlashTestBenchmarkRead(size_t bytes);

#endif /*AR_TEST_H*/
/*! @} */
