/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       led_manager_config.h
\brief      Configuration related definitions for LEDs.
*/

#ifndef LED_MANAGER_CONFIG_H
#define LED_MANAGER_CONFIG_H

/*!@{ @name LED pin PIO assignments (chip specific)
      @brief The LED pads can either be controlled by the led_controller hardware
             or driven as PIOs. The following define the PIO numbers used to
             control the LED pads as PIOs.
*/
#define CHIP_LED_0_PIO CHIP_LED_BASE_PIO
#if CHIP_NUM_LEDS > 1
#define CHIP_LED_1_PIO CHIP_LED_BASE_PIO + 1
#endif
#if CHIP_NUM_LEDS > 2
#define CHIP_LED_2_PIO CHIP_LED_BASE_PIO + 2
#endif
#if CHIP_NUM_LEDS > 3
#define CHIP_LED_3_PIO CHIP_LED_BASE_PIO + 3
#endif
#if CHIP_NUM_LEDS > 4
#define CHIP_LED_4_PIO CHIP_LED_BASE_PIO + 4
#endif
#if CHIP_NUM_LEDS > 5
#define CHIP_LED_5_PIO CHIP_LED_BASE_PIO + 5
#endif
//!@}

#if defined(HAVE_1_LED)

/*! The number of LEDs led_manager will control. If one LED is configured,
    it will use appConfigLed0Pio(), if two LEDs are configured it will use
    appConfigLed0Pio() and appConfigLed1Pio() etc. */
#define appConfigNumberOfLeds()  (1)

/*! PIO to control LED0 */
#define appConfigLed0Pio()       CHIP_LED_0_PIO
/*! PIO to control LED1 (not used) */
#define appConfigLed1Pio()       (0)
/*! PIO to control LED2 (not used) */
#define appConfigLed2Pio()       (0)

#elif defined(HAVE_3_LEDS)

/* The number of LEDs led_manager will control. */
#define appConfigNumberOfLeds()  (3)
/*! PIO to control LED0 */
#define appConfigLed0Pio()       CHIP_LED_0_PIO
/*! PIO to control LED1 */
#define appConfigLed1Pio()       CHIP_LED_1_PIO
/*! PIO to control LED2 */
#define appConfigLed2Pio()       CHIP_LED_2_PIO

#else
#error LED config not correctly defined.
#endif

/*! Returns boolean TRUE if PIO is an LED pin, otherwise FALSE */
#define appConfigPioIsLed(pio) (((pio) >= CHIP_LED_0_PIO) && ((pio <= CHIP_LED_5_PIO)))

/*! Returns the LED number for a LED PIO. Assumes led is an LED PIO. */
#define appConfigPioLedNumber(pio) ((pio) - CHIP_LED_0_PIO)

/*! Product specific range of PIO that can wake the chip from dormant */
#define appConfigPioCanWakeFromDormant(pio) ((pio) >= 1 && ((pio) <= 8))

#endif /* LED_MANAGER_CONFIG_H */
