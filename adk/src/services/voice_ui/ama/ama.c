/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama.c
\brief  Implementation of the service interface for Amazon AVS
*/

#ifdef INCLUDE_AMA
#include "ama.h"
#include "ama_actions.h"
#include "ama_anc.h"
#include "ama_audio.h"
#include "ama_battery.h"
#include "ama_ble.h"
#include "ama_config.h"
#include "ama_setup_tracker.h"
#include "ama_data.h"
#include "ama_eq.h"
#include "ama_extended_init.h"
#include "ama_message_handlers_init.h"
#include "ama_voice_ui_handle.h"
#include "ama_send_command.h"
#include "ama_send_response.h"
#include "ama_state.h"
#include "ama_notify_app_msg.h"
#include "ama_transport_version.h"
#include "ama_transport.h"
#include "ama_transport_rfcomm.h"
#include "ama_tws.h"
#include "ama_log.h"
#include "ama_speech_config.h"

#include "bt_device.h"
#include <feature.h>
#include "gatt_server_gap.h"
#include "local_name.h"
#include <ps.h>
#include <stdlib.h>
#include <voice_ui.h>

#ifdef INCLUDE_ACCESSORY
#include "ama_transport_accessory.h"
#endif

static voice_ui_handle_t *voice_ui_handle = NULL;

/* Forward declaration */
static void ama_EventHandler(ui_input_t   event_id);

static void ama_DeselectVoiceAssistant(void);
static void ama_SelectVoiceAssistant(void);
static void ama_SessionCancelled(bool capture_suspended);
static void ama_HandleWakeUpWordDetectionEnable(bool enable);
static void ama_HandlePrivacyModeEnable(bool enable);

static voice_ui_if_t ama_interface =
{
    .va_provider = voice_ui_provider_ama,
    .reboot_required_on_provider_switch = FALSE,
    .EventHandler = ama_EventHandler,
    .DeselectVoiceAssistant = ama_DeselectVoiceAssistant,
    .SelectVoiceAssistant = ama_SelectVoiceAssistant,
    .GetBtAddress = AmaTransport_GetBtAddress,
    .SetWakeUpWordDetectionEnable = ama_HandleWakeUpWordDetectionEnable,
    .SetPrivacyModeEnable = ama_HandlePrivacyModeEnable,
    .BatteryUpdate = AmaBattery_Update,
#ifdef ENABLE_ANC
    .AncEnableUpdate = AmaAnc_AncEnableUpdate,
    .LeakthroughEnableUpdate = AmaAnc_PassthroughEnableUpdate,
    .LeakthroughGainUpdate = AmaAnc_PassthroughLevelUpdate,
#endif
    .SessionCancelled = ama_SessionCancelled,
    .EqUpdate = Ama_EqUpdate,
    .audio_if =
    {
        .CaptureDataReceived = AmaAudio_HandleVoiceData,
#ifdef INCLUDE_AMA_WUW
        .WakeUpWordDetected = AmaAudio_WakeWordDetected,
#else
        .WakeUpWordDetected = NULL,
#endif /* INCLUDE_AMA_WUW */
    },
};

static void ama_HandleWakeUpWordDetectionEnable(bool enable)
{
    DEBUG_LOG_DEBUG("ama_HandleWakeUpWordDetectionEnable: %u", enable);

    if (enable)
    {
        AmaAudio_StartWakeWordDetection();
    }
    else
    {
        AmaAudio_StopWakeWordDetection();
    }

    AmaSendCommand_SyncState(AMA_FEATURE_WAKE_WORD, STATE__VALUE_BOOLEAN, enable);
}

static void ama_HandlePrivacyModeEnable(bool enable)
{
    DEBUG_LOG_DEBUG("ama_HandlePrivacyModeEnable: %u", enable);

    if (enable)
    {
        if(AmaData_IsSendingVoiceData())
        {
            AmaSendCommand_StopSpeech(ERROR_CODE__USER_CANCELLED, Ama_GetCurrentSpeechDialogId());
        }

        AmaAudio_StopWakeWordDetection();
        VoiceUi_Notify(VOICE_UI_AMA_PRIVACY_MODE_ENABLED);
    }
    else
    {
        AmaAudio_StartWakeWordDetection();
        VoiceUi_Notify(VOICE_UI_AMA_PRIVACY_MODE_DISABLED);
    }

    AmaSendCommand_SyncState(AMA_FEATURE_PRIVACY_MODE, STATE__VALUE_BOOLEAN, enable);
}

