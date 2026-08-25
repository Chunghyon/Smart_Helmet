/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef PEER_PAIR_LE_ADVERTISING_H_
#define PEER_PAIR_LE_ADVERTISING_H_

/*! \brief Setup the LE advertising data for peer pairing

    \returns TRUE if the advertising data was setup successfully, else FALSE
 */
bool PeerPairLe_SetupLeAdvertisingData(void);

/*! \brief Trigger an update the LE advertising data for peer pairing

    \returns TRUE if the update was successful, else FALSE
 */
bool PeerPairLe_UpdateLeAdvertisingData(void);



#endif /* PEER_PAIR_LE_ADVERTISING_H_ */
