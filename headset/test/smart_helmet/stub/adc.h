/*!
\file       adc.h
\brief      Host-test stub for the QCC ADK ADC interface.

The enum must include adcsel_led0..adcsel_led4 because smart_helmet_config.h
maps the gas channels and SENS_IN onto them.
*/
#ifndef SH_TEST_ADC_H
#define SH_TEST_ADC_H

#include <csrtypes.h>
#include <message.h>

typedef enum
{
    adcsel_vref_hq_buff = 0,
    adcsel_led0,
    adcsel_led1,
    adcsel_led2,
    adcsel_led3,
    adcsel_led4,
    adcsel_pmu_vbat_sns
} vm_adc_source_type;

#define VM_UART_RATE_115K2   (0x01d8u)
#define VM_UART_STOP_ONE     (0u)
#define VM_UART_PARITY_NONE  (0u)

static inline int AdcReadRequest(Task t, vm_adc_source_type src,
                                 uint16 flags, uint16 delay)
{
    UNUSED(t); UNUSED(src); UNUSED(flags); UNUSED(delay);
    return 1;
}

#endif /* SH_TEST_ADC_H */
