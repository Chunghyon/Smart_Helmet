/*!
\copyright  Copyright (c) 2020 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       a2dp_profile_caps_aptx_adaptive.c
\defgroup   a2dp_profile_aptx_adaptive A2DP profile aptx adaptive
\ingroup    a2dp_profile
\brief      A2DP profile for aptX Adaptive
*/

#include "a2dp_profile_caps.h"
#include "a2dp_profile_caps_aptx_adaptive.h"
#include "adk_log.h"
#include <a2dp.h>
#include <av.h>
#include <byte_utils.h>
#include <feature.h>
#include <panic.h>

#define RESERVED                      (0x00)
#define LENGTH_OF_CP_TYPE_SCMS_VALUE  (2)
#define OFFSET_FOR_LENGTH_OF_TWS_CAPS (10)


/*!
 *  Hardcoded aptX adaptive LL Mode latency values
 *  Value is set by latency / 5
 *  AOSP Generic Mode enabled
 */
#ifdef INCLUDE_STEREO
#define APTX_AD_MIN_LL_TTP_5G   (uint8)(0x30)  /* base TTP + 15 (3*5) */
#define APTX_AD_MIN_LL_TTP_2G4  (uint8)(0x7)   /* 5G TTP + 35 (7*5) */
#define APTX_AD_MAX_LL_TTP_5G   (uint8)(0x60)  /* base TTP + 30 */
#define APTX_AD_MAX_LL_TTP_2G4  (uint8)(0xA)   /* Max LL 5G TTP + 50 */
#else
/*!
 *  Earbud min/max latency is higher than headset for LL
 */
#define APTX_AD_MIN_LL_TTP_5G   (uint8)(0x50)  /* base TTP + 25 (5*5) */
#define APTX_AD_MIN_LL_TTP_2G4  (uint8)(0x3)   /* 5G TTP + 15 (3*5) */
#define APTX_AD_MAX_LL_TTP_5G   (uint8)(0x50)  /* base TTP + 25 */
#define APTX_AD_MAX_LL_TTP_2G4  (uint8)(0x3)   /* Max LL 5G TTP + 15 */
#endif

/*!
 *  Hardcoded aptX adaptive LL Mode latency values
 *  AOSP Generic Mode disabled
 */
#define APTX_AD_LL_TTP_MIN_IN_1MS  0        // Minimum latency in milliseconds for low-latency mode
#define APTX_AD_LL_TTP_MAX_IN_4MS  125      // Max latency for low-latency mode in 4ms units (i.e. 125*4ms)

/*!
 *  Hardcoded aptX adaptive HQ and TWS Mode latency values
 *  AOSP Generic Mode disabled
 */
#ifdef INCLUDE_STEREO
#define APTX_AD_HQ_TTP_MIN_IN_1MS  200      /* Minimum latency in milliseconds for HQ mode */
#else
#define APTX_AD_HQ_TTP_MIN_IN_1MS  250      /* Minimum latency in milliseconds for HQ mode for earbuds */
#endif
#define APTX_AD_HQ_TTP_MAX_IN_4MS  125      /* Max latency for HQ mode in 4ms units (i.e. 125*4ms) */
#define APTX_AD_TWS_TTP_MIN_IN_1MS 100      /* Minimum latency in milliseconds for TWS mode */
#define APTX_AD_TWS_TTP_MAX_IN_4MS 125      /* Max latency for TWS mode in 4ms units (i.e. 125*4ms) */

/*!
 *  Hardcoded aptX adaptive latency limits for source applications
 *  AOSP Generic Mode disabled
 */
