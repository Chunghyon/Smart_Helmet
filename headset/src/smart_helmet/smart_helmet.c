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
#include "smart_helmet_uart.h"

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
    /* SystemState_PowerOn runs the init table again. The module is still
     * up in passthrough; close so the next verify sends exit before reset. */
    if (sh_ready)
    {
        SmartHelmet_Close();
    }
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
    /* Sensors and Wi-SUN stay down until headset power-on.
     * OLED comes up so LIMBO can show battery percent. */
    SmartHelmet_SensorsInitDisplay();
    sh_ready = TRUE;
    CC_LOGN("SmartHelmet: buses ready, sensors and Wi-SUN held until power-on");
    return TRUE;
}

static bool sh_powered;

void SmartHelmet_PowerOn(void)
{
    if (!sh_ready || sh_powered)
    {
        return;
    }
    sh_powered = TRUE;
    SmartHelmet_SensorsDisplayResume();
    CC_LOGN("SmartHelmet: power on, start sensors and Wi-SUN");
    SmartHelmet_AdcInit(&sh_task_data);
    SmartHelmet_SensorsInit();
    SmartHelmet_SensorsStartVerify(&sh_task_data);
    SmartHelmet_VitalsInit();
#if (SMART_HELMET_ENABLE_VITALS_CSV || SMART_HELMET_ENABLE_SENS_DUMP)
    SmartHelmet_VitalsSetSink(smartHelmetTelemetrySink, NULL);
#endif
    SmartHelmet_UartStartVerify();
    SmartHelmet_AdcRequestScan();
}

void SmartHelmet_PowerOff(void)
{
    if (!sh_powered)
    {
        SmartHelmet_SensorsDisplayBlank();
        return;
    }
    sh_powered = FALSE;
    CC_LOGN("SmartHelmet: power off, sleep sensors and Wi-SUN");
    SmartHelmet_ReportStop();
    SmartHelmet_SensorsStopVerify();
    SmartHelmet_SensorsSleep();
    SmartHelmet_UartSleep();
    SmartHelmet_AdcStop();
    SmartHelmet_SensorsDisplayBlank();
}

void SmartHelmet_PowerSave(void)
{
    CC_LOGN("SmartHelmet: power save, blank OLED");
    SmartHelmet_PowerOff();
}

/* Keep in step with headsetState in headset_sm.h. Do not include that
 * header here: it makes kalcc treat Task/Message as a different type. */
#define SH_STATE_LIMBO        2u
#define SH_STATE_POWERING_ON  3u
#define SH_STATE_PAIRING      4u
#define SH_STATE_IDLE         5u
#define SH_STATE_BUSY         6u

static unsigned sh_ui_state;

void SmartHelmet_UiRefresh(unsigned state)
{
    sh_ui_state = state;
    if (state == SH_STATE_POWERING_ON ||
        state == SH_STATE_PAIRING ||
        state == SH_STATE_IDLE ||
        state == SH_STATE_BUSY)
    {
        SmartHelmet_PowerOn();
    }
    else if (sh_powered)
    {
        SmartHelmet_PowerOff();
    }
    SmartHelmet_SensorsShowAdcMv(0, 0, 0, 0);
}

unsigned SmartHelmet_UiState(void)
{
    return sh_ui_state;
}

void SmartHelmet_ReportStop(void)
{
    MessageCancelAll(&sh_task_data, SMART_HELMET_REPORT_TICK);
}

