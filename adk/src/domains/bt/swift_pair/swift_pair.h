/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       swift_pair.h
\brief      Header file for the Swift Pair
*/

#ifndef SWIFT_PAIR_H_
#define SWIFT_PAIR_H_

#ifdef INCLUDE_SWIFT_PAIR

/*! Initialise the swit pair application module.

    This function is called to initilaze swift pairing module.
    If a message is processed then the function returns TRUE.

    \param  init_task       Initialization Task

    \returns TRUE if successfully processed
 */
bool SwiftPair_Init(Task init_task);

#endif /* INCLUDE_SWIFT_PAIR */

#endif /* SWIFT_PAIR_H_ */
