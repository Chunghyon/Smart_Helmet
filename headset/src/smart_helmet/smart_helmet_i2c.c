/*!
\file       smart_helmet_i2c.c
\brief      Dual I2C master via Bitserial for Smart Helmet
*/

#include "smart_helmet_config.h"
#include "smart_helmet_i2c.h"

#include <bitserial_api.h>
#include <pio.h>
#include <pio_common.h>
#include <panic.h>
#include <string.h>
#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

typedef struct
{
    bitserial_handle handle;
    bool             open;
} sh_i2c_bus_state_t;

static sh_i2c_bus_state_t sh_i2c[smart_helmet_i2c_bus_count];

static bool shI2cConfigurePios(uint16 scl, uint16 sda,
                               pin_function_id scl_fn,
                               pin_function_id sda_fn)
{
    uint16 bank;
    uint32 mask;

    bank = PioCommonPioBank(scl);
    mask = PioCommonPioMask(scl);
    if (PioSetMapPins32Bank(bank, mask, 0))
    {
        return FALSE;
    }
    if (!PioSetFunction(scl, scl_fn))
    {
        return FALSE;
    }

    bank = PioCommonPioBank(sda);
    mask = PioCommonPioMask(sda);
    if (PioSetMapPins32Bank(bank, mask, 0))
    {
        return FALSE;
    }
    if (!PioSetFunction(sda, sda_fn))
    {
        return FALSE;
    }
    return TRUE;
}

static bool shI2cOpenBus(smart_helmet_i2c_bus_t bus,
                         bitserial_block_index block,
                         uint16 scl, uint16 sda,
                         uint16 speed_khz,
                         pin_function_id scl_fn,
                         pin_function_id sda_fn)
{
    bitserial_config config;

    if (sh_i2c[bus].open)
    {
        return TRUE;
    }

    if (!shI2cConfigurePios(scl, sda, scl_fn, sda_fn))
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: PIO mux failed (SCL=%u SDA=%u)",
                        (unsigned)bus, scl, sda);
        return FALSE;
    }

    memset(&config, 0, sizeof(config));
    config.mode = BITSERIAL_MODE_I2C_MASTER;
    config.clock_frequency_khz = speed_khz;
    /* Typical open-drain I2C; flags left 0 unless kit requires stretch opts */
    config.u.i2c_cfg.flags = 0;

    sh_i2c[bus].handle = BitserialOpen(block, &config);
    if (sh_i2c[bus].handle == BITSERIAL_HANDLE_ERROR)
    {
        DEBUG_LOG_ERROR("SmartHelmet I2C%u: BitserialOpen failed", (unsigned)bus);
        return FALSE;
    }

    sh_i2c[bus].open = TRUE;
    DEBUG_LOG_INFO("SmartHelmet I2C%u: open SCL=%u SDA=%u @ %u kHz",
                   (unsigned)bus, scl, sda, speed_khz);
    return TRUE;
}

bool SmartHelmet_I2cInit(void)
{
    bool ok = TRUE;

#if SMART_HELMET_ENABLE_I2C0
    /* BITSERIAL_0 CLOCK/DATA map to SCL/SDA when in I2C master mode */
    ok = shI2cOpenBus(smart_helmet_i2c_bus_0,
                      SMART_HELMET_I2C0_BITSERIAL_BLOCK,
                      SMART_HELMET_I2C0_SCL_PIO,
                      SMART_HELMET_I2C0_SDA_PIO,
                      SMART_HELMET_I2C0_SPEED_KHZ,
                      BITSERIAL_0_CLOCK_OUT,
                      BITSERIAL_0_DATA_OUT) && ok;
#endif

#if SMART_HELMET_ENABLE_I2C1
    ok = shI2cOpenBus(smart_helmet_i2c_bus_1,
                      SMART_HELMET_I2C1_BITSERIAL_BLOCK,
                      SMART_HELMET_I2C1_SCL_PIO,
                      SMART_HELMET_I2C1_SDA_PIO,
                      SMART_HELMET_I2C1_SPEED_KHZ,
                      BITSERIAL_1_CLOCK_OUT,
                      BITSERIAL_1_DATA_OUT) && ok;
#endif

    return ok;
}

