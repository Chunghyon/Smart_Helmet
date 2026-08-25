/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_receive_extended_command_3p.c
\brief  Implementation of the APIs to handle extended commands for 3P
*/

#ifdef INCLUDE_AMA

#include "ama_receive_extended_command.h"

#include "ama_command_handlers.h"

void AmaProtocol_ReceiveExtendedCommand(ControlEnvelope * control_envelope_in)
{
    AmaCommandHandlers_NotHandled(control_envelope_in);
}

#endif /* INCLUDE_AMA */
