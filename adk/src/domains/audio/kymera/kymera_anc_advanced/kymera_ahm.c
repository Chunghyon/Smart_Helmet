/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_ahm.c
\brief     Implementation of ANC hardware manager and related functionality
*/

#include "kymera_ahm.h"
#include "kymera_data.h"
#include "kymera_setup.h"

#include "anc_state_manager.h"

#ifdef ENABLE_ADAPTIVE_ANC

#define KYMERA_AHM_SAMPLE_RATE (16000)
#define CUR_MODE_STATUS_OFFSET   0
#define NUM_STATUS_VAR 23

#define NUM_AHM_CONFIG_FLAGS                     (32)
#define DISABLE_ANC_CLOCK_CHECK_FLAG_POS         (0)
#define DISABLE_IN_OUT_EAR_CHECK_FLAG_POS        (1)
#define DISABLE_FF_GAIN_RANGE_ADJUST_FLAG_POS    (2)
#define DISABLE_FF_RAMP_FLAG_POS                 (4)
#define DISABLE_FB_RAMP_FLAG_POS                 (5)

#define AHM_TASK_PERIOD (1000U)
#define AHM_DECIM_FACTOR (1U)

#define CONFIG_MASK(pos)                     (1U << (pos))
#define CONFIG_FLAG(flag, pos)               ((flag) << (pos))

#define DISABLE_FF_GAIN_RANGE_ADJUST_FLAG    (TRUE)
#define ENABLE_FF_GAIN_RANGE_ADJUST_FLAG     (FALSE)

static kymera_chain_handle_t anc_ahm_chain;

#define kymeraAhm_IsActive() (KymeraAhm_GetChain() != NULL)
#define kymeraAhm_PanicIfNotActive()  if(!kymeraAhm_IsActive()) \
                                                                Panic()
#define kymeraAhm_PanicIfActive()  if(kymeraAhm_IsActive()) \
                                                                Panic()
                                                                
kymera_chain_handle_t KymeraAhm_GetChain(void)
{
    return anc_ahm_chain;
}

Operator KymeraAhm_GetOperator(void)
{
    return ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);
}

static void kymeraAhm_SetChain(kymera_chain_handle_t chain)
{
    anc_ahm_chain = chain;
}

static get_status_data_t* kymeraAhm_GetStatusData(void)
{
    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);
    get_status_data_t* get_status = OperatorsCreateGetStatusData(NUM_STATUS_VAR);
    OperatorsGetStatus(op, get_status);
    return get_status;
}

static void kymeraAhm_GetCurrentMode(ahm_sysmode_t *ahm_mode)
{
    get_status_data_t* get_status = kymeraAhm_GetStatusData();
    *ahm_mode = (ahm_sysmode_t)(get_status->value[CUR_MODE_STATUS_OFFSET]);
    free(get_status);
}

void KymeraAhm_Create(void)
{
    kymeraAhm_PanicIfActive();
    kymeraAhm_SetChain(PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_ahm_config)));
}

static void kymeraAhm_ConfigureFfGainRangeAdjustParam(anc_mode_t current_mode)
{
    if(AncConfig_IsAncModeStaticLeakThrough(current_mode))
    {
        if(!KymeraAhm_IsFfFineGainRangeAdjustDisabled())
        {
            KymeraAhm_DisableFfFineGainRangeAdjust();
        }
    }
}

/* topology configuration */
static void kymeraAhm_SetChannel(Operator op, adaptive_anc_hw_channel_t hw_channel)
{
    ahm_channel_t channel = ahm_anc_channel_0;

    if(appKymeraIsParallelAncFilterEnabled()) /* Parallel topolgy */
    {
        channel = ahm_anc_both_channels;
    }
    else /* Single topolgy */
    {
        if(hw_channel == adaptive_anc_hw_channel_1)
        {
            channel = ahm_anc_channel_1;
        }
    }

    OperatorsAhmSetChannelCtrl(op, channel);
}

#ifdef ENABLE_ANC_FAST_MODE_SWITCH

/* Sets IIR coefficients to AHM capability */
static void kymeraAhm_SetIirCoefficientsForPath(Operator op, audio_anc_instance instance, audio_anc_path_id path)
{
    DEBUG_LOG("kymeraAhm_SetIirCoefficientsForPath, instance: enum:audio_anc_instance:%d, path: enum:audio_anc_path_id:%d", instance, path);

    uint8 num_denominators = AncReadNumOfDenominatorCoefficients();
    uint8 num_numerators = AncReadNumOfNumeratorCoefficients();
    iir_coefficients_t *denominator, *numerator;

    denominator = OperatorsCreateIirCoefficientsData(num_denominators);
    numerator = OperatorsCreateIirCoefficientsData(num_numerators);

    AncReadModelCoefficients(instance, path, (uint32*)denominator->coefficients, (uint32*)numerator->coefficients);
    OperatorsAhmSetIirCoefficients(op, instance, path, numerator, denominator);

    free(denominator);
    free(numerator);
}

