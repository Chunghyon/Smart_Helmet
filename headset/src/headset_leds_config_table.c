/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_leds_config_table.c
\brief      Headset led configuration table module
*/
#include "headset_leds_config_table.h"
#include "headset_led.h"
#include "headset_sm.h"

#include <domain_message.h>
#include <ui_indicator_leds.h>
#include <pairing.h>
#include <charger_monitor.h>
#include <power_manager.h>
#include <telephony_messages.h>
#include <hfp_profile.h>
#include <av.h>
#include <media_player.h>
#include <voice_sources.h>

const ui_provider_context_consumer_indicator_table_t headset_ui_leds_context_indications_table[] =
{
    {.provider=ui_provider_handset_pairing,
     .context=context_handset_pairing_active,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_pairing,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_telephony,
     .context=context_voice_ringing_incoming,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_call_incoming,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_telephony,
     .context=context_voice_ringing_outgoing,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call,                    { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call_with_incoming,      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_call_incoming,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call_with_outgoing,      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_call_with_held,          { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_telephony,
     .context=context_voice_in_multiparty_call,         { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_sco,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_media_player,
     .context=context_media_player_streaming,           { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_streaming,
                                                          .led.priority = LED_PRI_LOW}},
    {.provider=ui_provider_app_sm,
     .context=context_app_sm_idle,                      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_idle,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_app_sm,
     .context=context_app_sm_idle_connected,            { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_idle_connected,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
    {.provider=ui_provider_app_sm,
     .context=context_app_sm_exit_idle,                 { .led.action = LED_STOP_PATTERN,
                                                          .led.data.pattern = NULL,
                                                          .led.priority = LED_PRI_LOW,
                                                          .led.local_only = TRUE }},
};

const ui_event_indicator_table_t headset_ui_leds_table[] =
{
    {.sys_event=POWER_ON,                               { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_power_on,
                                                          .led.priority = LED_PRI_EVENT,
                                                          .led.local_only = TRUE },
                                                          .await_indication_completion = TRUE },

    {.sys_event=POWER_OFF,                              { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_power_off,
                                                          .led.priority = LED_PRI_EVENT,
                                                          .led.local_only = TRUE },
                                                          .await_indication_completion = TRUE },

    {.sys_event=TELEPHONY_CALL_CONNECTION_FAILURE,      { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_error,
                                                          .led.priority = LED_PRI_EVENT,
                                                          .led.local_only = TRUE}},

    {.sys_event=TELEPHONY_ERROR,                        { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_error,
                                                          .led.priority = LED_PRI_EVENT,
                                                          .led.local_only = TRUE}},

    {.sys_event=AV_ERROR,                               { .led.action = LED_START_PATTERN,
                                                          .led.data.pattern = app_led_pattern_error,
                                                          .led.priority = LED_PRI_EVENT}},

    {.sys_event=CHARGER_MESSAGE_DETACHED,               { .led.action = LED_CANCEL_FILTER,
                                                          .led.data.filter = NULL,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},

    {.sys_event=CHARGER_MESSAGE_DISABLED,               { .led.action = LED_CANCEL_FILTER,
                                                          .led.data.filter = NULL,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},

    {.sys_event=CHARGER_MESSAGE_COMPLETED,              { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_charging_complete,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},

    {.sys_event=CHARGER_MESSAGE_CHARGING_OK,            { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_charging_ok,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},

    {.sys_event=CHARGER_MESSAGE_CHARGING_LOW,           { .led.action = LED_SET_FILTER,
                                                          .led.data.filter = app_led_filter_charging_low,
                                                          .led.priority = LED_PRI_MEDIUM,
                                                          .led.local_only = TRUE}},
};

uint8 HeadsetLedsConfigTable_GetSize(void)
{
    return ARRAY_DIM(headset_ui_leds_table);
}

uint8 HeadsetLedsConfigTable_ContextsTableGetSize(void)
{

    return ARRAY_DIM(headset_ui_leds_context_indications_table);
}
