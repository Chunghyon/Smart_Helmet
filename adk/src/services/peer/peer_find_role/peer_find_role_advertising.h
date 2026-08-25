/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef PEER_FIND_ROLE_ADVERTISING_H_
#define PEER_FIND_ROLE_ADVERTISING_H_

/*! \brief Setup the LE advertising data for Peer Find Role

    \returns TRUE if the advertising data was setup successfully, else FALSE
 */
bool PeerFindRole_SetupLeAdvertisingData(void);

/*! \brief Trigger an update the LE advertising data for Peer Find Role

    \returns TRUE if the update was successful, else FALSE
 */
bool PeerFindRole_UpdateLeAdvertisingData(void);

/*! \brief Add the Peer addr to the LE acceptor list.

    The Peer Find Role adverts will only allow devices on the LE acceptor list
    to connect. So the peer address needs to be on the acceptor list so that
    peer find role can succeed.
*/
void PeerFindRole_AddPeerToAcceptorList(void);

/*! \brief Remove the Peer addr from the LE acceptor list. */
void PeerFindRole_RemovePeerFromAcceptorList(void);

#endif /* PEER_FIND_ROLE_ADVERTISING_H_ */
