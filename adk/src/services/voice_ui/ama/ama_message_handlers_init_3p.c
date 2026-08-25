/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_message_handlers_init_3p.c
\brief  Implementation of the APIs for handling AMA events
*/

#ifdef INCLUDE_AMA

#include "ama_message_handlers_init.h"
#include "ama_notify_app_msg.h"
#include <logging.h>

void Ama_ClearTimeoutsFromExtendedMessageHandlerTask(void)
{
}

Task Ama_GetExtendedMessageHandlerTask(void)
{
    return NULL;
}

void Ama_InitialiseAppMessageHandlers(void)
{
    DEBUG_LOG("Ama_InitialiseAppMessageHandlers");
    AmaProtocol_InitialiseAppNotifier(Ama_GetCoreMessageHandlerTask());
}

#endif /* INCLUDE_AMA */
