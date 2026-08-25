/*!
\copyright  Copyright (c) 2008 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_pairing_request_state.c
\brief      Fast Pair Wait for Pairing Request State Event handling
*/


#include "fast_pair_wait_pairing_request_state.h"
#include "fast_pair_wait_aes_key_state.h"
#include "fast_pair_pairing_if.h"
#include "fast_pair_gfps.h"
#include "fast_pair_events.h"
#include "fast_pair_advertising.h"
#include "fast_pair_wait_additional_data_state.h"
#include "cryptoalgo.h"
#include "fast_pair_bloom_filter.h"

static bool fastPair_SendKbPResponse(fast_pair_state_event_crypto_encrypt_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastPair_SendKbPResponse");

    if(args->crypto_encrypt_cfm->status == success)
    {
        uint8* encrypted_data;
        uint8 *big_endian_enc_data = PanicUnlessMalloc(FAST_PAIR_ENCRYPTED_RESPONSE_LEN);

        encrypted_data = (uint8 *)args->crypto_encrypt_cfm->encrypted_data;

        theFastPair = fastPair_GetTaskData();

        /* FP Seeker expects data to be in big endian format, convert it */
        fastPair_ConvertEndiannessFormat(encrypted_data, FAST_PAIR_ENCRYPTED_RESPONSE_LEN, big_endian_enc_data);

        fastPair_SendFPNotification(FAST_PAIR_KEY_BASED_PAIRING, big_endian_enc_data);
        free(big_endian_enc_data);

        /* If personalized name is requested in KBP request, prepare and send encrypted Additional data to seeker */
        if(TRUE == theFastPair->session_data.kbp_pname_request_flag)
        {
            if(TRUE == fastPair_GetEncryptedAdditionalDataHavingPName())
            {
                DEBUG_LOG("Personalized name is requested in KBP request, Sending encrypted additional data to seeker...");
            }
            else
            {
                DEBUG_LOG("Couldn't get encrypted additional data having pname.");
            }
        }

        if(!fastPair_IsProviderPairingRequested())
        {
            fastPair_StartPairing();
        }
        status = TRUE;
    }
    return status;
}

static bool fastPair_HandlePairingRequest(bool* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;
    
    DEBUG_LOG("fastPair_HandlePairingRequest");

    theFastPair = fastPair_GetTaskData();

    /* If TRUE, the Pairing request was receieved with I/O Capapbailities set to 
       DisplayKeyboard or DisplayYes/No. If False Remote I/O Capabalities was set
       to No Input No Output
     */
    if(*args == TRUE)
    {        
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_WAIT_PASSKEY);
        status = TRUE;
    }
    else
    {    
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
    }
    return status;
}

static bool fastpair_StateWaitPairingRequestHandleAuthCfm(fast_pair_state_event_auth_args_t* args)
{
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();

    if(args->auth_cfm->status == auth_status_success)
    {
        DEBUG_LOG("fastpair_StateWaitPairingRequestHandleAuthCfm. CL_SM_AUTHENTICATE_CFM status %d", args->auth_cfm->status);
        fastPair_AdvNotifyChangeInIdentifiable(FALSE);

        /* After setting the identifiable parameter to unidentifiable, Set the FP state to idle */
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);

        return TRUE;
    }
    return FALSE;
}

static bool fastpair_StateWaitPairingRequestProcessACLDisconnect(fast_pair_state_event_disconnect_args_t* args)
{
    bool status = FALSE;
    uint8 index;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastpair_StateWaitPairingRequestProcessACLDisconnect");

    theFastPair = fastPair_GetTaskData();

    if(args->disconnect_ind->tpaddr.transport == TRANSPORT_BLE_ACL)
    {
        memset(&theFastPair->rpa_bd_addr, 0x0, sizeof(bdaddr));

        for(index = 0; index < MAX_BLE_CONNECTIONS; index++)
        {
            if(BdaddrIsSame(&theFastPair->peer_bd_addr[index], &args->disconnect_ind->tpaddr.taddr.addr))
            {
                DEBUG_LOG("fastpair_StateWaitPairingRequestProcessACLDisconnect. Reseting peer BD address and own RPA of index %x", index);
                memset(&theFastPair->peer_bd_addr[index], 0x0, sizeof(bdaddr));
                memset(&theFastPair->own_random_address[index], 0x0, sizeof(bdaddr));

                /* If the disconnecting device is not a peer earbud i.e. FP seeker, move to idle state. */
                if(FALSE == BtDevice_LeDeviceIsPeer(&(args->disconnect_ind->tpaddr)))
                {
                    DEBUG_LOG("fastpair_StateWaitPairingRequestProcessACLDisconnect: Remote device closed the connection. Moving to FAST_PAIR_STATE_IDLE ");
                    fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
                }
            }
        }
        status = TRUE;
    }
    return status;
}