#define APTX_AD_SRC_LL_TTP_MIN_IN_1MS   0   /* Minimum latency accepted by source for LL mode, in milliseconds. */
#define APTX_AD_SRC_LL_TTP_MAX_IN_4MS   75  /* Max latency accepted by source for LL mode, in 4ms units (i.e. 75*4ms). */
#define APTX_AD_SRC_HQ_TTP_MIN_IN_1MS   0   /* Minimum latency accepted by source for HQ mode, in milliseconds. */
#define APTX_AD_SRC_HQ_TTP_MAX_IN_4MS   75  /* Max latency accepted by source for HQ mode, in 4ms units (i.e. 75*4ms). */
#define APTX_AD_SRC_TWS_TTP_MIN_IN_1MS  100 /* Minimum latency accepted by source for TWS mode, in milliseconds. */
#define APTX_AD_SRC_TWS_TTP_MAX_IN_4MS  75  /* Max latency accepted by source for TWS mode, in 4ms units (i.e. 75*4ms). */

/*!
 *  Hardcoded aptX adaptive capability extension features for source applications
 */
#define APTX_AD_SRC_CAPABILITY_EXTENSION_VERSION    0x01
#define APTX_AD_SRC_FIRST_SETUP_PREFERENCE          0x02
#define APTX_AD_SRC_SECOND_SETUP_PREFERENCE         0x03
#define APTX_AD_SRC_THIRD_SETUP_PREFERENCE          0x03
#define APTX_AD_SRC_FOURTH_SETUP_PREFERENCE         0x03

#define APTX_AD_SRC_SUPPORTED_FEATURES  (aptx_ad_default_features | aptx_ad_twm_support | \
                                         aptx_ad_2_1_kernel_support | aptx_ad_split_streaming)
#define APTX_AD_SRC_SAMPLE_RATES        (aptx_ad_sample_rate_96000 | aptx_ad_sample_rate_96000_twm | \
                                         aptx_ad_sample_rate_48000)
#define APTX_AD_SRC_CHANNEL_MODES       (aptx_ad_channel_mode_stereo | aptx_ad_channel_mode_joint_stereo)

/* General aptX adaptive capability extension markers */
#define APTX_AD_NO_FURTHER_EXPANSION        (0x00)
#define APTX_AD_CAPABILITY_EXTENSION_END    (0xAA)

/* Max TTP latency values in the service capability are not in units of 1ms */
#define MAX_TTP_LATENCY_UNIT_IN_MS (4)

#define REPEAT_OCTET_5_TIMES(x)  (x), (x), (x), (x), (x)
#define REPEAT_OCTET_10_TIMES(x) REPEAT_OCTET_5_TIMES(x), REPEAT_OCTET_5_TIMES(x)
#define REPEAT_OCTET_14_TIMES(x) REPEAT_OCTET_10_TIMES(x), (x), (x), (x), (x)
#define REPEAT_OCTET_26_TIMES(x) REPEAT_OCTET_10_TIMES(x), REPEAT_OCTET_10_TIMES(x), REPEAT_OCTET_5_TIMES(x), (x)


/*!
 * The number of octets the aptX Adaptive service capability
 */
typedef enum
{
    length_of_aptx_adaptive_cap = 42,
    length_of_src_aptx_adaptive_cap = length_of_aptx_adaptive_cap,
    length_of_tws_aptx_adaptive_cap = OFFSET_FOR_LENGTH_OF_TWS_CAPS + length_of_aptx_adaptive_cap
} length_of_aptx_ad_service_capability_t;


#define APTX_AD_EMBEDDED_SERVICE_CAPABILITY \
    AVDTP_SERVICE_MEDIA_CODEC, \
    length_of_aptx_adaptive_cap, \
    AVDTP_MEDIA_TYPE_AUDIO << 2, \
    AVDTP_MEDIA_CODEC_NONA2DP,\
    SPLIT_IN_4_OCTETS(A2DP_QTI_VENDOR_ID), \
    SPLIT_IN_2_OCTETS(A2DP_QTI_APTX_AD_CODEC_ID), \
    aptx_ad_sample_rate_48000 | aptx_ad_generic_supported ,\
    aptx_ad_channel_mode_joint_stereo | aptx_ad_channel_mode_stereo, \
    APTX_AD_MIN_LL_TTP_5G | APTX_AD_MIN_LL_TTP_2G4, \
    APTX_AD_MAX_LL_TTP_5G | APTX_AD_MAX_LL_TTP_2G4, \
    APTX_AD_HQ_TTP_MIN_IN_1MS, \
    APTX_AD_HQ_TTP_MAX_IN_4MS, \
    APTX_AD_TWS_TTP_MIN_IN_1MS, \
    APTX_AD_TWS_TTP_MAX_IN_4MS, \
    REPEAT_OCTET_26_TIMES(0),\
    AVDTP_SERVICE_CONTENT_PROTECTION, \
    LENGTH_OF_CP_TYPE_SCMS_VALUE, \
    AVDTP_CP_TYPE_SCMS_LSB, \
    AVDTP_CP_TYPE_SCMS_MSB, \
    AVDTP_SERVICE_DELAY_REPORTING, \
    0


