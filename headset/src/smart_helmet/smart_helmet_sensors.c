/*!
\file       smart_helmet_sensors.c
\brief      Lightweight bring-up / poll for I2C sensors on the helmet hub

CJMCU-8118  = CCS811 gas + HDC1080 temperature/humidity
LIS3DH      = 3-axis accelerometer
GY-906-BAA  = MLX90614 IR temperature
SSD1315     = optional OLED (stub only unless SMART_HELMET_ENABLE_SSD1315)
*/

#include "smart_helmet_config.h"
#include "smart_helmet_i2c.h"
#include "smart_helmet_sensors.h"

#include <message.h>
#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

static smart_helmet_sensor_data_t sh_sensors;
static Task sh_probe_task;
static uint16 sh_probe_try;

/* CCS811 registers */
#define CCS811_REG_STATUS        0x00
#define CCS811_REG_MEAS_MODE     0x01
#define CCS811_REG_ALG_RESULT    0x02
#define CCS811_REG_APP_START     0xF4
#define CCS811_REG_HW_ID         0x20
#define CCS811_HW_ID_VALUE       0x81

/* HDC1080 registers (TI, 7-bit addr 0x40) */
#define HDC1080_REG_TEMP         0x00
#define HDC1080_REG_HUMIDITY     0x01
#define HDC1080_REG_CONFIG       0x02
#define HDC1080_REG_MANUF_ID     0xFE
#define HDC1080_REG_DEVICE_ID    0xFF
#define HDC1080_MANUF_ID_VALUE   0x5449
#define HDC1080_DEVICE_ID_VALUE  0x1050

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

#if SMART_HELMET_ENABLE_CCS811
static bool shProbeCcs811(void)
{
    uint8 id = 0;
    if (!SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_0,
                                SMART_HELMET_ADDR_CCS811,
                                CCS811_REG_HW_ID, &id, 1))
    {
        return FALSE;
    }
    DEBUG_LOG_INFO("SmartHelmet: CCS811 HW_ID=0x%02x", id);
    return id == CCS811_HW_ID_VALUE;
}

static bool shInitCcs811(void)
{
    uint8 mode = 0x10; /* constant power, 1 Hz */
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
#endif

#if SMART_HELMET_ENABLE_HDC1080
static bool shHdcReadU16(uint8 reg, uint16 *out)
{
    uint8 buf[2] = {0, 0};
    if (!SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_0,
                                SMART_HELMET_ADDR_HDC1080,
                                reg, buf, 2))
    {
        return FALSE;
    }
    *out = ((uint16)buf[0] << 8) | buf[1];
    return TRUE;
}

static bool shProbeHdc1080(void)
{
    uint16 manuf = 0;
    uint16 devid = 0;
    if (!shHdcReadU16(HDC1080_REG_MANUF_ID, &manuf) ||
        !shHdcReadU16(HDC1080_REG_DEVICE_ID, &devid))
    {
        return FALSE;
    }
    DEBUG_LOG_INFO("SmartHelmet: HDC1080 manuf=0x%04x devid=0x%04x",
                   manuf, devid);
    return (manuf == HDC1080_MANUF_ID_VALUE) &&
           (devid == HDC1080_DEVICE_ID_VALUE);
}

static bool shInitHdc1080(void)
{
    /* 0x1000: 14-bit temp + 14-bit humidity, heater off */
    uint8 cfg[2] = { 0x10, 0x00 };
    return SmartHelmet_I2cWriteReg(smart_helmet_i2c_bus_0,
                                   SMART_HELMET_ADDR_HDC1080,
                                   HDC1080_REG_CONFIG, cfg, 2);
}
#endif

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

#if SMART_HELMET_ENABLE_MLX90614
static int16 shMlxRawToCentiC(uint16 raw)
{
    int32 centi_k = ((int32)raw * 2);
    return (int16)(centi_k - 27315);
}

