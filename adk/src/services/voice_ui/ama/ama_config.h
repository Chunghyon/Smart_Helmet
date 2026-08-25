/*!
\copyright  Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_config.h
\brief  Configuration for audio functionality for Amazon Voice Service
*/

#ifndef AMA_CONFIG_H
#define AMA_CONFIG_H

#include "ama_data.h"
#include "accessories.pb-c.h"

#define AMA_CONFIG_DEVICE_TYPE          "A32E8VQVU960EJ"

#ifndef AMA_DEFAULT_CODEC
#if defined(__QCC304X__) || defined(__QCC514X__)
#define AMA_DEFAULT_CODEC ama_codec_msbc
#else
#define AMA_DEFAULT_CODEC ama_codec_opus	
#endif
#endif

#define AMA_DEFAULT_CODEC_OVER_RFCOMM   AMA_DEFAULT_CODEC      /* Compile time choice between ama_codec_msbc or ama_codec_opus */
#define AMA_DEFAULT_CODEC_OVER_IAP2     AMA_DEFAULT_CODEC      /* Compile time choice between ama_codec_msbc or ama_codec_opus */
#define AMA_DEFAULT_OPUS_FORMAT         AUDIO_FORMAT__OPUS_16KHZ_32KBPS_CBR_0_20MS    /* Compile time choice between AMA_OPUS_16KBPS or AMA_OPUS_32KBPS */


#define AMA_MAX_NUMBER_OF_MICS          (1)                 /* Max number of microphones to use, based on HW availability less may be used */
#define AMA_MIN_NUMBER_OF_MICS          (1)                 /* Min number of microphoness to use, app will panic if not enough are available */

#define MAX_AMA_LOCALES (12)

#ifndef AMA_DEFAULT_LOCALE
#define AMA_DEFAULT_LOCALE              "en-GB"             /* Compile time choice for default locale (must exist in RO file system) */
#endif

#ifndef AMA_AVAILABLE_LOCALES
/* List of locales available in the RO file system. Names are as defined by AVS documentation. Change as necessary */
#define AMA_AVAILABLE_LOCALES           "de-DE", "en-AU", "en-CA", "en-GB", "en-IN", "en-US", "es-ES", "es-MX", "fr-CA", "fr-FR", "it-IT", "ja-JP"
#endif

#ifndef AMA_LOCALE_TO_MODEL_OVERRIDES
/* Extend this list for new locales that use a model defined for another locale e.g. locale:English Canadian uses locale:English US */
#define AMA_LOCALE_TO_MODEL_OVERRIDES   {"en-CA", "en-US"},/* {<locale name>, <model name>}, {<locale name>, <model name>} */
#endif

#ifdef HAVE_RDP_UI
#define ama_GetActionMapping() (1)  /* "Custom RDP event translation" */
#else
#define ama_GetActionMapping() (0)  /* "Dedicated assistant physical button (one button)" */
#endif /* (HAVE_RDP_UI) */

/* Specific 'pmalloc' pool configurations. */
#define PMALLOC_AMA_EXTRAS { 512, 4 },

#define NUMBER_OF_SUPPORTED_TRANSPORTS (2)

#define ASSISTANT_OVERRIDEN FALSE

/* Bit masks for the RESPONSE__PAYLOAD_DEVICE_FEATURES features field */
#define AMA_DEVICE_FEATURE_BATTERY_LEVEL                (1 << 7)
#define AMA_DEVICE_FEATURE_ANC                          (1 << 8)
#define AMA_DEVICE_FEATURE_PASSTHROUGH                  (1 << 9)
#define AMA_DEVICE_FEATURE_WAKE_WORD                    (1 << 10)
#define AMA_DEVICE_FEATURE_PRIVACY_MODE                 (1 << 11)
#define AMA_DEVICE_FEATURE_EQUALIZER                    (1 << 12)

/*! \brief Get the associated devices
 *  \param The requested device id
 *  \return The device ID
*/
uint32_t* Ama_GetAssociatedDevices(uint32 device_id);

/*! \brief Get the number of associated devices
 *  \return The device ID
*/
size_t Ama_GetNumberAssociatedDevices(void);

/*! \brief Get the device ID for requested device
 *  \param The requested device id
 *  \return The device ID
*/
uint32 Ama_GetDeviceId(uint32 device_id);

/*! \brief Get the device type for requested device
 *  \param The requested device id
 *  \return The device type
*/
char * Ama_GetDeviceType(uint32 device_id);

/*! \brief Get the serial number for requested device
 *  \param The requested device id
 *  \return The serial number
*/
char * Ama_GetSerialNumber(uint32 device_id);

/*! \brief Get the number of transports supported
 *  \return The number of transports supported
*/
uint8 Ama_GetNumTransportSupported(void);

/*! \brief Get the local BT address
 *  \return Pointer to the local BT address
*/
bdaddr* Ama_GetLocalAddress(void);

/*! \brief Populate the device information structure
 *  \param Pointer to device information structure to populate
 *  \param The requested device id
*/
void Ama_PopulateDeviceInformation(DeviceInformation * device_information, uint32 device_id);

/*! \brief Populate the device configuration structure
 *  \param Pointer to device configuration structure to populate
*/
void Ama_PopulateDeviceConfiguration(DeviceConfiguration * device_config);

/*! \brief Populates a DeviceFeatures structure with common features
 *  \param device_features Pointer to the DeviceFeatures structure to be populated
 */
void Ama_PopulateCommonDeviceFeatures(DeviceFeatures * device_features);
/*! \brief Populates a DeviceFeatures structure
 *  \param device_features Pointer to the DeviceFeatures structure to be populated
 */
void Ama_PopulateDeviceFeatures(DeviceFeatures * device_features);

#endif // AMA_CONFIG_H
