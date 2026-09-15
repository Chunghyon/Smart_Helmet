/*!
\file       smart_helmet_adc.h
\brief      ADC interface for SENS_IN, CO, NH3, NO2

Two cadences (same pads, no HW change):
  - Full scan (gas + SENS_IN) at SMART_HELMET_ADC_PERIOD_MS
  - SENS_IN-only at SMART_HELMET_ADC_SENS_PERIOD_MS for vitals proxy
*/

#ifndef SMART_HELMET_ADC_H
#define SMART_HELMET_ADC_H

#include <csrtypes.h>
#include <message.h>
#include <stdbool.h>

typedef enum
{
    smart_helmet_adc_sens_in = 0,
    smart_helmet_adc_co,
    smart_helmet_adc_nh3,
    smart_helmet_adc_no2,
    smart_helmet_adc_channel_count
} smart_helmet_adc_channel_t;

typedef struct
{
    uint16 millivolts[smart_helmet_adc_channel_count];
    bool   valid[smart_helmet_adc_channel_count];
} smart_helmet_adc_sample_t;

/*! \brief Initialise ADC client task state and start gas + SENS timers. */
void SmartHelmet_AdcInit(Task client_task);

/*! \brief Cancel periodic scan timers. */
void SmartHelmet_AdcStop(void);

/*! \brief Request a full scan of all gas / sensor ADC channels. */
void SmartHelmet_AdcRequestScan(void);

/*! \brief Request a single SENS_IN conversion (vitals path). */
void SmartHelmet_AdcRequestSensIn(void);

/*!
 * \brief Handle MESSAGE_ADC_RESULT (and internal timers).
 * \return TRUE if the message was consumed.
 */
bool SmartHelmet_AdcHandleMessage(Task task, MessageId id, Message message);

/*! \brief Latest completed sample set (full scan and/or last SENS_IN). */
const smart_helmet_adc_sample_t *SmartHelmet_AdcGetLastSample(void);

#endif /* SMART_HELMET_ADC_H */
