#ifndef HEADSET_TONES_CONFIG_TABLE_H
#define HEADSET_TONES_CONFIG_TABLE_H

#include <csrtypes.h>
#include <ui_indicator_tones.h>

extern const ui_event_indicator_table_t headset_ui_tones_table[];
extern const ui_repeating_indication_table_t headset_ui_repeating_tones_table[];

uint8 HeadsetTonesConfigTable_SingleGetSize(void);
uint8 HeadsetTonesConfigTable_RepeatingGetSize(void);

#endif // HEADSET_TONES_CONFIG_TABLE_H
