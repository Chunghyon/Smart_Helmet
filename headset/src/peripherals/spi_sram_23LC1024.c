/* Copyright (c) 2019-2022 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Microchip 23LC1024 SPI SRAM chip.
 * Up to 20MHz
 *
 * A number of tests for Bitserial READ and WRITE over SPI.
 */

#include <bitserial_api.h>

#include <csrtypes.h>
#include <hydra_macros.h>

#include <stdlib.h>
#include <message.h>
#include <pio.h>
#include <pio_common.h>
#include <panic.h>

#include "assert.h"

#include "adk_log.h"

#include "spi_sram_23lc1024.h"

// Bitserial traps in Apps P0
//#define SPI_RAM_BITSERIAL_BLOCK BITSERIAL_BLOCK_0
// Bitserial traps in Apps P1 (if supported)
#define SPI_RAM_BITSERIAL_BLOCK P1_BITSERIAL_BLOCK_0

#define CMD_READ  0x03 /* Read data from memory */
#define CMD_WRITE 0x02 /* Write data to memory */
#define CMD_RDMR  0x05 /* Read Mode Register */
#define CMD_WRMR  0x01 /* Write Mode Register */

#define MODE_REG_GET_MODE(x) ((x >> 6) & 3)
#define MODE_REG_SET_MODE(x) ((x & 3) << 6)
#define MODE_BYTE 0 /* read/write one byte at a time */
#define MODE_PAGE 2 /* read/write one page (32 bytes) at a time */
#define MODE_SEQUENTIAL 1 /* read/write the entire chip */

/* serial frequency */
#define SPI_SPEED_KHZ 8000

#define SPI_CS_PIO 35
#define SPI_CLOCK_PIO 36
#define SPI_MISO_PIO 38
#define SPI_MOSI_PIO 37
#define SPI_INT_PIO 39

static struct
{
    uint16 pio;
    pin_function_id func[2];
} spi_pios[] = {
      {SPI_CS_PIO,    {BITSERIAL_0_SEL_OUT,   BITSERIAL_1_SEL_OUT}},
      {SPI_CLOCK_PIO, {BITSERIAL_0_CLOCK_OUT, BITSERIAL_1_CLOCK_OUT}},
      {SPI_MISO_PIO,  {BITSERIAL_0_DATA_IN,   BITSERIAL_1_DATA_IN}},
      {SPI_MOSI_PIO,  {BITSERIAL_0_DATA_OUT,  BITSERIAL_1_DATA_OUT}}
};

static bitserial_handle spi_handle = BITSERIAL_HANDLE_ERROR;

typedef struct bits_xfer_t
{
    bitserial_transfer_handle tr_handle;
    spi_sram_cb_t cb;
    void *cb_context;
    struct bits_xfer_t *next;

    uint8 data[1 /* cmd */ + 3 /* address */];
} bits_xfer_t;

static bits_xfer_t *xfers;
static int xfers_count = 0;
static int xfers_count_max = 0;

static bool bitserial_spi_initialized;

TaskData bitserialTask;

static void BitserialMsgHandler(Task task, MessageId id, Message message);

void spi_sram_23lc1024_close(void)
{
    uint32 i;
    uint16 pio, bank;
    uint32 mask;

    if (!bitserial_spi_initialized)
    {
        return;
    }

    for(i = 0; i < ARRAY_DIM(spi_pios); i++)
    {
        pio = spi_pios[i].pio;
        bank = PioCommonPioBank(pio);
        mask = PioCommonPioMask(pio);
        PioSetMapPins32Bank(bank, mask, 0);
        PioSetFunction(pio, OTHER);
    }

    bank = PioCommonPioBank(SPI_INT_PIO);
    mask = PioCommonPioMask(SPI_INT_PIO);

    PioSetMapPins32Bank(bank, mask, 0);
    PioSetFunction(SPI_INT_PIO, OTHER);

    if (spi_handle != BITSERIAL_HANDLE_ERROR)
    {
        BitserialClose(spi_handle);
        spi_handle = BITSERIAL_HANDLE_ERROR;
    }

    bitserial_spi_initialized = FALSE;

    DEBUG_LOG_ALWAYS("SPI SRAM: closed");
}

