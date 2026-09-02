/*!
\file       smart_helmet_i2c.c
\brief      Dual I2C master via Bitserial for Smart Helmet

Base: github 2026090117 (15eb0cb)
Local patch:
  1) Program 7-bit slave address into Bitserial I2C config before xfer
  2) Mux CLOCK/DATA IN as well as OUT (open-drain bidirectional)
  3) One combined BitserialTransfer(tx, rx) instead of split NULL-tx read
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

#include "smart_helmet_config.h"
#include "smart_helmet_i2c.h"

#include <bitserial_api.h>
#include <pio.h>
#include <pio_common.h>
#include <string.h>
#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

typedef struct
{
    bitserial_handle       handle;
    bitserial_block_index  block;
    uint16                 scl;
    uint16                 sda;
    uint16                 speed_khz;
    uint8                  addr7;
    bool                   open;
} sh_i2c_bus_state_t;

static sh_i2c_bus_state_t sh_i2c[smart_helmet_i2c_bus_count];

static bool shI2cSetFunction(uint16 pio, pin_function_id fn, const char *tag)
{
    if (!PioSetFunction(pio, fn))
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C: PioSetFunction PIO%u %s FAILED",
                        pio, tag);
        return FALSE;
    }
	CC_LOGN("SmartHelmet I2C: PioSetFunction PIO%u %s ok", pio, tag);
    return TRUE;
}

static bool shI2cUnmapAppsGpio(uint16 pio)
{
    uint16 bank = PioCommonPioBank(pio);
    uint32 mask = PioCommonPioMask(pio);

    /* 0 = release from Apps GPIO so the bitserial pad function can attach */
    if (PioSetMapPins32Bank(bank, mask, 0))
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C: PioSetMapPins32Bank PIO%u FAILED", pio);
        return FALSE;
    }
    return TRUE;
}

static bool shI2cConfigurePios(uint16 scl, uint16 sda, bitserial_block_index block)
{
	CC_LOGN("SmartHelmet I2C: mux start SCL=PIO%u SDA=PIO%u block=%u",
                   scl, sda, (unsigned)block);

    if (!shI2cUnmapAppsGpio(scl) || !shI2cUnmapAppsGpio(sda))
    {
        return FALSE;
    }

    if (block == BITSERIAL_BLOCK_0)
    {
        if (!shI2cSetFunction(scl, BITSERIAL_0_CLOCK_OUT, "BS0_CLK_OUT"))
        {
            return FALSE;
        }
        if (!shI2cSetFunction(scl, BITSERIAL_0_CLOCK_IN,  "BS0_CLK_IN"))
        {
            return FALSE;
        }
        if (!shI2cSetFunction(sda, BITSERIAL_0_DATA_OUT,  "BS0_DAT_OUT"))
        {
            return FALSE;
        }
        if (!shI2cSetFunction(sda, BITSERIAL_0_DATA_IN,   "BS0_DAT_IN"))
        {
            return FALSE;
        }
    }
    else
    {
        if (!shI2cSetFunction(scl, BITSERIAL_1_CLOCK_OUT, "BS1_CLK_OUT"))
        {
            return FALSE;
        }
        if (!shI2cSetFunction(scl, BITSERIAL_1_CLOCK_IN,  "BS1_CLK_IN"))
        {
            return FALSE;
        }
        if (!shI2cSetFunction(sda, BITSERIAL_1_DATA_OUT,  "BS1_DAT_OUT"))
        {
            return FALSE;
        }
        if (!shI2cSetFunction(sda, BITSERIAL_1_DATA_IN,   "BS1_DAT_IN"))
        {
            return FALSE;
        }
    }

	CC_LOGN("SmartHelmet I2C: mux done SCL=PIO%u SDA=PIO%u", scl, sda);
    return TRUE;
}

static void shI2cFillConfig(bitserial_config *config, uint16 speed_khz, uint8 addr7)
{
    memset(config, 0, sizeof(*config));
    config->mode = BITSERIAL_MODE_I2C_MASTER;
    config->clock_frequency_khz = speed_khz;
    config->u.i2c_cfg.flags = 0;
    config->u.i2c_cfg.i2c_address = addr7;
}

