/*!
\file       smart_helmet_adc.c
\brief      Sequential ADC sampling for Smart Helmet

Full scan: SENS_IN + CO + NH3 + NO2 at SMART_HELMET_ADC_PERIOD_MS (air quality).
SENS-only: SENS_IN at SMART_HELMET_ADC_SENS_PERIOD_MS for vitals band-energy.
PIR_OUT PIO is not used; SENS_IN is the radar IF analog path.
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

#include "smart_helmet_config.h"
#include "smart_helmet_adc.h"
#include "smart_helmet_sensors.h"
#include "smart_helmet_vitals.h"

#include <adc.h>
#include <message.h>
#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
#include <vm.h>
#endif
#include <panic.h>
#include <string.h>
#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

enum
{
    SMART_HELMET_ADC_INTERNAL_TRIGGER = 0x5000, /*!< full gas scan */
    SMART_HELMET_ADC_SENS_TRIGGER     = 0x5001  /*!< SENS_IN only */
};

typedef enum
{
    sh_adc_mode_idle = 0,
    sh_adc_mode_full,
    sh_adc_mode_sens
} sh_adc_mode_t;

static Task sh_adc_task;
static uint8 sh_adc_index;
static sh_adc_mode_t sh_adc_mode;
static uint16 sh_adc_vref_mv;
static smart_helmet_adc_sample_t sh_adc_sample;
static bool sh_sens_pending;
#if SMART_HELMET_ENABLE_SENS_OVERSAMPLE
static uint8  sh_sens_os_n;     /*!< conversions accumulated this tick */
static uint32 sh_sens_os_acc;   /*!< running sum of scaled millivolts */
#endif

static const vm_adc_source_type sh_adc_sources[smart_helmet_adc_channel_count] =
{
    SMART_HELMET_ADC_SENS_IN,
    SMART_HELMET_ADC_CO,
    SMART_HELMET_ADC_NH3,
    SMART_HELMET_ADC_NO2
};

static void shAdcFeedVitalsSens(uint16 mv)
{
#if SMART_HELMET_ENABLE_VITALS_PROXY
#if SMART_HELMET_ENABLE_VITALS_TIMESTAMP
    /*
     * Timestamp at delivery so the DSP can measure the real interval instead
     * of assuming SMART_HELMET_VITALS_FS_HZ. VmGetTimerTime() is a free
     * running microsecond counter; wrap is handled by the unsigned difference
     * in the vitals layer.
     */
    SmartHelmet_VitalsPushSensInMvAt(mv, (uint32)VmGetTimerTime());
#else
    SmartHelmet_VitalsPushSensInMv(mv);
#endif
    SmartHelmet_VitalsOnSensSample();
#else
    UNUSED(mv);
#endif
}

#if SMART_HELMET_ADC_SENS_PRIORITY
/*! A SENS tick could not be served. Report it instead of sampling late. */
static void shAdcSensDropped(void)
{
#if SMART_HELMET_ENABLE_VITALS_PROXY
    SmartHelmet_VitalsNoteSensDropped();
#endif
}
#endif

static void shAdcRequestNextFull(void)
{
    if (sh_adc_index >= smart_helmet_adc_channel_count)
    {
        sh_adc_mode = sh_adc_mode_idle;
#if 0
        CC_LOGN("SmartHelmet ADC: SENS=%umV CO=%umV NH3=%umV NO2=%umV",
                       sh_adc_sample.millivolts[smart_helmet_adc_sens_in],
                       sh_adc_sample.millivolts[smart_helmet_adc_co],
                       sh_adc_sample.millivolts[smart_helmet_adc_nh3],
                       sh_adc_sample.millivolts[smart_helmet_adc_no2]);
#endif
        SmartHelmet_SensorsShowAdcMv(
            sh_adc_sample.millivolts[smart_helmet_adc_sens_in],
            sh_adc_sample.millivolts[smart_helmet_adc_co],
            sh_adc_sample.millivolts[smart_helmet_adc_nh3],
            sh_adc_sample.millivolts[smart_helmet_adc_no2]);
#if !SMART_HELMET_ADC_SENS_PRIORITY
        if (sh_sens_pending)
        {
            sh_sens_pending = FALSE;
            SmartHelmet_AdcRequestSensIn();
        }
#endif
        return;
    }

    AdcReadRequest(sh_adc_task, adcsel_vref_hq_buff, 0, 0);
    AdcReadRequest(sh_adc_task, sh_adc_sources[sh_adc_index], 0, 0);
}

static void shAdcStartSens(void)
{
    sh_adc_mode = sh_adc_mode_sens;
    sh_adc_index = smart_helmet_adc_sens_in;
#if SMART_HELMET_ENABLE_SENS_OVERSAMPLE
    sh_sens_os_n = 0;
    sh_sens_os_acc = 0;
#endif
    AdcReadRequest(sh_adc_task, adcsel_vref_hq_buff, 0, 0);
    AdcReadRequest(sh_adc_task, SMART_HELMET_ADC_SENS_IN, 0, 0);
}