static void kymeraAhm_SetIirCoefficients(Operator op, audio_anc_instance instance)
{
    kymeraAhm_SetIirCoefficientsForPath(op, instance, AUDIO_ANC_PATH_ID_FFA);
    kymeraAhm_SetIirCoefficientsForPath(op, instance, AUDIO_ANC_PATH_ID_FFB);
    kymeraAhm_SetIirCoefficientsForPath(op, instance, AUDIO_ANC_PATH_ID_FB);
}

#endif /* ENABLE_ANC_FAST_MODE_SWITCH */


/*Reads the static gain for current mode in library*/
static void kymeraAhm_SetStaticGain(Operator op, audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel)
{
    uint16 coarse_gain;
    uint8 fine_gain;

    uint16 *static_gains = PanicUnlessMalloc(sizeof(uint16) * ahm_static_gain_max);
    memset(static_gains, 0, sizeof(uint16) * ahm_static_gain_max);

    audio_anc_instance inst = hw_channel ? AUDIO_ANC_INSTANCE_1 : AUDIO_ANC_INSTANCE_0;

    /*If hybrid is configured, feedforward path is AUDIO_ANC_PATH_ID_FFB and feedback path will be AUDIO_ANC_PATH_ID_FFA*/
    audio_anc_path_id feedback_anc_path = (feedforward_anc_path==AUDIO_ANC_PATH_ID_FFB)?(AUDIO_ANC_PATH_ID_FFA):(AUDIO_ANC_PATH_ID_FFB);

     /*Update gains*/
    AncReadCoarseGainFromInstance(inst, feedforward_anc_path, &coarse_gain);
    static_gains[ahm_static_gain_ff_coarse] = (uint16)coarse_gain;
    AncReadFineGainFromInstance(inst, feedforward_anc_path, &fine_gain);
    static_gains[ahm_static_gain_ff_fine] = (uint16)fine_gain;

    AncReadCoarseGainFromInstance(inst, feedback_anc_path, &coarse_gain);
    static_gains[ahm_static_gain_fb_coarse] = (uint16)coarse_gain;
    AncReadFineGainFromInstance(inst, feedback_anc_path, &fine_gain);
    static_gains[ahm_static_gain_fb_fine] = (uint16)fine_gain;

    AncReadCoarseGainFromInstance(inst, AUDIO_ANC_PATH_ID_FB, &coarse_gain);
    static_gains[ahm_static_gain_ec_coarse] = (uint16)coarse_gain;
    AncReadFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FB, &fine_gain);
    static_gains[ahm_static_gain_ec_fine] = (uint16)fine_gain;

    AncReadRxMixCoarseGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFA, &coarse_gain);
    static_gains[ahm_static_gain_rxmix_ffa_coarse] = (uint16)coarse_gain;
    AncReadRxMixFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFA, &fine_gain);
    static_gains[ahm_static_gain_rxmix_ffa_fine] = (uint16)fine_gain;

    AncReadRxMixCoarseGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFB, &coarse_gain);
    static_gains[ahm_static_gain_rxmix_ffb_coarse] = (uint16)coarse_gain;
    AncReadRxMixFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFB, &fine_gain);
    static_gains[ahm_static_gain_rxmix_ffb_fine] = (uint16)fine_gain;

    OperatorsAhmSetStaticGain(op, static_gains);

    free(static_gains);
}

static uint32 kymeraAhm_GetConfigFlags(void)
{
    Operator ahm_op;
    kymeraAhm_PanicIfNotActive();
    ahm_op = KymeraAhm_GetOperator();
    return OperatorsAhmGetConfig(ahm_op);
}

static void kymeraAhm_SetConfigFlags(bool config_flag_value, unsigned config_flag_pos)
{
    Operator ahm_op;
    uint32 ahm_config;

    kymeraAhm_PanicIfNotActive();
    ahm_op = KymeraAhm_GetOperator();

    /* Read current AHM configs */
    ahm_config = kymeraAhm_GetConfigFlags();
    /* Reset flag corresponding to config flag position */
    ahm_config = (ahm_config & ~(CONFIG_MASK(config_flag_pos)));
    /* Update flag to flag value */
    ahm_config |= CONFIG_FLAG(config_flag_value, config_flag_pos);

    OperatorsAhmSetConfig(ahm_op, ahm_config);
}

