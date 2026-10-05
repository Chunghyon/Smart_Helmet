/*!
\file       smart_helmet.c
\brief      Smart Helmet interface orchestration
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

#include "smart_helmet.h"
#include "smart_helmet_config.h"
#include "smart_helmet_sensors.h"
#include "smart_helmet_vitals.h"

#include <message.h>
#include <logging.h>
#include <string.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

static TaskData sh_task_data;
static Task sh_client_task;
static bool sh_ready;

#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
/*
 * Test telemetry: forward each record to the Wi-SUN UART as a plain ASCII
 * line, so a serial capture can be analysed offline. Dropped silently when
 * the UART is busy; telemetry must never stall the vitals pipeline.
 */
static void smartHelmetTelemetrySink(const char *line, uint16 len, void *ctx)
{
    static const uint8 eol[2] = { '\r', '\n' };

    UNUSED(ctx);
    if (!line || !len)
    {
        return;
    }
    (void)SmartHelmet_UartSend((const uint8 *)line, len);
    (void)SmartHelmet_UartSend(eol, sizeof(eol));
}
#endif

static void smartHelmetTaskHandler(Task task, MessageId id, Message message)
{
    if (id == SMART_HELMET_REPORT_TICK)
    {
        SmartHelmet_ReportStart();
        return;
    }
    if (SmartHelmet_SensorsHandleMessage(task, id, message))
    {
        return;
    }
    if (SmartHelmet_HandleMessage(task, id, message))
    {
        return;
    }
    /* Unhandled — ignore */
}

bool SmartHelmet_Init(Task client_task)
{
    sh_client_task = client_task;
    sh_task_data.handler = smartHelmetTaskHandler;

    /* ADC, UART and I2C retry run on the module task so headset SM does not
     * have to forward stream / timer messages. */

    if (!SmartHelmet_I2cInit())
    {
        DEBUG_LOG_ERROR("SmartHelmet: I2C init failed");
        return FALSE;
    }

    /* ADC gas + SENS_IN vitals timers on the module task (not headset SM). */
    SmartHelmet_AdcInit(&sh_task_data);

    if (!SmartHelmet_UartInit(&sh_task_data))
    {
        DEBUG_LOG_ERROR("SmartHelmet: UART init failed");
        /* Non-fatal for sensor-only bring-up */
    }
    else
    {
        SmartHelmet_UartStartVerify();
    }

    SmartHelmet_SensorsInit();
    SmartHelmet_SensorsStartVerify(&sh_task_data);
    SmartHelmet_VitalsInit();
#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
    SmartHelmet_VitalsSetSink(smartHelmetTelemetrySink, NULL);
#endif
    SmartHelmet_AdcRequestScan();

    sh_ready = TRUE;
    CC_LOGN("SmartHelmet: interfaces ready (vitals=SENS_IN@%uHz, no PIR PIO)",
            (unsigned)SMART_HELMET_VITALS_FS_HZ);
    return TRUE;
}

void SmartHelmet_Close(void)
{
    SmartHelmet_SensorsStopVerify();
    SmartHelmet_UartStopVerify();
    MessageCancelAll(&sh_task_data, SMART_HELMET_REPORT_TICK);
    SmartHelmet_AdcStop();
    SmartHelmet_UartClose();
    SmartHelmet_I2cClose();
    sh_ready = FALSE;
}

bool SmartHelmet_HandleMessage(Task task, MessageId id, Message message)
{
    if (SmartHelmet_SensorsHandleMessage(task, id, message))
    {
        return TRUE;
    }
    if (SmartHelmet_AdcHandleMessage(task, id, message))
    {
        return TRUE;
    }
    if (SmartHelmet_UartHandleMessage(task, id, message))
    {
        return TRUE;
    }
    return FALSE;
}

void SmartHelmet_PollSensors(void)
{
    if (!sh_ready)
    {
        return;
    }
    /* Gas scan is timer-driven; optional extra kick is fine. */
    SmartHelmet_AdcRequestScan();
    SmartHelmet_SensorsPoll();

    /* Optional LIS3DH motion gate when the part is populated/enabled. */
    {
        const smart_helmet_sensor_data_t *s = SmartHelmet_SensorsGetData();
        if (s && s->lis3dh_ok)
        {
            uint8 burst = SmartHelmet_SensorsAccelBurstCount();

            if (burst)
            {
                /* FIFO mode: forward the whole evenly-spaced burst. */
                const smart_helmet_accel_sample_t *a =
                    SmartHelmet_SensorsAccelBurst();
                uint8 i;

                for (i = 0; a && i < burst; i++)
                {
                    SmartHelmet_VitalsPushAccel(a[i].x_mg, a[i].y_mg, a[i].z_mg);
                }
            }
            else
            {
                SmartHelmet_VitalsPushAccel(s->lis3dh_x, s->lis3dh_y, s->lis3dh_z);
            }
        }
    }
    /* SENS_IN sampling + VitalsProcess run from ADC SENS timer path. */
}

void SmartHelmet_VitalsTick(void)
{
    SmartHelmet_VitalsProcess();
}

bool SmartHelmet_WisunSend(const uint8 *data, uint16 len)
{
    return SmartHelmet_UartSend(data, len);
}

