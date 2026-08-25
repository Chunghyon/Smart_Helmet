/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Headset Tones UI Indicator configuration table
*/

#include "headset_tones_config_table.h"

#include "headset_tones.h"

#include "headset_buttons.h"

#include <domain_message.h>
#include <ui_inputs.h>
#include <ui_indicator_tones.h>

#include <av.h>
#include <gaming_mode.h>
#include <pairing.h>
#include <telephony_messages.h>
#include <handset_service_protected.h>
#include <power_manager.h>
#include <volume_service.h>
#ifdef INCLUDE_AMA
#include <voice_ui_message_ids.h>
#endif

#if INCLUDE_TONES
const ui_event_indicator_table_t headset_ui_tones_table[] =
{
    {.sys_event=TELEPHONY_INCOMING_CALL_OUT_OF_BAND_RINGTONE,  { .tone.tone = app_tone_hfp_ring,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_TRANSFERED,                          { .tone.tone = app_tone_hfp_talk_long_press,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_LINK_LOSS_OCCURRED,                  { .tone.tone = app_tone_hfp_link_loss,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_ERROR,                               { .tone.tone = app_tone_error,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=AV_LINK_LOSS,                                  { .tone.tone = app_tone_av_link_loss,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=AV_ERROR,                                      { .tone.tone = app_tone_error,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#ifdef INCLUDE_AV
    {.sys_event=AV_A2DP_NOT_ROUTED,                            { .tone.tone = app_tone_a2dp_not_routed,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif /* INCLUDE_AV */
#ifdef INCLUDE_GAMING_MODE
    {.sys_event=GAMING_MODE_ON,                                { .tone.tone = app_tone_gaming_mode_on,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=GAMING_MODE_OFF,                               { .tone.tone = app_tone_gaming_mode_off,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif																  
    {.sys_event=VOLUME_SERVICE_MIN_VOLUME,                     { .tone.tone = app_tone_volume_limit,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=VOLUME_SERVICE_MAX_VOLUME,                     { .tone.tone = app_tone_volume_limit,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=LI_MFB_BUTTON_HELD_1SEC,                      { .tone.tone = app_tone_button,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#ifndef INCLUDE_GAMING_MODE
    {.sys_event=LI_MFB_BUTTON_HELD_3SEC,                      { .tone.tone = app_tone_button_2,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif
    {.sys_event=LI_MFB_BUTTON_HELD_6SEC,                      { .tone.tone = app_tone_button_3,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=LI_MFB_BUTTON_HELD_8SEC,                      { .tone.tone = app_tone_button_4,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=LI_MFB_BUTTON_HELD_FACTORY_RESET_DS,                 { .tone.tone = app_tone_button_factory_reset,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#ifdef EXCLUDE_CONN_PROMPTS
    {.sys_event=HANDSET_SERVICE_FIRST_PROFILE_CONNECTED_IND,    { .tone.tone = app_tone_hfp_connected,
                                                                  .tone.queueable = TRUE,
                                                                  .tone.interruptible = FALSE }},
    {.sys_event=HANDSET_SERVICE_DISCONNECTED_IND,               { .tone.tone = app_tone_hfp_disconnected,
                                                                  .tone.queueable = TRUE,
                                                                  .tone.interruptible = FALSE }},
#endif
#ifdef INCLUDE_AMA
    {.sys_event=VOICE_UI_AMA_UNREGISTERED,                     { .tone.tone = app_tone_ama_unregistered,
                                                                  .tone.queueable = TRUE,
                                                                  .tone.interruptible = FALSE }},
    {.sys_event=VOICE_UI_AMA_NOT_CONNECTED,                    { .tone.tone = app_tone_ama_not_connected,
                                                                    .tone.queueable = TRUE,
                                                                    .tone.interruptible = FALSE }},
    {.sys_event=VOICE_UI_AMA_PRIVACY_MODE_ENABLED,             { .tone.tone = app_tone_ama_privacy_mode_enabled,
                                                                    .tone.queueable = TRUE,
                                                                    .tone.interruptible = FALSE }},
    {.sys_event=VOICE_UI_AMA_PRIVACY_MODE_DISABLED,            { .tone.tone = app_tone_ama_privacy_mode_disabled,
                                                                    .tone.queueable = TRUE,
                                                                    .tone.interruptible = FALSE }},
#endif  /* INCLUDE_AMA */
#ifdef EXCLUDE_POWER_PROMPTS
    {.sys_event=POWER_ON,                                     { .tone.tone = app_tone_power_on,
                                                                  .tone.queueable = TRUE,
                                                                  .tone.interruptible = FALSE,
                                                                  .tone.button_feedback = TRUE }},
#endif
    {.sys_event=POWER_OFF,                                     { .tone.tone = app_tone_battery_empty,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE },
                                                                 .await_indication_completion = TRUE}
};

const ui_repeating_indication_table_t headset_ui_repeating_tones_table[] =
{
    {.triggering_sys_event = TELEPHONY_MUTE_ACTIVE,              .reminder_period = 15,
                                                                 .cancelling_sys_event = TELEPHONY_MUTE_INACTIVE,
                                                               { .tone.tone = app_tone_hfp_mute_reminder,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }}
};
#endif

uint8 HeadsetTonesConfigTable_SingleGetSize(void)
{
#if INCLUDE_TONES
    return ARRAY_DIM(headset_ui_tones_table);
#else
    return 0;
#endif
}

uint8 HeadsetTonesConfigTable_RepeatingGetSize(void)
{
#if INCLUDE_TONES
    return ARRAY_DIM(headset_ui_repeating_tones_table);
#else
    return 0;
#endif
}

