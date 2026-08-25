/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       ama_actions.c
\brief      Implementation for ama user events
*/
#ifdef INCLUDE_AMA
#include "ama_actions.h"
#include "ama.h"
#include "ama_audio.h"
#include "ama_config.h"
#include "ama_data.h"
#include "ama_send_command.h"
#include "ama_speech_config.h"
#include "bt_device.h"
#include <stdlib.h>
#include <voice_ui.h>
#ifdef INCLUDE_AMA_WUW
#include <voice_ui_va_client_if.h>
#endif

#define NUM_VA_TRANSLATION_TABLES ARRAY_DIM(va_translation_tables)

/* Ama Events */
typedef enum
{
    AMA_BUTTON_TAPPED,
    AMA_BUTTON_DOUBLE_TAPPED
}ama_button_events_t;

typedef struct
{
    ui_input_t voice_assistant_user_event;
    ama_button_events_t action_event;
} va_event_translation_t;

typedef struct
{
    const va_event_translation_t* event_translations;
    unsigned num_translations;
} va_translation_table_t;

static void amaActions_SetVaActionsTranslationTable(uint8 translation_id);
static void amaActions_ActionOnEvent(ama_button_events_t event_id);
static va_translation_table_t va_translation_table;

static const va_event_translation_t one_button_va_event_translations[] =
{
    { ui_input_va_3, AMA_BUTTON_TAPPED},
    { ui_input_va_4, AMA_BUTTON_DOUBLE_TAPPED}
};

static const va_event_translation_t custom_rdp_va_event_translations[] =
{
    { ui_input_va_5, AMA_BUTTON_TAPPED},
};

static va_translation_table_t va_translation_tables[] =
{
    {one_button_va_event_translations, ARRAY_DIM(one_button_va_event_translations)},
    {custom_rdp_va_event_translations, ARRAY_DIM(custom_rdp_va_event_translations)}
};

/************************************************************************/
void AmaActions_Init(void)
{
    amaActions_SetVaActionsTranslationTable(ama_GetActionMapping());
}
/************************************************************************/
static void amaActions_SetVaActionsTranslationTable(uint8 translation_id)
{
     if(translation_id < NUM_VA_TRANSLATION_TABLES)
        va_translation_table = va_translation_tables[translation_id];
     else
         Panic();
}
/************************************************************************/
bool AmaActions_HandleVaEvent(ui_input_t voice_assistant_user_event)
{
    ama_button_events_t action_event;
    uint16 index;
    bool handled = FALSE;
    for (index = 0; (index < va_translation_table.num_translations); index++)
    {
        if (voice_assistant_user_event == va_translation_table.event_translations[index].voice_assistant_user_event)
        {
            action_event = va_translation_table.event_translations[index].action_event;
            DEBUG_LOG("AmaActions_HandleVaEvent sending action enum:ama_button_events_t:%d", action_event);
            amaActions_ActionOnEvent(action_event);
            handled = TRUE;
        }   
    }
    return handled;
}

/************************************************************************/

static void amaActions_ActionOnEvent(ama_button_events_t event_id)
{
    DEBUG_LOG_DEBUG("amaActions_ActionOnEvent: enum:ama_button_events_t:%d", event_id);

    switch(event_id)
    {
        case AMA_BUTTON_TAPPED:
        {
            if(AmaData_IsSendingVoiceData())
            {
                AmaAudio_Cancel();
            }

            AmaAudio_StartButtonActivatedCapture(SPEECH_INITIATOR__TYPE__TAP);
        }
        break;

        case AMA_BUTTON_DOUBLE_TAPPED:
        {
            if (VoiceUi_IsSessionInProgress())
            {
                if(AmaData_IsSendingVoiceData())
                {
                    DEBUG_LOG_VERBOSE("amaActions_ActionOnEvent: AMA_BUTTON_DOUBLE_TAPPED: Cancel session");
                    AmaAudio_Cancel();
                }
            }
            else
            {
                DEBUG_LOG_VERBOSE("amaActions_ActionOnEvent: AMA_BUTTON_DOUBLE_TAPPED: Toggle privacy mode");
                VoiceUi_SetPrivacyModeEnable(!VoiceUi_IsPrivacyModeEnabled());
            }
        }
        break;

        default:
        {
            DEBUG_LOG_WARN("amaActions_ActionOnEvent: Unhandled button");
        }
        break;
    }
}
#endif /* INCLUDE_AMA */
