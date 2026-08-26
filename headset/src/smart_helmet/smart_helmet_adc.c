/*!
\file       smart_helmet_adc.c
\brief      Sequential ADC sampling for Smart Helmet air-quality inputs
*/

#include "smart_helmet_config.h"
#include "smart_helmet_adc.h"

#include <adc.h>
#include <message.h>
#include <panic.h>
#include <string.h>
#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

enum
{
    SMART_HELMET_ADC_INTERNAL_TRIGGER = 0x5000
};

static Task sh_adc_task;
static uint8 sh_adc_index;
static bool sh_adc_busy;
static uint16 sh_adc_vref_mv;
static smart_helmet_adc_sample_t sh_adc_sample;

static const vm_adc_source_type sh_adc_sources[smart_helmet_adc_channel_count] =
{
    SMART_HELMET_ADC_SENS_IN,
    SMART_HELMET_ADC_CO,
    SMART_HELMET_ADC_NH3,
    SMART_HELMET_ADC_NO2
};

static void shAdcRequestNext(void)
{
    if (sh_adc_index >= smart_helmet_adc_channel_count)
    {
        sh_adc_busy = FALSE;
        DEBUG_LOG_INFO("SmartHelmet ADC: scan complete SENS=%umV CO=%umV NH3=%umV NO2=%umV",
                       sh_adc_sample.millivolts[smart_helmet_adc_sens_in],
                       sh_adc_sample.millivolts[smart_helmet_adc_co],
                       sh_adc_sample.millivolts[smart_helmet_adc_nh3],
                       sh_adc_sample.millivolts[smart_helmet_adc_no2]);
        return;
    }

    /* Vref then channel — same pattern as thermistor peripheral */
    AdcReadRequest(sh_adc_task, adcsel_vref_hq_buff, 0, 0);
    AdcReadRequest(sh_adc_task, sh_adc_sources[sh_adc_index], 0, 0);
}

void SmartHelmet_AdcInit(Task client_task)
{
    sh_adc_task = client_task;
    sh_adc_index = 0;
    sh_adc_busy = FALSE;
    sh_adc_vref_mv = 0;
    memset(&sh_adc_sample, 0, sizeof(sh_adc_sample));
    DEBUG_LOG_INFO("SmartHelmet ADC: init (SENS_IN/CO/NH3/NO2)");
}

void SmartHelmet_AdcRequestScan(void)
{
    if (!sh_adc_task)
    {
        return;
    }
    if (sh_adc_busy)
    {
        DEBUG_LOG_VERBOSE("SmartHelmet ADC: scan already busy");
        return;
    }
    sh_adc_busy = TRUE;
    sh_adc_index = 0;
    shAdcRequestNext();
}

bool SmartHelmet_AdcHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    if (id == MESSAGE_ADC_RESULT)
    {
        const MessageAdcResult *result = (const MessageAdcResult *)message;
        uint32 scaled_mv;

        if (result->adc_source == adcsel_vref_hq_buff)
        {
            /* HQ buffer Vref is nominally 1.2 V — store raw reading for scale */
            sh_adc_vref_mv = result->reading;
            return TRUE;
        }

        if (sh_adc_index < smart_helmet_adc_channel_count &&
            result->adc_source == sh_adc_sources[sh_adc_index])
        {
            /* Convert using same ratio style as ADK thermistor:
             * reading_mv ~= reading * Vref_known / vref_reading
             * When vref reading unavailable, pass through raw counts. */
            if (sh_adc_vref_mv)
            {
                scaled_mv = ((uint32)result->reading * 1200u) / sh_adc_vref_mv;
            }
            else
            {
                scaled_mv = result->reading;
            }

            sh_adc_sample.millivolts[sh_adc_index] = (uint16)scaled_mv;
            sh_adc_sample.valid[sh_adc_index] = TRUE;
            sh_adc_index++;
            shAdcRequestNext();
            return TRUE;
        }
        return FALSE;
    }

    if (id == SMART_HELMET_ADC_INTERNAL_TRIGGER)
    {
        SmartHelmet_AdcRequestScan();
        return TRUE;
    }

    return FALSE;
}

const smart_helmet_adc_sample_t *SmartHelmet_AdcGetLastSample(void)
{
    return &sh_adc_sample;
}
