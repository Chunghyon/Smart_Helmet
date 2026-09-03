/*!
\file       smart_helmet_sensors.c
\brief      Lightweight bring-up / poll for I2C sensors on the helmet hub

CJMCU-8118  = CCS811 gas + HDC1080 temperature/humidity
LIS3DH      = 3-axis accelerometer
GY-906-BAA  = MLX90614 IR temperature
SSD1315     = 128x64 OLED on I2C1 (or I2C0) — probe, init, splash
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

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
    bool rd = SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_0,
                                     SMART_HELMET_ADDR_CCS811,
                                     CCS811_REG_HW_ID, &id, 1);
    bool match = rd && (id == CCS811_HW_ID_VALUE);
    DEBUG_LOG_ALWAYS("SmartHelmet: CCS811 HW_ID expect=0x%02x got=0x%02x rd=%u",
                     CCS811_HW_ID_VALUE, id, rd ? 1u : 0u);
    return match;
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
    uint8 first[2] = {0, 0};
    uint8 buf[2] = {0, 0};
    uint16 v0, v1;

    /* HDC1080 ID pointer applies one transaction late with
     * BitserialTransfer (manuf/devid swap). Set pointer with STOP,
     * discard the first read, use the second. */
    if (!SmartHelmet_I2cWrite(smart_helmet_i2c_bus_0,
                              SMART_HELMET_ADDR_HDC1080, &reg, 1))
    {
        return FALSE;
    }
    if (!SmartHelmet_I2cRead(smart_helmet_i2c_bus_0,
                             SMART_HELMET_ADDR_HDC1080, first, 2))
    {
        return FALSE;
    }
    if (!SmartHelmet_I2cRead(smart_helmet_i2c_bus_0,
                             SMART_HELMET_ADDR_HDC1080, buf, 2))
    {
        return FALSE;
    }
    v0 = ((uint16)first[0] << 8) | first[1];
    v1 = ((uint16)buf[0] << 8) | buf[1];
    if (v0 != v1)
    {
        CC_LOGN("SmartHelmet: HDC1080 reg=0x%02x 1st=0x%04x 2nd=0x%04x (use 2nd)",
                reg, v0, v1);
    }
    *out = v1;
    return TRUE;
}

static bool shProbeHdc1080(void)
{
#if 1
    uint16 manuf = 0;
    uint16 devid = 0;
    bool rd_m = shHdcReadU16(HDC1080_REG_MANUF_ID, &manuf);
    bool rd_d = shHdcReadU16(HDC1080_REG_DEVICE_ID, &devid);
    bool match = rd_m && rd_d &&
                 (manuf == HDC1080_MANUF_ID_VALUE) &&
                 (devid == HDC1080_DEVICE_ID_VALUE);
	if(manuf == HDC1080_MANUF_ID_VALUE) {
		CC_LOGN("SmartHelmet: HDC1080_MANUF_ID ok(0x%04x)", manuf);
	}
	else {
		CC_LOGN("SmartHelmet: HDC1080_MANUF_ID fail. expect=0x%04x got=0x%04x rd=%u",
						 HDC1080_MANUF_ID_VALUE, manuf, rd_m ? 1u : 0u);
	}
	if(devid == HDC1080_DEVICE_ID_VALUE) {
		CC_LOGN("SmartHelmet: HDC1080_DEVICE_ID ok(0x%04x)", devid);
	}
	else {
		CC_LOGN("SmartHelmet: HDC1080_DEVICE_ID fail. expect=0x%04x got=0x%04x rd=%u",
						 HDC1080_DEVICE_ID_VALUE, devid, rd_d ? 1u : 0u);
	}
#else
	uint16 manuf = 0;
	bool rd_m = shHdcReadU16(HDC1080_REG_MANUF_ID, &manuf);
	bool match = rd_m && (manuf == HDC1080_MANUF_ID_VALUE);
	if(manuf == HDC1080_MANUF_ID_VALUE) {
		CC_LOGN("SmartHelmet: HDC1080_MANUF_ID ok(0x%04x)", manuf);
	}
	else {
		CC_LOGN("SmartHelmet: HDC1080_MANUF_ID fail. expect=0x%04x got=0x%04x rd=%u",
						 HDC1080_MANUF_ID_VALUE, manuf, rd_m ? 1u : 0u);
	}
#endif
    return match;
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
		CC_LOGN("SmartHelmet: MLX90614 raw=0x%04x rejected", raw);
        return FALSE;
    }
    *out_x100 = shMlxRawToCentiC(raw);
    return TRUE;
}
#endif

