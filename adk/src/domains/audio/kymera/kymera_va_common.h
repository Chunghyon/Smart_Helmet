/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Common kymera VA definitions

*/

#ifndef KYMERA_VA_COMMON_H_
#define KYMERA_VA_COMMON_H_

#include <operator.h>

typedef void (* OperatorFunction) (Operator *array, unsigned length_of_array);

/*! \brief Set the VA sample rate.
    \param sample_rate sample rate to set
*/
void Kymera_SetVaSampleRate(unsigned sample_rate);

/*! \brief Get the VA sample rate.
    \return VA sample rate
*/
unsigned Kymera_GetVaSampleRate(void);

#endif /* KYMERA_VA_COMMON_H_ */
