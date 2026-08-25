/*!
\copyright  Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_audio.h
\brief  File consists of function decalration for Amazon Voice Service's audio specific interface
*/

#ifndef AMA_AUDIO_H
#define AMA_AUDIO_H

#include "ama_config.h"
#include "ama_msg_types.h"
#include <voice_ui_va_client_if.h>
#include <va_audio_types.h>

typedef struct _ama_supported_locales
{
    uint8 num_locales;
    const char *name[MAX_AMA_LOCALES];
}ama_supported_locales_t;

typedef enum {
    ama_audio_trigger_tap,
    ama_audio_trigger_press,
    ama_audio_trigger_wake_word
}ama_audio_trigger_t;

/*! \brief Triggers a button activated capture
*/
void AmaAudio_StartButtonActivatedCapture(SpeechInitiator__Type type);

/*! \brief Stops the voice capture chain
*/
void AmaAudio_StopCapture(void);

/*! \brief Start wake word detection
*/
void AmaAudio_StartWakeWordDetection(void);

/*! \brief Stop wake word detection
*/
void AmaAudio_StopWakeWordDetection(void);

/*! \brief Handle voice data
 *  \param src The audio source to extract data from
 *  \return The timeout before the next handle voice data in ms
*/
unsigned AmaAudio_HandleVoiceData(Source src);

/*! \brief Notify that wake word has been detected
 *  \param capture_params Pointer to the capture parameters
 *  \param wuw_info Pointer to the wake word detection info
 *  \return TRUE if wake word capture started, otherwise FALSE
*/
bool AmaAudio_WakeWordDetected(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info);

/*! \brief Start a Tap-to-Talk session
 */
void AmaAudio_StartTapToTalkCapture(void);

/*! \brief Starts the voice capture chain
*/
bool AmaAudio_Provide(const AMA_SPEECH_PROVIDE_IND_T* ind);

/*! \brief Cancels the AVS speech session
*/
void AmaAudio_Cancel(void);

/*! \brief Validates the locale
    \param locale string
    \return bool TRUE if the locale is valid
*/
bool AmaAudio_ValidateLocale(const char *locale);

/*! \brief Initialises AMA audio data
*/
void AmaAudio_Init(void);

/*! \brief Gets the current locale
*/
const char* AmaAudio_GetCurrentLocale(void);

/*! \brief Gets the supported locales
 *  \param supported_locales Pointer to the list of supported locales
*/
void AmaAudio_GetSupportedLocales(ama_supported_locales_t * supported_locales);

/*! \brief Populate the locale data
 *  \param locales Pointer to the locale structure to be populated
 *  \param locale_arr Pointer to the locale array to be populated
 *  \param local_ptr_arr Pointer to the locale pointer array to be populated
*/
void AmaAudio_PopulateLocales(Locales * locales, Locale * locale_arr, Locale ** locale_ptr_arr);

/*! \brief Sets the current locale
*/
void AmaAudio_SetLocale(const char* locale);


/*! \brief Gets the voice assistant locale setting from the Device database.
    \param locale Buffer to hold the locale name.
    \param locale_size Size of locale buffer.
    \return TRUE on success, FALSE otherwise.
 */
bool AmaAudio_GetDeviceLocale(char *locale, uint8 locale_size);

/*! \brief Gets the stored model for a given locale
*/
const char *AmaAudio_GetModelFromLocale(const char* locale);

/*! \brief Register the locale specific prompt handler with the UI prompts
*/
void AmaAudio_RegisterLocalePrompts(void);

/*! \brief Deregister the locale specific prompt handler from the UI prompts
*/
void AmaAudio_DeregisterLocalePrompts(void);

/*! \brief Configures AMA for the selected codec.
    \param ama_codec_t selected codec
 */
void Ama_ConfigureCodec(ama_codec_t codec);

/*! \brief Sets the readiness state of the gateway
 *  \param ready TRUE if Alexa is ready with network connectivity, FALSE otherwise
*/
void AmaAudio_SetAlexaReady(bool ready);

/*! \brief Notify the AMA audio module that the session has successfully started
*/
void AmaAudio_NotifySessionStarted(void);

#endif /* AMA_AUDIO_H*/