#if SMART_HELMET_ENABLE_SSD1315
#define SSD1315_CTRL_CMD   (0x00)
#define SSD1315_CTRL_DATA  (0x40)
#define SSD1315_WIDTH      (128)
#define SSD1315_PAGES      (8)

static smart_helmet_i2c_bus_t shSsdBus(void)
{
    return SMART_HELMET_SSD1315_ON_I2C1 ? smart_helmet_i2c_bus_1
                                       : smart_helmet_i2c_bus_0;
}

static bool shSsdRaw(const uint8 *tx, uint16 n)
{
    return SmartHelmet_I2cWrite(shSsdBus(), SMART_HELMET_ADDR_SSD1315, tx, n);
}

static bool shSsdCmd(uint8 cmd)
{
    uint8 buf[2];
    buf[0] = SSD1315_CTRL_CMD;
    buf[1] = cmd;
    return shSsdRaw(buf, 2);
}

static bool shSsdCmdList(const uint8 *cmds, uint16 n)
{
    uint8 buf[17];
    uint16 i = 0;
    while (i < n)
    {
        uint16 chunk = (uint16)(n - i);
        uint16 j;
        if (chunk > 16)
        {
            chunk = 16;
        }
        buf[0] = SSD1315_CTRL_CMD;
        for (j = 0; j < chunk; j++)
        {
            buf[1 + j] = cmds[i + j];
        }
        if (!shSsdRaw(buf, (uint16)(1 + chunk)))
        {
            return FALSE;
        }
        i = (uint16)(i + chunk);
    }
    return TRUE;
}

static bool shSsdData(const uint8 *data, uint16 n)
{
    uint8 buf[17];
    uint16 i = 0;
    while (i < n)
    {
        uint16 chunk = (uint16)(n - i);
        uint16 j;
        if (chunk > 16)
        {
            chunk = 16;
        }
        buf[0] = SSD1315_CTRL_DATA;
        for (j = 0; j < chunk; j++)
        {
            buf[1 + j] = data[i + j];
        }
        if (!shSsdRaw(buf, (uint16)(1 + chunk)))
        {
            return FALSE;
        }
        i = (uint16)(i + chunk);
    }
    return TRUE;
}

/* 5x7 glyphs, LSB = top pixel. Index 0 = space. */
static const uint8 sh_font5x7[][5] =
{
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
};

static bool shSsdSetPageCol(uint8 page, uint8 col)
{
    uint8 cmds[3];
    cmds[0] = (uint8)(0xB0 | (page & 0x07));
    cmds[1] = (uint8)(0x00 | (col & 0x0F));
    cmds[2] = (uint8)(0x10 | ((col >> 4) & 0x0F));
    return shSsdCmdList(cmds, 3);
}

static bool shSsdClear(void)
{
    uint8 zeros[16];
    uint8 page;
    uint8 i;
    for (i = 0; i < 16; i++)
    {
        zeros[i] = 0;
    }
    for (page = 0; page < SSD1315_PAGES; page++)
    {
        uint8 col = 0;
        if (!shSsdSetPageCol(page, 0))
        {
            return FALSE;
        }
        while (col < SSD1315_WIDTH)
        {
            uint8 n = (uint8)(SSD1315_WIDTH - col);
            if (n > 16)
            {
                n = 16;
            }
            if (!shSsdData(zeros, n))
            {
                return FALSE;
            }
            col = (uint8)(col + n);
        }
    }
    return TRUE;
}

