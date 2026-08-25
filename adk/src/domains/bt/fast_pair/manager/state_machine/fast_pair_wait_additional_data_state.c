/*!
\copyright  Copyright (c) 2008 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_additional_data_state.c
\brief      Fast Pair Wait for Additional Data State Event handling
*/

#include <stdlib.h>
#include "fast_pair_wait_additional_data_state.h"
#include "fast_pair_session_data.h"
#include "fast_pair_events.h"
#include "fast_pair_gfps.h"
#include "fast_pair_wait_account_key_state.h"
#include "fast_pair_wait_aes_key_state.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair_pname_state.h"
#include "cryptoalgo.h"

static bool fastpair_StateAdditionalDataProcessACLDisconnect(fast_pair_state_event_disconnect_args_t* args)
{
    bool status = FALSE;
    uint8 index;
    fastPairTaskData *theFastPair;

    DEBUG_LOG("fastpair_StateAdditionalDataProcessACLDisconnect");

    theFastPair = fastPair_GetTaskData();

    if(args->disconnect_ind->tpaddr.transport == TRANSPORT_BLE_ACL)
    {
        memset(&theFastPair->rpa_bd_addr, 0x0, sizeof(bdaddr));

        for(index = 0; index < MAX_BLE_CONNECTIONS; index++)
        {
            if(BdaddrIsSame(&theFastPair->peer_bd_addr[index], &args->disconnect_ind->tpaddr.taddr.addr))
            {
                DEBUG_LOG("fastpair_StateAdditionalDataProcessACLDisconnect. Reseting peer BD address and own RPA of index %x", index);
                memset(&theFastPair->peer_bd_addr[index], 0x0, sizeof(bdaddr));
                memset(&theFastPair->own_random_address[index], 0x0, sizeof(bdaddr));

                /* If the disconnecting device is not a peer earbud i.e. FP seeker, move to idle state. */
                if(FALSE == BtDevice_LeDeviceIsPeer(&(args->disconnect_ind->tpaddr)))
                {
                    DEBUG_LOG("fastpair_StateAdditionalDataProcessACLDisconnect: Remote device closed the connection. Moving to FAST_PAIR_STATE_IDLE ");
                    fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
                }
            }
        }
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

/* Handle Personalized Name Write */
static bool fastPair_HandlePnameWrite(void)
{
    fast_pair_additional_data_type data_type = FAST_PAIR_ADDITIONAL_DATA_TYPE_NONE;
    fastPairTaskData *theFastPair = fastPair_GetTaskData();
    bool status = FALSE;
    fastPairState prev_state = theFastPair->prev_state;

    /* For naming flow2, only if the DataID is 0x01 in KBP packet declare the data as for personalized name */
    if(FAST_PAIR_STATE_WAIT_AES_KEY == prev_state)
    {
        if(FAST_PAIR_DEVICE_ACTION_REQ_DATA_ID_PNAME == theFastPair->session_data.kbp_action_request_data_id)
        {
            data_type = FAST_PAIR_ADDITIONAL_DATA_TYPE_PNAME;
        }
        else
        {
            DEBUG_LOG_ERROR("fastPair_HandlePnameWrite: Unsupported Data ID %d",theFastPair->session_data.kbp_action_request_data_id);
            status = FALSE;
        }
    }
    else /* Naming flow1 i.e. (FAST_PAIR_STATE_WAIT_ACCOUNT_KEY == prev_state) */
    {
        data_type = FAST_PAIR_ADDITIONAL_DATA_TYPE_PNAME;
    }

    /* If the data type of additional data is personalized name, move to PNAME state */
    if(data_type == FAST_PAIR_ADDITIONAL_DATA_TYPE_PNAME)
    {
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_PNAME);
        DEBUG_LOG("fastPair_HandlePnameWrite: Calling fastPair_PNameWrite ");
        /* Decoding done. Pass the data to pname. */
        fastPair_PNameWrite(theFastPair->pname_add_data.data, theFastPair->pname_add_data.data_size);
        status = TRUE;
    }
    else
    {
         DEBUG_LOG_ERROR("fastPair_HandlePnameWrite: UNKNOWN data type. Should not have come here ");
         status = FALSE;
    }

    /* Everything done. Move to idle state */
    DEBUG_LOG("fastPair_HandlePnameWrite . Moving to FAST_PAIR_STATE_IDLE ");
    fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);

    return status;
}

