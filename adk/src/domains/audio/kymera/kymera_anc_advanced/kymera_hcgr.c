/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_hcgr.c
\brief     Implementation of HCGR and related functionality
*/

#include "kymera_hcgr.h"
#include "kymera_ahm.h"
#include "anc_state_manager.h"
#include "kymera_data.h"
#include "kymera_setup.h"

#ifdef ENABLE_ADAPTIVE_ANC

typedef struct
{
    TaskData task;
    howling_detect_user_state_t hcgr_user_state;
}howlingDetectTaskData;

static howlingDetectTaskData howling_detect_task_data;

#define howlingDetectGetTaskData() (&howling_detect_task_data)
#define howlingDetectGetTask()     (&howling_detect_task_data.Task)

#define KYMERA_HCGR_SAMPLE_RATE (16000)
#define NUM_STATUS_VAR_HCGR 4

static kymera_chain_handle_t anc_hcgr_chain;

#define kymeraHcgr_IsActive() (kymeraHcgr_GetChain() != NULL)

#define kymeraHcgr_PanicIfNotActive()  if(!kymeraHcgr_IsActive()) \
                                                                Panic()
#define kymeraHcgr_PanicIfActive()  if(kymeraHcgr_IsActive()) \
                                                                Panic()

static howling_detect_user_state_t kymeraHcgr_GetCurrentState(void)
{
    return howlingDetectGetTaskData()->hcgr_user_state;
}

static kymera_chain_handle_t kymeraHcgr_GetChain(void)
{
    return anc_hcgr_chain;
}

static void kymeraHcgr_SetChain(kymera_chain_handle_t chain)
{
    anc_hcgr_chain = chain;
}

static bool kymeraHcgr_GetUserState(void)
{
    return howlingDetectGetTaskData()->hcgr_user_state;
}

void KymeraHcgr_Create(void)
{
    DEBUG_LOG("KymeraHcgr_Create");
    kymeraHcgr_PanicIfActive();
    kymeraHcgr_SetChain(PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_hcgr_config)));
}

/*Link HC to ANC HW Manager*/
static void Kymera_LinkHcgrToHwMgr(bool link)
{
    DEBUG_LOG("Kymera_LinkHcgrToHwMgr");
    Operator op_hcgr = ChainGetOperatorByRole(kymeraHcgr_GetChain(), OPR_HCGR);
    Operator op_ahm = KymeraAhm_GetOperator();

    if (op_hcgr && op_ahm)
    {
       OperatorsAncLinkHwManager(op_hcgr, link, op_ahm);
    }
}

bool KymeraHcgr_IsHowlingDetectionEnabled(void)
{
    bool status = FALSE;
    if(kymeraHcgr_GetCurrentState() == howling_detection_enabled)
    {
        status = TRUE;
    }
    return status;
}

void KymeraHcgr_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    kymeraHcgr_PanicIfNotActive();
    uint32 hcgr_config = 0;

    Operator op = ChainGetOperatorByRole(kymeraHcgr_GetChain(), OPR_HCGR);
    if(op)
    {
        DEBUG_LOG("KymeraHcgr_Configure, mode %d",param->current_mode);
        KymeraHcgr_SetUcid((kymera_operator_ucid_t)param->current_mode);
        /* Added below workaround code to introduce short delay between UCID update and AHM Linking */
        hcgr_config = OperatorsHowlingControlGetConfig(op);
        if(hcgr_config)
        {
            DEBUG_LOG("KymeraHcgr_Configure, FB path gain control");
        }
        else
        {
            DEBUG_LOG("KymeraHcgr_Configure, FF path gain control");
        }
        Kymera_LinkHcgrToHwMgr(TRUE);
        /* Associate kymera task to receive unsolicited messages */
        MessageOperatorTask(op, &theKymera->task);
    }
}

void KymeraHcgr_Connect(void)
{
    DEBUG_LOG("KymeraHcgr_Connect");
    kymeraHcgr_PanicIfNotActive();
    ChainConnect(kymeraHcgr_GetChain());
}

void KymeraHcgr_Start(void)
{
    DEBUG_LOG("KymeraHcgr_Start");
    kymeraHcgr_PanicIfNotActive();
    ChainStart(kymeraHcgr_GetChain());
}

void KymeraHcgr_Stop(void)
{
    DEBUG_LOG("KymeraHcgr_Stop");
    kymeraHcgr_PanicIfNotActive();
    ChainStop(kymeraHcgr_GetChain());
}

void KymeraHcgr_Disconnect(void)
{
//    do-nothing
}

void KymeraHcgr_Destroy(void)
{
    DEBUG_LOG("KymeraHcgr_Destroy");
    kymeraHcgr_PanicIfNotActive();
    ChainDestroy(kymeraHcgr_GetChain());
    kymeraHcgr_SetChain(NULL);
}

bool KymeraHcgr_IsActive(void)
{
    return kymeraHcgr_IsActive();
}

void KymeraHcgr_SetUcid(kymera_operator_ucid_t ucid)
{
    DEBUG_LOG("KymeraHcgr_SetUcid");
    PanicFalse(Kymera_SetOperatorUcid(kymeraHcgr_GetChain(), OPR_HCGR, ucid));
}

void KymeraHcgr_UpdateUserState(howling_detect_user_state_t state)
{
    howlingDetectGetTaskData()->hcgr_user_state = state;
}

void KymeraHcgr_SetHowlingControlSysMode(hc_sysmode_t mode)
{
    DEBUG_LOG("KymeraHcgr_SetHowlingControlSysMode, Mode:%d",mode);
    Operator op = ChainGetOperatorByRole(kymeraHcgr_GetChain(), OPR_HCGR);

    if(op)
    {
        if(kymeraHcgr_GetUserState() == howling_detection_enabled)
        {
            DEBUG_LOG_INFO("KymeraHcgr_SetHowlingControlSysMode, HCGR sysmode, enum:hc_sysmode_t:%d", mode);
            OperatorsHowlingControlSetSysmodeCtrl(op, mode);
        }
        else
        {
            DEBUG_LOG_INFO("KymeraHcgr_SetHowlingControlSysMode, HCGR sysmode, enum:hc_sysmode_t:%d", hc_sysmode_standby);
            OperatorsHowlingControlSetSysmodeCtrl(op, hc_sysmode_standby);
        }
    }
}

Sink KymeraHcgr_GetFbMicPathSink(void)
{
    if(kymeraHcgr_IsActive())
    {
        PanicNull(kymeraHcgr_GetChain());
        return ChainGetInput(kymeraHcgr_GetChain(), EPR_HCGR_IN);
    }
    return NULL;
}

#endif

