/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_account_key_state.c
\brief      Fast Pair Wait for Account Key State Event handling
*/


#include "fast_pair_wait_account_key_state.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair_session_data.h"
#include "fast_pair_account_key_sync.h"
#include "fast_pair_events.h"
#include "fast_pair_gfps.h"
#include "user_accounts.h"

static bool fastPair_SendKbPResponseRetroactively(fast_pair_state_event_crypto_encrypt_args_t* args)
{
    bool status = FALSE;

    DEBUG_LOG("fastPair_SendKbPResponseRetroactively");

    if(args->crypto_encrypt_cfm->status == success)
    {
        uint8* encrypted_data;
        /* FP Seeker expects data to be in big endian format */
        uint8 *big_endian_enc_data = PanicUnlessMalloc(FAST_PAIR_ENCRYPTED_RESPONSE_LEN);

        encrypted_data = (uint8 *)args->crypto_encrypt_cfm->encrypted_data;

        /* Convert the little endian KbP response data to big endian format as FP seeker expects the response in big endian format. */
        fastPair_ConvertEndiannessFormat(encrypted_data, FAST_PAIR_ENCRYPTED_RESPONSE_LEN, big_endian_enc_data);

        fastPair_SendFPNotification(FAST_PAIR_KEY_BASED_PAIRING, big_endian_enc_data);
        free(big_endian_enc_data);

        status = TRUE;
    }
    return status;
}

static bool fastPair_ValidateAccountKey(fast_pair_state_event_crypto_decrypt_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

    if (args->crypto_decrypt_cfm->status == success)
    {
        uint8* decrypted_data;
        uint8 *big_endian_decr_data = PanicUnlessMalloc(FAST_PAIR_ACCOUNT_KEY_LEN);
        uint16 added_acc_key_index;

        decrypted_data = (uint8 *)args->crypto_decrypt_cfm->decrypted_data;

        /* Convert the decrypted data to big endian format before validating or storing */
        fastPair_ConvertEndiannessFormat(decrypted_data, FAST_PAIR_ACCOUNT_KEY_LEN, big_endian_decr_data);

        added_acc_key_index = UserAccounts_AddAccountKey(big_endian_decr_data, FAST_PAIR_ACCOUNT_KEY_LEN, user_account_type_fast_pair);
        /* If the index of the added account key is valid, associate the account key to the handset device */
        if(added_acc_key_index >= 0 && added_acc_key_index < MAX_NUM_ACCOUNT_KEYS)
        {
            bdaddr handset_public_addr;
            bool account_key_association = FALSE;
            DEBUG_LOG("fastPair_ValidateAccountKey: Index of the added Account Key - %d", added_acc_key_index);

            memcpy(&handset_public_addr, &theFastPair->handset_bd_addr, sizeof(bdaddr));

            if(!BdaddrIsZero(&handset_public_addr))
            {
                account_key_association = UserAccounts_AssociateAccountKeyWithHandset(big_endian_decr_data, FAST_PAIR_ACCOUNT_KEY_LEN, &handset_public_addr);
            }
            else
            {
                DEBUG_LOG_ERROR("Handset Address is Zero. Unexpected Outcome. Shouldn't have reached here");
            }

            if(account_key_association == TRUE)
            {
                DEBUG_LOG("Account Key Association took place successfully.");
            }
            else
            {
                DEBUG_LOG("Failed to associate the account key with the handset device.");
            }
        }

        free(big_endian_decr_data);

        /*! Account Key Sharing */
        AccountKeySync_Sync();

        /* Regenerate New Account Key Filter */
        fastPair_AccountKeyAdded();

        status = TRUE;
    }

    /* Set Fast Pair state to Wait for additional data */
    fastPair_SetState(theFastPair, FAST_PAIR_STATE_WAIT_ADDITIONAL_DATA);

    return status;

}