/* AES CTR Encrypt/Decrypt Event Handler */
static bool fastPair_StateWaitPairingRequestAesCtrEventHandler(fast_pair_state_event_crypto_aes_ctr_args_t* args)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;
    uint8 hmac_sha256_out[SHA256_DIGEST_SIZE];

    DEBUG_LOG("fastPair_StateWaitPairingRequestAesCtrEventHandler");

    theFastPair = fastPair_GetTaskData();

    if(args->crypto_aes_ctr_cfm->status == success)
    {
        uint8 *encrypted_data = (uint8 *)args->crypto_aes_ctr_cfm->data;
        uint8 *key_index = (uint8 *)theFastPair->session_data.aes_key;
        uint8 *key_index_hmac_sha256 = PanicUnlessMalloc(FAST_PAIR_AES_KEY_LEN);
        uint8 *big_endian_enc_data = PanicUnlessMalloc(theFastPair->pname_add_data.pname_length);
        uint8 *nonce_big_endian = PanicUnlessMalloc(FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX * 2);

        /* Convert the input data to big endian before processing it to hmac-sha256 */
        if(theFastPair->pname_add_data.pname_length%2 == 0)
        {
            fastPair_ConvertEndiannessFormat(encrypted_data, theFastPair->pname_add_data.pname_length, big_endian_enc_data);
        }
        else
        {
            /* Ignore the first octet as we had appended 0x00 just to make a complete word */
            fastPair_ConvertEndiannessFormat((encrypted_data + 1), theFastPair->pname_add_data.pname_length, big_endian_enc_data);
        }
        fastPair_ConvertEndiannessFormat(key_index, FAST_PAIR_AES_KEY_LEN, key_index_hmac_sha256);
        fastPair_ConvertEndiannessFormat(theFastPair->pname_add_data.nonce, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX, nonce_big_endian);

        hmac_sha256(big_endian_enc_data, theFastPair->pname_add_data.pname_length, hmac_sha256_out, key_index_hmac_sha256, nonce_big_endian, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX);

        /* Copy first 8 bytes of hmac_sha256_out */
        memcpy(theFastPair->pname_add_data.encr_add_data_pname, hmac_sha256_out, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX);
        /* Copy the encrypted data after copying 8 bytes of hmac sha256 output and 8 bytes of nonce used in AES CTR encryption */
        memcpy(&(theFastPair->pname_add_data.encr_add_data_pname[FAST_PAIR_ADD_DATA_PACKET_DATA_INDEX]), big_endian_enc_data, theFastPair->pname_add_data.pname_length);

        fastPair_SendFPAdditionalDataNotification(theFastPair->pname_add_data.encr_add_data_pname, theFastPair->pname_add_data.add_data_length);

        free(key_index_hmac_sha256);
        free(big_endian_enc_data);
        free(nonce_big_endian);
        free(theFastPair->pname_add_data.nonce);

        status = TRUE;
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

bool fastPair_StateWaitPairingRequestHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_StateWaitPairingRequestHandleEvent event [%d]", event.id);
    /* Return if event is related to handset connection allowed/disallowed and is handled */
    if(fastPair_HandsetConnectStatusChange(event.id))
    {
        return TRUE;
    }

    switch (event.id)
    {
        case fast_pair_state_event_timer_expire:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;

        case fast_pair_state_event_crypto_encrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastPair_SendKbPResponse((fast_pair_state_event_crypto_encrypt_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_pairing_request:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            else
            {
                status = fastPair_HandlePairingRequest((bool *)event.args);
            }
        }
        break;

        case fast_pair_state_event_power_off:
        {
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;

        case fast_pair_state_event_provider_initiate_pairing:
        {
            fastPair_InitiateFastPairing();
        }
        break;

        case fast_pair_state_event_auth:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StateWaitPairingRequestHandleAuthCfm((fast_pair_state_event_auth_args_t *) event.args);
        }
        break;

        case fast_pair_state_event_disconnect:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StateWaitPairingRequestProcessACLDisconnect((fast_pair_state_event_disconnect_args_t*)event.args);
        }
        break;

        case fast_pair_state_event_crypto_aes_ctr:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            DEBUG_LOG("fastPair_StateWaitPairingRequestHandleEvent. fast_pair_state_event_crypto_aes_ctr");
            status = fastPair_StateWaitPairingRequestAesCtrEventHandler((fast_pair_state_event_crypto_aes_ctr_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_hash:
        {
            status = fastPair_HandleAdvBloomFilterCalc((fast_pair_state_event_crypto_hash_args_t *)event.args);
        }
        break;

        default:
        {
            DEBUG_LOG("Unhandled event [%d]", event.id);
        }
        break;
    }
    return status;
}
