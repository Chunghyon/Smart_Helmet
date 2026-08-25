/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_receive_extended_command.h
\brief  Definition of the API to handle unpacked data from the phone for extended commands
*/

#ifndef __AMA_RECEIVE_EXTENDED_COMMAND_H_
#define __AMA_RECEIVE_EXTENDED_COMMAND_H_

#include "ama_transport_version_types.h"
#include "accessories.pb-c.h"

/*! \brief Pass command envelope to the relevant handler
 *  \param control_envelope_in Command envelope
 */
void AmaProtocol_ReceiveExtendedCommand(ControlEnvelope * control_envelope_in);

#endif /* __AMA_RECEIVE_EXTENDED_COMMAND_H_ */

