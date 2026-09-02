/*!
\file       smart_helmet_sensors.h
\brief      Sensor device helpers on Smart Helmet I2C buses
*/

#ifndef SMART_HELMET_SENSORS_H
#define SMART_HELMET_SENSORS_H

#include <csrtypes.h>
#include <stdbool.h>
#include <message.h>

enum
{
    /*! Periodic I2C identity retry (scope-friendly 1 s cadence). */
    SMART_HELMET_I2C_PROBE_RETRY = 0x5100
};

typedef struct
{
    bool     ccs811_ok;
    bool     hdc1080_ok;
    bool     lis3dh_ok;
    bool     mlx90614_ok;
    bool     ssd1315_ok;
    uint16   ccs811_eco2;
    uint16   ccs811_tvoc;
    int16    hdc_temp_x100;     /* 0.01 °C */
    uint16   hdc_humidity_x100; /* 0.01 %RH */
    int16    lis3dh_x;
    int16    lis3dh_y;
    int16    lis3dh_z;
    int16    object_temp_x100; /* 0.01 °C */
    int16    ambient_temp_x100;
} smart_helmet_sensor_data_t;

void SmartHelmet_SensorsInit(void);
void SmartHelmet_SensorsPoll(void);
const smart_helmet_sensor_data_t *SmartHelmet_SensorsGetData(void);

/*! \brief First probe + start 1 s retry on \a retry_task until expected IDs. */
void SmartHelmet_SensorsStartVerify(Task retry_task);

/*! \brief Cancel pending probe-retry timer. */
void SmartHelmet_SensorsStopVerify(void);

/*! \brief Handle SMART_HELMET_I2C_PROBE_RETRY. Returns TRUE if consumed. */
bool SmartHelmet_SensorsHandleMessage(Task task, MessageId id, Message message);

#endif /* SMART_HELMET_SENSORS_H */
