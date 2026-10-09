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
    int16    lis3dh_x;          /* mg */
    int16    lis3dh_y;          /* mg */
    int16    lis3dh_z;          /* mg */
    int16    object_temp_x100; /* 0.01 °C */
    int16    ambient_temp_x100;
} smart_helmet_sensor_data_t;

/*! One accelerometer sample in milli-g. */
typedef struct
{
    int16 x_mg;
    int16 y_mg;
    int16 z_mg;
} smart_helmet_accel_sample_t;

void SmartHelmet_SensorsInit(void);
void SmartHelmet_SensorsInitDisplay(void);
void SmartHelmet_SensorsPoll(void);
const smart_helmet_sensor_data_t *SmartHelmet_SensorsGetData(void);

/*!
 * \brief Number of accelerometer samples drained in the last poll.
 *
 * Always 0 unless SMART_HELMET_LIS3DH_USE_FIFO is enabled, in which case the
 * caller should forward the whole burst so the motion reference keeps the
 * sensor's own even time base.
 */
uint8 SmartHelmet_SensorsAccelBurstCount(void);

/*! \brief Accelerometer burst drained in the last poll (NULL when unused). */
const smart_helmet_accel_sample_t *SmartHelmet_SensorsAccelBurst(void);

/*! \brief First probe + start 1 s retry on \a retry_task until expected IDs. */
void SmartHelmet_SensorsStartVerify(Task retry_task);

/*! \brief Cancel pending probe-retry timer. */
void SmartHelmet_SensorsStopVerify(void);

/*! \brief Handle SMART_HELMET_I2C_PROBE_RETRY. Returns TRUE if consumed. */
bool SmartHelmet_SensorsHandleMessage(Task task, MessageId id, Message message);

/*! rief Draw CO/NH3/NO2/SENS_IN millivolts on the SSD1315. No-op if OLED off. */
/*! Draw battery %, border-router link, and call/music. ADC args unused. */
void SmartHelmet_SensorsShowAdcMv(uint16 sens_mv, uint16 co_mv,
                                 uint16 nh3_mv, uint16 no2_mv);

#endif /* SMART_HELMET_SENSORS_H */

void SmartHelmet_SensorsSleep(void);
void SmartHelmet_SensorsDisplayOff(void);
void SmartHelmet_SensorsDisplayBlank(void);
void SmartHelmet_SensorsDisplayResume(void);
void SmartHelmet_SensorsDisplayOn(void);

