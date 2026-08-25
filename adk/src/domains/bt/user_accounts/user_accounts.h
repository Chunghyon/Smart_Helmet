/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       user_accounts.h
\brief      Module to store and access user account(crypto keys) information.
*/

#ifndef USER_ACCOUNTS_H_
#define USER_ACCOUNTS_H_
#include "fast_pair_account_key_sync.h"

#define MAX_NUM_ACCOUNT_KEYS                        (5)
#define MAX_USER_ACCOUNT_KEY_LEN                    (16)
#define INVALID_USER_ACCOUNT_KEY_INDEX              (0xFF)

/*! \brief Enum to list out account key types */
typedef enum
{
    user_account_type_invalid,
    user_account_type_fast_pair,
    user_account_type_max
} user_account_type;

typedef struct user_account_key_index
{
    uint8 num_handsets_linked;
    uint8 account_key_type;
    uint8 account_key_len;
    uint8 account_key_start_loc;
} user_account_key_index_t;

typedef struct user_account_key_info
{
    user_account_key_index_t account_key_index[MAX_NUM_ACCOUNT_KEYS];
    uint8  account_keys[MAX_USER_ACCOUNT_KEY_LEN * MAX_NUM_ACCOUNT_KEYS];
} user_account_key_info_t;


/*! \brief Initialise User Account Module
*/
void UserAccounts_Init(void);

/*! \brief Register User Accounts PDDU
 */
void UserAccounts_RegisterPersistentDeviceDataUser(void);

/*! \brief Add the account key
\param account_key account key to be added
\param account_key_len account key len
\param account_key_type account key type to be added
\return Index of the added account key.
*/
uint16 UserAccounts_AddAccountKey(const uint8* account_key, uint16 account_key_len, uint16 account_key_type);


/*! \brief Get account keys by Type
\param account_keys account key to be obtained
\param account_key_type account key type to be obtained
\return number of account keys.
*/
uint16 UserAccounts_GetAccountKeys(uint8** account_keys, uint16 account_key_type);

/*! \brief Get number of account keys stored
\return number of account keys.
*/
uint16 UserAccounts_GetNumAccountKeys(void);

/*! \brief Associate Account Key to Handset
\param account_key account key to be associated
\param account_key_len account key len
\param bd_addr public address of the handset device
\return TRUE to indicate successful association, FALSE otherwise.

*/
bool UserAccounts_AssociateAccountKeyWithHandset(const uint8* account_key, uint16 account_key_len, const bdaddr *bd_addr);

/*! \brief Get Account Key associated to Handset
\param account_key_len account key len
\param bd_addr public address of the handset device
\return Pointer to the account key if a valid account key is found, NULL otherwise. Caller should free the memory allocation if valid.
 */
uint8* UserAccounts_GetAccountKeyWithHandset(uint16 account_key_len, const bdaddr *bd_addr);

/*! \brief Store the user account keys with the index values
     This inteface can be used to store the complete user account key info
    \param user_account_key_sync_req_t* reference to the user account key info
    \return bool TRUE if account keys are stored else FALSE
 */
bool UserAccounts_StoreAllAccountKeys(account_key_sync_req_t* account_key_info);

/*! \brief Delete the account key
\param account_key_index account key index to be deleted
\return TRUE to indicate successful deletion, FALSE otherwise.
*/
bool UserAccounts_DeleteAccountKey(uint16 account_key_index);


/*! \brief Delete All account keys
\return TRUE to indicate successful deletion, FALSE otherwise.
*/
bool UserAccounts_DeleteAllAccountKeys(void);

#endif /* USER_ACCOUNTS_H_ */
