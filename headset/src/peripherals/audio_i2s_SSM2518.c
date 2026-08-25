/*!
\copyright  Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       audio_i2s_SSM2518.c
\brief      Support for I2S audio output, implementing the 'Device' API defined
            in audio_i2s_common.h, with chip specific I2C commands to configure
            the SSM2518 external I2S amplifier.
*/

#ifdef INCLUDE_SSM2518_I2S_AMP

#include "audio_i2s_SSM2518.h"

#include <gain_utils.h>
#include <stdlib.h>
#include <panic.h>
#include <file.h>
#include <print.h>
#include <stream.h> 
#include <kalimba.h>
#include <kalimba_standard_messages.h>
#include <message.h>
#include <transform.h>
#include <string.h>
#include <i2c.h>
#include <pio_common.h>
#include <stdio.h>

#include "audio_i2s_common.h"


/*! Amp power state */
typedef enum
{
    standby = 0, /*!< Can be used during external amplifier in low power down mode */
    active,      /*!< Used when external amplifier is in running/active state */
    power_off    /*!< Used to denote external amplifier is powered off */
} amp_status_t;

/*! Collection of data representing current I2S hardware/software state */
typedef struct
{
    amp_status_t amp_status;
} state_t;


/*! Amplifier state */
static state_t state = { .amp_status = power_off };

/*! Default I2S configuration data for this chip */
const i2s_config_t device_i2s_config =
{
    .master_mode = 1,       /*!< Amp is only capable of operating as I2S Slave */
    .data_format = 0,       /*!< I2S left justified */
    .bit_delay = 1,         /*!< I2S delay 1 bit */
#ifdef ENABLE_I2S_OUTPUT_24BIT
    .bits_per_sample = 24,  /*!< 24-bit data words per channel */
#else
    .bits_per_sample = 16,  /*!< 16-bit data words per channel */
#endif
    .bclk_scaling = 256,    /*!< Allows full range of sample rates down to 8kHz */
    .mclk_scaling = 0,
    .enable_pio = PIN_INVALID
};


static void enableAmpPio(bool enable);
static void setAmpState(amp_status_t state);


/*! \brief Power on and configure the I2S device. */
bool AudioI2SDeviceInitialise(uint32 sample_rate)
{
    setAmpState(active);

    uint8 i2c_data[2];
   
    /* Reset the amp to a known state. */
    i2c_data[0] = RESET_POWER_CONTROL;
    i2c_data[1] = (S_RST|SPWDN);
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Set to normal operation with external MCLK (remove SPWDN), configure for
       NO_BLCK. Both the MCLK & BCLK inputs to the amp will actually be driven
       by the BCLK output of the QCC I2S interface, at 256 * fs. This multiplier
       was chosen because it allows operation down to 8kHz - see Table 11 "MCLK
       Frequency Settings" of the Analogue Devices SSM2518 datasheet for details
       (page 24). */
    i2c_data[0] = RESET_POWER_CONTROL;
    i2c_data[1] = (NO_BCLK);

    /* BCLK output is always 256 * fs, i.e. scales with sample rate. Therefore
       MCS needs to be set appropriately to tell the amp what frequency to
       expect on its MCLK/BCLK input. */
    if(sample_rate <= 12000)
        i2c_data[1] |= MCS_2048_TO_3072_KHZ;
    else if(sample_rate <= 24000)
        i2c_data[1] |= MCS_4096_TO_6144_KHZ;
    else if(sample_rate <= 48000)
        i2c_data[1] |= MCS_8192_TO_12288_KHZ;
    else
        i2c_data[1] |= MCS_16384_TO_24576_KHZ;

    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Disable automatic sample rate detection. */
    i2c_data[0] = EDGE_CLOCK_CONTROL;
    i2c_data[1] = (ASR_OFF);
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Set use I2S with default settings. */
    i2c_data[0] = SERIAL_INTERFACE_SAMPLE_RATE_CONTROL;
    i2c_data[1] = 0x00;
    /* Determine sample rate setting. */
    if(sample_rate <= 12000)
        i2c_data[1] |= FS_8_TO_12_KHZ;
    else if(sample_rate <= 24000)
        i2c_data[1] |= FS_16_TO_24_KHZ;
    else if(sample_rate <= 48000)
        i2c_data[1] |= FS_32_TO_48_KHZ;
    else
        i2c_data[1] |= FS_64_TO_96_KHZ;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));
    
    /* Set to no BCLK generation, 50% duty cycle, rising edges, MSB first. */
    i2c_data[0] = SERIAL_INTERFACE_CONTROL;
    i2c_data[1] = 0x00;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Set master/channel mute controls to on, enable auto mute, 3.6V PVDD. */
    i2c_data[0] = VOLUME_MUTE_CONTROL;
    i2c_data[1] = (M_MUTE | L_MUTE | R_MUTE);
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Set DAC/AMP power modes for normal high performance operation, default
       AR_TIME, auto power down enabled. */
    i2c_data[0] = POWER_FAULT_CONTROL;
    i2c_data[1] = (AR_TIME_40MS|APWDN_EN);
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    return TRUE;
}