void KymeraAhm_SetTargetGain(uint16 gain)
{
    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);

    if(op)
    {    
        DEBUG_LOG("KymeraAhm_SetTargetGain fine gain %d ", gain);
        OperatorsAhmSetFineTargetGain(op, gain);
    }
}

/*For the current operational mode*/
uint16 KymeraAhm_GetStaticFeedForwardFineGain(adaptive_anc_hw_channel_t hw_channel, audio_anc_path_id control_path)
{
    uint8 fine_gain=0;

    audio_anc_instance inst = (hw_channel) ? AUDIO_ANC_INSTANCE_1 : AUDIO_ANC_INSTANCE_0;
    AncReadFineGainFromInstance(inst, control_path, &fine_gain);
    return (uint16) fine_gain;
}

void KymeraAhm_Configure(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    kymeraAhm_PanicIfNotActive();

    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);

    if(op)
    {
        /* UCID configuration */
        KymeraAhm_SetUcid((kymera_operator_ucid_t)param->current_mode);

        kymeraAhm_ConfigureFfGainRangeAdjustParam(param->current_mode);

        /* topology configuration */
        kymeraAhm_SetChannel(op, param->hw_channel);

        /* feedforward path configuration */
        OperatorsAhmSetFeedforwardPathCtrl(op, (param->control_path == AUDIO_ANC_PATH_ID_FFB) ? ahm_ff_path_ffb: ahm_ff_path_ffa);

#ifndef ENABLE_ANC_FAST_MODE_SWITCH
        /* Set static gains: FF static is static + world volume */
        kymeraAhm_SetStaticGain(op, param->control_path, param->hw_channel);
#endif
        
        /*Ambient control configuration*/
        if (AncConfig_IsAncModeAdaptiveLeakThrough(param->current_mode))
        {
            OperatorsAhmSetAmbientCtrl(op, ahm_ambient_ctrl_aamb_mode);
        }

        /* In Ear configuration */
        OperatorsAhmSetInEarCtrl(op, param->in_ear);

        OperatorsStandardSetSampleRate(op, KYMERA_AHM_SAMPLE_RATE);

#ifdef ENABLE_ANC_FAST_MODE_SWITCH
        KymeraAhm_TriggerTransition(param->control_path, param->hw_channel, ahm_trigger_transition_ctrl_start);
#endif

        OperatorsAhmSetTaskPeriod(op, AHM_TASK_PERIOD, AHM_DECIM_FACTOR);

        /* Associate kymera task to receive unsolicited messages */
        MessageOperatorTask(op, &theKymera->task);
    }
}

void KymeraAhm_Connect(void)
{
    kymeraAhm_PanicIfNotActive();
    ChainConnect(KymeraAhm_GetChain());
}

void KymeraAhm_Start(void)
{
    kymeraAhm_PanicIfNotActive();
    ChainStart(KymeraAhm_GetChain());
}

void KymeraAhm_Stop(void)
{
    kymeraAhm_PanicIfNotActive();
    ChainStop(KymeraAhm_GetChain());
}

void KymeraAhm_Destroy(void)
{
    kymeraAhm_PanicIfNotActive();
    ChainDestroy(KymeraAhm_GetChain());
    kymeraAhm_SetChain(NULL);
}

bool KymeraAhm_IsActive(void)
{
    return kymeraAhm_IsActive();
}

void KymeraAhm_ApplyModeChange(const KYMERA_INTERNAL_AANC_ENABLE_T* param)
{
    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);
    
    if(op)
    {
        /* UCID configuration */
        KymeraAhm_SetUcid((kymera_operator_ucid_t)param->current_mode);

        kymeraAhm_ConfigureFfGainRangeAdjustParam(param->current_mode);

        /* feedforward path configuration */
        OperatorsAhmSetFeedforwardPathCtrl(op, (param->control_path == AUDIO_ANC_PATH_ID_FFB) ? ahm_ff_path_ffb: ahm_ff_path_ffa);

#ifndef ENABLE_ANC_FAST_MODE_SWITCH
        /* Set static gains: FF static is static + world volume */
        kymeraAhm_SetStaticGain(op, param->control_path, param->hw_channel);
#endif

        /*Ambient control configuration*/
        if (AncConfig_IsAncModeAdaptiveLeakThrough(param->current_mode))
        {
            OperatorsAhmSetAmbientCtrl(op, ahm_ambient_ctrl_aamb_mode);
        }
    }
}

