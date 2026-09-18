/*!
\file       smart_helmet.c
\brief      Smart Helmet interface orchestration
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

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

    /* ADC gas + SENS_IN vitals timers on the module task (not headset SM). */
    SmartHelmet_AdcInit(&sh_task_data);

    if (!SmartHelmet_UartInit(self))
    {
        DEBUG_LOG_ERROR("SmartHelmet: UART init failed");
        /* Non-fatal for sensor-only bring-up */
    }

    SmartHelmet_SensorsInit();
    SmartHelmet_SensorsStartVerify(&sh_task_data);
    SmartHelmet_VitalsInit();
    SmartHelmet_AdcRequestScan();

    sh_ready = TRUE;
    CC_LOGN("SmartHelmet: interfaces ready (vitals=SENS_IN@%uHz, no PIR PIO)",
            (unsigned)SMART_HELMET_VITALS_FS_HZ);
    return TRUE;
}

void SmartHelmet_Close(void)
{
    SmartHelmet_SensorsStopVerify();
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
