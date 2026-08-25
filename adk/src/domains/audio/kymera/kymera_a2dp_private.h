/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_a2dp_private.h
\brief      Private (internal) kymera A2DP header file.

*/

#ifndef KYMERA_A2DP_PRIVATE_H
#define KYMERA_A2DP_PRIVATE_H

#include <a2dp.h>
#include <source.h>
#include <chain.h>
#include <kymera_volume.h>
#include <audio_a2dp_types.h>

typedef enum
{
    kymera_a2dp_idle,
    kymera_a2dp_preparing,
    kymera_a2dp_prepared,
    kymera_a2dp_starting,
    kymera_a2dp_streaming,
    kymera_a2dp_forwarding,
} kymera_a2dp_state_t;

#define MIXER_GAIN_RAMP_SAMPLES 24000

/*! Defines used for audio sync source configuration */

/* Synchronisation interval (in msec) for audio sync source stream.*/
#define AUDIO_SYNC_MS_INTERVAL (300)

/* MTU for audio sync source stream. It should be multiple of audio
 * sync sample (6 bytes). It has been set to 48 in order to fit the
 * the source stream packet in 2-DH1 (56 bytes) radio packet which
 * also contains 2 byte header and 4 bytes L2CAP header.
 */
#define AUDIO_SYNC_PACKET_MTU (48)

/*! \brief Timeout for A2DP Data Sync event (in milliseconds) */
#define A2DP_MIRROR_DATA_SYNC_IND_TIMEOUT_MS  (100)

/*! After starting A2DP mirroring, the maximum time to wait (in milliseconds) for a
    #MESSAGE_MORE_DATA before reverting to unsynchronised primary/secondary start */
#define A2DP_MIRROR_MESSAGE_MORE_DATA_TIMEOUT_MS (500)

/*! When the primary is in mode MIRROR_PROFILE_A2DP_START_PRIMARY_SYNC_UNMUTE
    the secondary sends a message informing the primary of the instant at
    which to unmute the output - the secondary will also unmute its output at
    this instant. If for some reason the unmute message is not received from the
    secondary, the primary needs to unmute after a timeout. */
#define A2DP_MIRROR_SYNC_UNMUTE_TIMEOUT_MS D_SEC(1)

/*! Latest a2dp parameters. This is to create a config struct for the output chain creation */
typedef struct
{
    /*! The current A2DP stream endpoint identifier. */
    uint8 seid;
    /*! The output sample rate */
    uint32 rate;
} a2dp_params_getter_t;

/*! \brief Helper function to unpack a2dp codec settings into individual variables.

    \param codec_settings The A2DP codec settings to unpack.
    \param seid [out] The stream endpoint id.
    \param source [out] The media source.
    \param rate [out] The media sample rate in Hz.
    \param cp_enabled [out] Content protection enabled.
    \param mtu [out] Media channel L2CAP mtu.
 */
void appKymeraGetA2dpCodecSettingsCore(const a2dp_codec_settings *codec_settings,
                                              uint8 *seid, Source *source, uint32 *rate,
                                              bool *cp_enabled, uint16 *mtu);

/*! \brief Helper function to unpack a2dp codec settings for aptX adaptive into individual variables.

    \param codec_settings The A2DP codec settings to unpack.
    \param split_tx [out] split tx enabled
    \param r22_dec [out] R2.2 decode is enabled
    \param r22_enc [out] R2.2 encode is enabled
 */
void appKymeraGetA2dpCodecSettingsAptxAdaptive(const a2dp_codec_settings *codec_settings, bool *split_tx, bool *r22_dec, bool *r22_enc);

/*! \brief Helper function to configure the RTP decoder.

    \param op The operator id of the RTP decoder.
    \param codec_type The codec type to configure.
    \param mode working mode to configure.
    \param rate The sample rate to configure in Hz.
    \param cp_enabled Content protection enabled.
    \param buffer_size The size of the buffer to use in words. If zero the buffer size will not be configured.
 */
void appKymeraConfigureRtpDecoder(Operator op, rtp_codec_type_t codec_type, rtp_working_mode_t mode, uint32 rate, bool cp_header_enabled, unsigned buffer_size);

/*! \brief Helper function to initially configure the l/r mixer in the A2DP input chain.

    \param chain The chain containing the left/right mixer.
    \param rate The sample rate to configure in Hz.
    \param stereo_lr_mix If TRUE the mixer will output a 50%/50% mix of the
            incoming stereo channels. If FALSE the mixer will output 100% left/right
            based on the is_left parameter.
    \param is_left Earbud is left/right.
 */
void appKymeraConfigureLeftRightMixer(kymera_chain_handle_t chain, uint32 rate, bool stereo_lr_mix, bool is_left);