/*!
 * Default aptX Adaptive Capabilities for the application to pass to the A2DP library during initialisation.
 *  NOTE: The capability is modified by A2dpProfileAptxAdInitServiceCapability() before passing it to the A2DP library,
 *  therefore it is the end result of this modification that reflects the "real" service capability and this array
 *  initialisation is merely used as a base to simplify the code it.
 *  The octets populated by A2dpProfileAptxAdInitServiceCapability() are initialised here as RESERVED (as well as octets
 *  that are actually reserved for future use).
 */
uint8 aptx_ad_caps_sink[52] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    APTX_AD_EMBEDDED_SERVICE_CAPABILITY
};

/*!
 * True Wireless Stereo service capability for aptX Adaptive
 */
const uint8 tws_aptx_ad_caps[62] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    length_of_tws_aptx_adaptive_cap,
    AVDTP_MEDIA_TYPE_AUDIO << 2,
    AVDTP_MEDIA_CODEC_NONA2DP,
    SPLIT_IN_4_OCTETS(A2DP_CSR_VENDOR_ID),
    SPLIT_IN_2_OCTETS(A2DP_CSR_TWS_APTX_AD_CODEC_ID),
    APTX_AD_EMBEDDED_SERVICE_CAPABILITY
};

/*!
 * AptX Adaptive capabilities for A2DP source applications.
 */
const uint8 aptx_ad_caps_src[] =
{
    AVDTP_SERVICE_MEDIA_TRANSPORT,
    0,
    AVDTP_SERVICE_MEDIA_CODEC,
    length_of_src_aptx_adaptive_cap,
    AVDTP_MEDIA_TYPE_AUDIO << 2,
    AVDTP_MEDIA_CODEC_NONA2DP,
    SPLIT_IN_4_OCTETS(A2DP_QTI_VENDOR_ID),
    SPLIT_IN_2_OCTETS(A2DP_QTI_APTX_AD_CODEC_ID),

    APTX_AD_SRC_SAMPLE_RATES,
    APTX_AD_SRC_CHANNEL_MODES,

    APTX_AD_SRC_LL_TTP_MIN_IN_1MS,
    APTX_AD_SRC_LL_TTP_MAX_IN_4MS,
    APTX_AD_SRC_HQ_TTP_MIN_IN_1MS,
    APTX_AD_SRC_HQ_TTP_MAX_IN_4MS,
    APTX_AD_SRC_TWS_TTP_MIN_IN_1MS,
    APTX_AD_SRC_TWS_TTP_MAX_IN_4MS,

    RESERVED,

    APTX_AD_SRC_CAPABILITY_EXTENSION_VERSION,
    SPLIT_IN_4_OCTETS(APTX_AD_SRC_SUPPORTED_FEATURES),
    APTX_AD_SRC_FIRST_SETUP_PREFERENCE,
    APTX_AD_SRC_SECOND_SETUP_PREFERENCE,
    APTX_AD_SRC_THIRD_SETUP_PREFERENCE,
    APTX_AD_SRC_FOURTH_SETUP_PREFERENCE,
    APTX_AD_NO_FURTHER_EXPANSION,
    APTX_AD_CAPABILITY_EXTENSION_END,

    REPEAT_OCTET_14_TIMES(RESERVED)
};



