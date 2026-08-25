/*!
\copyright  Copyright (c) 2020-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Provides TWS support in the accessory domain
*/


#ifndef AMA_TWS_H
#define AMA_TWS_H

#include "ama_transport_types.h"
#include "tws_topology_role_change_client_if.h"

/*! \brief Handle local disconnection completed event
 */
void AmaTws_HandleLocalDisconnectionCompleted(void);

/*! \brief Disconnect if it is required
 *  \param reason The reason for the local disconnection
 */
void AmaTws_DisconnectIfRequired(ama_local_disconnect_reason_t reason);

/*! \brief Check if disconnection is required
 *  \return TRUE if diconnect required, otherwise FALSE
 */
bool AmaTws_IsDisconnectRequired(void);

/*! \brief Notify interested modules that role has changed
 *  \param role The new TWS topology role
 */
void AmaTws_RoleChangedIndication(tws_topology_role role);

#ifdef HOSTED_TEST_ENVIRONMENT
void Ama_tws_Reset(void);
#endif

#endif /* AMA_TWS_H */