static bool shReadMlx90614(uint8 ram_addr, int16 *out_x100)
{
    uint8 cmd = (uint8)(MLX90614_CMD_RAM | ram_addr);
    uint8 buf[3] = {0};
    uint16 raw;

    if (!SmartHelmet_I2cTransfer(smart_helmet_i2c_bus_0,
                                 SMART_HELMET_ADDR_MLX90614,
                                 &cmd, 1, buf, 3))
    {
        return FALSE;
    }
    raw = (uint16)buf[0] | ((uint16)buf[1] << 8);
    if (raw == 0u || raw == 0xFFFFu)
    {
        DEBUG_LOG_INFO("SmartHelmet: MLX90614 raw=0x%04x rejected", raw);
        return FALSE;
    }
    *out_x100 = shMlxRawToCentiC(raw);
    return TRUE;
}
#endif

#if SMART_HELMET_ENABLE_SSD1315
static bool shProbeSsd1315(void)
{
    smart_helmet_i2c_bus_t bus =
        SMART_HELMET_SSD1315_ON_I2C1 ? smart_helmet_i2c_bus_1
                                    : smart_helmet_i2c_bus_0;
    uint8 dummy = 0x00;
    DEBUG_LOG_INFO("SmartHelmet: SSD1315 probe @0x%02x I2C%u",
                   SMART_HELMET_ADDR_SSD1315, (unsigned)bus);
    return SmartHelmet_I2cWrite(bus, SMART_HELMET_ADDR_SSD1315, &dummy, 1);
}
#endif

static bool shSensorsRequiredOk(void)
{
#if SMART_HELMET_ENABLE_CCS811
    if (!sh_sensors.ccs811_ok)
    {
        return FALSE;
    }
#endif
#if SMART_HELMET_ENABLE_HDC1080
    if (!sh_sensors.hdc1080_ok)
    {
        return FALSE;
    }
#endif
#if SMART_HELMET_ENABLE_MLX90614
    if (!sh_sensors.mlx90614_ok)
    {
        return FALSE;
    }
#endif
#if SMART_HELMET_ENABLE_LIS3DH
    if (!sh_sensors.lis3dh_ok)
    {
        return FALSE;
    }
#endif
    return TRUE;
}

static void shSensorsScheduleRetry(void)
{
    if (!sh_probe_task)
    {
        return;
    }
    MessageCancelAll(sh_probe_task, SMART_HELMET_I2C_PROBE_RETRY);
    MessageSendLater(sh_probe_task,
                     SMART_HELMET_I2C_PROBE_RETRY,
                     NULL,
                     SMART_HELMET_I2C_PROBE_RETRY_MS);
}