static bool shI2cOpenHandle(sh_i2c_bus_state_t *st, uint8 addr7)
{
    bitserial_config config;

    shI2cFillConfig(&config, st->speed_khz, addr7);

	CC_LOGN("SmartHelmet I2C: BitserialOpen block=%u addr=0x%02x speed=%u",
                   (unsigned)st->block, addr7, st->speed_khz);

    st->handle = BitserialOpen(st->block, &config);
    if (st->handle == BITSERIAL_HANDLE_ERROR)
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C: BitserialOpen FAILED block=%u addr=0x%02x",
                        (unsigned)st->block, addr7);
        st->open = FALSE;
        return FALSE;
    }

    st->addr7 = addr7;
    st->open = TRUE;
	CC_LOGN("SmartHelmet I2C: BitserialOpen ok handle=%d addr=0x%02x",
                   (int)st->handle, addr7);
    return TRUE;
}

static bool shI2cSetAddr(smart_helmet_i2c_bus_t bus, uint8 addr7)
{
    sh_i2c_bus_state_t *st = &sh_i2c[bus];
    bitserial_result result;

    if (!st->open)
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: set-addr 0x%02x but bus not open",
                        (unsigned)bus, addr7);
        return FALSE;
    }

    if (st->addr7 == addr7)
    {
		CC_LOGN("SmartHelmet I2C%u: addr already 0x%02x",
                       (unsigned)bus, addr7);
        return TRUE;
    }

	CC_LOGN("SmartHelmet I2C%u: set-addr 0x%02x -> 0x%02x",
                   (unsigned)bus, st->addr7, addr7);

#if defined(BITSERIAL_PARAMS_I2C_DEVICE_ADDRESS)
    result = BitserialChangeParam(st->handle,
                                  BITSERIAL_PARAMS_I2C_DEVICE_ADDRESS,
                                  addr7);
	CC_LOGN("SmartHelmet I2C%u: ChangeParam I2C_DEVICE_ADDRESS res=%d",
                   (unsigned)bus, (int)result);
    if (result == BITSERIAL_RESULT_SUCCESS)
    {
        st->addr7 = addr7;
        return TRUE;
    }
    DEBUG_LOG_WARN("SmartHelmet I2C%u: ChangeParam failed, reopen", (unsigned)bus);
#elif defined(BITSERIAL_PARAM_I2C_ADDRESS)
    result = BitserialChangeParam(st->handle, BITSERIAL_PARAM_I2C_ADDRESS, addr7);
	CC_LOGN("SmartHelmet I2C%u: ChangeParam I2C_ADDRESS res=%d",
                   (unsigned)bus, (int)result);
    if (result == BITSERIAL_RESULT_SUCCESS)
    {
        st->addr7 = addr7;
        return TRUE;
    }
    DEBUG_LOG_WARN("SmartHelmet I2C%u: ChangeParam failed, reopen", (unsigned)bus);
#else
    UNUSED(result);
	CC_LOGN("SmartHelmet I2C%u: no ChangeParam symbol, reopen", (unsigned)bus);
#endif

    BitserialClose(st->handle);
    st->handle = BITSERIAL_HANDLE_ERROR;
    st->open = FALSE;
    st->addr7 = 0;
    return shI2cOpenHandle(st, addr7);
}

static bool shI2cOpenBus(smart_helmet_i2c_bus_t bus,
                         bitserial_block_index block,
                         uint16 scl, uint16 sda,
                         uint16 speed_khz)
{
    sh_i2c_bus_state_t *st = &sh_i2c[bus];

    if (st->open)
    {
        return TRUE;
    }

    st->block = block;
    st->scl = scl;
    st->sda = sda;
    st->speed_khz = speed_khz;
    st->addr7 = 0;
    st->handle = BITSERIAL_HANDLE_ERROR;

    if (!shI2cConfigurePios(scl, sda, block))
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: PIO mux failed (SCL=%u SDA=%u)",
                        (unsigned)bus, scl, sda);
        return FALSE;
    }

    /* Address is programmed per-transfer; open with 0 so the handle exists.
     * First real xfer will ChangeParam / reopen with the slave address. */
    return shI2cOpenHandle(st, 0);
}

