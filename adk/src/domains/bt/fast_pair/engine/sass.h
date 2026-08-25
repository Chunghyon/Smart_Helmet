/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       sass.h
\brief      Header file for the component handling GFP Smart Audio Source Switching.
*/

#ifndef SASS_H
#define SASS_H

#include "device.h"
#include <bdaddr.h>
#include <task_list.h>

#define SASS_MESSAGE_CODE_INDEX  0
#define SASS_ADD_DATA_LEN_UPPER_INDEX  1
#define SASS_ADD_DATA_LEN_LOWER_INDEX  2
#define SASS_ADD_DATA_INDEX 3
#define SASS_MAC_LEN 8
#define SASS_MSG_NOUNCE_LEN 8
#define SASS_SESSION_NONCE_LEN 8
#define SASS_NONCE_LEN SASS_MSG_NOUNCE_LEN + SASS_SESSION_NONCE_LEN
#define SASS_INUSE_ACCOUNT_KEY_LEN 16
#define SASS_INUSE_ACC_KEY_STRING_LEN 6
#define SASS_ACCOUNT_KEY_LEN 16
#define SASS_IV_LEN SASS_MSG_NOUNCE_LEN + SASS_SESSION_NONCE_LEN

#define SASS_ACTIVE_DEVICE_FLAG 1
#define SASS_NOTIFY_CONN_STATUS_ADD_DATA_LEN 12
#define SASS_CONN_STATUS_DATA_LEN 3

#define SASS_NOTIFY_CAPABILITY_ADD_DATA_LEN 4
#define SASS_VERSION_CODE 0x0101
#define SASS_MAX_CONNECTIONS 2

/* SASS capability flags */
#define SASS_STATE                           (1 << 7)
#define SASS_MP_CONFIG                       (1 << 6)
#define SASS_MP_CURRENT_STATE                (1 << 5)
#define SASS_ON_HEAD_DETECTION               (1 << 4)
#define SASS_ON_HEAD_DETECTION_CURRENT_STATE (1 << 3)

/* Connection Flags */
#define SASS_AD_ON_HEAD_DETECTION       (1 << 7)
#define SASS_AD_CONNECTION_AVAILABILITY (1 << 6)
#define SASS_AD_FOCUS_MODE              (1 << 5)
#define SASS_AD_AUTO_RECONNECTED        (1 << 4)

/* Connection State */
#define SASS_NO_CONNECTION             0x0
#define SASS_PAGING                    0x1
#define SASS_CONNECTED_NO_DATA         0x2
#define SASS_NON_AUDIO_DATA            0x3
#define SASS_A2DP_STREAMING_ONLY       0x4
#define SASS_A2DP_STREAMING_WITH_AVRCP 0x5
#define SASS_HFP_CALL                  0x6
#define SASS_DISABLE_CONNECTION_SWITCH 0xF

/* Switch Multipoint state events */
#define SASS_SWITCH_OFF_MULTIPOINT 0x00
#define SASS_SWITCH_ON_MULTIPOINT 0x01

/* Switch back events */
#define SASS_SWITCH_BACK 0x01
#define SASS_SWITCH_BACK_AND_AUDIO_RESUME 0x02

/* Switch active audio source flags */
#define SASS_SWITCH_TO_THIS_DEVICE (1<<7)
#define SASS_RESUME_PLAY_ON_SWITCH_TO_DEVICE (1<<6)
#define SASS_REJECT_SCO (1<<5)
#define SASS_DISCONNECT_BLUETOOTH (1<<4)

/* Notify Multipoint-switch Event */
#define SASS_NOTIFY_MP_SWITCH_REASON_FLAG_LEN            (1)
#define SASS_NOTIFY_MP_SWITCH_TARGET_DEVICE_FLAG_LEN     (1)
#define SASS_NOTIFY_MP_SWITCH_EVENT_ADD_DATA_LEN_DEFAULT (4)
/* Switching Reason */
#define SASS_NOTIFY_MP_SWITCH_EVENT_REASON_UNSPECIFIED    (0x00)
#define SASS_NOTIFY_MP_SWITCH_EVENT_REASON_A2DP_STREAMING (0x01)
#define SASS_NOTIFY_MP_SWITCH_EVENT_REASON_HFP            (0x02)
/* Target Device */
#define SASS_NOTIFY_MP_SWITCH_EVENT_TO_THIS_DEVICE        (0x01)
#define SASS_NOTIFY_MP_SWITCH_EVENT_TO_ANOTHER_DEVICE     (0x02)

typedef struct
{
    uint8 connection_state;
    uint8 custom_data;
    uint8 connected_devices_bitmap;
}sass_connection_status_field_t;

typedef struct
{
    bdaddr switch_to_device_bdaddr;
    bdaddr switch_away_device_bdaddr;
    bool   is_switch_active_audio_src_pending;
    bool   resume_playing_on_switch_to_device;
}sass_switch_active_audio_source_t;

typedef struct
{
   uint8 remote_device_id;
   uint8* msg_nonce_for_notify_con_status;
}sass_conn_status_enc_pending_t;

/*! \brief SASS task structure */
typedef struct
{
    TaskData task;
    bdaddr mru_handset_bd_addr;
    sass_connection_status_field_t con_status;
    bool switch_back_audio_resume;
    sass_switch_active_audio_source_t switch_active_audio_source_event;
    bool is_bargein_for_sass_switch;
    sass_conn_status_enc_pending_t conn_status_enc_pending;
    bool  notify_conn_status_pending[SASS_MAX_CONNECTIONS];
    uint8 number_of_connected_seekers;
    bdaddr current_active_handset_addr;
    bool switch_back_is_initiated;
}sassTaskData;

