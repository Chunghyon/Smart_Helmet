/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       fast_pair_wait_additional_data_state.h
\brief      Header file of Fast Pair Additional Data state
*/

#ifndef FAST_PAIR_WAIT_ADDITIONAL_DATA_STATE_H_
#define FAST_PAIR_WAIT_ADDITIONAL_DATA_STATE_H_

#include "fast_pair.h"
#include "fast_pair_events.h"

/*! \brief Fast Pair module additional data types */
typedef enum fast_pair_additional_data_type
{
    FAST_PAIR_ADDITIONAL_DATA_TYPE_NONE,
    FAST_PAIR_ADDITIONAL_DATA_TYPE_PNAME            /*!< Personalized Name type */
} fast_pair_additional_data_type;

/*! @brief  Event handler for the Fast Pair Wait for Additional Data State.

    @param event fast pair event.

    @return TRUE if the event was successfully processed, otherwise FALSE.
*/
bool fastPair_StateWaitAdditionalDataHandleEvent(fast_pair_state_event_t event);


/*! @brief  Get encrypted Additional data (0x1237) notification packet using PName and key.
            Here key is the AES-key or Account key used for decoding the request packet.

    Additional data packet format:
    Byte 0-7: The first 8 bytes of HMAC-SHA256. HMAC-SHA256 is obtained using nonce & encrypted additiona data
    Byte 8-15: cryptographically random 8 bytes for Nonce. This is used by  encryption & HMAC-SHA256.
    Byte 16-len: AES-CTR encrypted Additional data. Here Additional data is PNAME obtained from persistent storage.
                 AES-CTR algorithmuses PName, key and nonce.

    @param none

    @return TRUE if successful, otherwise FALSE. Failure case can due to PName not present in PS store.
*/
bool fastPair_GetEncryptedAdditionalDataHavingPName(void);


#endif
