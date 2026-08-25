
/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the Headset Application user interface tone indications.
*/
#ifndef HEADSET_TONES_H
#define HEADSET_TONES_H

#include "domain_message.h"
#include "kymera.h"

extern const ringtone_note app_tone_button[];
extern const ringtone_note app_tone_button_2[];
extern const ringtone_note app_tone_button_3[];
extern const ringtone_note app_tone_button_4[];
extern const ringtone_note app_tone_button_dfu[];
extern const ringtone_note app_tone_button_factory_reset[];

extern const ringtone_note app_tone_hfp_connect[];
extern const ringtone_note app_tone_hfp_connected[];
extern const ringtone_note app_tone_hfp_disconnected[];
extern const ringtone_note app_tone_hfp_link_loss[];
extern const ringtone_note app_tone_hfp_sco_connected[];
extern const ringtone_note app_tone_hfp_sco_disconnected[];
extern const ringtone_note app_tone_hfp_mute_reminder[];
extern const ringtone_note app_tone_hfp_sco_unencrypted_reminder[];
extern const ringtone_note app_tone_hfp_ring[];
extern const ringtone_note app_tone_hfp_ring_caller_id[];
extern const ringtone_note app_tone_hfp_voice_dial[];
extern const ringtone_note app_tone_hfp_voice_dial_disable[];
extern const ringtone_note app_tone_hfp_answer[];
extern const ringtone_note app_tone_hfp_hangup[];
extern const ringtone_note app_tone_hfp_mute_active[];
extern const ringtone_note app_tone_hfp_mute_inactive[];
extern const ringtone_note app_tone_hfp_talk_long_press[];
extern const ringtone_note app_tone_pairing[];
extern const ringtone_note app_tone_paired[];
extern const ringtone_note app_tone_pairing_deleted[];
extern const ringtone_note app_tone_volume[];
extern const ringtone_note app_tone_volume_limit[];
extern const ringtone_note app_tone_error[];
extern const ringtone_note app_tone_battery_empty[];
extern const ringtone_note app_tone_power_on[];
extern const ringtone_note app_tone_power_off[];
extern const ringtone_note app_tone_paging_reminder[];

#ifdef INCLUDE_AV
extern const ringtone_note app_tone_av_connect[];
extern const ringtone_note app_tone_av_disconnect[];
extern const ringtone_note app_tone_av_remote_control[];
extern const ringtone_note app_tone_av_connected[];
extern const ringtone_note app_tone_av_disconnected[];
extern const ringtone_note app_tone_av_link_loss[];
extern const ringtone_note app_tone_gaming_mode_on[];
extern const ringtone_note app_tone_gaming_mode_off[];
extern const ringtone_note app_tone_a2dp_not_routed[];
#endif

#ifdef INCLUDE_AMA
extern const ringtone_note app_tone_ama_unregistered[];
extern const ringtone_note app_tone_ama_not_connected[];
extern const ringtone_note app_tone_ama_privacy_mode_disabled[];
extern const ringtone_note app_tone_ama_privacy_mode_enabled[];
#endif  /* INCLUDE_AMA */

//!@}

#endif // HEADSET_TONES_H