static bool shSsdDrawText(uint8 page, uint8 col, const char *s)
{
    if (!shSsdSetPageCol(page, col))
    {
        return FALSE;
    }
    while (s && *s)
    {
        uint8 ch = (uint8)*s++;
        uint8 gap = 0;
        const uint8 *g;
        if (ch < 0x20 || ch > 0x5A)
        {
            ch = (uint8)' ';
        }
        g = sh_font5x7[ch - 0x20];
        if (!shSsdData(g, 5) || !shSsdData(&gap, 1))
        {
            return FALSE;
        }
    }
    return TRUE;
}

static bool shSsdInitPanel(void)
{
    static const uint8 init_cmds[] =
    {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock */
        0xA8, 0x3F,       /* mux 1/64 */
        0xD3, 0x00,       /* offset */
        0x40,             /* start line 0 */
        0x8D, 0x14,       /* charge pump on */
        0x20, 0x02,       /* page addressing */
        0xA1,             /* segment remap */
        0xC8,             /* COM scan remap */
        0xDA, 0x12,       /* COM pins */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* precharge */
        0xDB, 0x40,       /* VCOM */
        0xA4,             /* resume RAM */
        0xA6,             /* normal (not inverse) */
        0x2E,             /* deactivate scroll */
        0xAF              /* display on */
    };
    return shSsdCmdList(init_cmds, (uint16)sizeof(init_cmds));
}

static bool shSsdShowSplash(void)
{
    if (!shSsdClear())
    {
        return FALSE;
    }
    /* 5x7 + pad = 6 px/char. 128-wide centering. */
    if (!shSsdDrawText(2, 28, "SMART HELMET"))
    {
        return FALSE;
    }
    if (!shSsdDrawText(4, 40, "QCC3044"))
    {
        return FALSE;
    }
    if (!shSsdDrawText(6, 46, "READY"))
    {
        return FALSE;
    }
    return TRUE;
}

static bool shProbeSsd1315(void)
{
    CC_LOGN("SmartHelmet: SSD1315 probe @0x%02x I2C%u",
            SMART_HELMET_ADDR_SSD1315, (unsigned)shSsdBus());
    /* Display-off is a harmless command and requires an ACK. */
    return shSsdCmd(0xAE);
}

