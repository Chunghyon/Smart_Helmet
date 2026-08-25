/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu_protocol_message_handler.h
\brief      Definition of the message handler APIs for the dfu_protocol module
*/

#ifndef DFU_PROTOCOL_MESSAGE_HANDLER_H
#define DFU_PROTOCOL_MESSAGE_HANDLER_H

/*! \brief Initalise the DFU protocol message handler */
void DfuProtocol_MessageHandlerInit(void);

/*! \brief Get the DFU protocol message handler task
    \return The message handler task */
Task DfuProtocol_GetMessageHandlerTask(void);

#endif // DFU_PROTOCOL_MESSAGE_HANDLER_H