static void shSensorsVerifyPass(void)
{
    sh_probe_try++;
    DEBUG_LOG_INFO("SmartHelmet I2C verify try=%u ccs=%u hdc=%u lis=%u mlx=%u",
                   sh_probe_try,
                   sh_sensors.ccs811_ok,
                   sh_sensors.hdc1080_ok,
                   sh_sensors.lis3dh_ok,
                   sh_sensors.mlx90614_ok);

#if SMART_HELMET_ENABLE_CCS811
    if (!sh_sensors.ccs811_ok)
    {
        DEBUG_LOG_INFO("SmartHelmet I2C verify CCS811 @0x%02x expect HW_ID=0x%02x",
                       SMART_HELMET_ADDR_CCS811, CCS811_HW_ID_VALUE);
        if (shProbeCcs811())
        {
            sh_sensors.ccs811_ok = shInitCcs811();
            DEBUG_LOG_INFO("SmartHelmet: CCS811 %s",
                           sh_sensors.ccs811_ok ? "ok" : "init fail");
        }
        else
        {
            DEBUG_LOG_WARN("SmartHelmet: CCS811 no HW_ID 0x81");
        }
    }
#endif

#if SMART_HELMET_ENABLE_HDC1080
    if (!sh_sensors.hdc1080_ok)
    {
        DEBUG_LOG_INFO("SmartHelmet I2C verify HDC1080 @0x%02x expect 0x5449/0x1050",
                       SMART_HELMET_ADDR_HDC1080);
        if (shProbeHdc1080())
        {
            sh_sensors.hdc1080_ok = shInitHdc1080();
            DEBUG_LOG_INFO("SmartHelmet: HDC1080 %s",
                           sh_sensors.hdc1080_ok ? "ok" : "init fail");
        }
        else
        {
            DEBUG_LOG_WARN("SmartHelmet: HDC1080 no TI ID");
        }
    }
#endif

#if SMART_HELMET_ENABLE_LIS3DH
    if (!sh_sensors.lis3dh_ok)
    {
        DEBUG_LOG_INFO("SmartHelmet I2C verify LIS3DH @0x%02x expect WHOAMI=0x%02x",
                       SMART_HELMET_ADDR_LIS3DH, LIS3DH_WHO_AM_I_VALUE);
        if (shProbeLis3dh())
        {
            sh_sensors.lis3dh_ok = shInitLis3dh();
            DEBUG_LOG_INFO("SmartHelmet: LIS3DH %s",
                           sh_sensors.lis3dh_ok ? "ok" : "init fail");
        }
        else
        {
            DEBUG_LOG_WARN("SmartHelmet: LIS3DH no WHO_AM_I 0x33");
        }
    }
#endif

#if SMART_HELMET_ENABLE_MLX90614
    if (!sh_sensors.mlx90614_ok)
    {
        DEBUG_LOG_INFO("SmartHelmet I2C verify MLX90614 @0x%02x RAM_TA",
                       SMART_HELMET_ADDR_MLX90614);
        if (shReadMlx90614(MLX90614_RAM_TA, &sh_sensors.ambient_temp_x100))
        {
            sh_sensors.mlx90614_ok = TRUE;
            DEBUG_LOG_INFO("SmartHelmet: MLX90614 ok TA=%d/100C",
                           sh_sensors.ambient_temp_x100);
        }
        else
        {
            DEBUG_LOG_WARN("SmartHelmet: MLX90614 no valid TA");
        }
    }
#endif

#if SMART_HELMET_ENABLE_SSD1315
    if (!shSensorsRequiredOk())
    {
        sh_sensors.ssd1315_ok = shProbeSsd1315();
    }
#endif

    if (shSensorsRequiredOk())
    {
        DEBUG_LOG_INFO("SmartHelmet I2C verify DONE try=%u", sh_probe_try);
        if (sh_probe_task)
        {
            MessageCancelAll(sh_probe_task, SMART_HELMET_I2C_PROBE_RETRY);
        }
        return;
    }

    DEBUG_LOG_INFO("SmartHelmet I2C verify retry in %u ms",
                   SMART_HELMET_I2C_PROBE_RETRY_MS);
    shSensorsScheduleRetry();
}

void SmartHelmet_SensorsInit(void)
{
    DEBUG_LOG_INFO("%s: enter", __func__);
    sh_sensors.ccs811_ok = FALSE;
    sh_sensors.hdc1080_ok = FALSE;
    sh_sensors.lis3dh_ok = FALSE;
    sh_sensors.mlx90614_ok = FALSE;
    sh_sensors.ssd1315_ok = FALSE;
    sh_probe_try = 0;

#if SMART_HELMET_ENABLE_CCS811
    DEBUG_LOG_INFO("%s: probe CCS811 @0x%02x", __func__, SMART_HELMET_ADDR_CCS811);
    if (shProbeCcs811())
    {
        sh_sensors.ccs811_ok = shInitCcs811();
        DEBUG_LOG_INFO("SmartHelmet: CCS811 (CJMCU-8118) %s",
                       sh_sensors.ccs811_ok ? "ok" : "init fail");
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: CCS811 not found @0x%02x",
                       SMART_HELMET_ADDR_CCS811);
    }