void SmartHelmet_Close(void)
{
    SmartHelmet_SensorsStopVerify();
    SmartHelmet_ReportStop();
    SmartHelmet_AdcStop();
    /* UartClose sends exit while the sink is still open. */
    SmartHelmet_UartClose();
    SmartHelmet_I2cClose();
    sh_ready = FALSE;
    sh_powered = FALSE;
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

/* MiCS-6814: RED/NH3 Rs falls with ppm, OX/NO2 Rs rises. First ADC is R0. */
static uint16 sh_gas_r0_mv[3];

static uint16 shGasRatioX100(uint16 mv, uint16 r0)
{
    uint32 rs;
    uint32 rs0;

    if (mv < 50 || mv >= SMART_HELMET_MICS_VCC_MV ||
        r0 < 50 || r0 >= SMART_HELMET_MICS_VCC_MV)
    {
        return 100;
    }
    rs = ((uint32)(SMART_HELMET_MICS_VCC_MV - mv) * 1000u) / mv;
    rs0 = ((uint32)(SMART_HELMET_MICS_VCC_MV - r0) * 1000u) / r0;
    if (rs0 == 0)
    {
        return 100;
    }
    return (uint16)((rs * 100u) / rs0);
}

static uint16 shCoPpm(uint16 ratio_x100)
{
    if (ratio_x100 >= 80) return 1;
    if (ratio_x100 >= 40) return 10;
    if (ratio_x100 >= 20) return 40;
    if (ratio_x100 >= 10) return 120;
    if (ratio_x100 >= 5) return 400;
    return 1000;
}

static uint16 shNh3Ppm(uint16 ratio_x100)
{
    if (ratio_x100 >= 80) return 1;
    if (ratio_x100 >= 40) return 15;
    if (ratio_x100 >= 20) return 60;
    if (ratio_x100 >= 10) return 180;
    return 500;
}

static uint16 shNo2PpmX100(uint16 ratio_x100)
{
    if (ratio_x100 <= 120) return 5;
    if (ratio_x100 <= 200) return 25;
    if (ratio_x100 <= 400) return 100;
    if (ratio_x100 <= 800) return 400;
    return 1000;
}

static uint16 shGasValue(uint8 ch, uint16 mv)
{
    if (sh_gas_r0_mv[ch] == 0 && mv >= 50 && mv < SMART_HELMET_MICS_VCC_MV)
    {
        sh_gas_r0_mv[ch] = mv;
    }
    return shGasRatioX100(mv, sh_gas_r0_mv[ch] ? sh_gas_r0_mv[ch] : mv);
}

static void shSendReportLines(const char *line, uint16 n)
{
    uint16 i = 0;

    while (i < n)
    {
        char frame[40];
        uint16 fn = 0;

        while (i < n && (line[i] == '\r' || line[i] == '\n'))
        {
            i++;
        }
        while (i < n && line[i] != '\r' && line[i] != '\n' &&
               fn + 3 < sizeof(frame))
        {
            frame[fn++] = line[i++];
        }
        while (i < n && line[i] != '\r' && line[i] != '\n')
        {
            i++;
        }
        if (!fn)
        {
            continue;
        }
        frame[fn++] = '\r';
        frame[fn++] = '\n';
        if (!SmartHelmet_UartSend((const uint8 *)frame, fn))
        {
            SmartHelmet_UartNoteReportSent(FALSE);
            return;
        }
    }
    SmartHelmet_UartNoteReportSent(TRUE);
}

void SmartHelmet_ReportStart(void)
{
    char line[192];
    uint16 n = 0;
#if SMART_HELMET_ENABLE_HDC1080
    const smart_helmet_sensor_data_t *s = SmartHelmet_SensorsGetData();
#endif
    const smart_helmet_vitals_status_t *v = SmartHelmet_VitalsGetStatus();
    const smart_helmet_adc_sample_t *adc = SmartHelmet_AdcGetLastSample();

    /* Command mode max is below this blob. PowerOn used to send it anyway
     * and the module answered "format err:cmd too long" every 5 s. */
    if (!SmartHelmet_UartInPassthrough())
    {
        SmartHelmet_ReportStop();
        CC_LOGN("SmartHelmet report held: not passthrough");
        return;
    }
    SmartHelmet_SensorsPoll();
    n = shAppend(line, n, sizeof(line), "motion:");
#if !SMART_HELMET_ENABLE_LIS3DH
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    n = shAppend(line, n, sizeof(line),
                 (v && v->motion == smart_helmet_motion_active) ? "moving" : "still");
#endif
    n = shAppend(line, n, sizeof(line), "\r\nrestart:");
    n = shAppend(line, n, sizeof(line), SmartHelmet_UartTakeRestart() ? "yes" : "no");
    n = shAppend(line, n, sizeof(line), "\r\nfall:");
#if !SMART_HELMET_ENABLE_LIS3DH
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    /* TakeFall holds a hit from any sample since the previous report, so a
     * short up/down shake is not lost when the 25-sample window rolls off. */
    n = shAppend(line, n, sizeof(line),
                 SmartHelmet_VitalsTakeFall() ? "yes" : "no");
#endif
    n = shAppend(line, n, sizeof(line), "\r\npulse:");
#if !SMART_HELMET_ENABLE_VITALS_PROXY
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    /* Ear-site skin micro-motion (pulse/resp band). LIS3DH motion stays in
     * motion:. unknown = moving or not ready. none = calm, no pulse and no resp. */
    if (!v)
    {
        n = shAppend(line, n, sizeof(line), "unknown");
    }
    else if (v->trend == smart_helmet_trend_absent)
    {
        n = shAppend(line, n, sizeof(line), "none");
    }
    else if (!v->valid)
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
    if (!s || !s->mlx90614_ok)
    {
        n = shAppend(line, n, sizeof(line), "na");
    }
    else
    {
        n = shAppendX100(line, n, sizeof(line), s->object_temp_x100);
        n = shAppend(line, n, sizeof(line), "C");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\namb:");
#if !SMART_HELMET_ENABLE_HDC1080
    n = shAppend(line, n, sizeof(line), "Disabled");
    UNUSED(shAppendX100);
#else
    if (!s || !s->hdc1080_ok)
    {
        n = shAppend(line, n, sizeof(line), "na");
    }
    else
    {
        n = shAppendX100(line, n, sizeof(line),
                         (int16)(s->hdc_temp_x100 + SMART_HELMET_AMB_OFFSET_X100));
        n = shAppend(line, n, sizeof(line), "C");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\nrh:");
#if !SMART_HELMET_ENABLE_HDC1080
    n = shAppend(line, n, sizeof(line), "Disabled");
    UNUSED(shAppendX100);
#else
    if (!s || !s->hdc1080_ok)
    {
        n = shAppend(line, n, sizeof(line), "na");
    }
    else
    {
        n = shAppendX100(line, n, sizeof(line), (int16)s->hdc_humidity_x100);
        n = shAppend(line, n, sizeof(line), "%");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\nvoc:");
#if !SMART_HELMET_ENABLE_CCS811
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    if (!s || !s->ccs811_ok)
    {
        n = shAppend(line, n, sizeof(line), "na");
    }
    else
    {
        n = shAppendU(line, n, sizeof(line), s->ccs811_tvoc);
        n = shAppend(line, n, sizeof(line), "ppb");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\nco:");
#if !SMART_HELMET_ENABLE_ADC
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    {
        uint16 mv = adc ? adc->millivolts[smart_helmet_adc_co] : 0;
        uint16 ratio = shGasValue(0, mv);
        n = shAppendU(line, n, sizeof(line), shCoPpm(ratio));
        n = shAppend(line, n, sizeof(line), "ppm");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\nnh3:");
#if !SMART_HELMET_ENABLE_ADC
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    {
        uint16 mv = adc ? adc->millivolts[smart_helmet_adc_nh3] : 0;
        uint16 ratio = shGasValue(1, mv);
        n = shAppendU(line, n, sizeof(line), shNh3Ppm(ratio));
        n = shAppend(line, n, sizeof(line), "ppm");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\nno2:");
#if !SMART_HELMET_ENABLE_ADC
    n = shAppend(line, n, sizeof(line), "Disabled");
#else
    {
        uint16 mv = adc ? adc->millivolts[smart_helmet_adc_no2] : 0;
        uint16 ratio = shGasValue(2, mv);
        n = shAppendX100(line, n, sizeof(line), (int16)shNo2PpmX100(ratio));
        n = shAppend(line, n, sizeof(line), "ppm");
    }
#endif
    n = shAppend(line, n, sizeof(line), "\r\n");
    shSendReportLines(line, n);
    MessageCancelAll(&sh_task_data, SMART_HELMET_REPORT_TICK);
    MessageSendLater(&sh_task_data, SMART_HELMET_REPORT_TICK, NULL,
                     SMART_HELMET_WISUN_REPORT_MS);
}
