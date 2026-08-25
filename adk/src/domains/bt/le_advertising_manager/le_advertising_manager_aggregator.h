/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for LE advertising manager aggregator, which is responsible for collecting individual advertising items and creating advertising sets

*/

#ifndef LE_ADVERTISING_MANAGER_AGGREGATOR_H_
#define LE_ADVERTISING_MANAGER_AGGREGATOR_H_

#ifdef LE_ADVERTISING_MANAGER_NEW_API

#include "le_advertising_manager.h"

/*! \brief Definition for the object to provide input for refresh operation
 */
typedef struct
{
    void (*advertising_state_update_callback)(void);
}le_adv_refresh_control_t;

/*! \brief API to initialise advertising item aggregator
    \note This is the API function to be called before calling any other API functions this module owns
*/
void LeAdvertisingManager_InitAggregator(void);

/*! \brief API to update advertising following a change in advertising state
    \param control Pointer to le_adv_refresh_control_t object to provide input for refresh operation
*/
void LeAdvertisingManager_QueueAdvertisingStateUpdate(le_adv_refresh_control_t * control);

/*! \brief API to update advertising following a change in client data 
    \param item_handle handle to the client data flagging an update
*/
void LeAdvertisingManager_QueueClientDataUpdate(le_adv_item_handle item_handle);


#endif /* LE_ADVERTISING_MANAGER_NEW_API */

#endif /* LE_ADVERTISING_MANAGER_AGGREGATOR_H_ */
