/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Type that defines an abstract block of data
*/

#ifndef DATA_BLOB_TYPES_H_
#define DATA_BLOB_TYPES_H_

typedef struct
{
    size_t data_length;
    void *data;
} data_blob_t;

#endif /* DATA_BLOB_TYPES_H_ */