#else
    DEBUG_LOG_INFO("%s: CCS811 skipped (SMART_HELMET_ENABLE_CCS811=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_HDC1080
    DEBUG_LOG_INFO("%s: probe HDC1080 @0x%02x", __func__, SMART_HELMET_ADDR_HDC1080);
    if (shProbeHdc1080())
    {
        sh_sensors.hdc1080_ok = shInitHdc1080();
        DEBUG_LOG_INFO("SmartHelmet: HDC1080 (CJMCU-8118) %s",
                       sh_sensors.hdc1080_ok ? "ok" : "init fail");
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: HDC1080 not found @0x%02x",
                       SMART_HELMET_ADDR_HDC1080);
    }
#else
    DEBUG_LOG_INFO("%s: HDC1080 skipped (SMART_HELMET_ENABLE_HDC1080=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_LIS3DH
    DEBUG_LOG_INFO("%s: probe LIS3DH @0x%02x", __func__, SMART_HELMET_ADDR_LIS3DH);
    if (shProbeLis3dh())
    {
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
    DEBUG_LOG_INFO("%s: LIS3DH skipped (SMART_HELMET_ENABLE_LIS3DH=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_MLX90614
    DEBUG_LOG_INFO("%s: probe MLX90614 @0x%02x", __func__, SMART_HELMET_ADDR_MLX90614);
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
#else
    DEBUG_LOG_INFO("%s: MLX90614 skipped (SMART_HELMET_ENABLE_MLX90614=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_SSD1315
    sh_sensors.ssd1315_ok = shProbeSsd1315();
    DEBUG_LOG_INFO("SmartHelmet: SSD1315 write %s (no ACK check)",
                   sh_sensors.ssd1315_ok ? "issued" : "fail");
#else
    DEBUG_LOG_INFO("%s: SSD1315 skipped (SMART_HELMET_ENABLE_SSD1315=0)", __func__);
#endif
    DEBUG_LOG_INFO("%s: exit ccs=%u hdc=%u lis=%u mlx=%u",
                   __func__,
                   sh_sensors.ccs811_ok,
                   sh_sensors.hdc1080_ok,
                   sh_sensors.lis3dh_ok,
                   sh_sensors.mlx90614_ok);
}

void SmartHelmet_SensorsPoll(void)
{
    uint8 buf[6];

#if SMART_HELMET_ENABLE_CCS811
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
#endif

#if SMART_HELMET_ENABLE_LIS3DH
    if (sh_sensors.lis3dh_ok)
    {
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

#if SMART_HELMET_ENABLE_MLX90614
    if (sh_sensors.mlx90614_ok)
    {
        shReadMlx90614(MLX90614_RAM_TA, &sh_sensors.ambient_temp_x100);
        shReadMlx90614(MLX90614_RAM_TOBJ1, &sh_sensors.object_temp_x100);
    }
#endif
	UNUSED(buf);
}

void SmartHelmet_SensorsStartVerify(Task retry_task)
{
    sh_probe_task = retry_task;
    if (!shSensorsRequiredOk())
    {
        DEBUG_LOG_INFO("SmartHelmet I2C verify: start 1s retry");
        shSensorsScheduleRetry();
    }
    else
    {
        DEBUG_LOG_INFO("SmartHelmet I2C verify: all required devices ok");
    }
}

void SmartHelmet_SensorsStopVerify(void)
{
    if (sh_probe_task)
    {
        MessageCancelAll(sh_probe_task, SMART_HELMET_I2C_PROBE_RETRY);
    }
    sh_probe_task = NULL;
}

bool SmartHelmet_SensorsHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);
    if (id != SMART_HELMET_I2C_PROBE_RETRY)
    {
        return FALSE;
    }
    shSensorsVerifyPass();
    return TRUE;
}
