/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Test interface for stream control like call accept, call reject, play, pause, etc
*/

#include "stream_control_test.h"
#include "ui.h"
#include <logging.h>

#ifdef GC_SECTIONS
/* Move all functions in KEEP_PM section to ensure they are not removed during
 * garbage collection */
#pragma unitcodesection KEEP_PM
#endif

void appTestMediaControlPause(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlPause");
    Ui_InjectUiInput(ui_input_pause);
}

/*! \brief Send the Media Control pause all command to the Handset
*/
void appTestMediaControlPauseAll(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlPauseAll");
    Ui_InjectUiInput(ui_input_pause_all);
}

/*! \brief Send the Media Control play command to the Handset
*/
void appTestMediaControlPlay(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlPlay");
    Ui_InjectUiInput(ui_input_play);
}

/*! \brief Send the Media Control stop command to the Handset
*/
void appTestMediaControlStop(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlStop");
    Ui_InjectUiInput(ui_input_stop_av_connection);
}

/*! \brief Send the Media Control forward command to the Handset
*/
void appTestMediaControlForward(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlForward");
    Ui_InjectUiInput(ui_input_av_forward);
}

/*! \brief Send the Media Control backward command to the Handset
*/
void appTestMediaControlBackward(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlBackward");
    Ui_InjectUiInput(ui_input_av_backward);
}

/*! \brief Send the Media Control fast forward start command to the Handset
*/
void appTestMediaControlFastForwardStart(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlFastForwardStart");
    Ui_InjectUiInput(ui_input_av_fast_forward_start);
}

/*! \brief Send the Media Control fast forward stop command to the Handset
*/
void appTestMediaControlFastForwardStop(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlFastForwardStop");
    Ui_InjectUiInput(ui_input_fast_forward_stop);
}

/*! \brief Send the Media Control rewind start command to the Handset
*/
void appTestMediaControlRewindStart(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlRewindStart");
    Ui_InjectUiInput(ui_input_av_rewind_start);
}

/*! \brief Send the Media Control rewind stop command to the Handset
*/
void appTestMediaControlRewindStop(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlRewindStop");
    Ui_InjectUiInput(ui_input_rewind_stop);
}

/*! \brief Send the Media Control play/pause toggle command
*/
void appTestMediaControlTogglePlayPause(void)
{
    DEBUG_LOG_ALWAYS("appTestMediaControlTogglePlayPause");
    Ui_InjectUiInput(ui_input_toggle_play_pause);
}

/*! \brief Send the incoming voice call accept command
*/
void appTestHandsetVoiceCallAccept(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallAccept");
    Ui_InjectUiInput(ui_input_voice_call_accept);
}

/*! \brief Send the incoming voice call reject command
*/
void appTestHandsetVoiceCallReject(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallReject");
    Ui_InjectUiInput(ui_input_voice_call_reject);
}

/*! \brief Send the ongoing voice call hangup command
*/
void appTestHandsetVoiceCallHangup(void)
{
    DEBUG_LOG_ALWAYS("appTestHandsetVoiceCallHangup");
    Ui_InjectUiInput(ui_input_voice_call_hang_up);
}
