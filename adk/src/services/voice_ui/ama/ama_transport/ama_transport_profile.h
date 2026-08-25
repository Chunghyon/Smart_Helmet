/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_transport_profile.h
\brief      Profile interface for Amazon AVS internal APIs
*/

#ifndef AMA_TRANSPORT_PROFILE_H
#define AMA_TRANSPORT_PROFILE_H

#include <bdaddr.h>

typedef void(*disconnect_callback_t)(void);

/*! \brief Initialise the AMA profile handling
*/
void AmaTransport_ProfileInit(void);

/*! \brief Tell profile whether it needs to disconnect AMA transport upon handset disconnection
 *  \param TRUE if disconnect is required, otherwise FALSE
*/
void AmaTransport_InternalSetProfileDisconnectRequired(bool disconnect_required);

/*! \brief Register callback to be used when profile manager initiates a disconnect
 *  \param callback The callback
*/
void AmaTransport_RegisterProfileClient(disconnect_callback_t callback);

/*! \brief Send a connected indication for the profile
 *  \param bd_addr Pointer to BT address
*/
void AmaTransport_SendProfileConnectedInd(const bdaddr * bd_addr);

/*! \brief Send a disconnected indication for the profile
 *  \param bd_addr Pointer to BT address
*/
void AmaTransport_SendProfileDisconnectedInd(const bdaddr * bd_addr);

#endif // AMA_TRANSPORT_PROFILE_H
