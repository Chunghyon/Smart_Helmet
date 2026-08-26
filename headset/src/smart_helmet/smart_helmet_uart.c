/*!
\file       smart_helmet_uart.c
\brief      Wi-SUN UART transport (QCC Stream UART)
*/

#include "smart_helmet_config.h"
#include "smart_helmet_uart.h"

#include <stream.h>
#include <source.h>
#include <sink.h>
#include <pio.h>
#include <pio_common.h>
#include <panic.h>
#include <string.h>
#include <logging.h>

DEBUG_LOG_DEFINE_LEVEL_VAR

static Sink sh_uart_sink;
static Source sh_uart_source;
static Task sh_uart_task;
static smart_helmet_uart_rx_cb_t sh_uart_rx_cb;
static void *sh_uart_rx_ctx;

static bool shUartMapPio(uint16 pio, pin_function_id fn)
{
    uint16 bank = PioCommonPioBank(pio);
    uint32 mask = PioCommonPioMask(pio);
    if (PioSetMapPins32Bank(bank, mask, 0))
    {
        return FALSE;
    }
    return PioSetFunction(pio, fn);
}

bool SmartHelmet_UartInit(Task client_task)
{
#if !SMART_HELMET_ENABLE_WISUN_UART
    UNUSED(client_task);
    return TRUE;
#else
    sh_uart_task = client_task;

    if (!shUartMapPio(SMART_HELMET_WISUN_UART_TX_PIO, UART_TX))
    {
        DEBUG_LOG_ERROR("SmartHelmet UART: TX PIO %u mux failed",
                        SMART_HELMET_WISUN_UART_TX_PIO);
        return FALSE;
    }
    if (!shUartMapPio(SMART_HELMET_WISUN_UART_RX_PIO, UART_RX))
    {
        DEBUG_LOG_ERROR("SmartHelmet UART: RX PIO %u mux failed",
                        SMART_HELMET_WISUN_UART_RX_PIO);
        return FALSE;
    }

    StreamUartConfigure(SMART_HELMET_WISUN_UART_BAUD,
                        VM_UART_STOP_ONE,
                        VM_UART_PARITY_NONE);

    sh_uart_sink = StreamUartSink();
    sh_uart_source = StreamUartSource();

    if (!sh_uart_sink || !sh_uart_source)
    {
        DEBUG_LOG_ERROR("SmartHelmet UART: StreamUart open failed");
        return FALSE;
    }

    SourceConfigure(sh_uart_source, VM_SOURCE_MESSAGES, VM_MESSAGES_ALL);
    SinkConfigure(sh_uart_sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);
    MessageStreamTaskFromSink(sh_uart_sink, sh_uart_task);
    MessageStreamTaskFromSource(sh_uart_source, sh_uart_task);

    DEBUG_LOG_INFO("SmartHelmet UART: Wi-SUN TX=PIO%u RX=PIO%u ready",
                   SMART_HELMET_WISUN_UART_TX_PIO,
                   SMART_HELMET_WISUN_UART_RX_PIO);
    return TRUE;
#endif
}

void SmartHelmet_UartClose(void)
{
    sh_uart_sink = 0;
    sh_uart_source = 0;
}

void SmartHelmet_UartSetRxCallback(smart_helmet_uart_rx_cb_t cb, void *ctx)
{
    sh_uart_rx_cb = cb;
    sh_uart_rx_ctx = ctx;
}

bool SmartHelmet_UartSend(const uint8 *data, uint16 len)
{
    uint16 offset;
    uint8 *snk;

    if (!sh_uart_sink || !data || !len)
    {
        return FALSE;
    }
    if (SinkSlack(sh_uart_sink) < len)
    {
        DEBUG_LOG_WARN("SmartHelmet UART: TX full need=%u", len);
        return FALSE;
    }
    offset = SinkClaim(sh_uart_sink, len);
    if (offset == 0xffff)
    {
        return FALSE;
    }
    snk = SinkMap(sh_uart_sink);
    if (!snk)
    {
        return FALSE;
    }
    memcpy(snk + offset, data, len);
    return SinkFlush(sh_uart_sink, len) != 0;
}

bool SmartHelmet_UartHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (id == MESSAGE_MORE_DATA)
    {
        uint16 size;
        const uint8 *ptr;

        if (!sh_uart_source)
        {
            return FALSE;
        }
        size = SourceBoundary(sh_uart_source);
        if (!size)
        {
            size = SourceSize(sh_uart_source);
        }
        if (!size)
        {
            return TRUE;
        }
        ptr = SourceMap(sh_uart_source);
        if (ptr && sh_uart_rx_cb)
        {
            sh_uart_rx_cb(ptr, size, sh_uart_rx_ctx);
        }
        else if (ptr)
        {
            DEBUG_LOG_VERBOSE("SmartHelmet UART: RX %u bytes", size);
        }
        SourceDrop(sh_uart_source, size);
        return TRUE;
    }

    if (id == MESSAGE_MORE_SPACE)
    {
        return TRUE;
    }

    return FALSE;
}
