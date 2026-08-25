/*!
\copyright  Copyright (c) 2018- 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_state.c
\brief  Implementation of the APIs to manage AMA device feature states
*/

#ifdef INCLUDE_AMA
#include <bdaddr.h>
#include <boot.h>
#include <connection.h>
#include <file.h>
#include <kalimba.h>
#include <kalimba_standard_messages.h>
#include <led.h>
#include <message.h>
#include <panic.h>
#include <pio.h>
#include <region.h>
#include <service.h>
#include <sink.h>
#include <source.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stream.h>
#include <util.h>
#include <vm.h>
#include <psu.h>
#include <charger.h>
#include <a2dp.h>
#include <audio_plugin_music_variants.h>
#include <transform.h>
#include <loader.h>
#include <partition.h>
#include <micbias.h>
#include <vmal.h>
#include <gatt_ama_server.h>
#include "ama_send_command.h"
#include <logging.h>
#include "voice_ui_anc.h"
#include "voice_ui_va_client_if.h"

#include "ama_debug.h"
#include "ama_state.h"
#include "ama_eq.h"
#include "ama_audio.h"

#define AMA_PASSTHROUGH_MIN_LEVEL 1
#define AMA_PASSTHROUGH_MAX_LEVEL 100

typedef struct __AmaFeatureState
{
    bool auxiliaryConnected;
    bool bluetoothA2dpEnabled;
    bool bluetoothHfpEnabled;

}AmaFeatureState;

static AmaFeatureState amaFeatureState;

void AmaState_Init(void)
{
    amaFeatureState.auxiliaryConnected = FALSE;
    amaFeatureState.bluetoothA2dpEnabled = TRUE;
    amaFeatureState.bluetoothHfpEnabled = TRUE;
}

#ifdef ENABLE_ANC
static uint16 amaState_ConvertPassthroughLevelToPercentage(uint16 level)
{
	if(level > AMA_PASSTHROUGH_MAX_LEVEL)
    {
    	level = AMA_PASSTHROUGH_MAX_LEVEL;
    }
    else if(level < AMA_PASSTHROUGH_MIN_LEVEL)
    {
        level = AMA_PASSTHROUGH_MIN_LEVEL;
    }

    return (level * 100) / AMA_PASSTHROUGH_MAX_LEVEL;
}
#endif /* ENABLE_ANC */

ErrorCode AmaState_GetState(uint32 feature, State * state)
{
    state->value_case = STATE__VALUE__NOT_SET;

    ErrorCode error_code = ERROR_CODE__SUCCESS;

    DEBUG_LOG("AmaState_GetState feature %x", feature);

    switch(feature)
    {
        case AMA_FEATURE_AUXILIARY_CONNECTED:
            state->u.boolean = (uint32)amaFeatureState.auxiliaryConnected;
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_A2DP_ENABLED:
            state->u.boolean = (uint32)amaFeatureState.bluetoothA2dpEnabled;
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_HFP_ENABLED:
            state->u.boolean = (uint32)amaFeatureState.bluetoothHfpEnabled;
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_A2DP_CONNECTED:
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_HFP_CONNECTED:
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_BLUETOOTH_CLASSIC_DISCOVERABLE:
            state->value_case = STATE__VALUE_BOOLEAN;
            break;
        case AMA_FEATURE_DEVICE_CALIBRATION_REQUIRED:
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_DEVICE_THEME:
            state->value_case = STATE__VALUE_INTEGER;
            break;

#ifdef ENABLE_ANC
       case AMA_FEATURE_ANC_ENABLE:
            state->u.boolean = VoiceUi_IsStaticAncEnabled();
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

       case AMA_FEATURE_PASSTHROUGH_ENABLE:
            state->u.boolean = VoiceUi_IsLeakthroughEnabled();
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

       case AMA_FEATURE_PASSTHROUGH_LEVEL:
           state->u.integer = VoiceUi_GetLeakthroughLevelAsPercentage();
           state->value_case = STATE__VALUE_INTEGER;
           break;
#endif /* ENABLE_ANC */

        case AMA_FEATURE_EQUALIZER_BASS:
            state->u.integer = Ama_EqGetEqualizerBass();
            state->value_case = STATE__VALUE_INTEGER;
            break;

        case AMA_FEATURE_EQUALIZER_MID:
            state->u.integer = Ama_EqGetEqualizerMid();
            state->value_case = STATE__VALUE_INTEGER;
            break;

        case AMA_FEATURE_EQUALIZER_TREBLE:
            state->u.integer = Ama_EqGetEqualizerTreble();
            state->value_case = STATE__VALUE_INTEGER;
            break;

        case AMA_FEATURE_PRIVACY_MODE:
            state->u.boolean = VoiceUi_IsPrivacyModeEnabled();
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        case AMA_FEATURE_WAKE_WORD:
            state->u.boolean = VoiceUi_IsWakeUpWordDetectionEnabled();
            state->value_case = STATE__VALUE_BOOLEAN;
            break;

        /* cannot get state for the features below */
        case AMA_FEATURE_DEVICE_DND_ENABLED:
        case AMA_FEATURE_DEVICE_CELLULAR_CONNECTIVITY_STATUS:
        case AMA_FEATURE_MESSAGE_NOTIFICATION:
        case AMA_FEATURE_REMOTE_NOTIFICATION:
        case AMA_FEATURE_CALL_NOTIFICATION:
            error_code = ERROR_CODE__UNSUPPORTED;
            break;

       default:
            error_code = ERROR_CODE__INVALID;
            break;
    }

    if(error_code == ERROR_CODE__SUCCESS)
    {
        if(state->value_case == STATE__VALUE_BOOLEAN)
        {
            DEBUG_LOG_VERBOSE("AmaState_GetState enum:State__ValueCase:%d value %u", state->value_case, state->u.boolean);
        }
        else if(state->value_case == STATE__VALUE_INTEGER)
        {
            DEBUG_LOG_VERBOSE("AmaState_GetState enum:State__ValueCase:%d value %u", state->value_case, state->u.integer);
        }
    }

    state->feature = feature;

    return error_code;
}

