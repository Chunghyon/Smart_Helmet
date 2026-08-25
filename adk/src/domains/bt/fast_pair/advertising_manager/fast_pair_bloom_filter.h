/*!
\copyright  Copyright (c) 2008 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Handles Fast Pair Bloom Filter generation
*/

#ifndef FAST_PAIR_BLOOM_FILTER_H_
#define FAST_PAIR_BLOOM_FILTER_H_

#include <connection_no_ble.h>
#define CsrBtCmCryptoHashCfm CL_CRYPTO_HASH_CFM_T


/*! Size of empty bloom filter. */
#define NO_BLOOM_FILTER_LEN                 (0)

/*! This is advertising data really, rather than account key/bloom filter
    data. Probably not needed here, or at least */
#define FP_ACCOUNT_DATA_FLAGS_SIZE          (1)
/*! Flags all reserved, this is already defined in fast_pair_advertising.c
    where it is used for the advert version for no account keys.
    \todo combine to single definition. */
#define FP_ACCOUNT_DATA_FLAGS               (0x00)

/*! Size of the account key length and type in bytes. */
#define FP_ACCOUNT_KEY_LENGTHTYPE_SIZE      (1)
/*! Account key type definition to show UI popup. */
#define FP_ACCOUNT_KEY_TYPE_UI_SHOW         (0x0)
/*! Account key type definition to hide UI popup. */
#define FP_ACCOUNT_KEY_TYPE_UI_HIDE         (0x2)

/*! Size of a fast pair account key in bytes. */
#define ACCOUNT_KEY_LEN                     (16)

/*! Size of fast pair bloom filter salt in bytes. */
#define FP_SALT_SIZE                        (2)
/*! Type of salt */
#define FP_SALT_TYPE                        (1)
/*! Size of the Salt field length and type in bytes. */
#define FP_SALT_LENGTHTYPE_SIZE             (1)
/*! Combined salt field length and type information. */
#define FP_SALT_LENGTHTYPE                  ((FP_SALT_SIZE << 4) + FP_SALT_TYPE)
/*! Total size of the salt field TLV in the account key data. */
#define FP_SALT_FIELD_TOTAL_SIZE            (FP_SALT_LENGTHTYPE_SIZE + FP_SALT_SIZE)

#define FP_ACCOUNT_DATA_START_POS (FP_ACCOUNT_DATA_FLAGS_SIZE + FP_ACCOUNT_KEY_LENGTHTYPE_SIZE)

/*! Size of the temp array used to generate hash for the bloom filter.
    May require additional space where battery notifications are supported.
*/
#define SHA256_INPUT_ARRAY_LENGTH   (ACCOUNT_KEY_LEN + FP_SALT_SIZE + FP_BATTERY_NOTFICATION_SIZE)

/*! @brief Private API to initialise the bloom filter data structure

    This will be initialised under fastpair advertisement module as the bloom filter data goes into the adverts

    \param  none

    \returns none
 */
void fastPair_InitBloomFilter(void);


/*! @brief Function to generate bloom filter

      Account key filter or Bloom filter is used in adverts in unidentifiable or BR/EDR non-discoverable mode only.
      
      The following are the trigger to generate bloom filter
      - During fastpair advertising init, to be ready with the data
      - During addition of new account key
      - During Adv Mgr callback to get Item, this is to ensure new bloom filter is ready with a new Salt.
      - Deletion of account keys is taken care through the session data API
      - Internal timer expiry to track Salt change in Account key Filter  
      - Receipt of new battery status information from the case
 */
void fastPair_GenerateBloomFilter(void);


/*! @brief Private API to get the precalculated Bloom Filter data length

    This will go into the fastpair advertisement data

    \param  none

    \returns bloom filter data length
 */
uint8 fastPairGetBloomFilterLen(void);


/*! @brief Private API to get the precalculated Bloom Filter data

    This will go into the fastpair advertisement data

    \param  void

    \returns pointer to bloom filter data
 */
uint8* fastPairGetBloomFilterData(void);



/*! @brief Private API to handle CRYPTO_HASH_CFM for Bloom filter generation

    Called from Fast pair state manager to inform FP Adv module on hash confirmation

    \param  cfm     Confirmation from Crypto hash module while generating bloom filters

    \returns none
 */
void fastPair_AdvHandleHashCfm(CsrBtCmCryptoHashCfm *cfm);


/*! @brief Private API to handle new account key addition

    Called from Fast pair state manager when a new account key is added on a successful fast pairing

    FP Adv module to generate bloom filters with the new data for adverts in BR/EDR non-discoverable mode

 */
void fastPair_AccountKeyAdded(void);


#endif /* FAST_PAIR_BLOOM_FILTER_H_ */
