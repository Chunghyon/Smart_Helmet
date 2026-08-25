/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       a2dp_profile_caps_aptx_adaptive.h
\defgroup   a2dp_profile_aptx_adaptive A2DP profile aptx adaptive
\ingroup    a2dp_profile
\brief      A2DP profile aptX Adaptive
*/

#ifndef A2DP_PROFILE_APTX_ADAPTIVE_H
#define A2DP_PROFILE_APTX_ADAPTIVE_H

#include "av_typedef.h"
#include <a2dp.h>

extern uint8 aptx_ad_caps_sink[52];
extern const uint8 tws_aptx_ad_caps[62];

#ifdef INCLUDE_APTX_ADAPTIVE
extern const sep_config_type av_aptx_adaptive_snk_sep;
extern const sep_config_type av_aptx_adaptive_src_sep;
#endif

/*!
 *  These enum values must match the aptX Adaptive A2DP capability options defined in the config xml files.
 */
typedef enum
{
    aptx_ad_advertise_r1 = 0,
    aptx_ad_advertise_r2 = 1,
    aptx_ad_advertise_r22 = 2
} aptx_ad_advertise_options_t;

/*!
 * Bits to set for each channel mode of the aptX Adaptive service capability
 */
typedef enum {
    aptx_ad_channel_mode_mono         = (1 << 0),
    aptx_ad_channel_mode_stereo       = (1 << 1),
    aptx_ad_channel_mode_tws_stereo   = (1 << 2),
    aptx_ad_channel_mode_joint_stereo = (1 << 3),
    aptx_ad_channel_mode_tws_mono     = (1 << 4),
    aptx_ad_channel_mode_tws_plus     = (1 << 5)
} aptx_ad_channel_mode_masks_t;

/*!
 * Octet offset from AVDTP_SERVICE_MEDIA_CODEC in aptX Adaptive decoder service capability.
 */
typedef enum {
    aptx_ad_sample_rate_offset                      = 10,
    aptx_ad_channel_mode_offset                     = 11,
    aptx_ad_ll_ttp_min_offset                       = 12,
    aptx_ad_ll_ttp_max_offset                       = 13,
    aptx_ad_hq_ttp_min_offset                       = 14,
    aptx_ad_hq_ttp_max_offset                       = 15,
    aptx_ad_tws_ttp_min_offset                      = 16,
    aptx_ad_tws_ttp_max_offset                      = 17,
    aptx_ad_version_number_offset                   = 19,
    aptx_ad_capability_extension_end_offset_for_r1  = 20,
    aptx_ad_supported_features_start_offset         = 20,
    aptx_ad_setup_preference_start_offset           = 24,
    aptx_ad_capability_extension_end_offset_for_r2  = 29
} octet_offsets_in_aptx_ad_decoder_specific_caps_t;

/*!
 * Bits to set for each sampling rate of the aptX Adaptive service capability
 */
typedef enum
{
    aptx_ad_sample_rate_96000_twm = (1 << 7),
    aptx_ad_sample_rate_44100_lossless = (1 << 6),
    aptx_ad_sample_rate_96000 = (1 << 5),
    aptx_ad_sample_rate_48000 = (1 << 4),
    aptx_ad_sample_rate_44100 = (1 << 3)
} aptx_ad_sample_rates_t;

typedef enum
{
    aptx_ad_cap_ext_version_1 = 0,
    aptx_ad_cap_ext_version_2 = 1,
} aptx_ad_cap_ext_version_t;

typedef enum
{
    aptx_ad_setup_preference_1 = 0,
    aptx_ad_setup_preference_2 = 1,
    aptx_ad_setup_preference_3 = 2,
    aptx_ad_setup_preference_4 = 3
} setup_preference_priority_t;

typedef enum
{
    aptx_ad_generic_unsupported = 0,
    aptx_ad_generic_supported   = 1
}aptx_ad_generic_supported_t;

/*************************************************************************
NAME
    A2dpProfileAptxAdInitServiceCapability

DESCRIPTION
    Initialise the aptx adaptive service capability based on configuration
    data from the Config Tool.

RETURNS
    None

**************************************************************************/
void A2dpProfileAptxAdInitServiceCapability(void);


#ifdef APTX_ADAPTIVE_SUPPORT_96K

/*! \brief enables sample rate in the capability exchange structure
    \return TRUE if the structure has been modifed. FALSE otherwise
 */
bool addAptxADSampleRate(aptx_ad_sample_rates_t rate);

/*! \brief removes sample rate in the capability exchange structure
    \return TRUE if the structure has been modifed. FALSE otherwise
 */
bool removeAptxADSampleRate(aptx_ad_sample_rates_t rate);

#endif

/*! \brief Select an appropriate codec configuration for aptX Adaptive

    Compare the remote device's capabilities to our own, and select a suitable
    configuration based on common capabilities, taking into consideration any
    application preferences e.g. preferred sample rate. The final selected codec
    capabilities are returned back via the remote_caps pointer, overwriting the
    remote device's capabilities that were passed in originally.

    \param av_instance The AV instance we are selecting codec configuration for.
    \param remote_caps The remote device's capabilities. These will be modified
                       in-place and overwritten with the final selected
                       capabilities, which will be a sub-set of the original
                       passed-in remote capabilities.

    \return TRUE if a common configuration successfully found, FALSE otherwise.
 */
bool A2dpProfileAptxAdSelectCodecConfiguration(avInstanceTaskData *av_instance, uint8 *remote_caps);

/*! \brief Modify selected sample rate in aptX Adaptive capabilities if required

    Since the application's preference for sample rate can change after codec
    negotiation has already taken place, it may become necessary to reconfigure
    the media channel to select a different rate. This function examines the
    currently configured codec capabilities, and modifies them in-place if a
    different sample rate is now required, overwriting the original sample rate
    selection and returning TRUE. Only the sample rate is altered, nothing else.
    If no rate change is required at this time, the function returns FALSE.

    \param av_instance The AV instance we are selecting codec configuration for.
    \param configured_caps The currently configured codec capabilities that were
                           selected last time. These will be modified in-place
                           and overwritten with a new sample rate, if required.

    \return TRUE if a new sample rate has been selected (capabilities modified),
            and the media channel now needs to be reconfigured. FALSE otherwise.
 */
bool A2dpProfileAptxAdReselectSampleRate(avInstanceTaskData *av_instance, uint8 *configured_caps);

#endif // A2DP_PROFILE_APTX_ADAPTIVE_H
