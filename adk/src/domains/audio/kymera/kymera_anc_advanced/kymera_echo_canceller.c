/*!
\copyright  Copyright (c) 2022  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_echo_canceller.c
\brief     Implementation of Echo canceller kymera related functionality
*/

#include "kymera_echo_canceller.h"
#include "kymera_config.h"
#include "kymera_data.h"
#include "kymera_setup.h"

#ifdef ENABLE_ADAPTIVE_ANC

#define MAX_CHAIN (2)
#define CHAIN_MIC_REF_PATH_SPLITTER (MAX_CHAIN-1)
#define CHAIN_FBC (CHAIN_MIC_REF_PATH_SPLITTER-1)

#define kymeraEchoCanceller_IsFbcActive() (kymeraEchoCanceller_GetChain(CHAIN_FBC) != NULL)
#define kymeraEchoCanceller_IsSplitterInMicRefPathActive() (kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER) != NULL)


static kymera_chain_handle_t echo_canceller_chains[MAX_CHAIN] = {0};

static kymera_chain_handle_t kymeraEchoCanceller_GetChain(uint8 index)
{
    return ((index < MAX_CHAIN) ? echo_canceller_chains[index] : NULL);
}

static Source kymeraEchoCanceller_GetOutput(uint8 index, chain_endpoint_role_t output_role)
{
    return ChainGetOutput(kymeraEchoCanceller_GetChain(index), output_role);
}

static Sink kymeraEchoCanceller_GetInput(uint8 index, chain_endpoint_role_t input_role)
{
    return ChainGetInput(kymeraEchoCanceller_GetChain(index), input_role);
}

static void kymeraEchoCanceller_SetChain(uint8 index, kymera_chain_handle_t chain)
{
    if(index < MAX_CHAIN)
        echo_canceller_chains[index] = chain;
}

/*********************************************************************/
/* Concurrency FBC Chains */
static void KymeraEchoCanceller_ConfigureFbcChain(void)
{
    DEBUG_LOG("kymeraEchoCanceller_ConfigureFbcChain");
    Operator op_ff_fbc = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_FBC), OPR_AANC_FBC_FF_MIC_PATH);
    Operator op_fb_fbc = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_FBC),OPR_AANC_FBC_ERR_MIC_PATH);
    if (op_ff_fbc)
    {
        PanicFalse(Kymera_SetOperatorUcid(kymeraEchoCanceller_GetChain(CHAIN_FBC), OPR_AANC_FBC_FF_MIC_PATH, UCID_ADAPTIVE_ANC_FBC));
    }
    if(op_fb_fbc)
    {
        PanicFalse(Kymera_SetOperatorUcid(kymeraEchoCanceller_GetChain(CHAIN_FBC), OPR_AANC_FBC_ERR_MIC_PATH, UCID_ADAPTIVE_ANC_FBC));
    }

    ChainConnect(kymeraEchoCanceller_GetChain(CHAIN_FBC));
}

static void kymeraEchoCanceller_DestroyFbcChain(void)
{
    DEBUG_LOG("kymeraEchoCanceller_DestroyFbcChain");
    if (kymeraEchoCanceller_IsFbcActive())
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        ChainDestroy(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        kymeraEchoCanceller_SetChain(CHAIN_FBC, NULL);
    }
}

static void kymeraEchoCanceller_ConfigureSplitterChainInMicRefPath(void)
{
    DEBUG_LOG("kymeraEchoCanceller_ConfigureSplitterChainInMicRefPath");
    if (kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
        Operator op = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER), OPR_AANC_SPLT_MIC_REF_PATH);

        if(op)
        {
            OperatorsSplitterSetWorkingMode(op, splitter_mode_clone_input);
            OperatorsSplitterEnableSecondOutput(op, FALSE);
            OperatorsSplitterSetDataFormat(op, operator_data_format_pcm);
        }
        ChainConnect(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
    }
}

