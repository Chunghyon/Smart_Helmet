/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Test interface for stream control like call accept, call reject, play, pause, etc
*/

/*! @{ */

#ifndef STREAM_CONTROL_TEST_H
#define STREAM_CONTROL_TEST_H

/*! \brief Send the Media Control pause command to the Handset
*/
void appTestMediaControlPause(void);

/*! \brief Send Media Control pause command to pause all the audio sources
*/
void appTestMediaControlPauseAll(void);

/*! \brief Send the Media Control play command to the Handset
*/
void appTestMediaControlPlay(void);

/*! \brief Send the Media Control stop command to the Handset
*/
void appTestMediaControlStop(void);

/*! \brief Send the Media Control forward command to the Handset
*/
void appTestMediaControlForward(void);

/*! \brief Send the Media Control backward command to the Handset
*/
void appTestMediaControlBackward(void);

/*! \brief Send the Media Control play/pause toggle command
*/
void appTestMediaControlTogglePlayPause(void);

/*! \brief Send the Media Control fast forward start command to the Handset
*/
void appTestMediaControlFastForwardStart(void);

/*! \brief Send the Media Control fast forward stop command to the Handset
*/
void appTestMediaControlFastForwardStop(void);

/*! \brief Send the Media Control rewind start command to the Handset
*/
void appTestMediaControlRewindStart(void);

/*! \brief Send the Media Control rewind stop command to the Handset
*/
void appTestMediaControlRewindStop(void);

/*! \brief Accept incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetVoiceCallAccept(void);

/*! \brief Reject incoming call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetVoiceCallReject(void);

/*! \brief End the current call, either local or forwarded (SCO forwarding)
*/
void appTestHandsetVoiceCallHangup(void);

#endif /* STREAM_CONTROL_TEST_H */

/*! @} */
