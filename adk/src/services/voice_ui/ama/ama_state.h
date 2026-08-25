/*!
\copyright  Copyright (c) 2018- 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_state.h
\brief  Definition of the APIs to manage AMA device feature states
*/

#ifndef __AMA_STATE_H_
#define __AMA_STATE_H_

#include "accessories.pb-c.h"

typedef enum _AMA_SETUP_STATE {
    AMA_SETUP_STATE_READY ,
    AMA_SETUP_STATE_WAITING ,  // just got at least one protbuf message
    AMA_SETUP_STATE_START,
    AMA_SETUP_STATE_COMPLETED,
    AMA_SETUP_STATE_SYNCHRONIZE

}AMA_SETUP_STATE;

/* Values for the feature field in _GetState and _SetState messages */
#define AMA_FEATURE_AUXILIARY_CONNECTED                   0x100

#define AMA_FEATURE_BLUETOOTH_A2DP_ENABLED                0x130
#define AMA_FEATURE_BLUETOOTH_HFP_ENABLED                 0x131
#define AMA_FEATURE_BLUETOOTH_A2DP_CONNECTED              0x132
#define AMA_FEATURE_BLUETOOTH_HFP_CONNECTED               0x133
#define AMA_FEATURE_BLUETOOTH_CLASSIC_DISCOVERABLE        0x134

#define AMA_FEATURE_DEVICE_CALIBRATION_REQUIRED           0x200
#define AMA_FEATURE_DEVICE_THEME                          0x201
#define AMA_FEATURE_DEVICE_DND_ENABLED                    0x202
#define AMA_FEATURE_DEVICE_CELLULAR_CONNECTIVITY_STATUS   0x203
#define AMA_FEATURE_PRIVACY_MODE                          0x204

#define AMA_FEATURE_ANC_ENABLE                            0x210
#define AMA_FEATURE_PASSTHROUGH_ENABLE                    0x211
#define AMA_FEATURE_PASSTHROUGH_LEVEL                     0x212

#define AMA_FEATURE_MESSAGE_NOTIFICATION                  0x300
#define AMA_FEATURE_CALL_NOTIFICATION                     0x301
#define AMA_FEATURE_REMOTE_NOTIFICATION                   0x302
#define AMA_FEATURE_WAKE_WORD                             0x352

#define AMA_FEATURE_EQUALIZER_BASS                        0x450
#define AMA_FEATURE_EQUALIZER_MID                         0x451
#define AMA_FEATURE_EQUALIZER_TREBLE                      0x452

#define AMA_FEATURE_DEVICE_POWERED_ON                     0x500
#define AMA_FEATURE_BOOT_TTS_MESSAGING                    0x501
#define AMA_FEATURE_CONNECTION_PROGRESS_LIGHT_TIME        0x502
#define AMA_FEATURE_CONNECTION_SUCCEEDED                  0x503
#define AMA_FEATURE_CONNECTION_PROGRESS_ERROR_LED         0x504
#define AMA_FEATURE_CONNECTION_SUCCESS_AUDIO_CUE          0x505
#define AMA_FEATURE_BACKOFF_TTS_MESSAGING                 0x506

#define AMA_FEATURE_ACCESSORY_RINGING_STATUS              0x510

#define AMA_FEATURE_ACCESSORY_MEDIA_ENHANCEMENT           0x511


#define AMA_FEATURE_INVALID                               0xFFFF

/*! \brief Initialise the AMA state module
 */
void AmaState_Init(void);

/*! \brief Get the state value for a feature
 *  \param feature The feature ID
 *  \param Pstate The state value to be populated
 *  \param pValueCase The type of value associated with the feature
 *  \return Information on the success of the call
 */
ErrorCode AmaState_GetState(uint32 feature, State * state);

/*! \brief Set the state value for a feature
 *  \param feature The feature ID
 *  \param valueCase The type of value associated with the feature
 *  \param state The value to set the state to
 *  \return Information on the success of the call
 */
ErrorCode AmaState_SetState(uint32 feature, State__ValueCase valueCase, uint32 state);

/*! \brief Synchronize the gateway state value for a feature
 *  \param feature The feature ID
 *  \param valueCase The type of value associated with the feature
 *  \param value The value to set the state to
 *  \return Information on the success of the call
 */
ErrorCode AmaState_SynchronizeState(uint32 feature, State__ValueCase valueCase, uint32 value);

#endif /* __AMA_STATE_H_ */