/*!@{ \name Standard sink endpoints
    \brief Predefined endpoints for audio Sink end point configurations  */
    /*! APTX Adaptive */
const sep_config_type av_aptx_adaptive_snk_sep = {AV_SEID_APTX_ADAPTIVE_SNK, DECODE_RESOURCE_ID, sep_media_type_audio, a2dp_sink, TRUE, 0, sizeof(aptx_ad_caps_sink), aptx_ad_caps_sink};
const sep_config_type av_aptx_adaptive_src_sep = {AV_SEID_APTX_ADAPTIVE_SRC, ENCODE_RESOURCE_ID, sep_media_type_audio, a2dp_source, FALSE, 0, sizeof(aptx_ad_caps_src), aptx_ad_caps_src};
/*!@} */

static uint8 * getStartOfCodecSpecificInformation(a2dp_role_type role)
{
    uint8 *service_caps;

    switch (role)
    {
        case a2dp_sink:
            service_caps = aptx_ad_caps_sink;
            break;

        case a2dp_source:
            service_caps = (uint8 *)aptx_ad_caps_src;
            break;

        default:
            /* Should never get here. */
            Panic();
            return NULL;
    }

    while (service_caps[0] != AVDTP_SERVICE_MEDIA_CODEC)
        service_caps += service_caps[1] + 2;

    return service_caps;
}

static void setCapabilityVersionNumber(uint8 version_number)
{
    uint8 *aptx_adaptive_codec_caps = getStartOfCodecSpecificInformation(a2dp_sink);
    aptx_adaptive_codec_caps[aptx_ad_version_number_offset] = version_number;
}

static void setSupportedFeatures(uint32 supported_features)
{
    uint8 *aptx_adaptive_codec_caps = getStartOfCodecSpecificInformation(a2dp_sink);
    ByteUtilsSet4Bytes(aptx_adaptive_codec_caps, aptx_ad_supported_features_start_offset, supported_features);
}

static void setSetupPreference(uint8 setup_preference, setup_preference_priority_t priority)
{
    uint8 *aptx_adaptive_codec_caps = getStartOfCodecSpecificInformation(a2dp_sink);
    aptx_adaptive_codec_caps[aptx_ad_setup_preference_start_offset + priority] = setup_preference;
}

static void setCapabilityExtensionEndForR1(void)
{
    uint8 *aptx_adaptive_codec_caps = getStartOfCodecSpecificInformation(a2dp_sink);
    aptx_adaptive_codec_caps[aptx_ad_capability_extension_end_offset_for_r1] = APTX_AD_CAPABILITY_EXTENSION_END;
}

static void setCapabilityExtensionEndForR2(void)
{
    uint8 *aptx_adaptive_codec_caps = getStartOfCodecSpecificInformation(a2dp_sink);
    aptx_adaptive_codec_caps[aptx_ad_capability_extension_end_offset_for_r2] = APTX_AD_CAPABILITY_EXTENSION_END;
}

#ifdef APTX_ADAPTIVE_SUPPORT_96K
bool addAptxADSampleRate(aptx_ad_sample_rates_t rate)
{
    uint8 *aptx_adaptive_codec_caps = getStartOfCodecSpecificInformation(a2dp_sink);

    if (aptx_adaptive_codec_caps[aptx_ad_sample_rate_offset] & rate )
        return FALSE;

    aptx_adaptive_codec_caps[aptx_ad_sample_rate_offset] |= rate;

    return TRUE;
}

