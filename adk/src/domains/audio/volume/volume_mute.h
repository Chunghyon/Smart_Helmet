/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file		volume_mute.h
\defgroup   volume_mute Volume Mute
\ingroup    volume_group
\brief      The mute state
*/

#ifndef VOLUME_MUTE_H_
#define VOLUME_MUTE_H_

/*! @{ */

/*! \brief Gets the mute state.

    \return TRUE if muted, else FALSE
 */
bool Volume_GetMuteState(void);

/*! \brief Sets the mute state

    \param state The mute state, TRUE to mute, else FALSE
 */
void Volume_SetMuteState(bool state);

/*! @} */

#endif /* VOLUME_MUTE_H_ */
