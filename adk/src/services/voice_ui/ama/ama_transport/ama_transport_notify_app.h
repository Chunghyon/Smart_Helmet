/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_notify_app.h
\brief      AMA transport app notification internal APIs
*/

#ifndef AMA_TRANSPORT_NOTIFY_APP_H
#define AMA_TRANSPORT_NOTIFY_APP_H

#include <message.h>
#include "ama_transport_types.h"

/*! \brief Initialise the app task list */
void AmaTransport_InitAppTaskList(void);

/*! \brief Register an app task to receive transport related notifications
    \param task The app task to send notifications to
*/
void AmaTransport_RegisterAppTask(Task task);

/*! \brief Notify the app that the transport has switched
    \param new_transport The transport type we have switched to
*/
void AmaTransport_NotifyAppTransportSwitched(ama_transport_type_t new_transport);

/*! \brief Notify the app that a local disconnect request has been completed
*/
void AmaTransport_NotifyAppLocalDisconnectComplete(void);

#endif // AMA_TRANSPORT_NOTIFY_APP_H
