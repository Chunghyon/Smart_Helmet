/*!
\file       smart_helmet.c
\brief      Smart Helmet interface orchestration
*/

#include "smart_helmet.h"
#include "smart_helmet_sensors.h"
#include "smart_helmet_vitals.h"

#include <message.h>
#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

static TaskData sh_task_data;
static Task sh_client_task;
static bool sh_ready;

static void smartHelmetTaskHandler(Task task, MessageId id, Message message)
{
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

    /* Prefer dedicated module task for stream/ADC callbacks so headset SM
     * is not required to forward every message. Client may still call
     * SmartHelmet_HandleMessage from its own handler if preferred. */
    Task self = &sh_task_data;
    if (client_task)
    {
        self = client_task;
    }

    if (!SmartHelmet_I2cInit())
    {
        DEBUG_LOG_ERROR("SmartHelmet: I2C init failed");
        return FALSE;
    }

    SmartHelmet_AdcInit(self);

    if (!SmartHelmet_UartInit(self))
    {
        DEBUG_LOG_ERROR("SmartHelmet: UART init failed");
        /* Non-fatal for sensor-only bring-up */
    }

    SmartHelmet_SensorsInit();
    SmartHelmet_SensorsStartVerify(&sh_task_data);
    SmartHelmet_VitalsInit();

    sh_ready = TRUE;
    DEBUG_LOG_INFO("SmartHelmet: interfaces ready");
    return TRUE;
}

void SmartHelmet_Close(void)
{
    SmartHelmet_SensorsStopVerify();
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
    SmartHelmet_AdcRequestScan();
    SmartHelmet_SensorsPoll();

    /* Feed LIS3DH into vitals proxy (raw LSB ~ mg scale depends on FS;
     * treat as relative units until full-scale config is applied). */
    {
        const smart_helmet_sensor_data_t *s = SmartHelmet_SensorsGetData();
        if (s && s->lis3dh_ok)
        {
            SmartHelmet_VitalsPushAccel(s->lis3dh_x, s->lis3dh_y, s->lis3dh_z);
        }
        {
            const smart_helmet_adc_sample_t *adc = SmartHelmet_AdcGetLastSample();
            if (adc && adc->valid[smart_helmet_adc_sens_in])
            {
                SmartHelmet_VitalsPushSensInMv(adc->millivolts[smart_helmet_adc_sens_in]);
            }
        }
        SmartHelmet_VitalsProcess();
    }
}

void SmartHelmet_VitalsTick(void)
{
    SmartHelmet_VitalsProcess();
}

bool SmartHelmet_WisunSend(const uint8 *data, uint16 len)
{
    return SmartHelmet_UartSend(data, len);
}
