/*!
\file       smart_helmet_sensors.h
\brief      Sensor device helpers on Smart Helmet I2C buses
*/

#ifndef SMART_HELMET_SENSORS_H
#define SMART_HELMET_SENSORS_H

#include <csrtypes.h>
#include <stdbool.h>

typedef struct
{
    bool     ccs811_ok;
    bool     lis3dh_ok;
    bool     mlx90614_ok;
    uint16   ccs811_eco2;
    uint16   ccs811_tvoc;
    int16    lis3dh_x;
    int16    lis3dh_y;
    int16    lis3dh_z;
    int16    object_temp_x100; /* 0.01 °C */
    int16    ambient_temp_x100;
} smart_helmet_sensor_data_t;

void SmartHelmet_SensorsInit(void);
void SmartHelmet_SensorsPoll(void);
const smart_helmet_sensor_data_t *SmartHelmet_SensorsGetData(void);

#endif /* SMART_HELMET_SENSORS_H */
