/*!
\copyright  Copyright (c) 2008 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   hfp_profile HFP Profile
\ingroup    profiles
\brief      HFP Profile private types.
*/

#ifndef HFP_PROFILE_PRIVATE_H_
#define HFP_PROFILE_PRIVATE_H_

#include "hfp_profile.h"
#include "hfp_profile_typedef.h"
#include "hfp_abstraction.h"

#include <battery_monitor.h>
#include <device.h>
#include <hfp.h>
#include <panic.h>
#include <task_list.h>
#include <bandwidth_manager.h>
#include <voice_sources.h>

#define PSKEY_LOCAL_SUPPORTED_FEATURES (0x00EF)
#define PSKEY_LOCAL_SUPPORTED_FEATURES_SIZE (4)
#define PSKEY_LOCAL_SUPPORTED_FEATURES_DEFAULTS { 0xFEEF, 0xFE8F, 0xFFDB, 0x875B }

#ifdef TEST_HFP_CODEC_PSKEY
/* Bitfields used for PS_KEY_TEST_HFP_CODEC */
#define HFP_CODEC_PS_BIT_NB             (1<<0)
#define HFP_CODEC_PS_BIT_WB             (1<<1)
#define HFP_CODEC_PS_BIT_SWB            (1<<2)
#endif

/*! \brief Get SLC status notify list */
#define appHfpGetSlcStatusNotifyList() (task_list_flexible_t *)(&(hfp_profile_task_data.slc_status_notify_list))

/*! \brief Get status notify list */
#define appHfpGetStatusNotifyList() (task_list_flexible_t *)(&(hfp_profile_task_data.status_notify_list))

/*! Macro for creating a message based on the message name */
#define MAKE_HFP_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);
/*! Macro for creating a variable length message based on the message name */
#define MAKE_HFP_MESSAGE_WITH_LEN(TYPE, LEN) \
    TYPE##_T *message = (TYPE##_T *)PanicUnlessMalloc(sizeof(TYPE##_T) + (LEN-1));


/* Values seen as possible to avoid problems with certain applications are

   HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS (5000) and
   HFP_CHECK_APTX_VOICE_PACKETS_FIRST_TIME_DELAY_MS (20000).

   Setting these values 0 below to disable this mechanism by default.

    NB: These interval should be greater than the link supervion timeout
    to prevent false triggering on a link loss.
*/

/* Interval in ms to check aptX voice packet status

*/
#define HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS   (5500)

/* Delay in ms to check aptX voice packet status first time.
   This is greater then regular interval (HFP_CHECK_APTX_VOICE_PACKETS_INTERVAL_MS)
   because SWBS decoder may start before the handset SWBS encoder has sent any
   SCO frames.So until the first actual encoded audio frame arrives, decoder logs
   it as a frame_error (since there is no data to decode).So we should
   start reading first frame after longer delay which will ensure that we have
   good sco frames if there is audio and will avoid false trigger of no swb audio. */

#define HFP_CHECK_APTX_VOICE_PACKETS_FIRST_TIME_DELAY_MS   (5500)

typedef enum
{
    hfp_profile_audio_connect_success,
    hfp_profile_audio_connect_in_progress,
    hfp_profile_audio_connect_failed
} hfp_profile_audio_connect_status_t;

typedef struct
{
    TaskData task;
    /*! List of tasks to notify of SLC connection status. */
    TASK_LIST_WITH_INITIAL_CAPACITY(HFP_SLC_STATUS_NOTIFY_LIST_INIT_CAPACITY) slc_status_notify_list;
    /*! List of tasks to notify of general HFP status changes */
    TASK_LIST_WITH_INITIAL_CAPACITY(HFP_STATUS_NOTIFY_LIST_INIT_CAPACITY) status_notify_list;
    /*! List of tasks requiring confirmation of HFP connect requests */
    task_list_with_data_t connect_request_clients;
    /*! List of tasks requiring confirmation of HFP disconnect requests */
    task_list_with_data_t disconnect_request_clients;

    /*! Task to handle TWS+ AT commands. */
    Task at_cmd_task;

    /*! The task to send SCO sync messages */
    Task sco_sync_task;
} hfpTaskData;

extern hfpTaskData hfp_profile_task_data;

