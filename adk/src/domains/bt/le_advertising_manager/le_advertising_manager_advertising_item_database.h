/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for LE advertising manager functionality associated with advertising item database

*/

#ifndef LE_ADVERTISING_MANAGER_ADVERTISING_ITEM_DATABASE_H_
#define LE_ADVERTISING_MANAGER_ADVERTISING_ITEM_DATABASE_H_

#ifdef LE_ADVERTISING_MANAGER_NEW_API

/*! \brief Data type for the iterator object to be used as input parameter for the API functions
 to retrieve advertising items stored in the advertising item database */
typedef struct
{
    le_adv_item_handle handle;
}le_adv_item_iterator_t;

/*! \brief Data type for the advertising item object */
struct _le_adv_item
{
    Task task;
    const le_adv_item_callback_t *callback;
};

/*! \brief API to initialise advertising item database
    \note This is the API function to be called before calling any other API functions this module owns
*/
void LeAdvertisingManager_InitAdvertisingItemDatabase(void);

/*! \brief API to retrieve maximum number of advertising items to be stored in the advertising item database
    \return Maximum number of advertising items of type uint32 to be stored in the advertising item database
*/
uint32 LeAdvertisingManager_GetAdvertisingItemDatabaseMaxSize(void);

/*! \brief API to retrieve the head item in the advertising item database
    \param[in] iterator Parameter of type le_adv_item_iterator_t 
    \return Valid pointer of type le_adv_item_handle for the head item in the advertising item database
*/
le_adv_item_handle LeAdvertisingManager_HeadItem(le_adv_item_iterator_t * iterator);

/*! \brief API to retrieve the next item in the advertising item database
    \param[in] iterator Parameter of type le_adv_item_iterator_t 
    \return Valid pointer of type le_adv_item_handle for the next item in the advertising item database
*/
le_adv_item_handle LeAdvertisingManager_NextItem(le_adv_item_iterator_t * iterator);

#endif /* LE_ADVERTISING_MANAGER_NEW_API */

#endif /* LE_ADVERTISING_MANAGER_ADVERTISING_ITEM_DATABASE_H_ */
