/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       dfu_config.h
\brief      Configuration related definitions for dfu domain.
*/

#ifndef DFU_CONFIG_H_
#define DFU_CONFIG_H_

#ifdef INCLUDE_DFU

/*! Timeout for entering the case after requesting host to put earbuds in case
 *  for case DFU. */
#define dfuConfigCaseDfuTimeoutToPutInCaseMs()        (D_SEC(60))

#endif /* INCLUDE_DFU */

#endif /* DFU_CONFIG_H_ */

