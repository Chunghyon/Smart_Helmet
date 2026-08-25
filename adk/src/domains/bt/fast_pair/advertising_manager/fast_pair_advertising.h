/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file        fast_pair_advertising.h
\brief      Handles Fast Pair Advertising 
*/

#ifndef FAST_PAIR_ADVERTISING_H_
#define FAST_PAIR_ADVERTISING_H_

/*! Private API to initialise fastpair advertising module

    Called from Fast pair state manager to initialise the fastpair advertising module

 */
bool fastPair_SetUpAdvertising(void);


/*! @brief Private API to handle change in BR/EDR Connectable state and notify the LE Advertising Manager

    Called from Fast pair state manager based on its registration as observer for BR/EDR connections on connection status changes

 */
bool fastPair_AdvNotifyChangeInConnectableState(bool connectable);

/*! @brief Private API to notify the LE Advertising Manager on FP adverts data change

    Called from fast pair state manager upon deleting the account keys or adverts data change

*/
bool fastPair_AdvNotifyDataChange(void);

/*! @brief Private API to provide BR/EDR discoverablity information to fastpair state manager

     Called from Fast pair state manager to decide on reading KBP public key based on this

*/
bool fastPair_AdvIsBrEdrDiscoverable(void);

/*! @brief Private API to set the identifiable parameter according to the data set returned by Adv Mgr

     Called from Fast Pair state manager on getting CL_SM_AUTHENTICATE_CFM to make handset unidentifiable

 */
bool fastPair_AdvNotifyChangeInIdentifiable(bool identifiable);

/*! @brief Private API to check the in use account key active state being advertised

    /returns state of InUseAccountKeyActive in the current advertising data
*/
bool fastPair_AdvInUseAccountKeyActiveState(void);

#endif /* FAST_PAIR_ADVERTISING_H_ */