static bool fastpair_StateWaitAccountKeyProcessACLDisconnect(fast_pair_state_event_disconnect_args_t* args)
{
    bool status = FALSE;
    uint8 index;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastpair_StateWaitAccountKeyProcessACLDisconnect");

    theFastPair = fastPair_GetTaskData();

    if(args->disconnect_ind->tpaddr.transport == TRANSPORT_BLE_ACL)
    {
        memset(&theFastPair->rpa_bd_addr, 0x0, sizeof(bdaddr));

        for(index = 0; index < MAX_BLE_CONNECTIONS; index++)
        {
            if(BdaddrIsSame(&theFastPair->peer_bd_addr[index], &args->disconnect_ind->tpaddr.taddr.addr))
            {
                DEBUG_LOG("fastpair_StateWaitAccountKeyProcessACLDisconnect. Reseting peer BD address and own RPA of index %x", index);
                memset(&theFastPair->peer_bd_addr[index], 0x0, sizeof(bdaddr));
                memset(&theFastPair->own_random_address[index], 0x0, sizeof(bdaddr));

                /* If the disconnecting device is not a peer earbud i.e. FP seeker, move to idle state. */
                if(FALSE == BtDevice_LeDeviceIsPeer(&(args->disconnect_ind->tpaddr)))
                {
                    DEBUG_LOG("fastpair_StateWaitAccountKeyProcessACLDisconnect: Remote device closed the connection. Moving to FAST_PAIR_STATE_IDLE ");
                    fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
                }
            }
        }
        status = TRUE;
    }
    return status;
}

static bool fastpair_AccountKeyWriteEventHandler(fast_pair_state_event_account_key_write_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    
    if(args->enc_data != NULL)
    {
        uint8 *encrypted_data = (uint8 *)args->enc_data;
        uint8 *little_endian_enc_data = PanicUnlessMalloc(FAST_PAIR_ACCOUNT_KEY_LEN);

        /* Convert the encryoted data to the little endian format before processing it to AES decrypt API */
        fastPair_ConvertEndiannessFormat(encrypted_data, FAST_PAIR_ACCOUNT_KEY_LEN, little_endian_enc_data);

        /* Decrypt Account Key */
        ConnectionDecryptBlockAes(&theFastPair->task, (uint16 *)little_endian_enc_data, theFastPair->session_data.aes_key);

        status = TRUE;
        free(little_endian_enc_data);
    }

    return status;
}

static bool fastPair_HandleAdvBloomFilterCalc(fast_pair_state_event_crypto_hash_args_t* args)
{
    DEBUG_LOG("fastPair_HandleAdvBloomFilterCalc");

    if(args->crypto_hash_cfm->status == success)
    {
        fastPair_AdvHandleHashCfm(args->crypto_hash_cfm);
        return TRUE;
    }
    return FALSE;
}

bool fastPair_StateWaitAccountKeyHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_StateWaitAccountKeyHandleEvent event [%d]", event.id);
    /* Return if event is related to handset connection allowed/disallowed and is handled */
    if(fastPair_HandsetConnectStatusChange(event.id))
    {
        return TRUE;
    }

    switch (event.id)
    {
        case fast_pair_state_event_disconnect:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StateWaitAccountKeyProcessACLDisconnect((fast_pair_state_event_disconnect_args_t*)event.args);
        }
        break;
        
        case fast_pair_state_event_timer_expire:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
            status = TRUE;
        }
        break;

        case fast_pair_state_event_crypto_encrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_SendKbPResponseRetroactively((fast_pair_state_event_crypto_encrypt_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_decrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_ValidateAccountKey((fast_pair_state_event_crypto_decrypt_args_t *)event.args);
        }
        break;
        

        case fast_pair_state_event_account_key_write:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_AccountKeyWriteEventHandler((fast_pair_state_event_account_key_write_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_hash:
        {
            status = fastPair_HandleAdvBloomFilterCalc((fast_pair_state_event_crypto_hash_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_power_off:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;
        
        default:
        {
            DEBUG_LOG("Unhandled event [%d]\n", event.id);
        }
        break;
    }
    return status;
}
