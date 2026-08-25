/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_aah.c
\brief     Implementation of Adverse acoustic event handling functionality in noise cancellation usecases.
*/

#include "kymera_aah.h"
#include "kymera_data.h"
#include "kymera_setup.h"

#include "anc_state_manager.h"
#include "kymera_ahm.h"

#if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_ANC_AAH)

static kymera_chain_handle_t anc_aah_chain;

#define kymeraAah_IsActive() (kymeraAah_GetChain() != NULL)
#define kymeraAah_PanicIfNotActive()  if(!kymeraAah_IsActive()) \
                                                                Panic()
#define kymeraAah_PanicIfActive()  if(kymeraAah_IsActive()) \
                                                                Panic()
                                                                
static kymera_chain_handle_t kymeraAah_GetChain(void)
{
    return anc_aah_chain;
}

static Operator kymeraAah_GetOperator(void)
{
    return ChainGetOperatorByRole(kymeraAah_GetChain(), OPR_AAH);
}

static void kymeraAah_SetChain(kymera_chain_handle_t chain)
{
    anc_aah_chain = chain;
}

/*Link AAH to ANC HW Manager*/
static void kymeraAah_LinkToHwMgr(bool link)
{
    DEBUG_LOG("kymeraAah_LinkToHwMgr");
    Operator op_aah = kymeraAah_GetOperator();
    Operator op_ahm = KymeraAhm_GetOperator();

    if (op_aah && op_ahm)
    {
       OperatorsAncLinkHwManager(op_aah, link, op_ahm);
    }
}

static void kymeraAah_SetUcid(kymera_operator_ucid_t ucid)
{
    DEBUG_LOG("kymeraAah_SetUcid");
    PanicFalse(Kymera_SetOperatorUcid(kymeraAah_GetChain(), OPR_AAH, ucid));
}

void KymeraAah_Create(void)
{
    kymeraAah_PanicIfActive();
    kymeraAah_SetChain(PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aah_config)));
}

void KymeraAah_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    kymeraAah_PanicIfNotActive();

    kymeraAah_LinkToHwMgr(TRUE);
    kymeraAah_SetUcid((kymera_operator_ucid_t)param->current_mode);
}

void KymeraAah_Connect(void)
{
    kymeraAah_PanicIfNotActive();
    ChainConnect(kymeraAah_GetChain());
}

void KymeraAah_Start(void)
{
    kymeraAah_PanicIfNotActive();
    ChainStart(kymeraAah_GetChain());
}

void KymeraAah_SetSysMode(aah_sysmode_t mode)
{
    DEBUG_LOG("KymeraAah_SetSysMode, Mode:%d",mode);
    Operator op = ChainGetOperatorByRole(kymeraAah_GetChain(), OPR_AAH);
    if(op)
    {
        OperatorsAahSetSysmodeCtrl(op, mode);
    }
}

void KymeraAah_Stop(void)
{
    kymeraAah_PanicIfNotActive();
    ChainStop(kymeraAah_GetChain());
}

void KymeraAah_Disconnect(void)
{
    kymeraAah_PanicIfNotActive();
    StreamDisconnect(KymeraAah_GetRefPathSource(), NULL);
    StreamDisconnect(KymeraAah_GetFFMicPathSource(), NULL);
    StreamDisconnect(KymeraAah_GetFBMicPathSource(), NULL);
}

void KymeraAah_Destroy(void)
{
    kymeraAah_PanicIfNotActive();
    ChainDestroy(kymeraAah_GetChain());
    kymeraAah_SetChain(NULL);
}

bool KymeraAah_IsActive(void)
{
    return kymeraAah_IsActive();
}

void KymeraAah_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    Operator op = ChainGetOperatorByRole(kymeraAah_GetChain(), OPR_AAH);

    if(op)
    {
        kymeraAah_SetUcid((kymera_operator_ucid_t)param->current_mode);
    }
}

Sink KymeraAah_GetRefPathSink(void)
{
    kymeraAah_PanicIfNotActive();
    return ChainGetInput(kymeraAah_GetChain(), EPR_AAH_REF_IN);
}

Sink KymeraAah_GetFFMicPathSink(void)
{
    kymeraAah_PanicIfNotActive();
    return ChainGetInput(kymeraAah_GetChain(), EPR_AAH_FF_IN);
}

Sink KymeraAah_GetFBMicPathSink(void)
{
    kymeraAah_PanicIfNotActive();
    return ChainGetInput(kymeraAah_GetChain(), EPR_AAH_FB_IN);
}

Source KymeraAah_GetRefPathSource(void)
{
    kymeraAah_PanicIfNotActive();
    return ChainGetOutput(kymeraAah_GetChain(), EPR_AAH_REF_OUT);
}

Source KymeraAah_GetFFMicPathSource(void)
{
    kymeraAah_PanicIfNotActive();
    return ChainGetOutput(kymeraAah_GetChain(), EPR_AAH_FF_OUT);
}

Source KymeraAah_GetFBMicPathSource(void)
{
    kymeraAah_PanicIfNotActive();
    return ChainGetOutput(kymeraAah_GetChain(), EPR_AAH_FB_OUT);
}

#endif /* #if defined(ENABLE_ADAPTIVE_ANC) && defined(ENABLE_ANC_AAH) */

