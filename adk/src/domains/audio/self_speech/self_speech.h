/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       self_speech.h
\brief     Self Speech support using VAD over Bone conducting mic
*/

#ifndef SELF_SPEECH_H
#define SELF_SPEECH_H

/*! \brief Init Self Speech Detect
*/
#ifdef ENABLE_SELF_SPEECH
bool SelfSpeech_Init(Task init_task);
#else
#define SelfSpeech_Init(init_task) FALSE
#endif

/*! \brief Enable Self Speech Detect
*/
#ifdef ENABLE_SELF_SPEECH
void Self_Speech_Enable(void);
#else
#define Self_Speech_Enable() ((void)(0))
#endif


/*! \brief Disable Self Speech Detect
*/
#ifdef ENABLE_SELF_SPEECH
void Self_Speech_Disable(void);
#else
#define Self_Speech_Disable() ((void)(0))
#endif

#endif /* SELF_SPEECH_H */