void KymeraAhm_SetSysMode(ahm_sysmode_t mode)
{
    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);
    
    if(op)
    {
        DEBUG_LOG_INFO("KymeraAhm_SetSysMode, ahm sysmode, enum:ahm_sysmode_t:%d", mode);
        OperatorsAhmSetSysmodeCtrl(op, mode);
    }
}

bool KymeraAhm_GetSysMode(ahm_sysmode_t *ahm_mode)
{
    PanicNull(ahm_mode);
    kymeraAhm_PanicIfNotActive();
    kymeraAhm_GetCurrentMode(ahm_mode);
    return TRUE;
}

void KymeraAhm_SetUcid(kymera_operator_ucid_t ucid)
{
    PanicFalse(Kymera_SetOperatorUcid(KymeraAhm_GetChain(), OPR_AHM, ucid));
}

Sink KymeraAhm_GetFFMicPathSink(void)
{
    kymeraAhm_PanicIfNotActive();
    return ChainGetInput(KymeraAhm_GetChain(), EPR_AHM_FF_MIC_IN);
}

Sink KymeraAhm_GetFBMicPathSink(void)
{
    kymeraAhm_PanicIfNotActive();
    return ChainGetInput(KymeraAhm_GetChain(), EPR_AHM_FB_MIC_IN);
}

void KymeraAhm_GetFFGain(uint16* gain)
{
    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);

    if(op)
    {
        *gain = (uint16) OperatorsAhmGetFeedForwardFineGain(op);
        DEBUG_LOG("KymeraAhm_GetFFGain FF fine gain 0x%x %d ", *gain, *gain);
    }
}

void KymeraAhm_GetFBGain(uint16* gain)
{
    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);

    if(op)
    {
        *gain = (uint16) OperatorsAhmGetFeedBackFineGain(op);
        DEBUG_LOG("KymeraAhm_GetFFGain FB fine gain 0x%x %d ", *gain, *gain);
    }
}

void KymeraAhm_UpdateInEarStatus(bool enable)
{
    Operator op = ChainGetOperatorByRole(KymeraAhm_GetChain(), OPR_AHM);

    if(op)
    {
        OperatorsAhmSetInEarCtrl(op, enable);
    }
}

bool KymeraAhm_UpdateFfPathFineGain(uint8 ff_fine_gain)
{
    Operator ahm_op = KymeraAhm_GetOperator();
    bool status = FALSE;

    if(ahm_op)
    {
        OperatorsAhmSetFfPathFineGainrCtrl(ahm_op, ff_fine_gain);
        status = TRUE;
    }

    return status;
}

bool KymeraAhm_IsFfFineGainRangeAdjustDisabled(void)
{
    uint32 ahm_config = kymeraAhm_GetConfigFlags();
    return (ahm_config & CONFIG_MASK(DISABLE_FF_GAIN_RANGE_ADJUST_FLAG_POS));
}

void KymeraAhm_DisableFfFineGainRangeAdjust(void)
{
    kymeraAhm_SetConfigFlags(DISABLE_FF_GAIN_RANGE_ADJUST_FLAG, DISABLE_FF_GAIN_RANGE_ADJUST_FLAG_POS);
}

void KymeraAhm_EnableFfFineGainRangeAdjust(void)
{
    kymeraAhm_SetConfigFlags(ENABLE_FF_GAIN_RANGE_ADJUST_FLAG, DISABLE_FF_GAIN_RANGE_ADJUST_FLAG_POS);
}

#ifdef ENABLE_ANC_FAST_MODE_SWITCH

/* Sets IIR coefficients, Static gains read from library to AHM capability and trigger transition */
void KymeraAhm_TriggerTransition(audio_anc_path_id control_path, adaptive_anc_hw_channel_t hw_channel, ahm_trigger_transition_ctrl_t transition)
{
    DEBUG_LOG("KymeraAhm_TriggerTransition, transition payload: enum:ahm_trigger_transition_ctrl_t:%d, ", transition);

    Operator op = KymeraAhm_GetOperator();

    kymeraAhm_SetIirCoefficients(op, AUDIO_ANC_INSTANCE_0);
    if (appKymeraIsParallelAncFilterEnabled())
    {
        kymeraAhm_SetIirCoefficients(op, AUDIO_ANC_INSTANCE_1);
    }

    /* Set static gains: FF static is static + world volume */
    kymeraAhm_SetStaticGain(op, control_path, hw_channel);

    OperatorsAhmSetTriggerTransitionCtrl(op, transition);
}

#endif /* ENABLE_ANC_FAST_MODE_SWITCH */

#endif

