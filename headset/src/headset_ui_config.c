/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       headset_ui_config.c
\brief      ui configuration table

    This file contains ui configuration table which maps different logical inputs to
    corresponding ui inputs based upon ui provider contexts.
*/

#include "headset_ui_config.h"
#include "ui.h"
#include "audio_curation.h"

#include "headset_buttons.h"

/* Needed for UI contexts - transitional; when table is code generated these can be anonymised
 * unsigned ints and these includes can be removed. */
#include "av.h"
#include "hfp_profile.h"
#include "bt_device.h"
#include "headset_sm.h"
#include "media_player.h"
#include "voice_ui.h"

#include <focus_select.h>
#include <telephony_service.h>
#include <handset_service.h>

const focus_select_audio_tie_break_t audio_source_focus_priority[] =
{
    FOCUS_SELECT_AUDIO_LINE_IN,
    FOCUS_SELECT_AUDIO_USB,
    FOCUS_SELECT_AUDIO_A2DP
};

const focus_select_voice_tie_break_t voice_source_focus_priority[] =
{
    FOCUS_SELECT_VOICE_HFP,
    FOCUS_SELECT_VOICE_USB
};

/*! \brief ui config table*/
const ui_config_table_content_t headset_ui_config_table[] =
{
    {APP_ANC_ENABLE,                   ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_on                       },

    {APP_ANC_DISABLE,                  ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_off                      },

    {APP_ANC_TOGGLE_ON_OFF,            ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_toggle_on_off            },
    {APP_ANC_TOGGLE_ON_OFF,            ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_toggle_on_off            },

    {APP_ANC_SET_NEXT_MODE,            ui_provider_audio_curation,  context_anc_enabled,                  ui_input_anc_set_next_mode            },
    {APP_ANC_SET_NEXT_MODE,            ui_provider_audio_curation,  context_anc_disabled,                 ui_input_anc_set_next_mode            },

    {APP_LEAKTHROUGH_TOGGLE_ON_OFF,    ui_provider_audio_curation,  context_leakthrough_disabled,         ui_input_leakthrough_toggle_on_off    },
    {APP_LEAKTHROUGH_TOGGLE_ON_OFF,    ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_toggle_on_off    },

    {APP_LEAKTHROUGH_SET_NEXT_MODE,    ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_set_next_mode    },

    {APP_LEAKTHROUGH_DISABLE,          ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_set_next_mode    },

    {APP_LEAKTHROUGH_ENABLE,           ui_provider_audio_curation,  context_leakthrough_enabled,          ui_input_leakthrough_set_next_mode    },

    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call,                ui_input_voice_call_hang_up           },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_ringing_outgoing,       ui_input_voice_call_hang_up           },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_accept            },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_incoming,  ui_input_voice_call_accept            },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_outgoing,  ui_input_voice_call_hang_up           },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_call_with_held,      ui_input_voice_call_hang_up           },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_call_held,              ui_input_voice_call_cycle             },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_telephony,       context_voice_in_multiparty_call,     ui_input_voice_call_hang_up           },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_media_player,    context_media_player_streaming,       ui_input_toggle_play_pause            },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_media_player,    context_media_player_idle,            ui_input_toggle_play_pause            },
    {LI_MFB_BUTTON_SINGLE_PRESS,       ui_provider_handset,         context_handset_not_connected,        ui_input_connect_handset              },

    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call,                ui_input_voice_transfer               },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_ringing_incoming,       ui_input_voice_call_reject            },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_incoming,  ui_input_voice_call_reject            },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_outgoing,  ui_input_voice_transfer               },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_call_with_held,      ui_input_voice_transfer               },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_call_held,              ui_input_voice_transfer               },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_telephony,       context_voice_in_multiparty_call,     ui_input_voice_transfer               },
    {LI_MFB_BUTTON_RELEASE_1SEC,       ui_provider_media_player,    context_media_player_streaming,       ui_input_stop_av_connection           },

    {LI_MFB_BUTTON_RELEASE_4SEC,       ui_provider_app_sm,          context_app_sm_active,                ui_input_sm_pair_handset              },

    {LI_MFB_BUTTON_RELEASE_3SEC,       ui_provider_telephony,       context_voice_in_call,                ui_input_mic_mute_toggle              },
    {LI_MFB_BUTTON_RELEASE_3SEC,       ui_provider_handset,         context_handset_connected,            ui_input_gaming_mode_toggle           },

    {LI_MFB_BUTTON_RELEASE_6SEC,       ui_provider_app_sm,          context_app_sm_inactive,              ui_input_sm_power_on                  },
    {LI_MFB_BUTTON_RELEASE_6SEC,       ui_provider_app_sm,          context_app_sm_active,                ui_input_sm_power_off                 },

    {LI_MFB_BUTTON_RELEASE_8SEC,       ui_provider_app_sm,          context_app_sm_active,                ui_input_sm_delete_handsets           },

    {LI_MFB_BUTTON_RELEASE_FACTORY_RESET_DS, ui_provider_app_sm,    context_app_sm_active,                ui_input_factory_reset_request        },

    {APP_VA_BUTTON_DOWN,               ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_1                         },
    {APP_VA_BUTTON_HELD_RELEASE,       ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_2                         },
    {APP_VA_BUTTON_SINGLE_CLICK,       ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_3                         },
    {APP_VA_BUTTON_DOUBLE_CLICK,       ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_4                         },
    {APP_VA_BUTTON_HELD_1SEC,          ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_5                         },
    {APP_VA_BUTTON_RELEASE,            ui_provider_voice_ui,        context_voice_ui_default,             ui_input_va_6                         },

