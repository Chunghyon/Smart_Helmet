/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       headset_led.h
\brief      Header file for the Headset Application user interface LED indications.
*/
#ifndef HEADSET_LED_H
#define HEADSET_LED_H


#include "led_manager.h"

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
/*!@}*/
extern const led_manager_hw_config_t headset_led_config;



/*! \brief The colour filter for the led_state applicable when charging but
           the battery voltage is still low.
    \param led_state The input state.
    \return The filtered led_state.
*/
extern uint16 app_led_filter_charging_low(uint16 led_state);

/*! \brief The colour filter for the led_state applicable when charging and the
           battery voltage is ok.
    \param led_state The input state.
    \return The filtered led_state.
*/
extern uint16 app_led_filter_charging_ok(uint16 led_state);

/*! \brief The colour filter for the led_state applicable when charging is complete.
    \param led_state The input state.
    \return The filtered led_state.
*/
extern uint16 app_led_filter_charging_complete(uint16 led_state);

//!@{ \name LED pattern and ringtone note sequence arrays.
extern const led_pattern_t app_led_pattern_power_on[];
extern const led_pattern_t app_led_pattern_power_off[];
extern const led_pattern_t app_led_pattern_error[];
extern const led_pattern_t app_led_pattern_idle[];
extern const led_pattern_t app_led_pattern_idle_connected[];
extern const led_pattern_t app_led_pattern_pairing[];
extern const led_pattern_t app_led_pattern_pairing_deleted[];
extern const led_pattern_t app_led_pattern_sco[];
extern const led_pattern_t app_led_pattern_call_incoming[];

#ifdef INCLUDE_AV
extern const led_pattern_t app_led_pattern_streaming[];
extern const led_pattern_t app_led_pattern_streaming_aptx[];
extern const led_pattern_t app_led_pattern_streaming_aptx_adaptive[];
#endif

//!@}


#endif // HEADSET_LED_H