bool removeAptxADSampleRate(aptx_ad_sample_rates_t rate)
{
    uint8 *aptx_adaptive_codec_caps = getStartOfCodecSpecificInformation(a2dp_sink);

    if ((aptx_adaptive_codec_caps[aptx_ad_sample_rate_offset] & rate) == 0)
        return FALSE;

    aptx_adaptive_codec_caps[aptx_ad_sample_rate_offset] ^= rate;

    return TRUE;
}
#endif
static void updateCapabilityBasedOnTheAdvertiseOption(aptx_ad_advertise_options_t advertise_option)
{
    switch (advertise_option)
    {
#ifdef INCLUDE_APTX_ADAPTIVE_22
        case aptx_ad_advertise_r22:
            setCapabilityVersionNumber(0x01);

#ifdef INCLUDE_MIRRORING
            setSupportedFeatures(aptx_ad_default_features | aptx_ad_2_1_kernel_support | aptx_ad_2_2_kern_dec_snk | aptx_ad_twm_support | aptx_ad_split_streaming);
#else
            setSupportedFeatures(aptx_ad_default_features | aptx_ad_2_1_kernel_support | aptx_ad_2_2_kern_dec_snk);
#endif
            setSetupPreference(0x02, aptx_ad_setup_preference_1);
            setSetupPreference(0x03, aptx_ad_setup_preference_2);
            setSetupPreference(0x03, aptx_ad_setup_preference_3);
            setSetupPreference(0x03, aptx_ad_setup_preference_4);
            setCapabilityExtensionEndForR2();
            break;
#endif
        case aptx_ad_advertise_r2:
            setCapabilityVersionNumber(0x01);
#ifdef INCLUDE_MIRRORING
            setSupportedFeatures(aptx_ad_default_features | aptx_ad_2_1_kernel_support | aptx_ad_twm_support | aptx_ad_split_streaming);
#else
            setSupportedFeatures(aptx_ad_default_features | aptx_ad_2_1_kernel_support);
#endif
            setSetupPreference(0x02, aptx_ad_setup_preference_1);
            setSetupPreference(0x03, aptx_ad_setup_preference_2);
            setSetupPreference(0x03, aptx_ad_setup_preference_3);
            setSetupPreference(0x03, aptx_ad_setup_preference_4);
            setCapabilityExtensionEndForR2();
            break;

        case aptx_ad_advertise_r1:
        default:
            setCapabilityVersionNumber(0x00);
            setCapabilityExtensionEndForR1();
            break;
    }
}

static bool selectSampleRate(uint8 supported_rates, avA2dpPreferredSampleRate preferred_rate, uint8 *selected_rate)
{
    bool rate_chosen = TRUE;

    /* Choose single sample rate from those common to both sink & source, taking
       into account application preference. Note that 44.1kHz support is now
       deprecated, so 48kHz is used in its place (with appropriate resampling).
       If there's no particular rate preference, try the highest rates first.
       Never choose a rate higher than the preference if one has been set. */
    switch (preferred_rate)
    {
        case A2DP_PREFERRED_RATE_NONE:
        case A2DP_PREFERRED_RATE_96K:
            if (supported_rates & aptx_ad_sample_rate_96000_twm)
            {
                *selected_rate = aptx_ad_sample_rate_96000_twm;
                DEBUG_LOG_DEBUG("A2dpProfileAptxAd: enum:avA2dpPreferredSampleRate:%d,"
                                " selecting 96kHz TWM", preferred_rate);
                break;
            }
            else if (supported_rates & aptx_ad_sample_rate_96000)
            {
                *selected_rate = aptx_ad_sample_rate_96000;
                DEBUG_LOG_DEBUG("A2dpProfileAptxAd: enum:avA2dpPreferredSampleRate:%d,"
                                " selecting 96kHz", preferred_rate);
                break;
            }
            /* Fall through */

        case A2DP_PREFERRED_RATE_44K1:
        case A2DP_PREFERRED_RATE_48K:
        default:
            if (supported_rates & aptx_ad_sample_rate_48000)
            {
                *selected_rate = aptx_ad_sample_rate_48000;
                DEBUG_LOG_DEBUG("A2dpProfileAptxAd: enum:avA2dpPreferredSampleRate:%d,"
                                " selecting 48kHz", preferred_rate);
                break;
            }
            /* No common sample rate. */
            rate_chosen = FALSE;
    }

    return rate_chosen;
}