/*! \brief Shut down and power off the I2S device. */
bool AudioI2SDeviceShutdown(void)
{
    uint8 i2c_data[2];

    /* Enable master mute */
    i2c_data[0] = VOLUME_MUTE_CONTROL;
    i2c_data[1] = (M_MUTE);
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Power down the amp */
    i2c_data[0] = RESET_POWER_CONTROL;
    i2c_data[1] = (SPWDN);
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Disable external amplifier */
    setAmpState(power_off);

    return TRUE;
}

/*! \brief Set the hardware gain of a single I2S channel on a specific I2S
    device via the I2C interface. The gain is passed in as a value in 1/60th's
    of a dB (with range -7200 to 0, representing -120dB to 0dB). This particular
    chip also supports positive gain values from 0 to +1440, i.e. up to +24dB.

    \param channel The I2S device and channel to set the volume of.
    \param gain The gain level required for that channel, in 1/60dB.

    \return TRUE if gain was successfully applied for the requested channel,
            FALSE otherwise.

    \note The minimum gain that the SSM2518 supports is -71.625dB, so anything
    less than or equal to -72dB (i.e. -4320 db/60) will be considered muted.
*/    
bool AudioI2SDeviceSetChannelGain(i2s_out_t channel, int16 gain)
{
    uint8 vol_mute_control_reg;
    uint8 i2c_data[2];
    int16 left;
    int16 sign = 1;

    PRINT(("I2S: SSM2518 SetGain [%d]dB/60 on channel [%x], ", gain, channel));

    /* Read current mute status from vol mute control register */
    i2c_data[0] = VOLUME_MUTE_CONTROL;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 1, NULL, 0));
    PanicFalse(I2cTransfer(ADDR_READ, NULL, 0, &vol_mute_control_reg, 1));

    switch (channel) {
        case i2s_out_1_left:
        case i2s_out_2_left:
            /* set the left volume level */
            i2c_data[0] = LEFT_VOLUME_CONTROL;
            break;

        case i2s_out_1_right:
        case i2s_out_2_right:
            /* set the right volume level */
            i2c_data[0] = RIGHT_VOLUME_CONTROL;
            break;

        default:
            return FALSE;
    }

    /* The SSM2518 has an inverted volume scale, with a range of 0x00 to 0xff
       representing +24dB to -71.625dB, respectively. Unity gain (0dB) is
       represented by 0x40, an offset of 64. The scale "gradient" is then +8
       steps per -3dB.

       Gain is converted to this range from the standard 1/60dB scale, which has
       a range of -7200 to 0, representing -120dB to 0dB respectively. There are
       60 steps per dB, or equivalently -180 steps per -3dB, for comparison. */

    /* First, invert the gain up into positive numbers */
    gain = (int16)(0 - gain);

    /* Make a note of the sign, to correctly deal with rounding later if dealing
       with negative numbers (gains above 0dB) */
    if (gain < 0)
        sign = -1;

    /* Input gain now in range 0-7200 (0dB to -120dB), i.e. +180 steps per -3dB.
       To scale to the SSM2518 range, this needs to be divided by 180/8 = 22.5.
       To achieve this, we'll multiply by a scale factor of 2/45. */
    #define GAIN_SCALE_NUMERATOR   2
    #define GAIN_SCALE_DENOMINATOR ((GAIN_SCALE_NUMERATOR * GainIn60thdB(3)) / VOLUME_GRADIENT_3dB)

    gain = (int16)(gain * GAIN_SCALE_NUMERATOR);
    left = (int16)(gain % GAIN_SCALE_DENOMINATOR);  /* store remainder before division */
    gain = (int16)(gain / GAIN_SCALE_DENOMINATOR);  /* Rounds down by default */

    if ((sign * left * 2) >= GAIN_SCALE_DENOMINATOR) /* round up if remainder >= divisor/2 */
        gain = (int16)(gain + sign);

    /* Input gain is now in the range 0-320 for 0db to -120dB.
       However, 0dB in the SSM2518 actually starts at 0x40, so we need to add a
       fixed offset of 64, to account for the non-zero y-intercept. */
    gain = (int16)(gain + VOLUME_INTERCEPT_0dB);

    /* Gain is now in the range 64-384 for 0dB to -120dB, we just need to cap the
       the minimum gain so that it fits inside the SSM2518's inverted 8-bit range,
       i.e. 64-256 for 0db to -72dB. */
    if (gain > (int16)GAIN_MIN_MUTE)
        gain = (int16)GAIN_MIN_MUTE; /* 0xff (-71.625dB) */

    /* Also limit the maximum gain to the bottom of the inverted range, too. */
    if (gain < (int16)GAIN_MAX_24dB)
        gain = (int16)GAIN_MAX_24dB; /* 0x00 (+24dB) */

    /* Finally, we now have a 0-255 gain range representing +24dB to -71.625dB,
       as per the SSM2518 spec, and can safely convert this to 8-bit. */
    i2c_data[1] = (uint8)(gain & 0xff);
    PRINT(("scaled [%x]\n", i2c_data[1]));

    /* Send over I2C */
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    /* Make sure vol mute control register is set appropriately */
    PRINT(("I2S: SSM2518 current VOLUME_MUTE_CONTROL register contents: 0x%02x", vol_mute_control_reg));
    if (gain == GAIN_MIN_MUTE)
    {
        /* Set mute for corresponding channel */
        if (i2c_data[0] == LEFT_VOLUME_CONTROL)
            vol_mute_control_reg |= L_MUTE;
        else if (i2c_data[0] == RIGHT_VOLUME_CONTROL)
            vol_mute_control_reg |= R_MUTE;
    }
    else
    {
        /* Make sure master/channel mutes are unset as required */
        vol_mute_control_reg &= ~M_MUTE;
        if (i2c_data[0] == LEFT_VOLUME_CONTROL)
            vol_mute_control_reg &= ~L_MUTE;
        else if (i2c_data[0] == RIGHT_VOLUME_CONTROL)
            vol_mute_control_reg &= ~R_MUTE;

        PRINT(("I2S: SSM2518 master mute off\n"));
    }

    /* Update mute status register */
    i2c_data[0] = VOLUME_MUTE_CONTROL;
    i2c_data[1] = vol_mute_control_reg;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));

    return TRUE;
}

