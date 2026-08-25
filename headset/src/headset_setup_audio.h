/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_setup_audio.h
\brief      Initialises audio (set audio chains and other audio component config)
*/

#ifndef HEADESET_SETUP_AUDIO_H
#define HEADESET_SETUP_AUDIO_H

/*@{*/

/*! \brief Initialise/Setup audio components
    \param init_task Unused can be NULL
    \return TRUE on success, FALSE otherwise.
*/
bool Headset_InitAudio(Task init_task);

/*@}*/

#endif /* HEADESET_SETUP_AUDIO_H */