bool SmartHelmet_I2cInit(void)
{
    bool ok = TRUE;

#if SMART_HELMET_ENABLE_I2C0
	CC_LOGN("SmartHelmet I2C0: init SCL=%u SDA=%u",
                   SMART_HELMET_I2C0_SCL_PIO, SMART_HELMET_I2C0_SDA_PIO);
    ok = shI2cOpenBus(smart_helmet_i2c_bus_0,
                      SMART_HELMET_I2C0_BITSERIAL_BLOCK,
                      SMART_HELMET_I2C0_SCL_PIO,
                      SMART_HELMET_I2C0_SDA_PIO,
                      SMART_HELMET_I2C0_SPEED_KHZ) && ok;
#endif

#if SMART_HELMET_ENABLE_I2C1
	CC_LOGN("SmartHelmet I2C1: init SCL=%u SDA=%u",
                   SMART_HELMET_I2C1_SCL_PIO, SMART_HELMET_I2C1_SDA_PIO);
    ok = shI2cOpenBus(smart_helmet_i2c_bus_1,
                      SMART_HELMET_I2C1_BITSERIAL_BLOCK,
                      SMART_HELMET_I2C1_SCL_PIO,
                      SMART_HELMET_I2C1_SDA_PIO,
                      SMART_HELMET_I2C1_SPEED_KHZ) && ok;
#endif

	CC_LOGN("SmartHelmet I2C: init %s", ok ? "ok" : "FAILED");
    return ok;
}

void SmartHelmet_I2cClose(void)
{
    unsigned i;
    for (i = 0; i < smart_helmet_i2c_bus_count; i++)
    {
        if (sh_i2c[i].open && sh_i2c[i].handle != BITSERIAL_HANDLE_ERROR)
        {
			CC_LOGN("SmartHelmet I2C%u: close", i);
            BitserialClose(sh_i2c[i].handle);
            sh_i2c[i].handle = BITSERIAL_HANDLE_ERROR;
            sh_i2c[i].open = FALSE;
            sh_i2c[i].addr7 = 0;
        }
    }
}