/*! \brief Internal message IDs */
enum hfp_profile_internal_messages
{
    HFP_INTERNAL_HSP_INCOMING_TIMEOUT = INTERNAL_MESSAGE_BASE, /*!< Internal message to indicate timeout from incoming call */
    HFP_INTERNAL_HFP_CONNECT_REQ,               /*!< Internal message to connect to HFP */
    HFP_INTERNAL_HFP_DISCONNECT_REQ,            /*!< Internal message to disconnect HFP */
    HFP_INTERNAL_HFP_LAST_NUMBER_REDIAL_REQ,    /*!< Internal message to request last number redial */
    HFP_INTERNAL_HFP_VOICE_DIAL_REQ,            /*!< Internal message to request voice dial */
    HFP_INTERNAL_HFP_VOICE_DIAL_DISABLE_REQ,    /*!< Internal message to disable voice dial */
    HFP_INTERNAL_HFP_CALL_ACCEPT_REQ,           /*!< Internal message to accept an incoming call */
    HFP_INTERNAL_HFP_CALL_REJECT_REQ,           /*!< Internal message to reject an incoming call */
    HFP_INTERNAL_HFP_CALL_HANGUP_REQ,           /*!< Internal message to hang up an active call */
    HFP_INTERNAL_HFP_MUTE_REQ,                  /*!< Internal message to mute an active call */
    HFP_INTERNAL_HFP_TRANSFER_REQ,              /*!< Internal message to transfer active call between AG and device */
    HFP_INTERNAL_NUMBER_DIAL_REQ,
    HFP_INTERNAL_OUT_OF_BAND_RINGTONE_REQ,      /*!< Internal message to request out of band ringtone indication */
    HFP_INTERNAL_HFP_RELEASE_WAITING_REJECT_INCOMING_REQ,
    HFP_INTERNAL_HFP_ACCEPT_WAITING_RELEASE_ACTIVE_REQ,
    HFP_INTERNAL_HFP_ACCEPT_WAITING_HOLD_ACTIVE_REQ,
    HFP_INTERNAL_HFP_ADD_HELD_TO_MULTIPARTY_REQ,
    HFP_INTERNAL_HFP_JOIN_CALLS_AND_HANG_UP,
    HFP_INTERNAL_CHECK_APTX_VOICE_PACKETS_COUNTER_REQ,


    /*! This must be the final message */
    HFP_INTERNAL_MESSAGE_END
};

/*! Internal connect request message */
typedef struct
{
    hfp_connection_type_t profile;  /*!< Profile to use */
} HFP_INTERNAL_HFP_CONNECT_REQ_T;

/*! Internal mute request message */
typedef struct
{
    bool mute;              /*!< Mute enable/disable */
} HFP_INTERNAL_HFP_MUTE_REQ_T;

/*! Internal audio transfer request message */
typedef struct
{
    voice_source_audio_transfer_direction_t direction;    /*!< Transfer to/from AG from/to Headset */
} HFP_INTERNAL_HFP_TRANSFER_REQ_T;

typedef struct
{
    unsigned length;
    uint8  number[1];
} HFP_INTERNAL_NUMBER_DIAL_REQ_T;

void hfpProfile_HandleInitComplete(bool compete_success);

/*! \brief Determine if a device supports secure connections

    \param bd_addr Pointer to read-only device BT address.
    \return bool TRUE if secure connects supported, FALSE otherwise.
*/
bool hfpProfile_IsSecureConnection(const bdaddr *bd_addr);

/*! \brief Send a HFP connect confirmation to the specified task.

    This function also removes the task from the pending connection requests task list.

    \param task - the task that shall be notified of the connect confirm.
    \param data - the task_list data, specifying the device address that was connected.
    \param arg - the data regarding the connection, i.e. success bool and bd_addr.
*/
bool HfpProfile_FindClientSendConnectCfm(Task task, task_list_data_t *data, void *arg);

/*! \brief Send a HFP disconnect confirmation to the specified task.

    This function also removes the task from the pending disconnect requests clients task list.

    \param task - the task that shall be notified of the disconnect confirm.
    \param data - the task_list data, specifying the device address that was disconnected.
    \param arg - the data regarding the disconnection, i.e. success bool and bd_addr.
*/
bool HfpProfile_FindClientSendDisconnectCfm(Task task, task_list_data_t *data, void *arg);


/*! \brief Initiate HFP connection to default

    Attempt to connect to the previously connected HFP AG.

    \return TRUE if a connection was requested. FALSE is returned
        in the case of an error such as HFP not being supported by
        the handset or there already being an HFP connection. The
        error will apply even if the existing HFP connection is
        to the requested handset.
*/
bool HfpProfile_ConnectHandset(void);