#if defined(HAVE_4_BUTTONS) || defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_BUTTON_VOLUME_DOWN,           ui_provider_app_sm,          context_app_sm_active,                ui_input_volume_down_start            },
    {APP_BUTTON_VOLUME_UP,             ui_provider_app_sm,          context_app_sm_active,                ui_input_volume_up_start              },
    {APP_BUTTON_VOLUME_DOWN_RELEASE,   ui_provider_app_sm,          context_app_sm_active,                ui_input_volume_stop                  },
    {APP_BUTTON_VOLUME_UP_RELEASE,     ui_provider_app_sm,          context_app_sm_active,                ui_input_volume_stop                  },
#endif /* HAVE_4_BUTTONS || HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */

#if defined(HAVE_6_BUTTONS) || defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_BUTTON_FORWARD,              ui_provider_media_player,     context_media_player_streaming,       ui_input_av_forward                   },
    {APP_BUTTON_FORWARD_HELD,         ui_provider_media_player,     context_media_player_streaming,       ui_input_av_fast_forward_start        },
    {APP_BUTTON_FORWARD_HELD_RELEASE, ui_provider_media_player,     context_media_player_streaming,       ui_input_fast_forward_stop            },
    {APP_BUTTON_BACKWARD,             ui_provider_media_player,     context_media_player_streaming,       ui_input_av_backward                  },
    {APP_BUTTON_BACKWARD_HELD,        ui_provider_media_player,     context_media_player_streaming,       ui_input_av_rewind_start              },
    {APP_BUTTON_BACKWARD_HELD_RELEASE,ui_provider_media_player,     context_media_player_streaming,       ui_input_rewind_stop                  },
#endif /* HAVE_6_BUTTONS || HAVE_7_BUTTONS || HAVE_9_BUTTONS */

#if defined(HAVE_7_BUTTONS) || defined(HAVE_9_BUTTONS)
    {APP_BUTTON_PLAY_PAUSE_TOGGLE,    ui_provider_media_player,     context_media_player_streaming,       ui_input_toggle_play_pause            },
    {APP_BUTTON_PLAY_PAUSE_TOGGLE,    ui_provider_media_player,     context_media_player_idle,            ui_input_toggle_play_pause            },
#endif /* HAVE_7_BUTTONS || HAVE_9_BUTTONS */
};


const ui_config_table_content_t* HeadsetUi_GetConfigTable(unsigned* table_length)
{
    *table_length = ARRAY_DIM(headset_ui_config_table);
    return headset_ui_config_table;
}

void HeadsetUi_ConfigureFocusSelection(void)
{
    FocusSelect_ConfigureAudioSourceTieBreakOrder(audio_source_focus_priority);
    FocusSelect_ConfigureVoiceSourceTieBreakOrder(voice_source_focus_priority);
}

bool HeadsetUi_IsLogicalInputScreenedInLimboState(unsigned logical_input)
{
    switch (logical_input)
    {
    case LI_MFB_BUTTON_RELEASE_6SEC:
        /* Power On button press is not screened. */
        return FALSE;
    default:
        return TRUE;
    }
}
