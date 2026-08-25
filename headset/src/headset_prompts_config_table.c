/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_prompts_config_table.c
\brief      Headset prompts configuration table module
*/
#include "headset_prompts_config_table.h"

#include <domain_message.h>
#include <ui_indicator_prompts.h>
#include <av.h>
#include <pairing.h>
#include <handset_service_protected.h>
#include <power_manager.h>
#include <voice_ui.h>

#ifdef INCLUDE_PROMPTS
const ui_event_indicator_table_t headset_ui_prompts_table[] =
{
#ifndef EXCLUDE_POWER_PROMPTS
    {.sys_event=POWER_ON,                {.prompt.filename = PROMPT_FILENAME("power_on"),
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE },
                                          .await_indication_completion = TRUE },
    {.sys_event=POWER_OFF,              { .prompt.filename = PROMPT_FILENAME("power_off"),
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE },
                                          .await_indication_completion = TRUE },
#endif
    {.sys_event=PAIRING_ACTIVE,         { .prompt.filename = PROMPT_FILENAME("pairing"),
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }},
    {.sys_event=PAIRING_COMPLETE,       { .prompt.filename = PROMPT_FILENAME("pairing_successful"),
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }},
    {.sys_event=PAIRING_FAILED,         { .prompt.filename = PROMPT_FILENAME("pairing_failed"),
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }},
#ifndef EXCLUDE_CONN_PROMPTS
    {.sys_event=HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND,    { .prompt.filename = PROMPT_FILENAME("connected"),
                                                                  .prompt.rate = 48000,
                                                                  .prompt.format = PROMPT_FORMAT,
                                                                  .prompt.interruptible = FALSE,
                                                                  .prompt.queueable = TRUE,
                                                                  .prompt.requires_repeat_delay = FALSE }},
    {.sys_event=HANDSET_SERVICE_DISCONNECTED_IND,               { .prompt.filename = PROMPT_FILENAME("disconnected"),
                                                                  .prompt.rate = 48000,
                                                                  .prompt.format = PROMPT_FORMAT,
                                                                  .prompt.interruptible = FALSE,
                                                                  .prompt.queueable = TRUE,
                                                                  .prompt.requires_repeat_delay = FALSE }},
#endif
#ifdef INCLUDE_GAA
    {.sys_event=VOICE_UI_MIC_OPEN,      { .prompt.filename = PROMPT_FILENAME("mic_open"),
                                          .prompt.rate = 16000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = TRUE,
                                          .prompt.queueable = FALSE,
                                          .prompt.requires_repeat_delay = FALSE }},
    {.sys_event=VOICE_UI_MIC_CLOSE,     { .prompt.filename = PROMPT_FILENAME("mic_close"),
                                          .prompt.rate = 16000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = TRUE,
                                          .prompt.queueable = FALSE,
                                          .prompt.requires_repeat_delay = FALSE }},
    {.sys_event=VOICE_UI_DISCONNECTED,  { .prompt.filename = PROMPT_FILENAME("bt_va_not_connected"),
                                          .prompt.rate = 48000,
                                          .prompt.format = PROMPT_FORMAT,
                                          .prompt.interruptible = FALSE,
                                          .prompt.queueable = TRUE,
                                          .prompt.requires_repeat_delay = TRUE }}
#endif /* INCLUDE_GAA */
};
#endif
uint8 HeadsetPromptsConfigTable_GetSize(void)
{
        #ifdef INCLUDE_PROMPTS
            return ARRAY_DIM(headset_ui_prompts_table);
        #else
            return 0;
        #endif
}

