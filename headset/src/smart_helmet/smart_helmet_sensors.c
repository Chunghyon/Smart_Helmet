/*!
\file       smart_helmet_sensors.c
\brief      Lightweight bring-up / poll for I2C sensors on the helmet hub

CJMCU-8118  = CCS811 gas sensor
LIS3DH      = 3-axis accelerometer
GY-906-BAA  = MLX90614 IR temperature
SSD1315     = optional OLED (stub only unless SMART_HELMET_ENABLE_SSD1315)
*/

#include "smart_helmet_config.h"
#include "smart_helmet_i2c.h"
#include "smart_helmet_sensors.h"

#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

static smart_helmet_sensor_data_t sh_sensors;

/* CCS811 registers */
#define CCS811_REG_STATUS        0x00
#define CCS811_REG_MEAS_MODE     0x01
#define CCS811_REG_ALG_RESULT    0x02
#define CCS811_REG_APP_START     0xF4
#define CCS811_REG_HW_ID         0x20
#define CCS811_HW_ID_VALUE       0x81

/* LIS3DH registers */
#define LIS3DH_REG_WHO_AM_I      0x0F
#define LIS3DH_WHO_AM_I_VALUE    0x33
#define LIS3DH_REG_CTRL_REG1     0x20
#define LIS3DH_REG_OUT_X_L       0x28

/* MLX90614 RAM */
#define MLX90614_CMD_RAM         0x00
#define MLX90614_RAM_TA          0x06
#define MLX90614_RAM_TOBJ1       0x07

const smart_helmet_sensor_data_t *SmartHelmet_SensorsGetData(void)
{
    return &sh_sensors;
}

static bool shProbeCcs811(void)
{
    uint8 id = 0;
    if (!SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_0,
                                SMART_HELMET_ADDR_CCS811,
                                CCS811_REG_HW_ID, &id, 1))
    {
        return FALSE;
    }
    return id == CCS811_HW_ID_VALUE;
}

static bool shInitCcs811(void)
{
    uint8 mode = 0x10; /* constant power, 1 Hz */
    /* App start (no payload) */
    if (!SmartHelmet_I2cWriteReg(smart_helmet_i2c_bus_0,
                                 SMART_HELMET_ADDR_CCS811,
                                 CCS811_REG_APP_START, NULL, 0))
    {
        /* Some modules already in app mode */
    }
    return SmartHelmet_I2cWriteReg(smart_helmet_i2c_bus_0,
                                   SMART_HELMET_ADDR_CCS811,
                                   CCS811_REG_MEAS_MODE, &mode, 1);
}

#if SMART_HELMET_ENABLE_LIS3DH
static bool shProbeLis3dh(void)
{
    uint8 id = 0;
    if (!SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_0,
                                SMART_HELMET_ADDR_LIS3DH,
                                LIS3DH_REG_WHO_AM_I, &id, 1))
    {
        return FALSE;
    }
    return id == LIS3DH_WHO_AM_I_VALUE;
}

static bool shInitLis3dh(void)
{
    uint8 ctrl1 = 0x57; /* 100 Hz, XYZ enable */
    return SmartHelmet_I2cWriteReg(smart_helmet_i2c_bus_0,
                                   SMART_HELMET_ADDR_LIS3DH,
                                   LIS3DH_REG_CTRL_REG1, &ctrl1, 1);
}
#endif

static int16 shMlxRawToCentiC(uint16 raw)
{
    /* Kelvin * 100 / 50  -> 0.01°C  (MLX resolution 0.02 K) */
    int32 centi_k = ((int32)raw * 2);
    return (int16)(centi_k - 27315);
}

static bool shReadMlx90614(uint8 ram_addr, int16 *out_x100)
{
    uint8 cmd = (uint8)(MLX90614_CMD_RAM | ram_addr);
    uint8 buf[3] = {0};
    uint16 raw;

    /* SMBus read: write command, read lo, hi, pec */
    if (!SmartHelmet_I2cTransfer(smart_helmet_i2c_bus_0,
                                 SMART_HELMET_ADDR_MLX90614,
                                 &cmd, 1, buf, 3))
    {
        return FALSE;
    }
    raw = (uint16)buf[0] | ((uint16)buf[1] << 8);
    *out_x100 = shMlxRawToCentiC(raw);
    return TRUE;
}

