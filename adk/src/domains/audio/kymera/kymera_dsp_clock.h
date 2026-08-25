/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera private header related to configuring the DSP clock/power settings
*/

#ifndef KYMERA_DSP_CLOCK_H_
#define KYMERA_DSP_CLOCK_H_

#include <audio_clock.h>

#define DEFAULT_LOW_POWER_CLK_SPEED_MHZ (32)
#define BOOSTED_LOW_POWER_CLK_SPEED_MHZ (45)

/*! \brief Boost the DSP clock to the maximum speed available.
    If the DSP is not running this call will fail.

    \return TRUE on success.
    \note Changing the clock with chains already started may cause audible
    glitches if using I2S output.
*/
bool appKymeraBoostDspClockToMax(void);

/*! \brief Configure power mode and clock frequencies of the DSP for the lowest
           power consumption possible based on the current state / codec.

   \note Calling this function with chains already started may cause audible
   glitches if using I2S output.
*/
void appKymeraConfigureDspPowerMode(void);

/*! \brief Update the DSP clock speed settings for the clock speed enums for the lowest
           power consumption possible based on the current state / codec.
*/
void appKymeraConfigureDspClockSpeed(void);

#endif /* KYMERA_DSP_CLOCK_H_ */
