/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_data.h
\brief      Definition of the data caching APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_DATA_H
#define DFU_PROTOCOL_DATA_H

#include <csrtypes.h>

#ifdef HOSTED_TEST_ENVIRONMENT
/* Use a smaller buffer for unit testing */
#define BUFFER_SIZE_IN_BYTES (12)
#else /* HOSTED_TEST_ENVIRONMENT */
#define BUFFER_SIZE_IN_BYTES (2048)
#endif /* HOSTED_TEST_ENVIRONMENT */

typedef struct
{
    /*! Pointer to the circular buffer, heap mem */
    uint8 * buffer;
    /*! The total size of the incoming DFU file */
    uint32 total_dfu_file_size;
    /*! The index in the buffer with the first byte of stored data */
    uint16 read_index;
    /*! The index in the buffer of the first empty element */
    uint16 write_index;
    /*! The DFU file offset at the read index of the buffer */
    uint32 read_offset;
    /*! The DFU file offset at the write index of the buffer */
    uint32 write_offset;
    /*! The DFU file offset that has been requested to be read */
    uint32 requested_read_offset;
    /*! The number of bytes requested to read */
    uint32 requested_number_of_bytes;
} dfu_protocol_data_t;
 
/*! \brief Create the data buffer, and initialise the DFU protocol data module
    \param total_dfu_file_size The total size of the expected DFU file in bytes */
void DfuProtocol_CreateDataInstance(uint32 total_dfu_file_size);

/*! \brief Destroy the data buffer */
void DfuProtocol_DestroyDataInstance(void);

/*! \brief Set the requested read offset 
    \param offset_from_previous request The offset in bytes from the previous request, 
                                        if it continuguous from the previous request this will be zero, 
                                        otherwise it will be non-zero */
void DfuProtocol_SetRequestedReadOffset(uint32 offset_from_previous_request);

/*! \brief Set the number of bytes requested
    \param number_of_bytes The number of bytes, this value will decremenet as data is read using DfuProtocol_ReadData,
                           the client should use DfuProtocol_GetRemainingNumberOfBytesRequested to keep track of progress
                           and to be aware of when the request has completed */
void DfuProtocol_SetRequestedNumberOfBytes(uint32 number_of_bytes);

/*! \brief Get the remaining number of bytes requested
    \return The number of bytes remaining in the current request*/
uint32 DfuProtocol_GetRemainingNumberOfBytesRequested(void);
 
/*! \brief Gets the number of bytes of data stored in the buffer */
uint16 DfuProtocol_GetLengthOfAvailableDataInBytes(void);

/*! \brief Writes data starting at the write_index, increases write_index by length
    \param data Pointer to the data to copy from
    \param length_to_write The amount of data to write to the internal buffer
    \param offset_in_dfu_file The offset in the DFU file of the data to write
    \return TRUE if the data fits and was written, otherwise FALSE */
bool DfuProtocol_WriteData(uint8 * data, uint16 length_to_write, uint32 offset_in_dfu_file);

/*! \brief Reads a given amount of data from the circular buffer at the requested read offset
           Requested read offset defaults to 0, but can be incremented using DfuProtocol_SetRequestedReadOffset
           Moves it into the buffer parameter passed
           Removes that data from the circular buffer
           Increases read_index and start_offset
    \param buffer_to_populate Pointer to the buffer to populate
    \param length_to_read The amount of data to read into the buffer to populate
    \return TRUE if the requested data was available and the buffer was populated, otherwise FALSE */
bool DfuProtocol_ReadData(uint8 * buffer_to_populate, uint16 length_to_read);

/*! \brief Check if reading a given length will equate to reading the last packet of the DFU file
    \param legnth_to_read The length in bytes*/
bool DfuProtocol_IsReadingLastPacket(uint16 length_to_read);

#ifdef HOSTED_TEST_ENVIRONMENT
dfu_protocol_data_t * DfuProtocol_GetInternalData(void);
#endif /* HOSTED_TEST_ENVIRONMENT */

#endif // DFU_PROTOCOL_DATA_H