static void ama_DeselectVoiceAssistant(void)
{
    DEBUG_LOG("ama_DeselectVoiceAssistant");
    AmaSendCommand_NotifyDeviceConfig(ASSISTANT_OVERRIDE_REQUIRED);
}

static void ama_SelectVoiceAssistant(void)
{
    DEBUG_LOG("ama_SelectVoiceAssistant");
    AmaSendCommand_NotifyDeviceConfig(ASSISTANT_OVERRIDEN);
    if(Ama_IsWakeUpWordFeatureIncluded())
    {
        if (AmaTransport_IsConnected())
        {
            AmaAudio_StartWakeWordDetection();
        }
    }
}

static void ama_SessionCancelled(bool capture_suspended)
{
    UNUSED(capture_suspended);
    AmaSendCommand_StopSpeech(ERROR_CODE__USER_CANCELLED, Ama_GetCurrentSpeechDialogId());
    AmaData_SetState(ama_state_idle);
}

/************************************************************************/
static void ama_EventHandler(ui_input_t event_id)
{
    DEBUG_LOG("ama_EventHandler: event_id enum:ui_input_t:%d", event_id);

    if (!AmaActions_HandleVaEvent(event_id))
    {
        DEBUG_LOG("ama_EventHandler: unhandled");
    }

}

static void ama_LicenseCheck(void)
{
    if (VoiceUi_IsTwsFeatureIncluded())
    {   /* Earbuds */
        if(Ama_IsWakeUpWordFeatureIncluded())
        {
            if (FeatureVerifyLicense(AVA_MONO))
            {
                DEBUG_LOG_VERBOSE("ama_LicenseCheck: APVA MONO is licensed");
            }
            else
            {
                DEBUG_LOG_WARN("ama_LicenseCheck: APVA MONO not licensed");
                if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_VERBOSE)
                    Panic();
            }
        }
    }
    else
    {   /* Headset */
        if(Ama_IsWakeUpWordFeatureIncluded())
        {
            if (FeatureVerifyLicense(AVA))
            {
                DEBUG_LOG_VERBOSE("ama_LicenseCheck: APVA is licensed");
            }
            else
            {
                DEBUG_LOG_WARN("ama_LicenseCheck: APVA not licensed");
                if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_VERBOSE)
                    Panic();
            }
        }
    }
}

/************************************************************************/
bool Ama_Init(Task init_task)
{
    UNUSED(init_task);

    bool status = TRUE;
    
    DEBUG_LOG("Ama_Init");

    AmaTransport_Init();
    AmaTransport_Register(Ama_GetCoreMessageHandlerTask());
    AmaTransport_Register(Ama_GetExtendedMessageHandlerTask());
    AmaTransport_RfcommInit();
#ifdef INCLUDE_ACCESSORY
    status = AmaTransport_AccessoryInit();
#endif

    voice_ui_handle = VoiceUi_Register(&ama_interface);
    Ama_InitialiseAppMessageHandlers();
    AmaState_Init();
    AmaActions_Init();

    /* LE advertising is used even when transport is not LE */
    AmaBle_SetupLeAdvertisingData();
    GattServerGap_UseCompleteLocalName(TRUE);

    Ama_ConfigureCodec(AMA_DEFAULT_CODEC);
    
    AmaData_SetState(ama_state_initialized);
    Ama_SetSpeechSettingsToDefault();
    AmaAudio_Init();
    AmaBattery_Init();
#ifdef ENABLE_ANC
    AmaAnc_Init();
#endif
    Ama_EqInit();

    ama_LicenseCheck();

#ifndef HAVE_RDP_UI
    AmaAudio_RegisterLocalePrompts();
#endif

    AmaTransport_RxDataReset();
    AmaAudio_StopCapture();

    if(Ama_IsWakeUpWordFeatureIncluded())
    {
        AmaAudio_StopWakeWordDetection();
    }
    
    Ama_ExtendedInit();

    return status;
}

/************************************************************************/
voice_ui_handle_t *Ama_GetVoiceUiHandle(void)
{
    return voice_ui_handle;
}

#endif /* INCLUDE_AMA */

