/*!
\copyright  Copyright (c) 2018-20 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_session_data.c
\brief      Fast pairing module device database access
*/

#include "fast_pair_session_data.h"

#include <rsa_pss_constants.h>
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
#include "fast_pair_account_key_sync.h"
#include "fast_pair_config.h"


#define FAST_PAIR_PRIVATE_KEY_LEN               (32)



/* Fast Pair seed */
const uint16 seed[FAST_PAIR_PRIVATE_KEY_LEN/2] = {0x11ac, 0x5a6e, 0x0e49, 0x5aa3, 0xe3e0, 0xbb20, 0xac0e, 0xf136, 0x5dfb, 0x5282, 0x002b, 0x37f2, 0x28f1,
0xd18c, 0xa613, 0x8de2};
/* Fast Pair scrambled anti spoofing private key */
const uint16 *private_key;

static void fastpair_serialise_persistent_device_data(device_t device, void *buf, uint8 offset)
{

    void *pname_value = NULL;
    size_t pname_size = 0;
    UNUSED(offset);

    /* Store pname data to PS store */
    if(Device_GetProperty(device, device_property_fast_pair_personalized_name, &pname_value, &pname_size) )
    {
        memcpy(buf, pname_value,sizeof(uint8)*FAST_PAIR_PNAME_STORAGE_LEN);
    }
    else
    {
        DEBUG_LOG("fastpair_serialise_persistent_device_data: Device_GetProperty(device_property_fast_pair_personalized_name) fails ");
    }
}

static void fastpair_deserialise_persistent_device_data(device_t device, void *buf, uint8 data_length, uint8 offset)
{
    UNUSED(offset);
    UNUSED(data_length);
    /* PS retrieve data to device database */
    Device_SetProperty(device, device_property_fast_pair_personalized_name, buf, sizeof(uint8)*FAST_PAIR_PNAME_STORAGE_LEN);
 }

static uint8 fastpair_get_device_data_len(device_t device)
{
    void *pname_value = NULL;
    size_t pname_size = 0;
    uint8 fast_pair_device_data_len = 0;

    if(DEVICE_TYPE_SELF == BtDevice_GetDeviceType(device))
    {
       if (Device_GetProperty(device, device_property_fast_pair_personalized_name, &pname_value, &pname_size))
       {
           PanicFalse((sizeof(uint8) * FAST_PAIR_PNAME_STORAGE_LEN) == pname_size);
           fast_pair_device_data_len += sizeof(uint8) * FAST_PAIR_PNAME_STORAGE_LEN;
       }
       return fast_pair_device_data_len;
    }
    return 0;
}


/*! \brief Register Fast Pair PDDU
 */
void FastPair_RegisterPersistentDeviceDataUser(void)
{
    DeviceDbSerialiser_RegisterPersistentDeviceDataUser(
        PDDU_ID_FAST_PAIR,
        fastpair_get_device_data_len,
        fastpair_serialise_persistent_device_data,
        fastpair_deserialise_persistent_device_data);
}

void fastPair_SetPrivateKey(const uint16 *key, unsigned size_of_key)
{
    PanicNull((uint16 *)key);
    PanicFalse(size_of_key >= FAST_PAIR_PRIVATE_KEY_LEN/2);

    private_key = key;
}

/*! \brief Get the Fast Pair anti spoofing private key
 */
void fastPair_GetAntiSpoofingPrivateKey(uint8* aspk)
{
    uint16* unscrambled_aspk = PanicUnlessMalloc(FAST_PAIR_PRIVATE_KEY_LEN);

    for(uint16 i=0, j=FAST_PAIR_PRIVATE_KEY_LEN/2; i <(FAST_PAIR_PRIVATE_KEY_LEN/2) && j > 0; i++,j--)
    {
		/* Use last 16 words of M array in rsa_decrypt_constant_mod structure */
        unscrambled_aspk[i] = private_key[i] ^ rsa_decrypt_constant_mod.M[RSA_SIGNATURE_SIZE - j] ^ seed[i];
    }
    ByteUtilsMemCpyUnpackString(aspk, (const uint16 *)&unscrambled_aspk[0], FAST_PAIR_PRIVATE_KEY_LEN);

    DEBUG_LOG("Fastpair session data : Unscrambled ASPK : ");
    for(uint16 j=0; j<FAST_PAIR_PRIVATE_KEY_LEN; j++)
    {
        DEBUG_LOG("%02x ", aspk[j]);
    }
    free(unscrambled_aspk);
}


