/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Microphone configuration
*/

#ifndef MICROPHONES_CONFIG_H_
#define MICROPHONES_CONFIG_H_

#if defined(HAVE_RDP_HW_18689)
//!@{ @name Parameters for microphone index 0 - Left analog MIC */
#define appConfigMic0Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic0BiasVoltage()              (3) /* 1.9v */
#define appConfigMic0Pio()                      (0x3)
#define appConfigMic0Type()                     (mic_type_analog)
#define appConfigMic0AudioInstance()            (AUDIO_INSTANCE_0)
#define appConfigMic0AudioChannel()             (AUDIO_CHANNEL_A)
//!@}
#else
  #ifdef INCLUDE_LIS25BA_ACCELEROMETER
  //!@{ @name Parameters for microphone index 0 - PCM accelerometer */
  #define appConfigMic0Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
  #define appConfigMic0BiasVoltage()              (3) /* 1.9v */
  #define appConfigMic0Pio()                      (0x16)
  #define appConfigMic0Type()                     (mic_type_pcm)
  #define appConfigMic0AudioInstance()            (AUDIO_INSTANCE_1)
  #define appConfigMic0AudioChannel()             (AUDIO_CHANNEL_SLOT_2)
  //!@}
  #else
  //!@{ @name Parameters for microphone index 0 - Left analog MIC */
  #define appConfigMic0Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
  #define appConfigMic0BiasVoltage()              (3) /* 1.9v */
  #define appConfigMic0Pio()                      (0x13)
  #define appConfigMic0Type()                     (mic_type_analog)
  #define appConfigMic0AudioInstance()            (AUDIO_INSTANCE_0)
  #define appConfigMic0AudioChannel()             (AUDIO_CHANNEL_A)
  //!@}
  #endif /* INCLUDE_LIS25BA_ACCELEROMETER */
#endif /* defined(HAVE_RDP_HW_18689) */

//!@{ @name Parameters for microphone index 1 - Right analog MIC */
#define appConfigMic1Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic1BiasVoltage()              (3) /* 1.9v */
#define appConfigMic1Pio()                      (0x16)
#define appConfigMic1Type()                     (mic_type_analog)
#define appConfigMic1AudioInstance()            (AUDIO_INSTANCE_0)
#define appConfigMic1AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#if defined(CORVUS_PG806)

//!@{ @name Parameters for microphone index 2 - HBL_L_FB Analog MIC connected to digital MIC ADC */
#define appConfigMic2Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic2BiasVoltage()              (3)
#define appConfigMic2Pio()                      (0)
#define appConfigMic2Type()                     (mic_type_digital)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

//!@{ @name Parameters microphone index 3 - HBL_L_FF Analog MIC connected to digital MIC ADC */
#define appConfigMic3Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic3BiasVoltage()              (3)
#define appConfigMic3Pio()                      (0)
#define appConfigMic3Type()                     (mic_type_digital)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

#elif defined(CORVUS_YD300)

//!@{ @name Parameters for microphone index 2 */
#define appConfigMic2Bias()                       (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic2BiasVoltage()              (7)
#define appConfigMic2Pio()                      (4)
#define appConfigMic2Type()                     (mic_type_digital)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Parameters microphone index 3 */
#define appConfigMic3Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic3BiasVoltage()              (7)
#define appConfigMic3Pio()                      (4)
#define appConfigMic3Type()                     (mic_type_digital)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#elif defined HAVE_RDP_HW_18689
//!@{ @name Parameters microphone index 2 */
#define appConfigMic2Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic2BiasVoltage()              (3)
#define appConfigMic2Pio()                      (6)
#define appConfigMic2Type()                     (mic_type_digital)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Parameters microphone index 3 */
#define appConfigMic3Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic3BiasVoltage()              (3)
#define appConfigMic3Pio()                      (3)
#define appConfigMic3Gain()                     (15)
#define appConfigMic3Type()                     (mic_type_digital)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_B)
//!@}
#elif defined HAVE_RDP_HW_MOTION
//!@{ @name Parameters microphone index 2 */
#define appConfigMic2Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic2BiasVoltage()              (3)
#define appConfigMic2Pio()                      (39)
#define appConfigMic2Type()                     (mic_type_digital)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Parameters microphone index 3 */
#define appConfigMic3Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic3BiasVoltage()              (3)
#define appConfigMic3Pio()                      (38)
#define appConfigMic3Type()                     (mic_type_digital)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#elif defined HAVE_RDP_HW_YE134
//!@{ @name Parameters microphone index 2 */
#define appConfigMic2Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic2BiasVoltage()              (3)
#define appConfigMic2Pio()                      (19)
#define appConfigMic2Type()                     (mic_type_digital)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Parameters microphone index 3 */
#define appConfigMic3Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic3BiasVoltage()              (3)
#define appConfigMic3Pio()                      (19)
#define appConfigMic3Type()                     (mic_type_digital)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#elif defined USE_CH430_LED_MAP /* CH430 based base board */
//!@{ @name Default parameters microphone index 2 */
#define appConfigMic2Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic2BiasVoltage()              (3) /* 1.9v */
#define appConfigMic2Pio()                      (4) /* NA  */
#define appConfigMic2Type()                     (mic_type_analog)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Default parameters microphone index 3 */
#define appConfigMic3Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic3BiasVoltage()              (3) /* 1.9v */
#define appConfigMic3Pio()                      (4) /* NA */
#define appConfigMic3Type()                     (mic_type_analog)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#else /* Default hardware setup */

//!@{ @name Default parameters microphone index 2 */
#define appConfigMic2Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic2BiasVoltage()              (0)
#define appConfigMic2Pio()                      (4)
#define appConfigMic2Type()                     (mic_type_digital)
#define appConfigMic2AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic2AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Default parameters microphone index 3 */
#define appConfigMic3Bias()                     (BIAS_CONFIG_PIO)
#define appConfigMic3BiasVoltage()              (0)
#define appConfigMic3Pio()                      (4)
#define appConfigMic3Type()                     (mic_type_digital)
#define appConfigMic3AudioInstance()            (AUDIO_INSTANCE_1)
#define appConfigMic3AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#endif /* HW platform */

//!@{ @name Parameters for microphone index 4 - HBL_L_FB Analog MIC connected to digital MIC ADC */
#define appConfigMic4Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic4BiasVoltage()              (3)
#define appConfigMic4Pio()                      (0)
#define appConfigMic4Type()                     (mic_type_digital)
#define appConfigMic4AudioInstance()            (AUDIO_INSTANCE_2)
#define appConfigMic4AudioChannel()             (AUDIO_CHANNEL_A)
//!@}

//!@{ @name Parameters microphone index 5 - HBL_L_FF Analog MIC connected to digital MIC ADC */
#define appConfigMic5Bias()                     (BIAS_CONFIG_MIC_BIAS_0)
#define appConfigMic5BiasVoltage()              (3)
#define appConfigMic5Pio()                      (0)
#define appConfigMic5Type()                     (mic_type_digital)
#define appConfigMic5AudioInstance()            (AUDIO_INSTANCE_2)
#define appConfigMic5AudioChannel()             (AUDIO_CHANNEL_B)
//!@}

#endif /* MICROPHONES_CONFIG_H_ */