void SmartHelmet_SensorsInit(void)
{
    DEBUG_LOG_INFO("%s: enter", __func__);
    sh_sensors.ccs811_ok = FALSE;
    sh_sensors.lis3dh_ok = FALSE;
    sh_sensors.mlx90614_ok = FALSE;

    DEBUG_LOG_INFO("%s: probe CCS811 @0x%02x", __func__, SMART_HELMET_ADDR_CCS811);
    if (shProbeCcs811())
    {
        DEBUG_LOG_INFO("%s: CCS811 present, init", __func__);
        sh_sensors.ccs811_ok = shInitCcs811();
        DEBUG_LOG_INFO("SmartHelmet: CCS811 (CJMCU-8118) %s",
                       sh_sensors.ccs811_ok ? "ok" : "init fail");
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: CCS811 not found @0x%02x",
                       SMART_HELMET_ADDR_CCS811);
    }

#if SMART_HELMET_ENABLE_LIS3DH
    DEBUG_LOG_INFO("%s: probe LIS3DH @0x%02x", __func__, SMART_HELMET_ADDR_LIS3DH);
    if (shProbeLis3dh())
    {
        DEBUG_LOG_INFO("%s: LIS3DH present, init", __func__);
        sh_sensors.lis3dh_ok = shInitLis3dh();
        DEBUG_LOG_INFO("SmartHelmet: LIS3DH %s",
                       sh_sensors.lis3dh_ok ? "ok" : "init fail");
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: LIS3DH not found @0x%02x",
                       SMART_HELMET_ADDR_LIS3DH);
    }
#else
    sh_sensors.lis3dh_ok = FALSE;
    DEBUG_LOG_INFO("%s: LIS3DH skipped (SMART_HELMET_ENABLE_LIS3DH=0)", __func__);
#endif

    DEBUG_LOG_INFO("%s: probe MLX90614 @0x%02x", __func__, SMART_HELMET_ADDR_MLX90614);
    /* MLX90614 has no WHO_AM_I — try a temperature read */
    if (shReadMlx90614(MLX90614_RAM_TA, &sh_sensors.ambient_temp_x100))
    {
        sh_sensors.mlx90614_ok = TRUE;
        DEBUG_LOG_INFO("SmartHelmet: MLX90614 (GY-906) ok TA=%d/100C",
                       sh_sensors.ambient_temp_x100);
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: MLX90614 not responding @0x%02x",
                       SMART_HELMET_ADDR_MLX90614);
    }

#if SMART_HELMET_ENABLE_SSD1315
    {
        smart_helmet_i2c_bus_t bus =
            SMART_HELMET_SSD1315_ON_I2C1 ? smart_helmet_i2c_bus_1
                                        : smart_helmet_i2c_bus_0;
        uint8 dummy = 0x00;
        DEBUG_LOG_INFO("%s: probe SSD1315 @0x%02x on I2C%u",
                       __func__, SMART_HELMET_ADDR_SSD1315, (unsigned)bus);
        if (SmartHelmet_I2cWrite(bus, SMART_HELMET_ADDR_SSD1315, &dummy, 1))
        {
            DEBUG_LOG_INFO("SmartHelmet: SSD1315 probe ok on I2C%u", (unsigned)bus);
        }
        else
        {
            DEBUG_LOG_WARN("SmartHelmet: SSD1315 not found");
        }
    }
#endif
    DEBUG_LOG_INFO("%s: exit ccs=%u lis=%u mlx=%u",
                   __func__,
                   sh_sensors.ccs811_ok,
                   sh_sensors.lis3dh_ok,
                   sh_sensors.mlx90614_ok);
}

void SmartHelmet_SensorsPoll(void)
{
    uint8 buf[6];

    if (sh_sensors.ccs811_ok)
    {
        if (SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_0,
                                   SMART_HELMET_ADDR_CCS811,
                                   CCS811_REG_ALG_RESULT, buf, 4))
        {
            sh_sensors.ccs811_eco2 = ((uint16)buf[0] << 8) | buf[1];
            sh_sensors.ccs811_tvoc = ((uint16)buf[2] << 8) | buf[3];
        }
    }

#if SMART_HELMET_ENABLE_LIS3DH
    if (sh_sensors.lis3dh_ok)
    {
        /* auto-increment: set MSB of sub-address */
        if (SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_0,
                                   SMART_HELMET_ADDR_LIS3DH,
                                   (uint8)(LIS3DH_REG_OUT_X_L | 0x80),
                                   buf, 6))
        {
            sh_sensors.lis3dh_x = (int16)(((uint16)buf[1] << 8) | buf[0]);
            sh_sensors.lis3dh_y = (int16)(((uint16)buf[3] << 8) | buf[2]);
            sh_sensors.lis3dh_z = (int16)(((uint16)buf[5] << 8) | buf[4]);
        }
    }
#endif

    if (sh_sensors.mlx90614_ok)
    {
        shReadMlx90614(MLX90614_RAM_TA, &sh_sensors.ambient_temp_x100);
        shReadMlx90614(MLX90614_RAM_TOBJ1, &sh_sensors.object_temp_x100);
    }
}
