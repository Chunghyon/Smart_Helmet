/*!
\copyright  Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Part of kymera.h used only during setup/init.
 
*/

#ifndef KYMERA_SETUP_H_
#define KYMERA_SETUP_H_

#include <chain.h>

/*@{*/

/*! \brief List of all supported audio chains. */
typedef struct
{
    const chain_config_t *chain_aptx_ad_tws_plus_decoder_config;
    const chain_config_t *chain_forwarding_input_aptx_left_config;
    const chain_config_t *chain_forwarding_input_aptx_right_config;
    const chain_config_t *chain_input_aac_stereo_mix_config;
    const chain_config_t *chain_input_sbc_stereo_mix_config;
    const chain_config_t *chain_input_aptx_stereo_mix_config;
    const chain_config_t *chain_aec_config;
    const chain_config_t *chain_tone_gen_config;
    const chain_config_t *chain_prompt_sbc_config;
    const chain_config_t *chain_prompt_aac_config;
    const chain_config_t *chain_prompt_pcm_config;
    const chain_config_t *chain_sco_nb_config;      /* only used by kymera_usb_sco */
    const chain_config_t *chain_sco_wb_config;      /* only used by kymera_usb_sco */
    const chain_config_t *chain_sco_swb_config;     /* only used by kymera_usb_sco */
    const chain_config_t *chain_aanc_config;
    const chain_config_t *chain_anc_config;
    const chain_config_t *chain_aancv2_config;
    const chain_config_t *chain_ahm_config;
    const chain_config_t *chain_adaptive_ambient_config;
    const chain_config_t *chain_hcgr_config;
    const chain_config_t *chain_aanc_fbc_config;
    const chain_config_t *chain_aanc_fbpath_fbc_config;
    const chain_config_t *chain_aanc_splitter_mic_ref_path_config;
    const chain_config_t *chain_wind_detect_config;
    const chain_config_t *chain_self_speech_detect_config;
    const chain_config_t *chain_aah_config;
    const chain_config_t *chain_fit_test_mic_path_config;
    const chain_config_t *chain_input_aptx_adaptive_stereo_mix_config;
    const chain_config_t *chain_input_aptx_adaptive_stereo_mix_q2q_config;
    const chain_config_t *chain_input_sbc_stereo_config;
    const chain_config_t *chain_input_aptx_stereo_config;
    const chain_config_t *chain_input_aptxhd_stereo_config;
    const chain_config_t *chain_input_aptx_adaptive_stereo_config;
    const chain_config_t *chain_input_aptx_adaptive_stereo_q2q_config;
    const chain_config_t *chain_input_aac_stereo_config;
    const chain_config_t *chain_music_processing_config_p0;
    const chain_config_t *chain_music_processing_config_p1;
    const chain_config_t *chain_input_wired_analog_stereo_config;
    const chain_config_t *chain_lc3_iso_mono_decoder_config;
    const chain_config_t *chain_input_usb_stereo_config;
    const chain_config_t *chain_usb_voice_rx_mono_config;
    const chain_config_t *chain_usb_voice_rx_stereo_config;
    const chain_config_t *chain_usb_voice_wb_config;
    const chain_config_t *chain_usb_voice_swb_config;
    const chain_config_t *chain_usb_voice_wb_2mic_config;
    const chain_config_t *chain_usb_voice_wb_2mic_binaural_config;
    const chain_config_t *chain_usb_voice_nb_config;
    const chain_config_t *chain_usb_voice_nb_2mic_config;
    const chain_config_t *chain_usb_voice_nb_2mic_binaural_config;
    const chain_config_t *chain_mic_resampler_config;
    const chain_config_t *chain_input_wired_sbc_encode_config;
    const chain_config_t *chain_input_wired_aptx_adaptive_encode_config;
    const chain_config_t *chain_input_usb_aptx_adaptive_encode_config;
    const chain_config_t *chain_input_wired_aptxhd_encode_config;
    const chain_config_t *chain_input_usb_aptxhd_encode_config;
    const chain_config_t *chain_input_wired_aptx_classic_encode_config;
    const chain_config_t *chain_input_usb_aptx_classic_encode_config;
    const chain_config_t *chain_input_usb_sbc_encode_config;
    const chain_config_t *chain_output_volume_mono_config;
    const chain_config_t *chain_output_volume_stereo_config;
    const chain_config_t *chain_output_volume_common_config;
    const chain_config_t *chain_va_graph_manager_config;
#ifdef INCLUDE_APTX_ADAPTIVE_22
    const chain_config_t *chain_input_aptx_adaptive_r3_mono_q2q_config;
    const chain_config_t *chain_input_aptx_adaptive_r3_mono_config;
#endif
} kymera_chain_configs_t;

/*! \brief The mic framework config */
typedef struct
{
    capability_id_t cap_id_splitter;
} kymera_mic_config_t;

/*! \brief Populate all audio chains configuration.

    Number of audio chains used and its specific configuration may depend
    on an application. Only pointers to audio chains used in particular application
    have to be populated.

    This function must be called before audio is used.

    \param configs Audio chains configuration.
*/
void Kymera_SetChainConfigs(const kymera_chain_configs_t *configs);

/*! \brief Get audio chains configuration.

    \return Audio chains configuration.
*/
const kymera_chain_configs_t *Kymera_GetChainConfigs(void);

/*! \brief Initializes mic framework

    The splitter used in mic framework can be either the ROM splitter or the downloadable.
    Decision of what to use is done in the application.

    \param splitter_cap_id the splitter capability ID
*/
void Kymera_MicInit(const kymera_mic_config_t* mic_config);

/*@}*/

#endif /* KYMERA_SETUP_H_ */