static bool shInitSsd1315(void)
{
    if (!shSsdInitPanel())
    {
        DEBUG_LOG_WARN("SmartHelmet: SSD1315 init cmds failed");
        return FALSE;
    }
    if (!shSsdShowSplash())
    {
        DEBUG_LOG_WARN("SmartHelmet: SSD1315 splash failed");
        return FALSE;
    }
    CC_LOGN("SmartHelmet: SSD1315 splash ok (SMART HELMET / QCC3044 / READY)");
    return TRUE;
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
	CC_LOGN("SmartHelmet I2C verify try=%u ccs=%u hdc=%u lis=%u mlx=%u oled=%u",
                   sh_probe_try,
                   sh_sensors.ccs811_ok,
                   sh_sensors.hdc1080_ok,
                   sh_sensors.lis3dh_ok,
                   sh_sensors.mlx90614_ok,
                   sh_sensors.ssd1315_ok);

#if SMART_HELMET_ENABLE_CCS811
    if (!sh_sensors.ccs811_ok)
    {
		CC_LOGN("SmartHelmet I2C verify CCS811 @0x%02x expect HW_ID=0x%02x",
                       SMART_HELMET_ADDR_CCS811, CCS811_HW_ID_VALUE);
        if (shProbeCcs811())
        {
            sh_sensors.ccs811_ok = shInitCcs811();
			CC_LOGN("SmartHelmet: CCS811 %s",
                           sh_sensors.ccs811_ok ? "ok" : "init fail");
        }
        else
        {
			DEBUG_LOG_WARN("SmartHelmet: CCS811 ID mismatch");
        }
    }
#endif

#if SMART_HELMET_ENABLE_HDC1080
    if (!sh_sensors.hdc1080_ok)
    {
		CC_LOGN("SmartHelmet I2C verify HDC1080 @0x%02x expect 0x5449/0x1050",
                       SMART_HELMET_ADDR_HDC1080);
        if (shProbeHdc1080())
        {
            sh_sensors.hdc1080_ok = shInitHdc1080();
			CC_LOGN("SmartHelmet: HDC1080 %s",
                           sh_sensors.hdc1080_ok ? "ok" : "init fail");
        }
        else
        {
			DEBUG_LOG_WARN("SmartHelmet: HDC1080 ID mismatch");
        }
    }
#endif

#if SMART_HELMET_ENABLE_LIS3DH
    if (!sh_sensors.lis3dh_ok)
    {
		CC_LOGN("SmartHelmet I2C verify LIS3DH @0x%02x expect WHOAMI=0x%02x",
                       SMART_HELMET_ADDR_LIS3DH, LIS3DH_WHO_AM_I_VALUE);
        if (shProbeLis3dh())
        {
            sh_sensors.lis3dh_ok = shInitLis3dh();
			CC_LOGN("SmartHelmet: LIS3DH %s",
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
		CC_LOGN("SmartHelmet I2C verify MLX90614 @0x%02x RAM_TA",
                       SMART_HELMET_ADDR_MLX90614);
        if (shReadMlx90614(MLX90614_RAM_TA, &sh_sensors.ambient_temp_x100))
        {
            sh_sensors.mlx90614_ok = TRUE;
			CC_LOGN("SmartHelmet: MLX90614 ok TA=%d/100C",
                           sh_sensors.ambient_temp_x100);
        }
        else
        {
            DEBUG_LOG_WARN("SmartHelmet: MLX90614 no valid TA");
        }
    }
#endif

#if SMART_HELMET_ENABLE_SSD1315
    if (!sh_sensors.ssd1315_ok)
    {
        CC_LOGN("SmartHelmet I2C verify SSD1315 @0x%02x",
                SMART_HELMET_ADDR_SSD1315);
        if (shProbeSsd1315() && shInitSsd1315())
        {
            sh_sensors.ssd1315_ok = TRUE;
            CC_LOGN("SmartHelmet: SSD1315 ok");
        }
        else
        {
            DEBUG_LOG_WARN("SmartHelmet: SSD1315 not ready");
        }
    }
#endif

    if (shSensorsRequiredOk())
    {
		CC_LOGN("SmartHelmet I2C verify DONE try=%u", sh_probe_try);
        if (sh_probe_task)
        {
            MessageCancelAll(sh_probe_task, SMART_HELMET_I2C_PROBE_RETRY);
        }
        return;
    }

	CC_LOGN("SmartHelmet I2C verify retry in %u ms",
                   SMART_HELMET_I2C_PROBE_RETRY_MS);
    shSensorsScheduleRetry();
}

void SmartHelmet_SensorsInit(void)
{
	CC_LOGN("%s: enter", __func__);
    sh_sensors.ccs811_ok = FALSE;
    sh_sensors.hdc1080_ok = FALSE;
    sh_sensors.lis3dh_ok = FALSE;
    sh_sensors.mlx90614_ok = FALSE;
    sh_sensors.ssd1315_ok = FALSE;
    sh_probe_try = 0;

#if SMART_HELMET_ENABLE_CCS811
	CC_LOGN("%s: probe CCS811 @0x%02x", __func__, SMART_HELMET_ADDR_CCS811);
    if (shProbeCcs811())
    {
        sh_sensors.ccs811_ok = shInitCcs811();
		CC_LOGN("SmartHelmet: CCS811 (CJMCU-8118) %s",
                       sh_sensors.ccs811_ok ? "ok" : "init fail");
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: CCS811 not found @0x%02x",
                       SMART_HELMET_ADDR_CCS811);
    }
#else
	CC_LOGN("%s: CCS811 skipped (SMART_HELMET_ENABLE_CCS811=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_HDC1080
	CC_LOGN("%s: probe HDC1080 @0x%02x", __func__, SMART_HELMET_ADDR_HDC1080);
    if (shProbeHdc1080())
    {
        sh_sensors.hdc1080_ok = shInitHdc1080();
		CC_LOGN("SmartHelmet: HDC1080 (CJMCU-8118) %s",
                       sh_sensors.hdc1080_ok ? "ok" : "init fail");
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: HDC1080 not found @0x%02x",
                       SMART_HELMET_ADDR_HDC1080);
    }
#else
	CC_LOGN("%s: HDC1080 skipped (SMART_HELMET_ENABLE_HDC1080=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_LIS3DH
	CC_LOGN("%s: probe LIS3DH @0x%02x", __func__, SMART_HELMET_ADDR_LIS3DH);
    if (shProbeLis3dh())
    {
        sh_sensors.lis3dh_ok = shInitLis3dh();
		CC_LOGN("SmartHelmet: LIS3DH %s",
                       sh_sensors.lis3dh_ok ? "ok" : "init fail");
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: LIS3DH not found @0x%02x",
                       SMART_HELMET_ADDR_LIS3DH);
    }
#else
	CC_LOGN("%s: LIS3DH skipped (SMART_HELMET_ENABLE_LIS3DH=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_MLX90614
	CC_LOGN("%s: probe MLX90614 @0x%02x", __func__, SMART_HELMET_ADDR_MLX90614);
    if (shReadMlx90614(MLX90614_RAM_TA, &sh_sensors.ambient_temp_x100))
    {
        sh_sensors.mlx90614_ok = TRUE;
		CC_LOGN("SmartHelmet: MLX90614 (GY-906) ok TA=%d/100C",
                       sh_sensors.ambient_temp_x100);
    }
    else
    {
        DEBUG_LOG_WARN("SmartHelmet: MLX90614 not responding @0x%02x",
                       SMART_HELMET_ADDR_MLX90614);
    }
#else
	CC_LOGN("%s: MLX90614 skipped (SMART_HELMET_ENABLE_MLX90614=0)", __func__);
#endif

#if SMART_HELMET_ENABLE_SSD1315
	CC_LOGN("%s: probe SSD1315 @0x%02x I2C%u",
                   __func__, SMART_HELMET_ADDR_SSD1315, (unsigned)shSsdBus());
    if (shProbeSsd1315() && shInitSsd1315())
    {
        sh_sensors.ssd1315_ok = TRUE;
        CC_LOGN("SmartHelmet: SSD1315 (OLED) ok");
    }
    else
    {
        sh_sensors.ssd1315_ok = FALSE;
        DEBUG_LOG_WARN("SmartHelmet: SSD1315 not found @0x%02x",
                       SMART_HELMET_ADDR_SSD1315);
    }
#else
	CC_LOGN("%s: SSD1315 skipped (SMART_HELMET_ENABLE_SSD1315=0)", __func__);
#endif
	CC_LOGN("%s: exit ccs=%u hdc=%u lis=%u mlx=%u oled=%u",
                   __func__,
                   sh_sensors.ccs811_ok,
                   sh_sensors.hdc1080_ok,
                   sh_sensors.lis3dh_ok,
                   sh_sensors.mlx90614_ok,
                   sh_sensors.ssd1315_ok);
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
		CC_LOGN("SmartHelmet I2C verify: start 1s retry");
        shSensorsScheduleRetry();
    }
    else
    {
		CC_LOGN("SmartHelmet I2C verify: all required devices ok");
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