static void kymeraEchoCanceller_DestroySplitterChainInMicRefPath(void)
{
    if (kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
        DEBUG_LOG("kymeraEchoCanceller_DestroySplitterChainInMicRefPath");
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        ChainDestroy(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        kymeraEchoCanceller_SetChain(CHAIN_MIC_REF_PATH_SPLITTER, NULL);
    }
}

static void kymeraEchoCanceller_ActivateMicRefPathSplitterSecondOutput(void)
{
    if(kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
        Operator splt_op = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER), OPR_AANC_SPLT_MIC_REF_PATH);
        DEBUG_LOG("kymeraEchoCanceller_ActivateMicRefPathSplitterSecondOutput");
        OperatorsSplitterEnableSecondOutput(splt_op, TRUE);
    }
}

static void kymeraEchoCanceller_DeactivateMicRefPathSplitterSecondOutput(void)
{
    if(kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
        Operator splt_op = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER), OPR_AANC_SPLT_MIC_REF_PATH);
        DEBUG_LOG("kymeraEchoCanceller_DeactivateMicRefPathSplitterSecondOutput");
        OperatorsSplitterEnableSecondOutput(splt_op, FALSE);
    }
}

static void kymeraEchoCanceller_ConnectSplitterFbcChain(void)
{
    DEBUG_LOG("kymeraEchoCanceller_ConnectSplitterFbcChain");
    if(kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
        /* Connect Splitter and FBC operators */
        PanicNull(StreamConnect(kymeraEchoCanceller_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT1), kymeraEchoCanceller_GetInput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_REF_IN)));
        PanicNull(StreamConnect(kymeraEchoCanceller_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT2), kymeraEchoCanceller_GetInput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_REF_IN)));
    }
}

static void KymeraEchoCanceller_DisconnectFbcOutputs(void)
{
    Operator op_ff_fbc = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_FBC), OPR_AANC_FBC_FF_MIC_PATH);
    if(op_ff_fbc)
    {
        StreamDisconnect(kymeraEchoCanceller_GetOutput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_OUT), NULL);
    }
    StreamDisconnect(kymeraEchoCanceller_GetOutput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_OUT), NULL);
}

static void kymeraEchoCanceller_DisconnectMicRefPathSplitterOutputs(void)
{
    if(kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
        StreamDisconnect(kymeraEchoCanceller_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT1), NULL);
        StreamDisconnect(kymeraEchoCanceller_GetOutput(CHAIN_MIC_REF_PATH_SPLITTER, EPR_SPLT_MIC_REF_OUT2), NULL);
    }
}

/*********************************************************************/
void KymeraEchoCanceller_Create(fbc_config_t fbc_config)
{
    DEBUG_LOG("KymeraEchoCanceller_Create");
    PanicNotNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
    switch(fbc_config)
    {
    case fb_path_only:
        kymeraEchoCanceller_SetChain(CHAIN_FBC, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aanc_fbpath_fbc_config)));
        break;

    case ff_fb_paths:
        kymeraEchoCanceller_SetChain(CHAIN_FBC, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aanc_fbc_config)));
        PanicNotNull(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        kymeraEchoCanceller_SetChain(CHAIN_MIC_REF_PATH_SPLITTER, PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_aanc_splitter_mic_ref_path_config)));
        break;

    default:
        break;
    }
}

void KymeraEchoCanceller_Configure(void)
{
    DEBUG_LOG("KymeraEchoCanceller_Configure");
    if(kymeraEchoCanceller_IsFbcActive())
    {
        KymeraEchoCanceller_ConfigureFbcChain();
        kymeraEchoCanceller_ConfigureSplitterChainInMicRefPath();
    }
}

void KymeraEchoCanceller_Destroy(void)
{
    DEBUG_LOG("KymeraEchoCanceller_Destroy");
    if (kymeraEchoCanceller_IsFbcActive())
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        kymeraEchoCanceller_DestroySplitterChainInMicRefPath();
        kymeraEchoCanceller_DestroyFbcChain();
    }
}