ErrorCode AmaState_SetState(uint32 feature, State__ValueCase valueCase, uint32 state)
{
    ErrorCode errorCode = ERROR_CODE__SUCCESS;

    DEBUG_LOG("AmaState_SetState feature 0x%04X state %u", feature, state);

    if(valueCase != STATE__VALUE_BOOLEAN &&
       valueCase != STATE__VALUE_INTEGER)
    {

        return ERROR_CODE__UNSUPPORTED;
    }

    switch(feature)
    {
        case AMA_FEATURE_BLUETOOTH_A2DP_ENABLED:
        case AMA_FEATURE_BLUETOOTH_HFP_ENABLED:
        case AMA_FEATURE_BLUETOOTH_CLASSIC_DISCOVERABLE:
        case AMA_FEATURE_DEVICE_CALIBRATION_REQUIRED:
        case AMA_FEATURE_DEVICE_THEME:
        break;

#ifdef ENABLE_ANC
        case AMA_FEATURE_ANC_ENABLE:
        {
            bool enable = (bool)state;
            if(enable)
            {
                VoiceUi_EnableStaticAnc();
            }
            else
            {
                VoiceUi_DisableStaticAnc();
            }
        }
        break;

        case AMA_FEATURE_PASSTHROUGH_ENABLE:
        {
            bool enable = (bool)state;
            if(enable)
            {
                VoiceUi_EnableLeakthrough();
            }
            else
            {
                VoiceUi_DisableLeakthrough();
            }
        }
        break;

        case AMA_FEATURE_PASSTHROUGH_LEVEL:
        {
            uint16 level = (uint16)state;
			uint16 percentage = amaState_ConvertPassthroughLevelToPercentage(level);
            VoiceUi_SetLeakthroughLevelFromPercentage((uint8)percentage);
        }
        break;
#endif /* ENABLE_ANC */

       case AMA_FEATURE_EQUALIZER_BASS:
           Ama_EqSetEqualizerBass(state);
           break;
       
       case AMA_FEATURE_EQUALIZER_MID:
           Ama_EqSetEqualizerMid(state);
           break;
       
       case AMA_FEATURE_EQUALIZER_TREBLE:
           Ama_EqSetEqualizerTreble(state);
           break;

       case AMA_FEATURE_PRIVACY_MODE:
            VoiceUi_SetPrivacyModeEnable(state);
            break;
#ifdef INCLUDE_AMA_WUW
       case AMA_FEATURE_WAKE_WORD:
            VoiceUi_SetWakeUpWordDetectionEnable(state);
            break;
#endif

       case AMA_FEATURE_AUXILIARY_CONNECTED:
       case AMA_FEATURE_BLUETOOTH_A2DP_CONNECTED:
       case AMA_FEATURE_BLUETOOTH_HFP_CONNECTED:

       errorCode = ERROR_CODE__UNSUPPORTED;
       break;


       default:
       break;
    }

    return errorCode;
}

ErrorCode AmaState_SynchronizeState(uint32 feature, State__ValueCase value_case, uint32 value)
{
    ErrorCode error_code = ERROR_CODE__SUCCESS;

    DEBUG_LOG("AmaState_SynchronizeState: feature=0x%04X type=enum:State__ValueCase:%d value=%u",
              feature, value_case, value);

    switch (feature)
    {
    case AMA_FEATURE_CONNECTION_SUCCEEDED:
        if (value_case == STATE__VALUE_BOOLEAN)
        {
            AmaAudio_SetAlexaReady((bool) value);
        }
        else
        {
            error_code = ERROR_CODE__UNSUPPORTED;
        }

        break;

    default:
        break;
    }

    DEBUG_LOG("AmaState_SynchronizeState: result=enum:ErrorCode:%d", error_code);

    return error_code;
}

#endif /* INCLUDE_AMA */
