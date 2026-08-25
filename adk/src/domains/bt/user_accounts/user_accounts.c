/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       user_accounts.c
\brief      Module to store and access user account(crypto keys) information
*/

#include "user_accounts.h"

#include <device.h>
#include <device_list.h>
#include <stdlib.h>
#include <byte_utils.h>
#include <panic.h>
#include <bt_device.h>
#include <logging.h>

#include <device_db_serialiser.h>
#include <device_properties.h>
#include <pddu_map.h>


static void user_accounts_serialise_persistent_device_data(device_t device, void *buf, uint8 offset)
{
    void *account_key_index_value = NULL;
    void *account_keys_value = NULL;
    size_t account_key_index_size = 0;
    size_t account_keys_size = 0;
    UNUSED(offset);

     /* store account key data to PS store*/
    if(Device_GetProperty(device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
        Device_GetProperty(device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
    {
        user_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(user_account_key_info_t));
        memcpy(buffer->account_key_index, account_key_index_value, sizeof(buffer->account_key_index));
        memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));
        memcpy(buf, buffer, sizeof(user_account_key_info_t));
        free(buffer);
    }
    else
    {
        DEBUG_LOG("user_accounts_serialise_persistent_device_data: Device_GetProperty(device_property_user_account_keys) fails ");
    }
}

static void user_accounts_deserialise_persistent_device_data(device_t device, void *buf, uint8 data_length, uint8 offset)
{
    UNUSED(offset);
    UNUSED(data_length);
    /* PS retrieve data to device database */
    user_account_key_info_t *buffer = (user_account_key_info_t *)buf;
    Device_SetProperty(device, device_property_user_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
    Device_SetProperty(device, device_property_user_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));

 }

