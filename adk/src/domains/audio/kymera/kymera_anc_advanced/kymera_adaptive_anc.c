/*!
\copyright  Copyright (c) 2021  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_adaptive_anc.c
\brief     Implementation of Adaptive ANC kymera related functionality
*/

#include "kymera_adaptive_anc.h"
#include "kymera_anc_common.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include "kymera_aec.h"
#include "kymera_output_if.h"
#include "kymera_ahm.h"
#include "kymera_internal_msg_ids.h"
#include <vmal.h>
#include <file.h>
#include <stdlib.h>
#include <cap_id_prim.h>

#ifdef ENABLE_ADAPTIVE_ANC

#define MAX_CHAIN (1)
#define CHAIN_AANCV2 (MAX_CHAIN-1)

#define kymeraAdaptiveAnc_IsAancActive() (KymeraAdaptiveAnc_GetChain() != NULL)
#define kymeraAdaptiveAnc_PanicIfNotActive()  if(!kymeraAdaptiveAnc_IsAancActive()) \
                                                                                    Panic()
/* By default AANC IIR filter sample rate set to 32kHz and for QCC517x based RDP's AANC IIR filter sample rate set to 64khz */
#ifdef DEFAULT_64K_IIR_FILTER
#define AANC_FILTER_SAMPLE_RATE (adaptive_anc_sample_rate_64khz)
#else
#define AANC_FILTER_SAMPLE_RATE (adaptive_anc_sample_rate_32khz)
#endif

#define NUM_STATUS_VAR           12
#define CUR_MODE_STATUS_OFFSET   0
#define FLAGS_STATUS_OFFSET      3
#define FLAG_POS_QUIET_MODE      20
#define BIT_MASK(FLAG_POS)       (1 << FLAG_POS)


static kymera_chain_handle_t adaptive_anc_chains[MAX_CHAIN] = {0};

kymera_chain_handle_t KymeraAdaptiveAnc_GetChain(void)
{
    return adaptive_anc_chains[0];
}

static Source kymeraAdaptiveAnc_GetOutput(chain_endpoint_role_t output_role)
{
    return ChainGetOutput(KymeraAdaptiveAnc_GetChain(), output_role);
}

static void kymeraAdaptiveAnc_SetChain(uint8 index, kymera_chain_handle_t chain)
{
    if(index < MAX_CHAIN)
        adaptive_anc_chains[index] = chain;
}

static get_status_data_t* kymeraAdaptiveAnc_GetStatusData(void)
{
    Operator op = ChainGetOperatorByRole(KymeraAdaptiveAnc_GetChain(), OPR_AANCV2);
    get_status_data_t* get_status = OperatorsCreateGetStatusData(NUM_STATUS_VAR);
    OperatorsGetStatus(op, get_status);
    return get_status;
}

static void kymeraAdaptiveAnc_GetStatusFlags(uint32 *flags)
{
    get_status_data_t* get_status = kymeraAdaptiveAnc_GetStatusData();
    *flags = (uint32)(get_status->value[FLAGS_STATUS_OFFSET]);
    free(get_status);
}

static void kymeraAdaptiveAnc_GetCurrentMode(adaptive_ancv2_sysmode_t *aancv2_mode)
{
    get_status_data_t* get_status = kymeraAdaptiveAnc_GetStatusData();
    *aancv2_mode = (adaptive_ancv2_sysmode_t)(get_status->value[CUR_MODE_STATUS_OFFSET]);
    free(get_status);
}

