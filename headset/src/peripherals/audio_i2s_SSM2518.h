/*!
\copyright  Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_i2s_SSM2518.h
\brief      The parameters / enums here define the message interface used to
            control the chip-specific I2C interface of the SSM2518 external I2S
            audio amplifier.
*/

#ifndef _CSR_I2S_SSM2518_PLUGIN_H_
#define _CSR_I2S_SSM2518_PLUGIN_H_

#include <gain_utils.h>
#include <library.h>
#include <stream.h>


/*! Register definitions for the I2C control interface of the SSM2518 device */
typedef enum
{
    RESET_POWER_CONTROL = 0x00,
    EDGE_CLOCK_CONTROL,
    SERIAL_INTERFACE_SAMPLE_RATE_CONTROL,
    SERIAL_INTERFACE_CONTROL,
    CHANNEL_MAPPING_CONTROL,
    LEFT_VOLUME_CONTROL,
    RIGHT_VOLUME_CONTROL,
    VOLUME_MUTE_CONTROL,
    FAULT_CONTROL_1,
    POWER_FAULT_CONTROL,
    DRC_CONTROL_1,
    DRC_CONTROL_2,
    DRC_CONTROL_3,
    DRC_CONTROL_4,
    DRC_CONTROL_5,
    DRC_CONTROL_6,
    DRC_CONTROL_7,
    DRC_CONTROL_8,
    DRC_CONTROL_9
} SSM2518_reg_map;

/*! @{ */
/*! I2C address bytes (also contain the read/write flag as bit 7) */
#define ADDR_WRITE 0x68
#define ADDR_READ  0x69
/*! @} */

/*! @{ */
/*! I2C Bitmasks for specific functions in various registers */
#define S_RST       (1<<7)
#define NO_BCLK     (1<<5)
#define SPWDN       (1<<0)
#define ASR_OFF     (1<<0)
#define BCLK_GEN    (1<<7)
#define AMUTE_OFF   (1<<7)
#define ANA_GAIN_5V (1<<5)
#define VOL_LINK    (1<<3)
#define R_MUTE      (1<<2)
#define L_MUTE      (1<<1)
#define M_MUTE      (1<<0)
#define AMP_LPM     (1<<4)
#define DAC_LPM     (1<<3)
#define APWDN_EN    (1<<0)
/*! @} */

/*! @{ */
/*! I2C multi-bit fields for parameter values in various registers */
#define MCS_16384_TO_24576_KHZ  (0x04<<1)
#define MCS_8192_TO_12288_KHZ   (0x02<<1)
#define MCS_4096_TO_6144_KHZ    (0x01<<1)
#define MCS_2048_TO_3072_KHZ    (0x00<<1)
#define FS_64_TO_96_KHZ (0x03<<0)
#define FS_32_TO_48_KHZ (0x02<<0)
#define FS_16_TO_24_KHZ (0x01<<0)
#define FS_8_TO_12_KHZ  (0x00<<0)
#define AR_TIME_80MS    (0x03<<6)
#define AR_TIME_40MS    (0x02<<6)
#define AR_TIME_20MS    (0x01<<6)
#define AR_TIME_10MS    (0x00<<6)
/*! @} */

/*! @{ */
/*! 8-bit values expected by the SSM2518 for various gain levels */
#define GAIN_MAX_24dB 0x00
#define GAIN_UNITY_0dB 0x40
#define GAIN_MIN_MUTE 0xff
/*! @} */

/*! @{ */
/*! Useful values for converting gain levels to the SSM2518's volume scale */
#define VOLUME_RANGE 256
#define VOLUME_GRADIENT_3dB 8
#define VOLUME_INTERCEPT_0dB GAIN_UNITY_0dB
#define VOLUME_MIN_MUTE_72DB GainIn60thdB(-72)
/*! @} */

#endif /* _CSR_I2S_SSM2518_PLUGIN_H_ */