static bool selectChannelMode(uint8 supported_modes, uint8 *selected_mode)
{
    bool mode_chosen = TRUE;

    /* Choose channel mode from those common to both sink & source. Note that
       the latest version of the encoder doesn't support any TWS/mono modes, so
       those are skipped. Also aptX Adaptive doesn't really support joint stereo,
       but some older sinks incorrectly advertise support for it even though they
       actually just decode in stereo. Therefore allow joint stereo, but always
       choose normal stereo in preference (we encode in stereo regardless). */
    if (supported_modes & aptx_ad_channel_mode_stereo)
    {
        *selected_mode = aptx_ad_channel_mode_stereo;
    }
    else if (supported_modes & aptx_ad_channel_mode_joint_stereo)
    {
        *selected_mode = aptx_ad_channel_mode_joint_stereo;
    }
    else
    {
        /* No common channel mode. */
        mode_chosen = FALSE;
    }

    return mode_chosen;
}

void A2dpProfileAptxAdInitServiceCapability(void)
{
#ifdef APTX_ADAPTIVE_SUPPORT_R1_ONLY
    updateCapabilityBasedOnTheAdvertiseOption(aptx_ad_advertise_r1);
#elif defined (INCLUDE_APTX_ADAPTIVE_22)
    if (FeatureVerifyLicense(APTX_ADAPTIVE_LOSSLESS_DECODE) == TRUE)
    {
        updateCapabilityBasedOnTheAdvertiseOption(aptx_ad_advertise_r22);
#ifdef APTX_ADAPTIVE_SUPPORT_96K
        addAptxADSampleRate(aptx_ad_sample_rate_44100_lossless);
#endif
    }
    else
    {
        updateCapabilityBasedOnTheAdvertiseOption(aptx_ad_advertise_r2);
    }
#else
    updateCapabilityBasedOnTheAdvertiseOption(aptx_ad_advertise_r2);
#endif

#ifdef APTX_ADAPTIVE_SUPPORT_96K
#ifdef INCLUDE_MIRRORING
    addAptxADSampleRate(aptx_ad_sample_rate_96000_twm);
#else
    addAptxADSampleRate(aptx_ad_sample_rate_96000);
#endif
#endif
}