static uint8 user_accounts_get_device_data_len(device_t device)
{
    void *account_key_index_value = NULL;
    void *account_keys_value = NULL;
    size_t account_key_index_size = 0;
    size_t account_keys_size = 0;
    uint8 user_accounts_device_data_len = 0;

    if(DEVICE_TYPE_SELF == BtDevice_GetDeviceType(device))
    {
        if(Device_GetProperty(device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
           Device_GetProperty(device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
        {
           user_accounts_device_data_len = account_key_index_size+account_keys_size;
           return user_accounts_device_data_len;
        }
    }
    return 0;
}


static void user_accounts_print_account_keys(uint16 num_keys, uint8* account_keys)
{
    DEBUG_LOG("user accounts : Number of account keys %d", num_keys);
    DEBUG_LOG("user accounts : Account keys : ");
    for(uint16 i=0; i<num_keys; i++)
    {
        DEBUG_LOG("%d) : ",i+1);
        for(uint16 j=0; j<MAX_USER_ACCOUNT_KEY_LEN; j++)
        {
            DEBUG_LOG("%02x ", (account_keys)[j+(i*MAX_USER_ACCOUNT_KEY_LEN)]);
        }
    }
}

/*! \brief Register User Accounts PDDU
 */
void UserAccounts_RegisterPersistentDeviceDataUser(void)
{
    DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        PDDU_ID_USER_ACCOUNTS,
        user_accounts_get_device_data_len,
        user_accounts_serialise_persistent_device_data,
        user_accounts_deserialise_persistent_device_data);
}



/*! \brief  Get account keys by type
 */
uint16 UserAccounts_GetAccountKeys(uint8** account_keys, uint16 account_key_type)
{
    uint16 num_keys = 0;
    device_t my_device = BtDevice_GetSelfDevice();

    if(my_device)
    {
        void *account_key_index_value = NULL;
        void *account_keys_value = NULL;
        size_t account_key_index_size = 0;
        size_t account_keys_size = 0;
        if(Device_GetProperty(my_device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
            Device_GetProperty(my_device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
        {
            user_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(user_account_key_info_t));
            *account_keys = PanicUnlessMalloc(MAX_USER_ACCOUNT_KEY_LEN * MAX_NUM_ACCOUNT_KEYS);
            memcpy(buffer->account_key_index, account_key_index_value, sizeof(buffer->account_key_index));
            memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));


            /* Validate account keys stored in Account key Index */
            for(uint16 count=0;count<MAX_NUM_ACCOUNT_KEYS;count++)
            {
                if((buffer->account_key_index[count].num_handsets_linked > 0) && (buffer->account_key_index[count].account_key_type == account_key_type))
                {
                    memcpy(*account_keys+(num_keys*MAX_USER_ACCOUNT_KEY_LEN), &buffer->account_keys[count*MAX_USER_ACCOUNT_KEY_LEN], MAX_USER_ACCOUNT_KEY_LEN);
                    num_keys++;
                }
            }
            user_accounts_print_account_keys(num_keys, *account_keys);
            free(buffer);
        }
        else
        {
            /* No account keys were found return from here */
            DEBUG_LOG("User Accounts : Number of account keys %d", num_keys);
        }
    }
    else
    {
        DEBUG_LOG("User Accounts : Unexpected Error. Shouldn't have reached here");
    }
    return num_keys;
}

/*! \brief Get the user account keys
 */
uint16 UserAccounts_GetNumAccountKeys(void)
{
    uint16 num_keys = 0;
    device_t my_device = BtDevice_GetSelfDevice();

    if(my_device)
    {
        void *account_key_index_value = NULL;
        size_t account_key_index_size = 0;

        if(Device_GetProperty(my_device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
            account_key_index_size)
        {
            user_account_key_index_t account_key_index[MAX_NUM_ACCOUNT_KEYS];
            
            memcpy(&account_key_index[0], account_key_index_value, account_key_index_size);

            for(uint16 count=0;count<MAX_NUM_ACCOUNT_KEYS;count++)
            {
                if((account_key_index[count].num_handsets_linked > 0) && (account_key_index[count].account_key_type > user_account_type_invalid) &&
                    (account_key_index[count].account_key_type < user_account_type_max))
                {
                    num_keys++;
                }
            }

        }
    }
    DEBUG_LOG("User Accounts : Number of account keys %d", num_keys);
    return num_keys;
}

/*! \brief Store the User account key
 */
uint16 UserAccounts_AddAccountKey(const uint8* account_key, uint16 account_key_len, uint16 account_key_type)
{
    uint16 added_account_key_index = INVALID_USER_ACCOUNT_KEY_INDEX;

    /* Check if received account key is valid */
    if(account_key && (account_key_len != 0) && (account_key_len <=MAX_USER_ACCOUNT_KEY_LEN))
    {
        /* First byte of account key must be 0x04 for fast pair account keys */
        if((account_key_type == user_account_type_fast_pair) && (account_key[0] != 0x04))
        {
            DEBUG_LOG("User Accounts : Invalid account key received");
            return INVALID_USER_ACCOUNT_KEY_INDEX;
        }
        
        /* SELF device will store the account keys */
        device_t my_device = BtDevice_GetSelfDevice();

        DEBUG_LOG("User Accounts : Store account key :");
        for(uint16 i=0; i<account_key_len; i++)
        {
            DEBUG_LOG("%02x ",account_key[i]);
        }

        if(my_device)
        {
            void *account_key_index_value = NULL;
            void *account_keys_value = NULL;
            size_t account_key_index_size = 0;
            size_t account_keys_size = 0;
            uint16 duplicate_account_key_index = 0xFF; /* Invalid Index as AKI varies from 0 to (MAX_NUM_ACCOUNT_KEYS-1) */
            uint16 count = 0;
            user_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(user_account_key_info_t));

            memset(buffer, 0x00, sizeof(user_account_key_info_t));
            /* SELF device is found, check whether account key index & account keys properties exists on SELF device */
            if(Device_GetProperty(my_device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
                Device_GetProperty(my_device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
            {
                memcpy(buffer->account_key_index, account_key_index_value, sizeof(buffer->account_key_index));
                memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));

                /* If account key to be added is a duplicate, exit without adding anything */
                for(count=0;count<MAX_NUM_ACCOUNT_KEYS;count++)
                {
                    if((buffer->account_key_index[count].num_handsets_linked > 0) && (buffer->account_key_index[count].account_key_type > user_account_type_invalid) &&
                        (buffer->account_key_index[count].account_key_type < user_account_type_max))
                    {
                        if(memcmp(account_key, &buffer->account_keys[count*MAX_USER_ACCOUNT_KEY_LEN], account_key_len) == 0)
                        {
                            duplicate_account_key_index = count;
                            buffer->account_key_index[count].num_handsets_linked++;
                            added_account_key_index = count;
                            break;
                        }
                    }
                }

                if(duplicate_account_key_index == 0xFF)
                {
                    /* No duplicate account key found. Add to existing list.*/
                    DEBUG_LOG("User Accounts : No duplicate account key found. Add to existing list");
                    
                    /* If list is not already full, add to the account key list */
                    for(count=0;count<MAX_NUM_ACCOUNT_KEYS;count++)
                    {
                        if(buffer->account_key_index[count].num_handsets_linked == 0)
                        {
                            user_account_key_index_t account_key_index;

                            account_key_index.num_handsets_linked = 1;
                            account_key_index.account_key_type = account_key_type;
                            account_key_index.account_key_len = account_key_len;
                            account_key_index.account_key_start_loc = count*MAX_USER_ACCOUNT_KEY_LEN;

                            memcpy(&buffer->account_key_index[count], &account_key_index, sizeof(user_account_key_index_t));
                            memcpy(&buffer->account_keys[count*MAX_USER_ACCOUNT_KEY_LEN], account_key, account_key_len);
                            added_account_key_index = count;
                            break;                            
                        }
                    }
                }
   
                /* Store Account key Index and Account keys to PS Store */
                Device_SetProperty(my_device, device_property_user_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
                Device_SetProperty(my_device, device_property_user_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));
                free(buffer);
                DeviceDbSerialiser_Serialise();
                DEBUG_LOG("Added account key index %d", added_account_key_index);
            }
            else
            {
                user_account_key_index_t account_key_index;
                
                account_key_index.num_handsets_linked = 1;
                account_key_index.account_key_type = account_key_type;
                account_key_index.account_key_len = account_key_len;
                account_key_index.account_key_start_loc = 0;
                added_account_key_index = 0;
                /* This is the first account key getting written, add it to the first slot */
                memcpy(&buffer->account_key_index[0], &account_key_index, sizeof(user_account_key_index_t));
                memcpy(&buffer->account_keys[0], account_key, account_key_len);
                Device_SetProperty(my_device, device_property_user_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
                Device_SetProperty(my_device, device_property_user_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));
                free(buffer);
                DeviceDbSerialiser_Serialise();
                DEBUG_LOG("Added acconut key index %d", added_account_key_index);
            }
        }
        else
        {
            DEBUG_LOG("User Accounts : Unexpected Error. Shouldn't have reached here");
        }
    }
    else
    {
        DEBUG_LOG("User Accounts : Invalid account key received");
    }
    return added_account_key_index;
}

/*! \brief Associate the user account key with the given handset
 */
bool UserAccounts_AssociateAccountKeyWithHandset(const uint8* account_key, uint16 account_key_len, const bdaddr *bd_addr)
{
    bool status = FALSE;
    device_t my_device = BtDevice_GetSelfDevice();

    if(my_device)
    {
        void *account_keys_value = NULL;
        size_t account_keys_size = 0;
        uint8 account_key_index = 0xFF;
        user_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(user_account_key_info_t));

        memset(buffer, 0x00, sizeof(user_account_key_info_t));
        if(Device_GetProperty(my_device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
        {
            memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));

            for(uint8 count=0; count < MAX_NUM_ACCOUNT_KEYS; count++)
            {
                if(memcmp(account_key, &buffer->account_keys[count*MAX_USER_ACCOUNT_KEY_LEN], account_key_len) == 0)
                {
                    device_t handset_device = BtDevice_GetDeviceForBdAddr(bd_addr);
                    account_key_index = count;
                    /* Associate the account key index to the handset device */
                    status = Device_SetPropertyU8(handset_device, device_property_handset_account_key_index, account_key_index);
                    /* Handset account key index device property should be persisted across reboot so that the right in-use account key would
                       be used after reboot and we can also make sure that non sass seeker will have invalid handset account key index. */
                    DeviceDbSerialiser_SerialiseDevice(handset_device);
                    break;
                }
            }
        }
        free(buffer);
    }
    else
    {
        DEBUG_LOG("User Accounts : Unexpected Error. Shouldn't have reached here");
    }
    return status;
}

/*! \brief Get the user account key associated with the handset
 */
uint8* UserAccounts_GetAccountKeyWithHandset(uint16 account_key_len, const bdaddr *bd_addr)
{
    uint8 acc_key_index = INVALID_USER_ACCOUNT_KEY_INDEX;
    device_t handset_device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if(handset_device)
    {
        Device_GetPropertyU8(handset_device, device_property_handset_account_key_index, &acc_key_index);

        device_t my_device = BtDevice_GetSelfDevice();
        if(my_device && (acc_key_index >= 0 && acc_key_index < MAX_NUM_ACCOUNT_KEYS))
        {
            void *account_key_index_value = NULL;
            void *account_keys_value = NULL;
            size_t account_key_index_size = 0;
            size_t account_keys_size = 0;

            if(Device_GetProperty(my_device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
                Device_GetProperty(my_device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
            {
                uint8* account_key = NULL;
                user_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(user_account_key_info_t));

                memset(buffer, 0x00, sizeof(user_account_key_info_t));
                memcpy(buffer->account_key_index, account_key_index_value, sizeof(buffer->account_key_index));
                memcpy(buffer->account_keys, account_keys_value, sizeof(buffer->account_keys));

                if(buffer->account_key_index[acc_key_index].num_handsets_linked > 0)
                {
                    /* Memory allocation for the account key to be returned, this should be freed by the caller of this function after its usage */
                    account_key = (uint8*)PanicUnlessMalloc(account_key_len);
                    /* Copy the account key associated with the given account key index */
                    memcpy(account_key, &buffer->account_keys[acc_key_index * MAX_USER_ACCOUNT_KEY_LEN], account_key_len);
                }
                else
                {
                    DEBUG_LOG("User Accounts : No Account Key Linked with the given handset.");
                }
                free(buffer);
                return account_key;
            }
            else
            {
                DEBUG_LOG("User Accounts : Unexpected Error. Device_GetProperty failed to fetch the data.");
            }
        }
        else
        {
            DEBUG_LOG("User Accounts : Unexpected Error. No SELF device or No valid account key linked with the given handset");
        }
    }
    else
    {
        DEBUG_LOG("User Accounts : Unexpected Error. No handset device found for the given BD Addr");
    }
    return NULL;
}

/*! \brief Store the Fast Pair account keys with the index values
 */
bool UserAccounts_StoreAllAccountKeys(account_key_sync_req_t *account_key_info)
{
    bool result = FALSE;

    /* Find the SELF device to add account keys to */
    device_t my_device = BtDevice_GetSelfDevice();
    if(my_device)
    {
        DEBUG_LOG("User Accounts : Storing the complete account key info.");
        Device_SetProperty(my_device, device_property_user_account_key_index, &account_key_info->account_key_index, sizeof(account_key_info->account_key_index));
        Device_SetProperty(my_device, device_property_user_account_keys, &account_key_info->account_keys, sizeof(account_key_info->account_keys));

        result = TRUE;
        DeviceDbSerialiser_Serialise();
    }
    return result;
}

/*! \brief Delete all user account keys
 */
bool UserAccounts_DeleteAllAccountKeys(void)
{
    bool result = FALSE;
    device_t my_device = BtDevice_GetSelfDevice();

    DEBUG_LOG("User Accounts : Delete all account keys");
    if(my_device)
    {
        void *account_key_index_value = NULL;
        void *account_keys_value = NULL;
        size_t account_key_index_size = 0;
        size_t account_keys_size = 0;
        if(Device_GetProperty(my_device, device_property_user_account_key_index, &account_key_index_value, &account_key_index_size) &&
            Device_GetProperty(my_device, device_property_user_account_keys, &account_keys_value, &account_keys_size))
        {
            user_account_key_info_t *buffer = PanicUnlessMalloc(sizeof(user_account_key_info_t));
            memset(buffer, 0x00, sizeof(user_account_key_info_t));

            Device_SetProperty(my_device, device_property_user_account_key_index, &buffer->account_key_index, sizeof(buffer->account_key_index));
            Device_SetProperty(my_device, device_property_user_account_keys, &buffer->account_keys, sizeof(buffer->account_keys));

            free(buffer);
            DeviceDbSerialiser_Serialise();
            result = TRUE;
        }
    }
    else
    {
        DEBUG_LOG("User Accounts : Unexpected Error. Shouldn't have reached here");
    }
    return result;
}

