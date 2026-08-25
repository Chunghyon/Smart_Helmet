/*!
\copyright  Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_receive_command.h
\brief  Definition of the APIs to handle unpacked data from the phone
*/

#ifndef __AMA_RECEIVE_COMMAND_H_
#define __AMA_RECEIVE_COMMAND_H_

#include "ama_transport_version_types.h"

/*! \brief Pass command packet to the relevant handler function
 *  \param data Command packet buffer
 *  \param length Length of the command packet buffer
 */
ama_rx_code_t AmaReceive_Command(char * data, uint16 length);

#endif /* __AMA_RECEIVE_COMMAND_H_ */