bool A2dpProfileAptxAdSelectCodecConfiguration(avInstanceTaskData *av_instance, uint8 *remote_caps)
{
    DEBUG_LOG_FN_ENTRY("A2dpProfileAptxAdSelectCodecConfiguration");

    if (!av_instance || !remote_caps)
    {
        DEBUG_LOG_DEBUG("A2dpProfileAptxAdSelectCodecConfiguration: Invalid parameters");
        return FALSE;
    }

    const uint8 *local_caps = getStartOfCodecSpecificInformation(a2dp_source);
    uint8 *selected_caps = remote_caps;

    /* We will return the final selected capabilities by overwriting the passed
       in array originally containing the remote's capabilities. So first store
       all the remote capabilities of interest. */
    uint8 remote_sample_rates = remote_caps[aptx_ad_sample_rate_offset];
    uint8 remote_channel_modes = remote_caps[aptx_ad_channel_mode_offset];
    uint32 remote_supported_features = 0;
    if (remote_caps[aptx_ad_version_number_offset] == aptx_ad_cap_ext_version_2)
    {
        remote_supported_features = ByteUtilsGet4BytesFromStream(&remote_caps[aptx_ad_supported_features_start_offset]);
    }

    DEBUG_LOG_DEBUG("A2dpProfileAptxAd: remote_caps: rates=0x%02x, modes=0x%02x, features=0x%08lx",
                    remote_sample_rates, remote_channel_modes, remote_supported_features);

    /* Also create aliases for local capabilities, for convenience. */
    uint8 local_sample_rates = local_caps[aptx_ad_sample_rate_offset];
    uint8 local_channel_modes = local_caps[aptx_ad_channel_mode_offset];
    uint32 local_supported_features = ByteUtilsGet4BytesFromStream(&local_caps[aptx_ad_supported_features_start_offset]);

    DEBUG_LOG_VERBOSE("A2dpProfileAptxAd:  local_caps: rates=0x%02x, modes=0x%02x, features=0x%08lx",
                      local_sample_rates, local_channel_modes, local_supported_features);

    /* Eliminate any capabilities not common to both sink & source. */
    selected_caps[aptx_ad_sample_rate_offset] = remote_sample_rates & local_sample_rates;
    selected_caps[aptx_ad_channel_mode_offset] = remote_channel_modes & local_channel_modes;
    if (remote_caps[aptx_ad_version_number_offset] == aptx_ad_cap_ext_version_2)
    {
        ByteUtilsSet4Bytes(selected_caps, aptx_ad_supported_features_start_offset,
                           remote_supported_features & local_supported_features);
    }

    DEBUG_LOG_VERBOSE("A2dpProfileAptxAd: common_caps: rates=0x%02x, modes=0x%02x, features=0x%08lx",
                      selected_caps[aptx_ad_sample_rate_offset], selected_caps[aptx_ad_channel_mode_offset],
                      remote_supported_features & local_supported_features);

    /* This is the only time we will have access to the full list of common
       sample rates supported by both sink & source. Store them in case we need
       to renegotiate a different sample rate later on. */
    av_instance->a2dp.supported_sample_rates = selected_caps[aptx_ad_sample_rate_offset];

    /* Choose single sample rate from the common rates remaining. */
    if (!selectSampleRate(av_instance->a2dp.supported_sample_rates,
                          av_instance->a2dp.preferred_sample_rate,
                          &selected_caps[aptx_ad_sample_rate_offset]))
    {
        /* Unable to find a common sample rate, reject codec. */
        DEBUG_LOG_ERROR("A2dpProfileAptxAd: Unable to find common sample rate!");
        return FALSE;
    }

    /* Choose single channel mode from the common modes remaining. */
    if (!selectChannelMode(selected_caps[aptx_ad_channel_mode_offset],
                           &selected_caps[aptx_ad_channel_mode_offset]))
    {
        /* Unable to find a common channel mode, reject codec. */
        DEBUG_LOG_ERROR("A2dpProfileAptxAd: Unable to find common channel mode!");
        return FALSE;
    }

    DEBUG_LOG_INFO("A2dpProfileAptxAd: selected_caps: rate=0x%02x, mode=0x%02x, features=0x%08lx",
                   selected_caps[aptx_ad_sample_rate_offset], selected_caps[aptx_ad_channel_mode_offset],
                   remote_supported_features & local_supported_features);
    return TRUE;
}

bool A2dpProfileAptxAdReselectSampleRate(avInstanceTaskData *av_instance, uint8 *configured_caps)
{
    DEBUG_LOG_FN_ENTRY("A2dpProfileAptxAdReselectSampleRate");

    if (!av_instance || !configured_caps || !av_instance->a2dp.supported_sample_rates)
    {
        /* Don't know which rates are supported, cannot make a new selection. */
        DEBUG_LOG_DEBUG("A2dpProfileAptxAd: Remote-supported sample rates unknown");
        return FALSE;
    }

    uint8 new_rate;
    uint8 current_rate = configured_caps[aptx_ad_sample_rate_offset];

    /* Check if new sample rate based on potentially updated preferences. */
    if (!selectSampleRate(av_instance->a2dp.supported_sample_rates,
                          av_instance->a2dp.preferred_sample_rate, &new_rate))
    {
        /* Shouldn't ever get here - there were common supported sample rates
           when configuration was originally selected. */
        DEBUG_LOG_ERROR("A2dpProfileAptxAd: Unable to find common sample rate!");
        return FALSE;
    }

    if (new_rate == current_rate)
    {
        /* Nothing to do. */
        DEBUG_LOG_DEBUG("A2dpProfileAptxAd: Preferred rate already selected");
        return FALSE;
    }

    configured_caps[aptx_ad_sample_rate_offset] = new_rate;
    return TRUE;
}