void SmartHelmet_AdcInit(Task client_task)
{
    sh_adc_task = client_task;
    sh_adc_index = 0;
    sh_adc_mode = sh_adc_mode_idle;
    sh_adc_vref_mv = 0;
    sh_sens_pending = FALSE;
#if SMART_HELMET_ENABLE_SENS_OVERSAMPLE
    sh_sens_os_n = 0;
    sh_sens_os_acc = 0;
#endif
    memset(&sh_adc_sample, 0, sizeof(sh_adc_sample));
    CC_LOGN("SmartHelmet ADC: init gas=%ums sens=%ums (vitals path, no PIR PIO)",
            SMART_HELMET_ADC_PERIOD_MS, SMART_HELMET_ADC_SENS_PERIOD_MS);
    if (sh_adc_task)
    {
        MessageCancelAll(sh_adc_task, SMART_HELMET_ADC_INTERNAL_TRIGGER);
        MessageCancelAll(sh_adc_task, SMART_HELMET_ADC_SENS_TRIGGER);
        MessageSendLater(sh_adc_task,
                         SMART_HELMET_ADC_INTERNAL_TRIGGER,
                         NULL,
                         SMART_HELMET_ADC_PERIOD_MS);
#if SMART_HELMET_ENABLE_VITALS_PROXY
        MessageSendLater(sh_adc_task,
                         SMART_HELMET_ADC_SENS_TRIGGER,
                         NULL,
                         SMART_HELMET_ADC_SENS_PERIOD_MS);
#endif
    }
}

void SmartHelmet_AdcStop(void)
{
    if (sh_adc_task)
    {
        MessageCancelAll(sh_adc_task, SMART_HELMET_ADC_INTERNAL_TRIGGER);
        MessageCancelAll(sh_adc_task, SMART_HELMET_ADC_SENS_TRIGGER);
    }
    sh_adc_mode = sh_adc_mode_idle;
    sh_sens_pending = FALSE;
}

void SmartHelmet_AdcRequestScan(void)
{
    if (!sh_adc_task)
    {
        return;
    }
    if (sh_adc_mode != sh_adc_mode_idle)
    {
        DEBUG_LOG_VERBOSE("SmartHelmet ADC: scan busy mode=%u", (unsigned)sh_adc_mode);
        return;
    }
    sh_adc_mode = sh_adc_mode_full;
    sh_adc_index = 0;
    shAdcRequestNextFull();
}

void SmartHelmet_AdcRequestSensIn(void)
{
    if (!sh_adc_task)
    {
        return;
    }
    if (sh_adc_mode != sh_adc_mode_idle)
    {
#if SMART_HELMET_ADC_SENS_PRIORITY
        /*
         * Never take the sample late: a deferred tick fired straight after the
         * gas scan bunches two samples together and modulates the sampling
         * interval at the gas-scan rate (1 Hz => a 60 BPM artefact right in
         * the middle of the search band). Drop it and report the gap instead.
         */
        shAdcSensDropped();
#else
        sh_sens_pending = TRUE;
#endif
        return;
    }
    shAdcStartSens();
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
            sh_adc_vref_mv = result->reading;
            return TRUE;
        }

        if (sh_adc_mode == sh_adc_mode_sens)
        {
            if (result->adc_source != SMART_HELMET_ADC_SENS_IN)
            {
                return FALSE;
            }
            if (sh_adc_vref_mv)
            {
                scaled_mv = ((uint32)result->reading * 1200u) / sh_adc_vref_mv;
            }
            else
            {
                scaled_mv = result->reading;
            }
#if SMART_HELMET_ENABLE_SENS_OVERSAMPLE
            sh_sens_os_acc += scaled_mv;
            sh_sens_os_n++;
            if (sh_sens_os_n < SMART_HELMET_SENS_OVERSAMPLE_N)
            {
                /* Stay in SENS mode and take another conversion right away. */
                AdcReadRequest(sh_adc_task, adcsel_vref_hq_buff, 0, 0);
                AdcReadRequest(sh_adc_task, SMART_HELMET_ADC_SENS_IN, 0, 0);
                return TRUE;
            }
            scaled_mv = sh_sens_os_acc / sh_sens_os_n;
            sh_sens_os_acc = 0;
            sh_sens_os_n = 0;
#endif
            sh_adc_sample.millivolts[smart_helmet_adc_sens_in] = (uint16)scaled_mv;
            sh_adc_sample.valid[smart_helmet_adc_sens_in] = TRUE;
            sh_adc_mode = sh_adc_mode_idle;
            shAdcFeedVitalsSens((uint16)scaled_mv);
            return TRUE;
        }

        if (sh_adc_mode == sh_adc_mode_full &&
            sh_adc_index < smart_helmet_adc_channel_count &&
            result->adc_source == sh_adc_sources[sh_adc_index])
        {
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

            if (sh_adc_index == smart_helmet_adc_sens_in)
            {
#if SMART_HELMET_ADC_SENS_PRIORITY
                /*
                 * This sample belongs to the gas scan, not to the vitals
                 * cadence; feeding it would insert an off-grid sample.
                 */
#else
                shAdcFeedVitalsSens((uint16)scaled_mv);
#endif
            }

            sh_adc_index++;
            shAdcRequestNextFull();
            return TRUE;
        }
        return FALSE;
    }

    if (id == SMART_HELMET_ADC_INTERNAL_TRIGGER)
    {
        SmartHelmet_AdcRequestScan();
        if (sh_adc_task)
        {
            MessageSendLater(sh_adc_task,
                             SMART_HELMET_ADC_INTERNAL_TRIGGER,
                             NULL,
                             SMART_HELMET_ADC_PERIOD_MS);
        }
        return TRUE;
    }

    if (id == SMART_HELMET_ADC_SENS_TRIGGER)
    {
#if SMART_HELMET_ENABLE_VITALS_PROXY
        SmartHelmet_AdcRequestSensIn();
        if (sh_adc_task)
        {
            MessageSendLater(sh_adc_task,
                             SMART_HELMET_ADC_SENS_TRIGGER,
                             NULL,
                             SMART_HELMET_ADC_SENS_PERIOD_MS);
        }
#endif
        return TRUE;
    }

    return FALSE;
}

const smart_helmet_adc_sample_t *SmartHelmet_AdcGetLastSample(void)
{
    return &sh_adc_sample;
}