/*! \brief Store HFP configuration

    \param device - the device in the database to serialise

    This function is called to store the current HFP configuration.

    The configuration is written to the device database and is serialised
    after a delay. This is to avoid multiple writes when the user adjusts the
    playback volume.
*/
void HfpProfile_StoreConfig(device_t device);

/*! \brief Handle HFP error

    Some error occurred in the HFP state machine, to avoid the state machine
    getting stuck, drop connection and move to 'disconnected' state.
*/
void HfpProfile_HandleError(hfpInstanceTaskData * instance, MessageId id, Message message);

/*! \brief Check SCO encryption

    This functions is called to check if SCO is encrypted or not.  If there is
    a SCO link active, a call is in progress and the link becomes unencrypted,
    send a Telephony message that could be used to provide an indication tone
    to the user, depenedent on UI configuration.
*/
void HfpProfile_CheckEncryptedSco(hfpInstanceTaskData * instance);

/*! \brief Handle SLC connection success */
void hfpProfile_HandleConnectCompleteSuccess(hfpInstanceTaskData * instance);

/*! \brief Handle SLC connection failure */
void hfpProfile_HandleConnectCompleteFailed(hfpInstanceTaskData * instance, bool hfp_not_supported);

/*! \brief Handle SLC disconnection */
void hfpProfile_HandleDisconnectComplete(hfpInstanceTaskData * instance, appHfpDisconnectReason reason);

/*! \brief Handle incoming audio connection indication */
void hfpProfile_HandleHfpAudioConnectIncoming(hfpInstanceTaskData * instance, bool is_esco);

/*! \brief Handle audio connection */
void hfpProfile_HandleHfpAudioConnectComplete(hfpInstanceTaskData* instance, hfp_profile_audio_connect_status_t status, Sink audio_sink, uint16 codec, uint8 wesco, uint8 tesco, uint16 qce_codec_mode_id);

/*! \brief Handle audio disconnection */
void hfpProfile_HandleAudioDisconnectIndication(hfpInstanceTaskData* instance, bool transferred);

/*! \brief Handle RING indication */
void hfpProfile_HandleRingIndication(hfpInstanceTaskData* instance, bool in_band);

/*! \brief Handle service indication */
void hfpProfile_HandleServiceIndication(hfpInstanceTaskData* instance, uint8 service);

/*! \brief Handle call state change */
void hfpProfile_HandleHfpCallStateIndication(hfpInstanceTaskData* instance);

/*! \brief Handle voice recognition indication */
void hfpProfile_HandleHfpVoiceRecognitionIndication(hfpInstanceTaskData* instance, bool enabled);

/*! \brief Handle voice recognition enable */
void hfpProfile_HandleHfpVoiceRecognitionEnableComplete(hfpInstanceTaskData* instance, bool complete_success);

/*! \brief Handle caller ID */
void hfpProfile_HandleHfpCallerIdIndication(hfpInstanceTaskData* instance, char* caller_number, phone_number_type_t type, char* caller_name);

/*! \brief Handle speaker gain */
void hfpProfile_HandleHfpVolumeSyncSpeakerGainIndication(hfpInstanceTaskData * instance, uint16 gain);

/*! \brief Handle mic gain */
void hfpProfile_HandleHfpVolumeSyncMicGainIndication(hfpInstanceTaskData * instance, uint16 gain);

/*! \brief Handle answer call confirmation */
void hfpProfile_HandleHfpCallAnswerComplete(hfpInstanceTaskData* instance, bool completed_successfully);

/*! \brief Handle terminate call confirmation */
void hfpProfile_HandleHfpCallTerminateComplete(hfpInstanceTaskData * instance, bool completed_successfully);

/*! \brief Handle call hold action confirmation */
void hfpProfile_HandleHfpCallHoldActionComplete(hfpInstanceTaskData * instance, bool completed_successfully);

/*! \brief Handle unrecognised AT command */
void hfpProfile_HandleHfpUnrecognisedAtCmdIndication(hfpInstanceTaskData* instance, uint8* data, uint16 size_data);

/*! \brief Handle send AT command complete */
void hfpProfile_HandleHfpAtCmdComplete(bool success);

/*! \brief Check if Packet Counter monitoring required or not */
bool hfpProfile_AptxVoicePacketsCounterToBeMonitored(const bdaddr *bd_addr, uint16 qce_codec_mode_id);


#endif /* HFP_PROFILE_PRIVATE_H_ */
