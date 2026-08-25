/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       headset_leds_config_table.h
\brief      Header file for the Headset led config table.
*/

#ifndef HEADSET_LEDS_CONFIG_TABLE_H
#define HEADSET_LEDS_CONFIG_TABLE_H

#include <csrtypes.h>
#include <ui_indicator_leds.h>

extern const ui_event_indicator_table_t headset_ui_leds_table[];
extern const ui_provider_context_consumer_indicator_table_t headset_ui_leds_context_indications_table[];
uint8 HeadsetLedsConfigTable_GetSize(void);
uint8 HeadsetLedsConfigTable_ContextsTableGetSize(void);
#endif // HEADSET_LEDS_CONFIG_TABLE_H