static void kymeraAdaptiveAnc_SetControlModelForParallelTopology(Operator op,audio_anc_path_id control_path,adaptive_anc_coefficients_t *numerator, adaptive_anc_coefficients_t *denominator)
{
    /*ANC library to read the control coefficients */
    AncReadModelCoefficients(AUDIO_ANC_INSTANCE_0, control_path, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsParallelAdaptiveAncSetControlModel(op,AUDIO_ANC_INSTANCE_0,numerator,denominator);
    AncReadModelCoefficients(AUDIO_ANC_INSTANCE_1, control_path, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsParallelAdaptiveAncSetControlModel(op,AUDIO_ANC_INSTANCE_1,numerator,denominator);
}

static void kymeraAdaptiveAnc_SetParallelTopology(Operator op)
{
    OperatorsAdaptiveAncV2SetParallelTopology(op,adaptive_anc_filter_config_parallel_topology);
}

static void kymeraAdaptiveAnc_SetControlModelForSingleTopology(Operator op,audio_anc_instance inst,audio_anc_path_id control_path,adaptive_anc_coefficients_t *numerator, adaptive_anc_coefficients_t *denominator)
{
    /*ANC library to read the control coefficients */
    AncReadModelCoefficients(inst, control_path, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsAdaptiveAncSetControlModel(op, numerator, denominator);
}


static void kymeraAdaptiveAnc_SetControlPlantModel(Operator op, audio_anc_path_id control_path, adaptive_anc_hw_channel_t hw_channel)
{
    /* Currently number of numerators & denominators are defaulted to QCC514XX specific value.
        However this might change for other family of chipset. So, ideally this shall be supplied from
        ANC library during the coarse of time */
    uint8 num_denominators = AncReadNumOfDenominatorCoefficients();
    uint8 num_numerators = AncReadNumOfNumeratorCoefficients();
    adaptive_anc_coefficients_t *denominator, *numerator;
    kymeraTaskData *theKymera = KymeraGetTaskData();

    audio_anc_instance inst = (hw_channel) ? AUDIO_ANC_INSTANCE_1 : AUDIO_ANC_INSTANCE_0;

    /* Register a listener with the AANC*/
    MessageOperatorTask(op, &theKymera->task);

    denominator = OperatorsCreateAdaptiveAncCoefficientsData(num_denominators);
    numerator= OperatorsCreateAdaptiveAncCoefficientsData(num_numerators);

    if(appKymeraIsParallelAncFilterEnabled())
    {
        kymeraAdaptiveAnc_SetControlModelForParallelTopology(op,control_path,numerator,denominator);
    }
    else
    {
        kymeraAdaptiveAnc_SetControlModelForSingleTopology(op,inst,control_path,numerator,denominator);
    }

    /* Free it, so that it can be re-used */
    free(denominator);
    free(numerator);

    denominator = OperatorsCreateAdaptiveAncCoefficientsData(num_denominators);
    numerator= OperatorsCreateAdaptiveAncCoefficientsData(num_numerators);
    /*ANC library to read the plant coefficients */
    AncReadModelCoefficients(inst, AUDIO_ANC_PATH_ID_FB, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsAdaptiveAncSetPlantModel(op, numerator, denominator);
    /* Free it, so that it can be re-used */
    free(denominator);
    free(numerator);
}

/*Link AANC to ANC HW Manager*/
static void KymeraAdaptiveAnc_LinkHwMgr(bool link)
{
    Operator aanc_op = ChainGetOperatorByRole(KymeraAdaptiveAnc_GetChain(), OPR_AANCV2);
    Operator ahm_op = KymeraAhm_GetOperator();

    if (aanc_op && ahm_op)
    {
        OperatorsAncLinkHwManager(aanc_op, link, ahm_op);
    }
}

static void kymeraAdaptiveAnc_ConfigureAancChain(Operator op, const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    OperatorsStandardSetUCID(op, param->current_mode);
    KymeraAdaptiveAnc_LinkHwMgr(TRUE);

    if(appKymeraIsParallelAncFilterEnabled())
    {
        kymeraAdaptiveAnc_SetParallelTopology(op);
    }

    OperatorsAdaptiveAncV2SetSampleRate(op, AANC_FILTER_SAMPLE_RATE);
    kymeraAdaptiveAnc_SetControlPlantModel(op, param->control_path, param->hw_channel);
}

void KymeraAdaptiveAnc_ConfigureChain(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    DEBUG_LOG("KymeraAdaptiveAnc_ConfigureChain");    
    PanicNull(KymeraAdaptiveAnc_GetChain());
    Operator op = ChainGetOperatorByRole(KymeraAdaptiveAnc_GetChain(), OPR_AANCV2);
    kymeraAdaptiveAnc_ConfigureAancChain(op, param);
}

void KymeraAdaptiveAnc_CreateChain(void)
{
    DEBUG_LOG("KymeraAdaptiveAnc_CreateChain");
    PanicNotNull(KymeraAdaptiveAnc_GetChain());
    kymeraAdaptiveAnc_SetChain(CHAIN_AANCV2, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aancv2_config)));
}

void KymeraAdaptiveAnc_ConnectChain(void)
{
    DEBUG_LOG("KymeraAdaptiveAnc_ConnectChain");
    ChainConnect(KymeraAdaptiveAnc_GetChain());
}

void KymeraAdaptiveAnc_StartChain(void)
{
    DEBUG_LOG("KymeraAdaptiveAnc_StartChain");
#ifndef ENABLE_ANC_FAST_MODE_SWITCH
    KymeraAdaptiveAnc_SetSysMode(adaptive_anc_sysmode_freeze);
#endif
    ChainStart(KymeraAdaptiveAnc_GetChain());
}

void KymeraAdaptiveAnc_StopChain(void)
{
    DEBUG_LOG("KymeraAdaptiveAnc_StopChain");
    ChainStop(KymeraAdaptiveAnc_GetChain());
}

void KymeraAdaptiveAnc_DisconnectChain(void)
{
    StreamDisconnect(kymeraAdaptiveAnc_GetOutput(EPR_AANCV2_FF_MIC_OUT), NULL);
    StreamDisconnect(kymeraAdaptiveAnc_GetOutput(EPR_AANCV2_ERR_MIC_OUT), NULL);
}

void KymeraAdaptiveAnc_DestroyChain(void)
{
    DEBUG_LOG("KymeraAdaptiveAnc_DestroyChain");

    PanicNull(KymeraAdaptiveAnc_GetChain());
    ChainDestroy(KymeraAdaptiveAnc_GetChain());
    kymeraAdaptiveAnc_SetChain(CHAIN_AANCV2, NULL);
}

void KymeraAdaptiveAnc_SetSysMode(adaptive_ancv2_sysmode_t mode)
{
    Operator op = ChainGetOperatorByRole(KymeraAdaptiveAnc_GetChain(), OPR_AANCV2);
    if(op)
    {
        DEBUG_LOG_INFO("KymeraAdaptiveAnc_SetSysMode, AANC2 sysmode, enum:adaptive_ancv2_sysmode_t:%d", mode);
        OperatorsAdaptiveAncV2SetSysMode(op, mode);
    }
}

void KymeraAdaptiveAnc_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    DEBUG_LOG("KymeraAdaptiveAnc_ApplyModeChange");
    PanicNull(KymeraAdaptiveAnc_GetChain());
    Operator op = ChainGetOperatorByRole(KymeraAdaptiveAnc_GetChain(), OPR_AANCV2);
    if(op)
    {
        OperatorsStandardSetUCID(op, param->current_mode);
        kymeraAdaptiveAnc_SetControlPlantModel(op, param->control_path, param->hw_channel);
        OperatorsAdaptiveAncSetHwChannelCtrl(op, param->hw_channel);
    }
}

bool KymeraAdaptiveAnc_IsNoiseLevelBelowQuietModeThreshold(void)
{
    uint32 flags;
    kymeraAdaptiveAnc_GetStatusFlags(&flags);
    return (flags & BIT_MASK(FLAG_POS_QUIET_MODE));
}

bool KymeraAdaptiveAnc_IsActive(void)
{
    return (kymeraAdaptiveAnc_IsAancActive());
}

bool KymeraAdaptiveAnc_GetSysMode(adaptive_ancv2_sysmode_t *aancv2_mode)
{
    PanicNull(aancv2_mode);
    kymeraAdaptiveAnc_PanicIfNotActive();
    kymeraAdaptiveAnc_GetCurrentMode(aancv2_mode);
    return TRUE;
}

uint16 KymeraAdaptiveAnc_GetFreezedGain(void)
{
    uint16 gain=0;
    
    PanicNull(KymeraAdaptiveAnc_GetChain());
    Operator op = ChainGetOperatorByRole(KymeraAdaptiveAnc_GetChain(), OPR_AANCV2);
    if(op)
    {
        gain = OperatorsAdaptiveAncV2GetAdaptiveGain(op);
    }
    
    DEBUG_LOG("KymeraAdaptiveAnc_GetFreezedGain %d", gain);
    return gain;
}

#endif