void KymeraEchoCanceller_Connect(void)
{
    DEBUG_LOG("KymeraEchoCanceller_Connect");

    if(kymeraEchoCanceller_IsFbcActive())
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));

        kymeraEchoCanceller_ConnectSplitterFbcChain();
        kymeraEchoCanceller_ActivateMicRefPathSplitterSecondOutput();
    }
}

void KymeraEchoCanceller_Start(void)
{
    DEBUG_LOG("KymeraEchoCanceller_Start");
    if(kymeraEchoCanceller_IsFbcActive())
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        ChainStart(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        if(kymeraEchoCanceller_IsSplitterInMicRefPathActive())
        {
            ChainStart(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
        }
    }
}

void KymeraEchoCanceller_Stop(void)
{
    DEBUG_LOG("KymeraEchoCanceller_Stop");
    if(kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
        ChainStop(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER));
    }
    if(kymeraEchoCanceller_IsFbcActive())
    {
        ChainStop(kymeraEchoCanceller_GetChain(CHAIN_FBC));
    }
}

void KymeraEchoCanceller_Disconnect(void)
{
    DEBUG_LOG("KymeraEchoCanceller_Disconnect");
    if(kymeraEchoCanceller_IsFbcActive())
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        kymeraEchoCanceller_DeactivateMicRefPathSplitterSecondOutput();
        KymeraEchoCanceller_DisconnectFbcOutputs();
        kymeraEchoCanceller_DisconnectMicRefPathSplitterOutputs();
    }
}

bool KymeraEchoCanceller_IsActive(void)
{
    return kymeraEchoCanceller_IsFbcActive();
}

/*! \brief Get the Echo canceller speaker Sink (in concurrency) for Adaptive AANC
*/
Sink KymeraEchoCanceller_GetSpkRefPathSink(void)
{
    DEBUG_LOG("KymeraEchoCanceller_GetSpkRefPathSink");
    if(kymeraEchoCanceller_IsSplitterInMicRefPathActive())
    {
       return ChainGetInput(kymeraEchoCanceller_GetChain(CHAIN_MIC_REF_PATH_SPLITTER), EPR_SPLT_MIC_REF_IN);
    }
    else
    {
        return ChainGetInput(kymeraEchoCanceller_GetChain(CHAIN_FBC), EPR_AANC_FBC_ERR_MIC_REF_IN);
    }
}

/*! \brief Get the Echo canceller Feed Forward mic Source
*/
Source KymeraEchoCanceller_GetFFMicPathSource(void)
{
    Operator op_ff_fbc = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_FBC), OPR_AANC_FBC_FF_MIC_PATH);
    if(op_ff_fbc)
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        return kymeraEchoCanceller_GetOutput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_OUT);
    }

    return NULL;
}

/*! \brief Get the Echo canceller Feed back mic Source
*/
Source KymeraEchoCanceller_GetFBMicPathSource(void)
{
    if(kymeraEchoCanceller_IsFbcActive())
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        return kymeraEchoCanceller_GetOutput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_OUT);
    }

    return NULL;
}

Sink KymeraEchoCanceller_GetFFMicPathSink(void)
{
    Operator op_ff_fbc = ChainGetOperatorByRole(kymeraEchoCanceller_GetChain(CHAIN_FBC), OPR_AANC_FBC_FF_MIC_PATH);
    if(op_ff_fbc)
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        return kymeraEchoCanceller_GetInput(CHAIN_FBC, EPR_AANC_FBC_FF_MIC_IN);
    }

    return NULL;
}

Sink KymeraEchoCanceller_GetFBMicPathSink(void)
{
    if(kymeraEchoCanceller_IsFbcActive())
    {
        PanicNull(kymeraEchoCanceller_GetChain(CHAIN_FBC));
        return kymeraEchoCanceller_GetInput(CHAIN_FBC, EPR_AANC_FBC_ERR_MIC_IN);
    }

    return NULL;
}

#endif

