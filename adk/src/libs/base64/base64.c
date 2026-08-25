/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       base64.h
\brief      Implementation of APIs used for base64 conversions
*/

#include "base64.h"

#include <panic.h>
#include <stdlib.h>

#define NUMBER_OF_BITS_IN_OCTET 8
#define NUMBER_OF_BITS_IN_SEXTET 6

#define NUMBER_OF_OCTETS_PER_BASE_64_CHAR 3
#define NUMBER_OF_SEXTETS_PER_BASE_64_CHAR 4

#define SIX_BITS 63

#define BASE_64_LOOKUP_TABLE_SIZE 64

static const char base_64_lookup_table[BASE_64_LOOKUP_TABLE_SIZE] =
{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

static __inline__ uint32 base64_RoundUpToNearestMultipleOf(uint32 number_to_round_up, uint8 round_to_multiple_of)
{
    PanicFalse(round_to_multiple_of != 0);
    uint32 remainder = (number_to_round_up % round_to_multiple_of);

    return (remainder == 0) ? number_to_round_up : (number_to_round_up + round_to_multiple_of) - remainder;
}

static uint32 base64_GetNumberOfPaddingCharacters(uint32 number_of_octets_in_input)
{
    uint32 number_of_bits_in_input = number_of_octets_in_input * NUMBER_OF_BITS_IN_OCTET;
    uint32 number_of_padding_bits = (Base64_GetEncodedOutputLength(number_of_octets_in_input) * NUMBER_OF_BITS_IN_SEXTET) - number_of_bits_in_input;
    return (number_of_padding_bits / NUMBER_OF_BITS_IN_SEXTET);
}

uint32 Base64_GetEncodedOutputLength(uint32 number_of_octets_in_input)
{
    uint32 number_of_bits_in_input = number_of_octets_in_input * NUMBER_OF_BITS_IN_OCTET;
    uint32 nearest_number_of_bits_for_whole_sextets = base64_RoundUpToNearestMultipleOf(number_of_bits_in_input, NUMBER_OF_BITS_IN_SEXTET);
    return base64_RoundUpToNearestMultipleOf(nearest_number_of_bits_for_whole_sextets/NUMBER_OF_BITS_IN_SEXTET, NUMBER_OF_SEXTETS_PER_BASE_64_CHAR);
}

void Base64_Encode(char * output_to_populate, uint32 output_length, uint8 * input_data, uint32 input_length)
{
    PanicNull(output_to_populate);
    PanicNull(input_data);

    uint32 encoded_output_length = Base64_GetEncodedOutputLength(input_length);

    PanicFalse(output_length >= encoded_output_length);

    uint32 groups_of_four_sextets_and_three_octets = encoded_output_length/NUMBER_OF_SEXTETS_PER_BASE_64_CHAR;
    unsigned i;

    for(i=0; i<groups_of_four_sextets_and_three_octets; i++)
    {
        /* Pack three octets into a variable, padding with zeros where necessary */
        uint32 three_octets = 0;

        if((i*NUMBER_OF_OCTETS_PER_BASE_64_CHAR)<input_length)
        {
            three_octets |= (uint32)*input_data++ << 16;
        }

        if(((i*NUMBER_OF_OCTETS_PER_BASE_64_CHAR)+1)<input_length)
        {
            three_octets |= (uint32)*input_data++ << 8;
        }

        if(((i*NUMBER_OF_OCTETS_PER_BASE_64_CHAR)+2)<input_length)
        {
            three_octets |= (uint32)*input_data++;
        }

        /* Extract four sextets from the three octets and covert to base64 characters using lookup table */
        *output_to_populate++ = base_64_lookup_table[(three_octets >> 18) & SIX_BITS];
        *output_to_populate++ = base_64_lookup_table[(three_octets >> 12) & SIX_BITS];
        *output_to_populate++ = base_64_lookup_table[(three_octets >> 6) & SIX_BITS];
        *output_to_populate++ = base_64_lookup_table[three_octets & SIX_BITS];
    }

    uint32 number_of_padding_characters = base64_GetNumberOfPaddingCharacters(input_length);

    for(i=0; i<number_of_padding_characters; i++)
    {
        *--output_to_populate = '=';
    }
}
