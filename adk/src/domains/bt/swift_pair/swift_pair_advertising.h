/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#ifndef SWIFT_PAIR_ADVERTISING_H_
#define SWIFT_PAIR_ADVERTISING_H_

/*! \brief Setup the LE advertising data for Swift Pair

    \returns TRUE if the advertising data was setup successfully, else FALSE
 */
bool SwiftPair_SetupLeAdvertisingData(void);

/*! \brief Trigger an update the LE advertising data for Swift Pair

    \param discoverable enables/disables discoverable advertising data
    \returns TRUE if the update was, else FALSE
 */
bool SwiftPair_UpdateLeAdvertisingData(bool discoverable);

#endif /* SWIFT_PAIR_ADVERTISING_H_ */