/* AES CTR Event Handler */
static bool fastPair_AesCtrEventHandler(fast_pair_state_event_crypto_aes_ctr_args_t* args)
{
    bool status = FALSE;

    DEBUG_LOG("fastPair_AesCtrEventHandler");

    if(args->crypto_aes_ctr_cfm->status == success)
    {
        fastPairTaskData *theFastPair;
        theFastPair = fastPair_GetTaskData();
        theFastPair->pname_add_data.data = (uint8 *)args->crypto_aes_ctr_cfm->data;

        status = TRUE;
    }

    if(status == TRUE)
    {
        if(fastPair_HandlePnameWrite() == TRUE)
        {
            DEBUG_LOG("Personalized name written successfully.");
        }
        else
        {
            DEBUG_LOG("Couldn't write personalized name.");
        }
    }

    return status;
}

/* Take the decision how to process the additional data. Right now, it is only Personalized name. */
static bool fastpair_AdditionalDataWriteEventHandler(fast_pair_state_event_additional_data_write_args_t* args)
{
    fastPairTaskData *theFastPair;
    fastPairState prev_state;
    uint8 *data_index_hmac_sha256;
    uint8 *nonce_index_hmac_sha256;
    uint8 *key_index_aes_ctr;
    uint8 data_sz_words;
    uint8 hmac_sha256_out[SHA256_DIGEST_SIZE];
    bool status = FALSE;

    DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler called ");

    theFastPair = fastPair_GetTaskData();

    if((NULL == args->enc_data)||(args->size <= FAST_PAIR_ADD_DATA_PACKET_DATA_INDEX))
    {
        DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler: Error- No pname data or wrong input data ");
        fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        return FALSE;
    }

    prev_state = theFastPair->prev_state;

    key_index_aes_ctr = (uint8 *)theFastPair->session_data.aes_key;
    nonce_index_hmac_sha256 = (uint8 *)(args->enc_data + FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX);
    data_index_hmac_sha256 = (uint8 *)(args->enc_data+FAST_PAIR_ADD_DATA_PACKET_DATA_INDEX);
    theFastPair->pname_add_data.data_size = args->size-FAST_PAIR_ADD_DATA_PACKET_DATA_INDEX;

    /* If Additional data is sent via naming flows 1 or 2 */
    if((FAST_PAIR_STATE_WAIT_ACCOUNT_KEY == prev_state)|| /* Naming flow-1 via inital pairing */
            (FAST_PAIR_STATE_WAIT_AES_KEY == prev_state)) /* Naming flow-2 via KBP Action Request*/
    {
        uint8 *key_index_hmac_sha256 = PanicUnlessMalloc(FAST_PAIR_AES_KEY_LEN);

        DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler: Prev state is %d. So it is in naming flow-1/2",prev_state);

        /* AES key should be in big endian format for HMAC-SHA256 */
        fastPair_ConvertEndiannessFormat(key_index_aes_ctr, FAST_PAIR_AES_KEY_LEN, key_index_hmac_sha256);

        hmac_sha256(data_index_hmac_sha256, theFastPair->pname_add_data.data_size, hmac_sha256_out, key_index_hmac_sha256, nonce_index_hmac_sha256, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX);
        free(key_index_hmac_sha256);

        /* Verify first 8 bytes of data with hmac_sha256_out. They should match. */
        if(memcmp(args->enc_data, hmac_sha256_out, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX)!=0)
        {
            DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler: HMAC sha256 of decoded data does not match with the one in input data.");
            status = FALSE;
        }
        else
        {
            DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler: Hmac sha256 output is matching with input data");
            status = TRUE;
        }

        /* Proceed further only if HMAC-SHA256 output is matching with input data */
        if(status == TRUE)
        {
            bool odd_length_pname = FALSE;
            if(theFastPair->pname_add_data.data_size % 2 == 0)
            {
                data_sz_words = theFastPair->pname_add_data.data_size/2;
            }
            else
            {
                data_sz_words = (theFastPair->pname_add_data.data_size + 1)/2;
                odd_length_pname = TRUE;
            }

            /* Input data must be in little endian format to process it in AES CTR API */
            uint8 *data_index_aes_ctr = PanicUnlessMalloc(data_sz_words * 2);
            uint8 *nonce_index_aes_ctr = PanicUnlessMalloc(FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX * 2);

            fastPair_ConvertEndiannessFormat(nonce_index_hmac_sha256, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX, nonce_index_aes_ctr);

            /* Nonce index must be 128 bits, Set ZERO in last 8 bytes */
            memset(nonce_index_aes_ctr + FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX, 0x00, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX);

            if(odd_length_pname)
            {
                /* Extra byte padding to make a complete word in case of odd size */
                data_index_aes_ctr[0] = 0x00;
                fastPair_ConvertEndiannessFormat(data_index_hmac_sha256, theFastPair->pname_add_data.data_size, (data_index_aes_ctr + 1));
            }
            else
            {
                fastPair_ConvertEndiannessFormat(data_index_hmac_sha256, theFastPair->pname_add_data.data_size, data_index_aes_ctr);
            }

            ConnectionEncryptDecryptAesCtrReq(&theFastPair->task, 0, cl_aes_ctr_big_endian, (uint16 *)key_index_aes_ctr, (uint16 *)nonce_index_aes_ctr, data_sz_words, (uint16 *)data_index_aes_ctr);

            free(nonce_index_aes_ctr);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("fastpair_AdditionalDataWriteEventHandler-ERROR: Came here from unexpected previous state %d",prev_state);
        status = FALSE;
    }

    return status;
}


static void generateNonce(uint8 nonce[AES_CTR_NONCE_SIZE])
{
    int valid_nonce_idx,i;
    uint8 rand1;

    nonce[0] = UtilRandom()&0xFF;
    valid_nonce_idx = 1;
    while(valid_nonce_idx < AES_CTR_NONCE_SIZE)
    {
        rand1 = UtilRandom()&0xFF;
        /* Check for duplicates */
        for(i=0;i<valid_nonce_idx;i++)
        {
            if(rand1 == nonce[i])
                 break;
        }
        /* No duplicates found*/
        if(i == valid_nonce_idx)
        {
            nonce[valid_nonce_idx] = rand1;
            valid_nonce_idx++;
        }
    }
}

/* Get Encrypted Additional Data with PName */
bool fastPair_GetEncryptedAdditionalDataHavingPName(void)
{
    uint8 pname[FAST_PAIR_PNAME_DATA_LEN];
    uint8 pname_len;
    fastPairTaskData *theFastPair = fastPair_GetTaskData();
    uint8 *nonce_index;

    if(FALSE == fastPair_GetPName(pname,&pname_len))
    {
        DEBUG_LOG("fastPair_GetEncryptedAdditionalDataHavingPName: Error in getting stored PName ");
        return FALSE;
    }

    /* If the length is 0 bytes*/
    if(0==pname_len)
    {
        DEBUG_LOG("fastPair_GetEncryptedAdditionalDataHavingPName: PName is empty");
        return FALSE;
    }
    /* If the length is more than allowed. Should not happen*/
    if(pname_len > FAST_PAIR_PNAME_DATA_LEN)
    {
        DEBUG_LOG_ERROR("fastPair_GetEncryptedAdditionalDataHavingPName: SHOULD NOT HAPPEN. PName length %d is more than allowed %d",pname_len,FAST_PAIR_PNAME_DATA_LEN);
        return FALSE;
    }

    nonce_index = theFastPair->pname_add_data.encr_add_data_pname + FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX;
    generateNonce(nonce_index);

    uint8 *nonce_index_aes_ctr = PanicUnlessMalloc(FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX * 2);
    uint8 pname_len_words;
    bool odd_length_pname = FALSE;

    if(pname_len % 2 == 0)
    {
        pname_len_words = pname_len/2;
    }
    else
    {
        pname_len_words = pname_len/2 + 1;
        odd_length_pname = TRUE;
    }

    uint8 *pname_data = PanicUnlessMalloc(pname_len_words * 2);
    /*Convert the big endian nonce index to little endian before processing it to AES CTR*/
    fastPair_ConvertEndiannessFormat(nonce_index, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX, nonce_index_aes_ctr);

    /* Nonce index must be 128 bits, Set ZERO in last 8 bytes */
    memset(nonce_index_aes_ctr + FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX, 0x00, FAST_PAIR_ADD_DATA_PACKET_NONCE_INDEX);
    theFastPair->pname_add_data.nonce = nonce_index_aes_ctr;

    if(odd_length_pname)
    {
        /* Extra byte padding to make a complete word in case of odd size */
        pname_data[0] = 0x00;
        memcpy((pname_data + 1), pname, pname_len);
    }
    else
    {
        memcpy(pname_data, pname, pname_len);
    }

    ConnectionEncryptDecryptAesCtrReq(&theFastPair->task, 0, cl_aes_ctr_big_endian, (uint16 *)theFastPair->session_data.aes_key, (uint16 *)theFastPair->pname_add_data.nonce, pname_len_words, (uint16 *) pname_data);

    theFastPair->pname_add_data.add_data_length = pname_len + FAST_PAIR_ADD_DATA_PACKET_DATA_INDEX;
    theFastPair->pname_add_data.pname_length = pname_len;

    return TRUE;
}

bool fastPair_StateWaitAdditionalDataHandleEvent(fast_pair_state_event_t event)
{
    bool status = FALSE;
    fastPairTaskData *theFastPair;

    theFastPair = fastPair_GetTaskData();
    DEBUG_LOG("fastPair_StateAdditionalDataHandleEvent event [%d]", event.id);

    switch (event.id)
    {
        case fast_pair_state_event_disconnect:
        {
            if(event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_StateAdditionalDataProcessACLDisconnect((fast_pair_state_event_disconnect_args_t*)event.args);
        }
        break;

        case fast_pair_state_event_timer_expire:
        {
            DEBUG_LOG("fastPair_StateWaitAdditionalDataHandleEvent: Moving to FAST_PAIR_STATE_IDLE ");
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
            status = TRUE;
        }
        break;

        case fast_pair_state_event_additional_data_write:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            status = fastpair_AdditionalDataWriteEventHandler((fast_pair_state_event_additional_data_write_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_crypto_hash:
        {
            fastPair_HandleAdvBloomFilterCalc((fast_pair_state_event_crypto_hash_args_t *)event.args);
        }
        break;
        case fast_pair_state_event_crypto_encrypt:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            fast_pair_state_event_crypto_encrypt_args_t* args= (fast_pair_state_event_crypto_encrypt_args_t *)event.args;
            uint16 result;
            result = args->crypto_encrypt_cfm->status;

            DEBUG_LOG("fastPair_SendFPNotification for FAST_PAIR_KEY_BASED_PAIRING");
            if(result == success)
            {
                uint8* encrypted_data;
                /* FP Seeker expects data to be in big endian format */
                uint8 *big_endian_enc_data = PanicUnlessMalloc(FAST_PAIR_ENCRYPTED_RESPONSE_LEN);
                uint8 index;

                encrypted_data = (uint8 *)args->crypto_encrypt_cfm->encrypted_data;

                for(index = 0; index < FAST_PAIR_ENCRYPTED_RESPONSE_LEN; index++)
                {
                    big_endian_enc_data[index] = encrypted_data[FAST_PAIR_ENCRYPTED_RESPONSE_LEN - index - 1];
                }

                fastPair_SendFPNotification(FAST_PAIR_KEY_BASED_PAIRING, big_endian_enc_data);
                free(big_endian_enc_data);
            }
        }
        break;

        case fast_pair_state_event_crypto_aes_ctr:
        {
            if (event.args == NULL)
            {
                return FALSE;
            }
            DEBUG_LOG("fast_pair_state_event_crypto_aes_ctr");
            status = fastPair_AesCtrEventHandler((fast_pair_state_event_crypto_aes_ctr_args_t *)event.args);
        }
        break;

        case fast_pair_state_event_power_off:
        {
            DEBUG_LOG("fastpair_AdditionalDataWriteEventHandler: Moving to FAST_PAIR_STATE_IDLE ");
            fastPair_SetState(theFastPair, FAST_PAIR_STATE_IDLE);
        }
        break;

        default:
        {
            DEBUG_LOG("fastPair_StateWaitAdditionalDataHandleEvent: Unhandled event [%d]\n", event.id);
        }
        break;
    }
    return status;
}