bool SmartHelmet_I2cTransfer(smart_helmet_i2c_bus_t bus,
                             uint8 addr7,
                             const uint8 *tx, uint16 tx_len,
                             uint8 *rx, uint16 rx_len)
{
    bitserial_result result;
    uint8 tx0 = 0;

    if (bus >= smart_helmet_i2c_bus_count || !sh_i2c[bus].open)
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: xfer aborted (open=%d)",
                        (unsigned)bus,
                        (bus < smart_helmet_i2c_bus_count) ? sh_i2c[bus].open : 0);
        return FALSE;
    }

    if (tx_len && !tx)
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: tx_len=%u but tx=NULL",
                        (unsigned)bus, tx_len);
        return FALSE;
    }
    if (rx_len && !rx)
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: rx_len=%u but rx=NULL",
                        (unsigned)bus, rx_len);
        return FALSE;
    }
    if (!tx_len && !rx_len)
    {
        DEBUG_LOG_WARN("SmartHelmet I2C%u: empty xfer addr=0x%02x",
                       (unsigned)bus, addr7);
        return TRUE;
    }

    if (tx_len)
    {
        tx0 = tx[0];
    }

	CC_LOGN("SmartHelmet I2C%u: xfer BEGIN addr=0x%02x tx=%u rx=%u tx0=0x%02x",
                   (unsigned)bus, addr7, tx_len, rx_len, tx0);

    if (!shI2cSetAddr(bus, addr7))
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: set-addr 0x%02x FAILED before transfer",
                        (unsigned)bus, addr7);
        return FALSE;
    }

    /* P0 panic 231 was observed inside BitserialTransfer() in I2C master
     * mode (log: "BitserialTransfer ENTER" then PANIC_P0 231).
     * Use the I2C-facing Write/Read traps instead of Transfer. */
    if (tx_len)
    {
		CC_LOGN("SmartHelmet I2C%u: BitserialWrite ENTER addr=0x%02x n=%u",
                       (unsigned)bus, addr7, tx_len);
        result = BitserialWrite(sh_i2c[bus].handle,
                                BITSERIAL_NO_MSG,
                                (uint8 *)tx,
                                tx_len,
                                0);
		CC_LOGN("SmartHelmet I2C%u: BitserialWrite EXIT res=%d",
                       (unsigned)bus, (int)result);
        if (result != BITSERIAL_RESULT_SUCCESS)
        {
            DEBUG_LOG_WARN("SmartHelmet I2C%u: write FAIL addr=0x%02x res=%d n=%u",
                           (unsigned)bus, addr7, (int)result, tx_len);
            return FALSE;
        }
    }

    if (rx_len)
    {
		CC_LOGN("SmartHelmet I2C%u: BitserialRead ENTER addr=0x%02x n=%u",
                       (unsigned)bus, addr7, rx_len);
        result = BitserialRead(sh_i2c[bus].handle,
                               BITSERIAL_NO_MSG,
                               rx,
                               rx_len,
                               0);
		CC_LOGN("SmartHelmet I2C%u: BitserialRead EXIT res=%d",
                       (unsigned)bus, (int)result);
        if (result != BITSERIAL_RESULT_SUCCESS)
        {
            DEBUG_LOG_WARN("SmartHelmet I2C%u: read FAIL addr=0x%02x res=%d n=%u",
                           (unsigned)bus, addr7, (int)result, rx_len);
            return FALSE;
        }
    }

    if (rx_len)
    {
		CC_LOGN("SmartHelmet I2C%u: xfer OK addr=0x%02x rx0=0x%02x",
                       (unsigned)bus, addr7, rx[0]);
    }
    else
    {
		CC_LOGN("SmartHelmet I2C%u: xfer OK addr=0x%02x write-only",
                       (unsigned)bus, addr7);
    }
    return TRUE;
}

bool SmartHelmet_I2cWrite(smart_helmet_i2c_bus_t bus,
                          uint8 addr7,
                          const uint8 *tx, uint16 tx_len)
{
    return SmartHelmet_I2cTransfer(bus, addr7, tx, tx_len, NULL, 0);
}

bool SmartHelmet_I2cRead(smart_helmet_i2c_bus_t bus,
                         uint8 addr7,
                         uint8 *rx, uint16 rx_len)
{
    return SmartHelmet_I2cTransfer(bus, addr7, NULL, 0, rx, rx_len);
}

bool SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_t bus,
                            uint8 addr7, uint8 reg,
                            uint8 *rx, uint16 rx_len)
{
	CC_LOGN("SmartHelmet I2C%u: ReadReg addr=0x%02x reg=0x%02x n=%u",
                   (unsigned)bus, addr7, reg, rx_len);
    return SmartHelmet_I2cTransfer(bus, addr7, &reg, 1, rx, rx_len);
}

bool SmartHelmet_I2cWriteReg(smart_helmet_i2c_bus_t bus,
                             uint8 addr7, uint8 reg,
                             const uint8 *tx, uint16 tx_len)
{
    uint8 buf[17];
    if (tx_len > 16)
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: WriteReg payload too long %u",
                        (unsigned)bus, tx_len);
        return FALSE;
    }
    buf[0] = reg;
    if (tx_len && tx)
    {
        memcpy(&buf[1], tx, tx_len);
    }
	CC_LOGN("SmartHelmet I2C%u: WriteReg addr=0x%02x reg=0x%02x n=%u",
                   (unsigned)bus, addr7, reg, tx_len);
    return SmartHelmet_I2cTransfer(bus, addr7, buf, (uint16)(1 + tx_len), NULL, 0);
}
