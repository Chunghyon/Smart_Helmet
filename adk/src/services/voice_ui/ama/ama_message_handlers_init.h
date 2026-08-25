/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_message_handlers_init.h
\brief  Defintion of the APIs for handling AMA events
*/

#ifndef AMA_MESSAGE_HANDLERS_INIT_H
#define AMA_MESSAGE_HANDLERS_INIT_H

/*! \brief Initialise the message handler for client level events.
 */
void Ama_InitialiseAppMessageHandlers(void);

/*! \brief Get the task for the core message handler.
 *  \return The task
 */
Task Ama_GetCoreMessageHandlerTask(void);

/*! \brief Get the task for the extended message handler.
 *  \return The task
 */
Task Ama_GetExtendedMessageHandlerTask(void);

/*! \brief Clear the extended message handler task of any active timeouts
 */
void Ama_ClearTimeoutsFromExtendedMessageHandlerTask(void);

#endif // AMA_MESSAGE_HANDLERS_INIT_H