static uint16 shAppend(char *dst, uint16 n, uint16 max, const char *s)
{
    while (*s && n + 1 < max)
    {
        dst[n++] = *s++;
    }
    dst[n] = '\0';
    return n;
}

static uint16 shAppendU(char *dst, uint16 n, uint16 max, uint16 v)
{
    char tmp[6];
    uint8 i = 0;

    if (v == 0)
    {
        return shAppend(dst, n, max, "0");
    }
    while (v && i < sizeof(tmp))
    {
        tmp[i++] = (char)('0' + (v % 10));
        v = (uint16)(v / 10);
    }
    while (i && n + 1 < max)
    {
        dst[n++] = tmp[--i];
    }
    dst[n] = '\0';
    return n;
}

static uint16 shAppendX100(char *dst, uint16 n, uint16 max, int16 v)
{
    uint16 mag;
    if (v < 0)
    {
        n = shAppend(dst, n, max, "-");
        mag = (uint16)(-v);
    }
    else
    {
        mag = (uint16)v;
    }
    n = shAppendU(dst, n, max, (uint16)(mag / 100));
    n = shAppend(dst, n, max, ".");
    n = shAppendU(dst, n, max, (uint16)((mag / 10) % 10));
    return shAppendU(dst, n, max, (uint16)(mag % 10));
}

void SmartHelmet_ReportStart(void)
{
    char line[192];
    uint16 n = 0;
    const smart_helmet_sensor_data_t *s = SmartHelmet_SensorsGetData();
    const smart_helmet_vitals_status_t *v = SmartHelmet_VitalsGetStatus();
    const smart_helmet_adc_sample_t *adc = SmartHelmet_AdcGetLastSample();

    UNUSED(s);

    n = shAppend(line, n, sizeof(line), "motion:");
#if !SMART_HELMET_ENABLE_LIS3DH
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    n = shAppend(line, n, sizeof(line),
                 (v && v->motion == smart_helmet_motion_active) ? "moving" : "still");
#endif
    n = shAppend(line, n, sizeof(line), "\r\nfall:");
#if !SMART_HELMET_ENABLE_LIS3DH
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    n = shAppend(line, n, sizeof(line),
                 (v && v->motion_rms_mg >= SMART_HELMET_FALL_RMS_MG) ? "yes" : "no");
#endif
    n = shAppend(line, n, sizeof(line), "\r\npulse:");
#if !SMART_HELMET_ENABLE_VITALS_PROXY
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    if (!v || !v->valid)
    {
        n = shAppend(line, n, sizeof(line), "unknown");
    }
    else if (v->trend == smart_helmet_trend_rising)
    {
        n = shAppend(line, n, sizeof(line), "rising");
    }
    else if (v->trend == smart_helmet_trend_falling)
    {
        n = shAppend(line, n, sizeof(line), "falling");
    }
    else
    {
        n = shAppend(line, n, sizeof(line), "stable");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\nbody:");
#if !SMART_HELMET_ENABLE_MLX90614
    n = shAppend(line, n, sizeof(line), "Disabled");
    UNUSED(shAppendX100);
#else
    n = shAppendX100(line, n, sizeof(line), s ? s->object_temp_x100 : 0);
#endif
    n = shAppend(line, n, sizeof(line), "\r\namb:");
#if !SMART_HELMET_ENABLE_HDC1080
    n = shAppend(line, n, sizeof(line), "Disabled");
    UNUSED(shAppendX100);
#else
    n = shAppendX100(line, n, sizeof(line), s ? s->hdc_temp_x100 : 0);
#endif
    n = shAppend(line, n, sizeof(line), "\r\nrh:");
#if !SMART_HELMET_ENABLE_HDC1080
    n = shAppend(line, n, sizeof(line), "Disabled");
    UNUSED(shAppendX100);
#else
    n = shAppendX100(line, n, sizeof(line), s ? (int16)s->hdc_humidity_x100 : 0);
#endif
    n = shAppend(line, n, sizeof(line), "\r\nvoc:");
#if !SMART_HELMET_ENABLE_CCS811
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    n = shAppendU(line, n, sizeof(line), s ? s->ccs811_tvoc : 0);
#endif
    n = shAppend(line, n, sizeof(line), "\r\nco:");
#if !SMART_HELMET_ENABLE_ADC
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    n = shAppendU(line, n, sizeof(line),
                  adc ? adc->millivolts[smart_helmet_adc_co] : 0);
#endif
    n = shAppend(line, n, sizeof(line), "\r\nnh3:");
#if !SMART_HELMET_ENABLE_ADC
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    n = shAppendU(line, n, sizeof(line),
                  adc ? adc->millivolts[smart_helmet_adc_nh3] : 0);
#endif
    n = shAppend(line, n, sizeof(line), "\r\nno2:");
#if !SMART_HELMET_ENABLE_ADC
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    n = shAppendU(line, n, sizeof(line),
                  adc ? adc->millivolts[smart_helmet_adc_no2] : 0);
#endif
    n = shAppend(line, n, sizeof(line), "\r\n");
    (void)SmartHelmet_UartSend((const uint8 *)line, n);
    MessageSendLater(&sh_task_data, SMART_HELMET_REPORT_TICK, NULL,
                     SMART_HELMET_WISUN_REPORT_MS);
}
