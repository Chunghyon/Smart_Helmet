/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       self_speech.c
\brief     Self Speech support using VAD over Bone conducting mic
*/

#ifdef ENABLE_SELF_SPEECH

#include "self_speech.h"
#include "self_speech_tws.h"
#include "kymera_self_speech_detect.h"
#include "kymera.h"
#include "phy_state.h"

typedef struct
{
    TaskData task;
}selfSpeechTaskData;

static selfSpeechTaskData self_speech_task_data;

#define selfSpeechGetTaskData() (&self_speech_task_data)
#define selfSpeechGetTask()     (&self_speech_task_data.Task)

static void selfSpeech_msgHandler(Task task, MessageId id, Message message);

static void selfSpeech_HandlePhyStateChangedInd(PHY_STATE_CHANGED_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("selfSpeech_HandlePhyStateChangedInd  new state %d, event %d ", ind->new_state, ind->event);

    switch(ind->new_state)
    {
        case PHY_STATE_IN_EAR:
            Self_Speech_Enable();
        break;
        
        case PHY_STATE_OUT_OF_EAR:
        case PHY_STATE_OUT_OF_EAR_AT_REST:
        case PHY_STATE_IN_CASE:
            Self_Speech_Disable();
            break;

        default:
            break;
    }
}


static void selfSpeech_msgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    switch(id)
    {
        case PHY_STATE_CHANGED_IND:
            selfSpeech_HandlePhyStateChangedInd((PHY_STATE_CHANGED_IND_T*)message);
            break;

        default:
        break;
    }
}

static bool selfSpeech_GetInEarStatus(void)
{
    return (appPhyStateGetState()==PHY_STATE_IN_EAR) ? (TRUE):(FALSE);
}

bool SelfSpeech_Init(Task init_task)
{
    UNUSED(init_task);
    selfSpeechTaskData *taskData = selfSpeechGetTaskData();
    taskData->task.handler=selfSpeech_msgHandler;

    /*Register with Kymera tasks to get the trigger and release from VAD*/
    Kymera_ClientRegister(&taskData->task);
    
    /* Register with Physical state as observer to know if there are any physical state changes */
    appPhyStateRegisterClient(&taskData->task);

    /*Check mic config?*/
    return TRUE;
}

void Self_Speech_Enable(void)
{
    if (selfSpeech_GetInEarStatus() 
        && SelfSpeechTws_IsPrimary())
    {
        DEBUG_LOG_FN_ENTRY("Self_Speech_Enable");
        KymeraSelfSpeechDetect_Enable();
    }
}

void Self_Speech_Disable(void)
{
    DEBUG_LOG_FN_ENTRY("Self_Speech_Disable");
    KymeraSelfSpeechDetect_Disable();
}


#endif /*ENABLE_SELF_SPEECH*/