bool spi_sram_23lc1024_init(void)
{
    bitserial_config config;
    uint32 i, status;
    uint16 pio, bank;
    uint32 mask;
    pin_function_id func;
    uint32 mode;
    uint8 spi_header[1 /* cmd */ + 3 /* address */];

    if (bitserial_spi_initialized)
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: init failed, already up");
        return FALSE;
    }

    bitserial_spi_initialized = TRUE;

    memset(&config, 0, sizeof(config));

    /* Configure Bitserial in SPI mode. */
    config.mode = BITSERIAL_MODE_SPI_MASTER;
    config.clock_frequency_khz = SPI_SPEED_KHZ;
    config.u.spi_cfg.sel_enabled = TRUE;
    config.u.spi_cfg.clock_sample_offset = 0;
    config.u.spi_cfg.select_time_offset = 0;
    config.u.spi_cfg.flags = BITSERIAL_SPI_MODE_0;

    spi_handle = BitserialOpen(SPI_RAM_BITSERIAL_BLOCK, &config);

    if (spi_handle == BITSERIAL_HANDLE_ERROR)
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: BitserialOpen failed");
        return FALSE;
    }

    if (SPI_RAM_BITSERIAL_BLOCK == BITSERIAL_BLOCK_0)
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: Bitserial open, slow traps (Apps P0)");
    }
    else
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: Bitserial open, fast traps (Apps P1)");
    }

    for(i = 0; i < ARRAY_DIM(spi_pios); i++)
    {
        pio = spi_pios[i].pio;
        func = spi_pios[i].func[BITSERIAL_BLOCK_0];
        bank = PioCommonPioBank(pio);
        mask = PioCommonPioMask(pio);
        status = PioSetMapPins32Bank(bank, mask, 0);
        if (status)
        {
            DEBUG_LOG_ALWAYS("SPI SRAM: pio setup failed for PIO %d "
                             "(PioSetMapPins32Bank)", pio);
            return FALSE;
        }
        status = PioSetDriveStrength32Bank(bank, mask, DRIVE_STRENGTH_ID3);
        if (status)
        {
            DEBUG_LOG_ALWAYS("SPI SRAM: pio setup failed for PIO %d "
                             "(PioSetDriveStrength32Bank)", pio);
            return FALSE;
        }
        status = PioSetFunction(pio, func);
        if (status == FALSE)
        {
            DEBUG_LOG_ALWAYS("SPI SRAM: pio setup failed for PIO %d "
                             "(PioSetFunction)", pio);
            return FALSE;
        }
    }

    /* SPI_INT_PIO has to be connected to the !HOLD pin.
     * We want this to be high all the time. */
    bank = PioCommonPioBank(SPI_INT_PIO);
    mask = PioCommonPioMask(SPI_INT_PIO);

    /* Map as SW controlled */
    PioSetMapPins32Bank(bank, mask, mask);
    /* Direction output */
    PioSetDir32Bank(bank, mask, mask);
    /* Output high */
    PioSet32Bank(bank, mask, mask);

    DEBUG_LOG_ALWAYS("SPI SRAM: started");

    /* Read the mode register */
    spi_header[0] = CMD_RDMR;
    spi_header[1] = 0;
    BitserialTransfer(spi_handle, BITSERIAL_NO_MSG,
                      &spi_header[0], 1,
                      &spi_header[1], 1);

    mode = MODE_REG_GET_MODE(spi_header[1]);

    DEBUG_LOG_ALWAYS("SPI SRAM: MODE_REG: mode %d", mode);

    if (mode != MODE_SEQUENTIAL || TRUE)
    {
        /* Set SEQUENTIAL mode */
        spi_header[0] = CMD_WRMR;
        spi_header[1] = MODE_REG_SET_MODE(MODE_SEQUENTIAL);
        BitserialTransfer(spi_handle, BITSERIAL_NO_MSG,
                          &spi_header[0], 2,
                          NULL, 0);

    }

    /* re-read mode */
    spi_header[0] = CMD_RDMR;
    spi_header[1] = 0;
    BitserialTransfer(spi_handle, BITSERIAL_NO_MSG,
                      &spi_header[0], 1,
                      &spi_header[1], 1);

    if (spi_header[1] == MODE_REG_SET_MODE(MODE_SEQUENTIAL))
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: detected");
    }
    else
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: NOT detected");
    }

    bitserialTask.handler = BitserialMsgHandler;

    /* Register task to receive messages with Bitserial events. */
    MessageBitserialTask(&bitserialTask);

    return TRUE;
}