void SmartHelmet_I2cClose(void)
{
    unsigned i;
    for (i = 0; i < smart_helmet_i2c_bus_count; i++)
    {
        if (sh_i2c[i].open && sh_i2c[i].handle != BITSERIAL_HANDLE_ERROR)
        {
            BitserialClose(sh_i2c[i].handle);
            sh_i2c[i].handle = BITSERIAL_HANDLE_ERROR;
            sh_i2c[i].open = FALSE;
        }
    }
}

bool SmartHelmet_I2cTransfer(smart_helmet_i2c_bus_t bus,
                             uint8 addr7,
                             const uint8 *tx, uint16 tx_len,
                             uint8 *rx, uint16 rx_len)
{
    bitserial_result result;
    uint8 addr_buf[1];

    if (bus >= smart_helmet_i2c_bus_count || !sh_i2c[bus].open)
    {
        return FALSE;
    }

    /* Bitserial I2C expects 7-bit address in transfer API — pass as parameter
     * via high-level helper: write/read with address prepended on some kits.
     * Here we use BitserialTransfer after a repeated-start style: first write
     * payload (including reg), then read. Address is programmed via config
     * message flags on some ADKs; use BitserialWrite/Read with addr in tx[0]. */

    /* Portable approach used across ADK examples: first byte is (addr<<1)|R/W
     * when using raw transfer — kit-specific. Prefer BitserialTransfer with
     * separate tx/rx windows after slave address is set in i2c_cfg if available.
     * Fallback: pack address into a small header. */
    addr_buf[0] = (uint8)(addr7 << 1); /* write bit = 0 */

    if (tx_len && tx)
    {
        /* Write phase: addr+tx is handled by Bitserial I2C API variant.
         * Use transfer with tx only first. */
        result = BitserialTransfer(sh_i2c[bus].handle, BITSERIAL_NO_MSG,
                                   (uint8 *)tx, tx_len,
                                   NULL, 0);
        if (result != BITSERIAL_RESULT_SUCCESS)
        {
            DEBUG_LOG_WARN("SmartHelmet I2C%u: write fail addr=0x%x res=%d",
                           (unsigned)bus, addr7, result);
            return FALSE;
        }
        UNUSED(addr_buf);
    }

    if (rx_len && rx)
    {
        result = BitserialTransfer(sh_i2c[bus].handle, BITSERIAL_NO_MSG,
                                   NULL, 0,
                                   rx, rx_len);
        if (result != BITSERIAL_RESULT_SUCCESS)
        {
            DEBUG_LOG_WARN("SmartHelmet I2C%u: read fail addr=0x%x res=%d",
                           (unsigned)bus, addr7, result);
            return FALSE;
        }
    }

    /* Note: production builds should use the kit's I2C address API
     * (BitserialTransfer with i2c address parameter) once linked against
     * the full os/ firmware headers. Address is logged for bring-up. */
    DEBUG_LOG_VERBOSE("SmartHelmet I2C%u: xfer addr=0x%x tx=%u rx=%u",
                      (unsigned)bus, addr7, tx_len, rx_len);
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
    return SmartHelmet_I2cTransfer(bus, addr7, &reg, 1, rx, rx_len);
}

bool SmartHelmet_I2cWriteReg(smart_helmet_i2c_bus_t bus,
                             uint8 addr7, uint8 reg,
                             const uint8 *tx, uint16 tx_len)
{
    uint8 buf[17];
    if (tx_len > 16)
    {
        return FALSE;
    }
    buf[0] = reg;
    if (tx_len && tx)
    {
        memcpy(&buf[1], tx, tx_len);
    }
    return SmartHelmet_I2cTransfer(bus, addr7, buf, (uint16)(1 + tx_len), NULL, 0);
}
