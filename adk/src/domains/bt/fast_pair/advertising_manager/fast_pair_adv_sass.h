/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_adv_sass.h
\brief      Handles Advertising Data for SASS feature
*/

#ifndef FAST_PAIR_ADV_SASS_H_
#define FAST_PAIR_ADV_SASS_H_

#include <task_list.h>
#include <connection_manager.h>

/* SASS connection state advertisement elements */
#define SASS_SIZE_AD_LENGTH_TYPE 1
#define SASS_SIZE_AD_CON_STATE   1
#define SASS_SIZE_AD_CUSTOM_DATA 1
#define SASS_SIZE_AD_CON_BITMAP  1
#define SASS_ADV_DATA_SIZE       (SASS_SIZE_AD_LENGTH_TYPE + SASS_SIZE_AD_CON_STATE + SASS_SIZE_AD_CUSTOM_DATA + SASS_SIZE_AD_CON_BITMAP)

#define SASS_RANDOM_RESOLVABLE_DATA_SIZE (SASS_SIZE_AD_LENGTH_TYPE + SASS_ADV_DATA_SIZE)

typedef struct
{
    /* In single point use-case, if in-use account key handset is connected, that will automatically be the active handset
       In Multi-point use-case, this flag will help in finding the connected and active i.e. focus foreground status device */
    bool  in_use_account_key_handset_connected_and_active;
    uint8 *in_use_account_key;
    bdaddr device_addr;
}sass_account_keys_data_t;

typedef struct
{
    uint8 *initial_vector_ready;
    uint8 account_key_filter_length;
    uint8 *random_resolvable_data;
    uint8 random_res_data_size;
}sass_random_resolvable_data_t;

typedef struct
{
    TaskData task;  
    bool updateFPAdverts;
    sass_account_keys_data_t key_data;
    sass_random_resolvable_data_t enc_data;
}fast_pair_adv_sass_t;

/*! Get SASS Advertising data size */
uint8 fastPair_SASSGetAdvDataSize(void);

/*! @brief Private API to get the advertising data for SASS feature

    This will go into the fastpair advertisement data

    \param  void

    \returns pointer to random resolvable data
 */
uint8 *fastPair_SASSGetAdvData(void);

/*! @brief Private API to get pointer to in use account Key. 

    The account key filter will be calcualted after modifying the first byte of in-use account key
    
    \param  bool* is_in_use_account_key_handset_connected
    
    \returns  Pointer to in use account key 
 */

uint8* fastPair_SASSGetInUseAccountKey(bool* is_in_use_account_key_handset_connected);

/*! @brief API to update the in use account Key. 

    In use account key would be updated whenever the MRU device gets updated.
    
    \param  const bdaddr* bd_addr bd address of MRU device
    
    \returns  void
 */
void FastPair_SASSUpdateInUseAccountKeyForMruDevice(const bdaddr *bd_addr);

/*! @brief API to get in use account key active flag. 

    \param  void
 
    \returns In Use Account Key Active Flag 
 */
bool FastPair_SASSGetInUseAccountKeyActiveFlag(void);

/*! @brief  API to trigger update of SASS adv payload. 

    \param  void
 
    \returns None 
 */
void fastPair_SASSUpdateAdvPayload(void);

/*! Private API to initialise Fast Pair SASS advertising module

    Called from Fast Pair advertising to initialise Fast Pair SASS advertising module
 */
void fastPair_SetUpSASSAdvertising(void);

#endif /* FAST_PAIR_ADV_SASS_H_ */
