/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_self_speech_detect.c
\brief     Implementation of Kymera Audio Chain related functionality for Self Speech Detect
*/

#include "kymera_self_speech_detect.h"
#include "kymera_anc_common.h"
#include "kymera_mic_if.h"
#include "kymera_data.h"
#include "kymera_ucid.h"
#include "kymera_setup.h"
#include "kymera_internal_msg_ids.h"
#include "anc_state_manager.h"

#if defined(ENABLE_SELF_SPEECH)

static kymera_chain_handle_t self_speech_detect_chain;

#define kymeraSelfSpeechDetect_IsActive() (KymeraSelfSpeechDetect_GetChain() != NULL)
#define kymeraSelfSpeechDetect_PanicIfNotActive()  if(!kymeraSelfSpeechDetect_IsActive()) \
                                                                Panic()
#define kymeraSelfSpeechDetect_PanicIfActive()  if(kymeraSelfSpeechDetect_IsActive()) \
                                                                Panic()
                                                                
kymera_chain_handle_t KymeraSelfSpeechDetect_GetChain(void)
{
    return self_speech_detect_chain;
}

static Operator KymeraSelfSpeechDetect_GetVadOperator(void)
{
    return ChainGetOperatorByRole(KymeraSelfSpeechDetect_GetChain(), OPR_SELF_SPEECH_VAD);
}

static void kymeraSelfSpeechDetect_SetChain(kymera_chain_handle_t chain)
{
    self_speech_detect_chain = chain;
}

static void kymeraSelfSpeechDetect_SetUcid(void)
{
    PanicFalse(Kymera_SetOperatorUcid(KymeraSelfSpeechDetect_GetChain(), OPR_SELF_SPEECH_VAD, UCID_SELF_SPEECH_ATR_VAD));
    PanicFalse(Kymera_SetOperatorUcid(KymeraSelfSpeechDetect_GetChain(), OPR_SELF_SPEECH_PEQ, UCID_SELF_SPEECH_PEQ));
}

void KymeraSelfSpeechDetect_Create(void)
{
    kymeraSelfSpeechDetect_PanicIfActive();
    kymeraSelfSpeechDetect_SetChain(PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_self_speech_detect_config)));
}

void KymeraSelfSpeechDetect_SetSysMode(atr_vad_sysmode_t mode)
{
    Operator op = KymeraSelfSpeechDetect_GetVadOperator();
    
    if(op)
    {
        OperatorsAtrVadSetSysmodeCtrl(op, mode);
    }
}

void KymeraSelfSpeechDetect_SetReleaseDuration(atr_vad_release_duration_t duration)
{
    Operator op = KymeraSelfSpeechDetect_GetVadOperator();
    
    if(op)
    {
        OperatorsAtrVadSetReleaseDuration(op, duration);
    }
}


void KymeraSelfSpeechDetect_Configure(void)
{   
    DEBUG_LOG("KymeraSelfSpeechDetect_Configure");
    if (KymeraSelfSpeechDetect_IsActive())
    {    
        kymeraTaskData *theKymera = KymeraGetTaskData();
        Operator op = KymeraSelfSpeechDetect_GetVadOperator();
        
        if (op)
        {
            /* UCID configuration */
            kymeraSelfSpeechDetect_SetUcid();
            KymeraSelfSpeechDetect_SetReleaseDuration(AncAutoAmbient_GetReleaseTimeConfig());

            /* Associate kymera task to receive unsolicited messages */
            MessageOperatorTask(op, &theKymera->task);
        }
    }
}

void KymeraSelfSpeechDetect_Connect(void)
{
    if (KymeraSelfSpeechDetect_IsActive())
    {
        DEBUG_LOG("KymeraSelfSpeechDetect_Connect");
        ChainConnect(KymeraSelfSpeechDetect_GetChain());
    }
}

void KymeraSelfSpeechDetect_Disconnect(void)
{
    if (KymeraSelfSpeechDetect_IsActive())
    {
        DEBUG_LOG("KymeraSelfSpeechDetect_Disconnect");
        StreamDisconnect(ChainGetOutput(KymeraSelfSpeechDetect_GetChain(), EPR_SELF_SPEECH_PEQ_OUT), ChainGetInput(KymeraSelfSpeechDetect_GetChain(), EPR_SELF_SPEECH_VAD_MIC_IN));
    }
}

void KymeraSelfSpeechDetect_Start(void)
{
    if (KymeraSelfSpeechDetect_IsActive())
    {
        DEBUG_LOG("KymeraSelfSpeechDetect_Start");
        ChainStart(KymeraSelfSpeechDetect_GetChain());
    }
}

void KymeraSelfSpeechDetect_Stop(void)
{
    if (KymeraSelfSpeechDetect_IsActive())
    {
        DEBUG_LOG("KymeraSelfSpeechDetect_Stop");
        ChainStop(KymeraSelfSpeechDetect_GetChain());
    }
}

void KymeraSelfSpeechDetect_Destroy(void)
{
    if (KymeraSelfSpeechDetect_IsActive())
    {
        ChainDestroy(KymeraSelfSpeechDetect_GetChain());
        kymeraSelfSpeechDetect_SetChain(NULL);
    }
}

bool KymeraSelfSpeechDetect_IsActive(void)
{
    return kymeraSelfSpeechDetect_IsActive();
}

Sink KymeraSelfSpeechDetect_GetMicPathSink(void)
{
    kymeraSelfSpeechDetect_PanicIfNotActive();
    return ChainGetInput(KymeraSelfSpeechDetect_GetChain(), EPR_SELF_SPEECH_PEQ_IN);
}

void KymeraSelfSpeechDetect_Enable(void)
{
    if (AncAutoAmbient_IsEnabled() && AncAutoAmbient_IsAmbientModeConfigured())
    {
        KymeraAncCommon_SelfSpeechDetectEnable();
    }
}

void KymeraSelfSpeechDetect_Disable(void)
{
    KymeraAncCommon_SelfSpeechDetectDisable();

    /*Go back to previous ANC state/mode when feature is disabled*/
    AncAutoAmbient_Release();
}

#endif

