/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_wind_detect.c
\brief     Implementation of Kymera Wind Noise Detect and related functionality
*/

#include "kymera_wind_detect.h"
#include "kymera_data.h"
#include "kymera_setup.h"

#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_WIND_DETECT)

static kymera_chain_handle_t wind_detect_chain;

#define kymeraWindDetect_IsActive() (KymeraWindDetect_GetChain() != NULL)
#define kymeraWindDetect_PanicIfNotActive()  if(!kymeraWindDetect_IsActive()) \
                                                                Panic()
#define kymeraWindDetect_PanicIfActive()  if(kymeraWindDetect_IsActive()) \
                                                                Panic()
                                                                
kymera_chain_handle_t KymeraWindDetect_GetChain(void)
{
    return wind_detect_chain;
}

static Operator KymeraWindDetect_GetOperator(void)
{
    return ChainGetOperatorByRole(KymeraWindDetect_GetChain(), OPR_WIND_DETECT);
}

static void kymeraWindDetect_SetChain(kymera_chain_handle_t chain)
{
    wind_detect_chain = chain;
}

static void kymeraWindDetect_SetSysMode(wind_detect_sysmode_t mode)
{
    Operator op = KymeraWindDetect_GetOperator();
    
    if(op)
    {
        OperatorsWindDetectSetSysmodeCtrl(op, mode);
    }
}

void KymeraWindDetect_Create(void)
{
    kymeraWindDetect_PanicIfActive();
    kymeraWindDetect_SetChain(PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_wind_detect_config)));
}

void KymeraWindDetect_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    kymeraWindDetect_PanicIfNotActive();

    Operator op = KymeraWindDetect_GetOperator();

    if(op)
    {
        /* UCID configuration */
        KymeraWindDetect_SetUcid((kymera_operator_ucid_t)param->current_mode);

        /* Use default sample rate of 16K */
        /* Use default buffer size */

        /* Associate kymera task to receive unsolicited messages */
        MessageOperatorTask(op, &theKymera->task);
    }
}

void KymeraWindDetect_Connect(void)
{
    kymeraWindDetect_PanicIfNotActive();
    ChainConnect(KymeraWindDetect_GetChain());
}

void KymeraWindDetect_Start(void)
{
    kymeraWindDetect_PanicIfNotActive();
    ChainStart(KymeraWindDetect_GetChain());
}

void KymeraWindDetect_Stop(void)
{
    kymeraWindDetect_PanicIfNotActive();
    ChainStop(KymeraWindDetect_GetChain());
}

void KymeraWindDetect_Destroy(void)
{
    kymeraWindDetect_PanicIfNotActive();
    ChainDestroy(KymeraWindDetect_GetChain());
    kymeraWindDetect_SetChain(NULL);
    WindDetect_Reset();
}

bool KymeraWindDetect_IsActive(void)
{
    return kymeraWindDetect_IsActive();
}

void KymeraWindDetect_SetSysMode1Mic(void)
{
    DEBUG_LOG("wind_detect_sysmode_1mic_detection");
    kymeraWindDetect_SetSysMode(wind_detect_sysmode_1mic_detection);
}

void KymeraWindDetect_SetSysMode2Mic(void)
{
    DEBUG_LOG("wind_detect_sysmode_2mic_detection");
    kymeraWindDetect_SetSysMode(wind_detect_sysmode_2mic_detection);
}

void KymeraWindDetect_SetSysModeStandby(void)
{
    DEBUG_LOG("wind_detect_sysmode_standby");
    kymeraWindDetect_SetSysMode(wind_detect_sysmode_standby);
}

void KymeraWindDetect_SetUcid(kymera_operator_ucid_t ucid)
{
    PanicFalse(Kymera_SetOperatorUcid(KymeraWindDetect_GetChain(), OPR_WIND_DETECT, ucid));
}

Sink KymeraWindDetect_GetFFMicPathSink(void)
{
    kymeraWindDetect_PanicIfNotActive();
    return ChainGetInput(KymeraWindDetect_GetChain(), EPR_WIND_DETECT_FF_MIC_IN);
}

Sink KymeraWindDetect_GetDiversityMicPathSink(void)
{
    kymeraWindDetect_PanicIfNotActive();
    return ChainGetInput(KymeraWindDetect_GetChain(), EPR_WIND_DETECT_DIV_MIC_IN);
}

void KymeraWindDetect_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    kymeraWindDetect_PanicIfNotActive();

    Operator op = KymeraWindDetect_GetOperator();

    if(op)
    {
        /* UCID configuration */
        KymeraWindDetect_SetUcid((kymera_operator_ucid_t)param->current_mode);    
    }
}


#endif

