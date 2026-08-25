/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       base64.h
\brief      Definition of APIs used for base64 conversions
*/

#ifndef BASE64_H_
#define BASE64_H_

#include <csrtypes.h>

/*! \brief Get the base64 encoded output length for an specified input length
 *  \param number_of_octets_in_input The length of the input in octets
 *  \return The length of the encoded output in chars */
uint32 Base64_GetEncodedOutputLength(uint32 number_of_octets_in_input);

/*! \brief Encode some raw data in base64 format
 *  \param output_to_populate A pointer to the buffer to be populated with a base64 encoding of the input data
 *                            Users are expected to allocate their own buffer, the size required can be found using
 *                            the Base64_GetEncodedOutputLength() API
 *                            The output will not be NULL terminating, users will need to add a NULL termintor
 *                            themselves if required
 *  \param output_length The length of the output buffer
 *  \param input_data A pointer to the buffer containing the raw input data
 *  \param input_length The length of the input data buffer */
void Base64_Encode(char * output_to_populate, uint32 output_length, uint8 * input_data, uint32 input_length);

#endif