/*****      PERSONALIZED NAME      *****/

/*! \brief Store the personalized name in persistent storage.
 */
bool fastPair_StorePNameInPSStore(const uint8 pname[FAST_PAIR_PNAME_STORAGE_LEN])
{
    device_t my_device = BtDevice_GetSelfDevice();

    if(NULL == my_device)
    {
        DEBUG_LOG_ERROR("fastPair_StorePName : Unexpected Error. Shouldn't have reached here");
        return FALSE;
    }

    /* Store personalized name to PS Store */
    Device_SetProperty(my_device, device_property_fast_pair_personalized_name, pname, sizeof(uint8)*FAST_PAIR_PNAME_STORAGE_LEN);
    DeviceDbSerialiser_Serialise();

    return TRUE;
}

/*! \brief Store the personalized name of length pname_len in persistent storage.
 */
bool fastPair_StorePName(const uint8 *pname, uint8 pname_len)
{
    uint8 buffer[FAST_PAIR_PNAME_STORAGE_LEN];
    if((NULL == pname)||(0 == pname_len))
    {
        DEBUG_LOG_ERROR("fastPair_StorePName: pname is null or length of pname is zero.");
        return FALSE;
    }
    else if(pname_len > FAST_PAIR_PNAME_DATA_LEN)
    {
        buffer[0] = FAST_PAIR_PNAME_DATA_LEN;
        DEBUG_LOG("fastPair_StorePName: Pname length - %d which is more than 64 bytes, truncate it to 64 bytes.",pname_len);
        memcpy(buffer+(uint8)sizeof(uint8), pname, FAST_PAIR_PNAME_DATA_LEN);
    }
    else
    {
        buffer[0]=pname_len;
        memcpy(buffer+(uint8)sizeof(uint8),pname,pname_len*sizeof(uint8));
    }
    fastPair_StorePNameInPSStore(buffer);
    return TRUE;
}

/*! \brief Read the personalized name from persistent storage.
 */
bool fastPair_GetPNameFromStore(uint8 pname[FAST_PAIR_PNAME_DATA_LEN], uint8 *pname_len)
{
    size_t pname_ps_size = 0;
    void *pname_ps_ptr=NULL;
    uint8 *pname_ptr=NULL;

    if(NULL == pname_len)
    {
        DEBUG_LOG_ERROR("fastPair_GetPNameFromStore : pname_len input parameter is NULL");
        return FALSE;
    }

    device_t my_device = BtDevice_GetSelfDevice();
    if(NULL == my_device)
    {
        DEBUG_LOG_ERROR("fastPair_GetPNameFromStore : Unexpected Error. Shouldn't have reached here");
        return FALSE;
    }

    if(Device_GetProperty(my_device, device_property_fast_pair_personalized_name, &pname_ps_ptr, &pname_ps_size) )
    {
        DEBUG_LOG("fastPair_GetPNameFromStore: Read personalized name of length %d from persistent store", pname_ps_size);
        if(FAST_PAIR_PNAME_STORAGE_LEN != pname_ps_size)
        {
            DEBUG_LOG_ERROR("fastPair_GetPNameFromStore: Unexpected Error. Read length %d is not equal to FAST_PAIR_PNAME_STORAGE_LEN",pname_ps_size);
            return FALSE;
        }

        /* Copy the length to pname_len and pname data to pname array */
        memcpy(pname_len,pname_ps_ptr,sizeof(uint8) );
        /* If the pname string length is 0 or more than FAST_PAIR_PNAME_DATA_LEN */
        if((0 == *pname_len)||(*pname_len > FAST_PAIR_PNAME_DATA_LEN))
        {
            DEBUG_LOG_ERROR("fastPair_GetPNameFromStore : Invalid pname length of %d. So, ignoring pname",*pname_len );
            return FALSE;
        }
        pname_ptr = (uint8 *)pname_ps_ptr + (uint8)(sizeof(uint8));
        memcpy(pname, pname_ptr, FAST_PAIR_PNAME_DATA_LEN*sizeof(uint8));
        return TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("FastPair_GetPName : Device_GetProperty API fails to read personalized name");
        return FALSE;
    }
}

/*! \brief Delete the personalized name
 */
void FastPair_DeletePName(void)
{
    uint8 pname[FAST_PAIR_PNAME_STORAGE_LEN];
    /* Set the personalized name to 0. */
    memset(pname, 0x00, FAST_PAIR_PNAME_STORAGE_LEN*sizeof(uint8));

    fastPair_StorePNameInPSStore(pname);
}