bool spi_sram_23lc1024_write(size_t offset, size_t size, const uint8 *data_ptr,
        spi_sram_cb_t cb, void *cb_context)
{
    bitserial_result result;

    bits_xfer_t *xfer = PanicUnlessMalloc(sizeof(*xfer) + size);

    xfer->cb_context = cb_context;
    xfer->cb = cb;

    xfer->next = xfers;

    xfer->data[0] = CMD_WRITE;
    xfer->data[1] = (uint8)(offset >> 16);
    xfer->data[2] = (uint8)(offset >> 8);
    xfer->data[3] = (uint8)(offset & 0xff);
    memcpy(&xfer->data[4], data_ptr, size);

    result = BitserialWrite(spi_handle, &xfer->tr_handle, xfer->data, 4 + size, 0);
    if (result != BITSERIAL_RESULT_SUCCESS)
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: failed to write data: %d", result);
        free(xfer);
        return FALSE;
    }

    xfers = xfer;
    xfers_count += 1;

    if (xfers_count > xfers_count_max)
    {
        xfers_count_max = xfers_count;
    }

    static size_t logged_offset = 0;
    if (offset - logged_offset > 100000)
    {
        DEBUG_LOG_ALWAYS("SPI SRAM: offset %d active xfers %d (max %d)",
                offset, xfers_count, xfers_count_max);
        logged_offset = offset;
    }

    return TRUE;
}

bool spi_sram_23lc1024_read(size_t offset, size_t size, uint8 *data_ptr,
        spi_sram_cb_t cb, void *cb_context)
{
    bitserial_result result;

    bits_xfer_t *xfer = PanicUnlessMalloc(sizeof(*xfer));

    xfer->cb_context = cb_context;
    xfer->cb = cb;

    xfer->next = xfers;

    xfer->data[0] = CMD_READ;
    xfer->data[1] = (uint8)(offset >> 16);
    xfer->data[2] = (uint8)(offset >> 8);
    xfer->data[3] = (uint8)(offset & 0xff);

    result = BitserialTransfer(spi_handle, &xfer->tr_handle,
                            xfer->data, 4, /*<- read part */
                            data_ptr, size /*<- write part */);
    if (result != BITSERIAL_RESULT_SUCCESS)
    {
        DEBUG_LOG_ALWAYS("Bitserial_SRAM: failed to read data: %d", result);
        free(xfer);
        return FALSE;
    }

    xfers = xfer;
    xfers_count += 1;

    return TRUE;
}

static void BitserialMsgHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    if (id == MESSAGE_BITSERIAL_EVENT)
    {
        const MessageBitserialEvent *bitserial_msg =
                (const MessageBitserialEvent *)message;

        bits_xfer_t **pp = &xfers;

        while (*pp)
        {
            if ((*pp)->tr_handle == bitserial_msg->transfer_handle)
            {
                bits_xfer_t *xfer = *pp;
                *pp = (*pp)->next;
                xfers_count -= 1;

                if (xfer->cb)
                {
                    xfer->cb(xfer->cb_context,
                            (bitserial_msg->result == BITSERIAL_RESULT_SUCCESS));
                }

                free(xfer);
                break;
            }
            pp = &((*pp)->next);
        }
    }
}