/*! \brief Convenience function to set the hardware gain of all I2S channels to
    the same level, or set the master gain, if available on a specific chip.
    Useful for muting, for example.

    \param gain The gain level to set in 1/60ths of a dB.

    \return TRUE if gain successfully applied to all channels,
            FALSE otherwise.
*/
bool AudioI2SDeviceSetGain(int16 gain)
{
    uint8 vol_mute_control_reg;
    uint8 i2c_data[2];

    bool l_ok = AudioI2SDeviceSetChannelGain(i2s_out_1_left, gain);
    bool r_ok = AudioI2SDeviceSetChannelGain(i2s_out_1_right, gain);

    /* Read current mute status from vol mute control register */
    i2c_data[0] = VOLUME_MUTE_CONTROL;
    PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 1, NULL, 0));
    PanicFalse(I2cTransfer(ADDR_READ, NULL, 0, &vol_mute_control_reg, 1));

    /* If volumes are muted then set master mute */
    if (gain <= VOLUME_MIN_MUTE_72DB)
    {
        i2c_data[1] = vol_mute_control_reg | M_MUTE;
        PRINT(("I2S: SSM2518 master mute on\n"));

        /* Send command over I2C */
        PanicFalse(I2cTransfer(ADDR_WRITE, i2c_data, 2, NULL, 0));
    }
    /* No need to clear master mute as already done by SetChannelGain() */

    return (l_ok && r_ok);
}

/*! \brief Get the overall delay of the I2S hardware, for TTP adjustment.
    \return Delay in milliseconds.
*/
uint16 AudioI2SDeviceGetDelay(void)
{
    return 0;
}


/* Enable/disable the external amplifier */
static void enableAmpPio(bool enable)
{
    uint8 enable_pio = AudioI2SCommonGetConfig()->enable_pio;
    /*check whther the pio is valid*/
    if(enable_pio != PIN_INVALID)
    {
        PanicFalse(PioCommonSetPio(enable_pio,pio_drive,enable));
    }
}

/* Set the state of the external amplifier */
static void setAmpState(amp_status_t requested_state)
{
    if(state.amp_status != requested_state)
    {
        switch (requested_state)
        {
            case active:
                enableAmpPio(TRUE);
                state.amp_status = active;
            break;

            case standby:
            break;

            case power_off:
                enableAmpPio(FALSE);
                state.amp_status = power_off;
            break;

            default:
            break;
        }
    }
}

#endif /* INCLUDE_SSM2518_I2S_AMP */