typedef enum
{
    sass_nak_message = 0, /* NAK message for incorrect additional data sent from Seeker */
    sass_ack_message,     /* ACK message for correct additional data sent from Seeker*/
    sass_no_ack_message   /* No ACK message for no additonal data from Seeker */
} sass_message_rsp;

/* Message Code for SASS message group */
typedef enum
{
    SASS_GET_CAPABILITY_EVENT = 0x10,
    SASS_NOTIFY_CAPABILITY_EVENT = 0x11,
    SASS_SET_MP_STATE_EVENT = 0x12,
    SASS_SET_SWITCHING_PREFERENCE_EVENT = 0x20,
    SASS_GET_SWITCHING_PREFERENCE_EVENT = 0x21,
    SASS_NOTIFY_SWITCHING_PREFERENCE_EVENT = 0x22,
    SASS_SWITCH_AUDIO_SOURCE_EVENT = 0x30,
    SASS_SWITCH_BACK_EVENT = 0x31,
    SASS_NOTIFY_MP_SWITCH_EVENT = 0x32,
    SASS_GET_CONNECTION_STATUS_EVENT = 0x33,
    SASS_NOTIFY_CONNECTION_STATUS_EVENT = 0x34,
    SASS_NOTIFY_SASS_INITIATED_CONN_EVENT = 0x40,
    SASS_INUSE_ACC_KEY_IND_EVENT = 0x41,
    SASS_SEND_CUSTOM_DATA_EVENT = 0x42,
    SASS_SET_DROP_CONN_TARGET_EVENT = 0x43
} SASS_MESSAGE_CODE;

/*! \brief  Send connection status field updates to SASS advertising manager.

    \param is_conn_status_change  connection status update

*/
typedef void (*sass_connection_status_change_callback)(void);

/*! \brief Get bdaddr of switch to device
    \param None
    \return bdaddr* pointer to the address of switch to device
*/
bdaddr* Sass_GetSwitchToDeviceBdAddr(void);

/*! \brief Resume playing on switch to device after switching.
 */
bool Sass_ResumePlayingOnSwitchToDevice(void);

/*! \brief Reset the states related to Switch Active Audio Source event
    \param None
    \return void
*/
void Sass_ResetSwitchActiveAudioSourceStates(void);

/*! \brief Reset bdaddr of switch to device
    \param None
    \return void
*/
void Sass_ResetSwitchToDeviceBdAddr(void);

/*! \brief Reset bdaddr of switch away device
    \param None
    \return void
*/
void Sass_ResetSwitchAwayDeviceBdAddr(void);

/*! \brief Reset Resume playing on switch to device flag after switching.
 */
void Sass_ResetResumePlayingFlag(void);

 /*! \brief Notify multipoint switch to all the connected seekers when there is a change in active audio source
    \param switch_event_reason Reason for sending MP switch event
    \return None
*/
void Sass_NotifyMultipointSwitchEvent(uint8 switch_event_reason);

/*! \brief Set connection state data leaving on-head detection flag unchanged
 *  \param connection_state value to be set
 */
void Sass_SetConnectionStateExceptOnHeadFlag(uint8 connection_state);

/*! \brief Get current connection state.
 */
uint8 Sass_GetConnectionState(void);

/*! \brief Set custom data for current streaming.
 *  \param custom data value to be updated
 */
void Sass_SetCustomData(uint8 custom_data);

/*! \brief Get current custom data.
 */
uint8 Sass_GetCustomData(void);

/*! \brief Set connected devices bitmap data
 *  \param connected_devices_bitmap value to be set
 */
void Sass_SetConnectedDeviceBitMap(uint8 connected_devices_bitmap);

/*! \brief Get current connected devices bitmap. 
 */
uint8 Sass_GetConnectedDeviceBitMap(void);

/*! \brief Get Handset BD Address associated with the in-use account key
 */
bool Sass_GetHandsetAddressAssociatedWithInUseAccountKey(bdaddr *bd_addr);

/*! \brief SASS advertising manager shall register a callback with SASS plugin to get connection status updates.
 */
void Sass_RegisterForConnectionStatusChange(sass_connection_status_change_callback callBack);

/*! \brief Check if any account key associated with the given handset device
 */
bool Sass_IsAccountKeyAssociatedWithDevice(device_t device);

/*! \brief Checks if latest connected device was for SASS switching
    \param device Device instance
    \return TRUE if last connected device was for SASS switching, otherwise FALSE
*/
bool Sass_IsConnectedDeviceForSassSwitch(device_t device);

/*! \brief Checks if latest barge-in connection was for SASS switching
    \param None
    \return TRUE if last barge-in connection was for SASS switching, otherwise FALSE
*/
bool Sass_IsBargeInForSassSwitch(void);

/*! \brief Disable connection switch for SASS
 *  \param void
    \return void
*/
void Sass_DisableConnectionSwitch(void);

/*! \brief Enable connection switch for SASS
 *  \param void
    \return void
*/
void Sass_EnableConnectionSwitch(void);

/*! \brief Check if connection switching for SASS is disabled
 *  \param void
    \return bool TRUE if connection switch for SASS is disabled, FALSE otherwise
*/
bool Sass_IsConnectionSwitchDisabled(void);

/*! \brief SASS Initialization.
 */
void Sass_Init(void);

#endif /* SASS_H */