/*! \brief Helper function to set/change the l/r mixer mode in the A2DP input chain.
    \param stereo_lr_mix If TRUE the mixer will output a 50%/50% mix of the
            incoming stereo channels. If FALSE the mixer will output 100% left/right
            based on the is_left parameter.
    \param is_left If TRUE, the earbud is left and 100% left channel media will
            be output by the mixer when stereo_lr_mix is FALSE.
            If FALSE, the earbud if right and 100% right channel media will be
            output by the mixer when stereo_lr_mix is FALSE.
 */
void appKymeraSetLeftRightMixerMode(kymera_chain_handle_t chain, bool stereo_lr_mix, bool is_left);

/*! \brief Reconfigure the SPC and mixer when in aptx classic mode
    \chain The chain containing the left/right mixer.
    \stereo_lr_mix if TRUE, enable the left / right mix. Feed data to both aptx decoders and mix at 50% volume each
    \is_left TRUE if we are the left channel
    \return Audio buffer size in words (16-bit).
 */
void appKymeraReConfigureClassicChain(kymera_chain_handle_t chain, bool stereo_lr_mix, bool is_left);

/*! \brief Convert the TTP Latency values passed in the capability exchange into values in milliseconds
    \param ttp_in_non_q2q_mode NQ2Q TTP Latency values from capability exchange.
    \param aptx_ad_ttp NQ2Q TTP Latency values for SSRC in milliseconds.
 */
void convertAptxAdaptiveTtpToOperatorsFormat(const aptx_adaptive_ttp_latencies_t ttp_in_non_q2q_mode,
                                             aptx_adaptive_ttp_in_ms_t *aptx_ad_ttp);

/*! \brief Adjust TTP Latency values to make sure they are between the recommended minimum and maximum values
    \param aptx_ad_ttp NQ2Q TTP Latency values for SSRC in milliseconds.
 */
void getAdjustedAptxAdaptiveTtpLatencies(aptx_adaptive_ttp_in_ms_t *aptx_ad_ttp);

/*! \brief Configure output mode for aptX adaptive
    \param mixer audio mixer operator
    \param stereo_lr_mix if TRUE, enable the left / right mix
    \param is_left TRUE if we are the left channel
    \return TRUE if mode has been set
 */
bool appKymeraSetAptxADMixerModes(Operator mixer, bool is_left, bool stereo_lr_mix);

/*! \brief Configure the L2CAP filter for aptX adaptive split tx mode
    \return TRUE if filters have been set
 */
bool appKymeraA2dpSetL2capFilter(void);

/*! \brief Disable the L2CAP filter for aptX adaptive split tx mode
    \return TRUE if the filters have been removed
 */
bool appKymeraA2dpDisableL2capFilter(void);


/*! \brief Is aptX adaptive R2.2 enabled
    \return TRUE if enabled
  */
#define appKymeraIsAptxR22Enabled() (KymeraGetTaskData()->aptx_adaptive_r22_dec == TRUE)

void appKymeraA2dpSetState(kymera_a2dp_state_t state);
kymera_a2dp_state_t appKymeraA2dpGetState(void);

void Kymera_A2dpHandlePrepareStage(const audio_a2dp_start_params_t *params);
void Kymera_A2dpHandleStartStage(uint8 seid, int16 volume_in_db);

/*! \brief Start local A2DP.

    \param codec_settings The A2DP codec settings to use.
    \param max_bitrate The max bitrate for the input stream (in bps). Ignored if zero.
    \param volume_in_db The initial volume to use.
    \param nq2q_ttp The aptX adaptive NQ2Q TTP Latency settings.

    \return TRUE if start is completed, else FALSE.

 */
bool Kymera_A2dpStart(const a2dp_codec_settings *codec_settings, uint32 max_bitrate, int16 volume_in_db,
                      aptx_adaptive_ttp_latencies_t nq2q_ttp);

/*! \brief Stop A2DP operation.

    Common function to all device types.
 */
void Kymera_A2dpCommonStop(Source source);

/*! \brief Start A2DP forwarding from the Primary to the Secondary.

    \param codec_settings The A2DP codec settings to use.

    In TWS legacy, this function starts forwarding media.
    In TWM, this function starts 'forwarding' audio synchronisation.
 */
void Kymera_A2dpStartForwarding(const a2dp_codec_settings *codec_settings);

/*! \brief Stop A2DP forwarding from the Primary to the Secondary.

    In TWS legacy, this function stops forwarding media.
    In TWM, this function stops 'forwarding' audio synchronisation.
 */
void Kymera_A2dpStopForwarding(Source source);

/*! \brief Configure RTP decoder startup period.
    \param op The operator id of the RTP decoder.
    \param startup_period At the start of stream the RTP decoder can be configured
           to wait for a period of time and evaluate the amount of data received
           and make a latency correction which corrects any error in TTP latency.
           The startup time should be less than the configured TTP latency.
*/
void Kymera_A2dpConfigureRtpDecoderStartupPeriod(Operator op, uint16 startup_period);

#endif /* KYMERA_A2DP_PRIVATE_H */
