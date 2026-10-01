/*!
\file       smart_helmet_uart.c
\brief      Wi-SUN UART transport (QCC Stream UART) and WS8856FLS link check
*/
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

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

static smart_helmet_wisun_status_t sh_wisun;
static uint8 sh_wisun_step;          /* 0 idle, 1 param, 2 ip, 3 done */
static uint8 sh_rx_asm[SMART_HELMET_WISUN_RX_BUF_SIZE];
static uint16 sh_rx_asm_len;

static bool shUartMapPio(uint16 pio, pin_function_id fn)
{
#if SMART_HELMET_ENABLE_WISUN_UART
    uint16 bank = PioCommonPioBank(pio);
    uint32 mask = PioCommonPioMask(pio);
    if (PioSetMapPins32Bank(bank, mask, 0))
    {
        return FALSE;
    }
#endif
    return PioSetFunction(pio, fn);
}

static bool shContainsFold(const uint8 *data, uint16 len, const char *needle)
{
    uint16 nlen = 0;
    uint16 i;

    if (!data || !needle)
    {
        return FALSE;
    }
    while (needle[nlen] != '\0')
    {
        nlen++;
    }
    if (!nlen || len < nlen)
    {
        return FALSE;
    }
    for (i = 0; i + nlen <= len; i++)
    {
        uint16 j;
        bool match = TRUE;

        for (j = 0; j < nlen; j++)
        {
            uint8 a = data[i + j];
            uint8 b = (uint8)needle[j];
            if (a >= 'A' && a <= 'Z')
            {
                a = (uint8)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z')
            {
                b = (uint8)(b - 'A' + 'a');
            }
            if (a != b)
            {
                match = FALSE;
                break;
            }
        }
        if (match)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static uint8 sh_rx_prev[48];
static uint16 sh_rx_prev_len;

static char shHexNibble(uint8 v)
{
    return (char)((v < 10) ? ('0' + v) : ('A' + (v - 10)));
}

static void shLogHex(const char *tag, const uint8 *data, uint16 len)
{
    char line[52];
    uint16 off = 0;

    if (!data)
    {
        return;
    }
    if (len > 36)
    {
        len = 36;
    }
    while (off < len)
    {
        uint16 n = 0;
        uint16 i;
        uint16 chunk = (uint16)(len - off);

        if (chunk > 12)
        {
            chunk = 12;
        }
        for (i = 0; i < chunk && n + 3 < sizeof(line); i++)
        {
            uint8 b = data[off + i];
            line[n++] = shHexNibble((uint8)(b >> 4));
            line[n++] = shHexNibble((uint8)(b & 0x0f));
            line[n++] = ' ';
        }
        line[n] = '\0';
        CC_LOGN("SmartHelmet UART %s +%u %s", tag, off, line);
        off = (uint16)(off + chunk);
    }
}

static void shLogRxShape(const uint8 *data, uint16 len)
{
    uint16 i;
    uint16 printable = 0;
    uint16 crlf = 0;
    uint16 zero = 0;
    uint16 same = 0;
    bool identical = FALSE;

    for (i = 0; i < len; i++)
    {
        uint8 c = data[i];
        if (c == '\r' || c == '\n')
        {
            crlf++;
        }
        else if (c >= 32 && c < 127)
        {
            printable++;
        }
        if (c == 0)
        {
            zero++;
        }
    }
    if (sh_rx_prev_len == len && len && len <= sizeof(sh_rx_prev))
    {
        identical = TRUE;
        for (i = 0; i < len; i++)
        {
            if (sh_rx_prev[i] == data[i])
            {
                same++;
            }
            else
            {
                identical = FALSE;
            }
        }
    }
    if (len && len <= sizeof(sh_rx_prev))
    {
        memcpy(sh_rx_prev, data, len);
        sh_rx_prev_len = len;
    }
    CC_LOGN("SmartHelmet UART RX shape len=%u ascii=%u crlf=%u nul=%u same=%u identical=%u",
            len, printable, crlf, zero, same, identical);
    if (identical && printable * 2 < len)
    {
        CC_LOGN("SmartHelmet UART hint: stable binary frame, baud mismatch unlikely");
    }
    else if (!identical && sh_rx_prev_len && printable * 4 < len)
    {
        CC_LOGN("SmartHelmet UART hint: varying garbage, baud/framing suspect");
    }
    else if (crlf && printable * 2 >= len)
    {
        CC_LOGN("SmartHelmet UART hint: text CLI, baud looks matched");
    }
}
static void shCopyPreview(const uint8 *data, uint16 len)
{
    uint16 i;
    uint16 n = 0;

    for (i = 0; i < len && n + 1 < sizeof(sh_wisun.last_line); i++)
    {
        uint8 c = data[i];
        if (c == '\r' || c == '\n')
        {
            if (n)
            {
                break;
            }
            continue;
        }
        sh_wisun.last_line[n++] = (c >= 32 && c < 127) ? (char)c : '.';
    }
    sh_wisun.last_line[n] = '\0';
}

static void shWisunNoteRx(const uint8 *data, uint16 len)
{
    if (!data || !len)
    {
        return;
    }
    sh_wisun.rx_seen = TRUE;
    shCopyPreview(data, len);

    if (shContainsFold(data, len, "8856") ||
        shContainsFold(data, len, "ws8856") ||
        shContainsFold(data, len, "role") ||
        shContainsFold(data, len, "status") ||
        shContainsFold(data, len, "phy"))
    {
        sh_wisun.module_seen = TRUE;
    }
    if (sh_wisun_step == 2 &&
        (shContainsFold(data, len, ":") || shContainsFold(data, len, "ip")))
    {
        sh_wisun.ip_seen = TRUE;
    }
}

static void shWisunLogResult(void)
{
    if (sh_wisun.module_seen)
    {
        sh_wisun.result = smart_helmet_wisun_module_ok;
        CC_LOGN("SmartHelmet WS8856 UART ok tx=%u rx=%u ip=%u try=%u line=%s",
                sh_wisun.tx_ok, sh_wisun.rx_seen, sh_wisun.ip_seen,
                sh_wisun.tries, sh_wisun.last_line);
    }
    else if (sh_wisun.rx_seen)
    {
        sh_wisun.result = smart_helmet_wisun_rx_unknown;
        CC_LOGN("SmartHelmet WS8856 UART rx but not CLI tx=%u try=%u line=%s",
                sh_wisun.tx_ok, sh_wisun.tries, sh_wisun.last_line);
    }
    else if (sh_wisun.tx_ok)
    {
        sh_wisun.result = smart_helmet_wisun_tx_only;
        CC_LOGN("SmartHelmet WS8856 UART fail: TX ok, no RX (PIO%u/%u 115200)",
                SMART_HELMET_WISUN_UART_TX_PIO, SMART_HELMET_WISUN_UART_RX_PIO);
    }
    else
    {
        sh_wisun.result = smart_helmet_wisun_idle;
        CC_LOGN("SmartHelmet WS8856 UART fail: TX flush failed");
    }
}

static void shWisunSendProbe(const char *cmd)
{
    uint16 len = 0;
    static const uint8 eol[2] = { '\r', '\n' };

    while (cmd[len] != '\0')
    {
        len++;
    }
    sh_wisun.tx_ok = SmartHelmet_UartSend((const uint8 *)cmd, len) &&
                     SmartHelmet_UartSend(eol, sizeof(eol));
    CC_LOGN("SmartHelmet WS8856: send %s tx=%u try=%u baud=115200 8N1",
            cmd, sh_wisun.tx_ok, sh_wisun.tries);
    shLogHex("TX", (const uint8 *)cmd, len);
    shLogHex("TX", eol, sizeof(eol));
}

static void shWisunArm(uint16 delay_ms)
{
    if (sh_uart_task)
    {
        MessageCancelAll(sh_uart_task, SMART_HELMET_WISUN_LINK_CHECK);
        MessageSendLater(sh_uart_task, SMART_HELMET_WISUN_LINK_CHECK,
                         NULL, delay_ms);
    }
}

bool SmartHelmet_UartInit(Task client_task)
{
#if !SMART_HELMET_ENABLE_WISUN_UART
    UNUSED(shUartMapPio);
    sh_uart_task = client_task;
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

    CC_LOGN("SmartHelmet UART: WS8856FLS TX=PIO%u RX=PIO%u 115200 8N1",
            SMART_HELMET_WISUN_UART_TX_PIO,
            SMART_HELMET_WISUN_UART_RX_PIO);
    return TRUE;
#endif
}

void SmartHelmet_UartClose(void)
{
    SmartHelmet_UartStopVerify();
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

void SmartHelmet_UartStartVerify(void)
{
#if !SMART_HELMET_ENABLE_WISUN_UART || !SMART_HELMET_ENABLE_WISUN_LINK_CHECK
    return;
#else
    memset(&sh_wisun, 0, sizeof(sh_wisun));
    sh_wisun_step = 1;
    sh_rx_asm_len = 0;
    CC_LOGN("SmartHelmet WS8856: link check start (param, then ip)");
    shWisunArm(SMART_HELMET_WISUN_LINK_BOOT_MS);
#endif
}

void SmartHelmet_UartStopVerify(void)
{
    if (sh_uart_task)
    {
        MessageCancelAll(sh_uart_task, SMART_HELMET_WISUN_LINK_CHECK);
    }
    sh_wisun_step = 0;
}

const smart_helmet_wisun_status_t *SmartHelmet_UartGetStatus(void)
{
    return &sh_wisun;
}

static void shWisunOnTimeout(void)
{
#if SMART_HELMET_ENABLE_WISUN_UART && SMART_HELMET_ENABLE_WISUN_LINK_CHECK
    if (sh_wisun_step == 1)
    {
        if (sh_wisun.module_seen)
        {
            sh_wisun_step = 2;
            sh_rx_asm_len = 0;
            shWisunSendProbe("ip");
            shWisunArm(SMART_HELMET_WISUN_LINK_TIMEOUT_MS);
            return;
        }
        if (sh_wisun.tries >= SMART_HELMET_WISUN_LINK_RETRIES)
        {
            sh_wisun_step = 3;
            shWisunLogResult();
            return;
        }
        sh_wisun.tries++;
        sh_rx_asm_len = 0;
        shWisunSendProbe("param");
        shWisunArm(SMART_HELMET_WISUN_LINK_TIMEOUT_MS);
        return;
    }

    if (sh_wisun_step == 2)
    {
        sh_wisun_step = 3;
        shWisunLogResult();
    }
#else
    UNUSED(shWisunSendProbe);
#endif
}

bool SmartHelmet_UartHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (id == SMART_HELMET_WISUN_LINK_CHECK)
    {
        shWisunOnTimeout();
        return TRUE;
    }

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
        if (ptr)
        {
            uint16 copy = size;

            if (sh_rx_asm_len + copy > SMART_HELMET_WISUN_RX_BUF_SIZE)
            {
                copy = (uint16)(SMART_HELMET_WISUN_RX_BUF_SIZE - sh_rx_asm_len);
            }
            if (copy)
            {
                memcpy(sh_rx_asm + sh_rx_asm_len, ptr, copy);
                sh_rx_asm_len = (uint16)(sh_rx_asm_len + copy);
            }
            shWisunNoteRx(ptr, size);
            shLogHex("RX", ptr, size);
            shLogRxShape(ptr, size);
            if (sh_uart_rx_cb)
            {
                sh_uart_rx_cb(ptr, size, sh_uart_rx_ctx);
            }
            else
            {
                CC_LOGN("SmartHelmet UART: RX %u bytes line=%s",
                        size, sh_wisun.last_line);
            }
            if (sh_wisun_step == 1 && sh_wisun.module_seen)
            {
                sh_wisun_step = 2;
                shWisunSendProbe("ip");
                shWisunArm(SMART_HELMET_WISUN_LINK_TIMEOUT_MS);
            }
            else if (sh_wisun_step == 2 && sh_wisun.ip_seen)
            {
                sh_wisun_step = 3;
                shWisunLogResult();
                if (sh_uart_task)
                {
                    MessageCancelAll(sh_uart_task, SMART_HELMET_WISUN_LINK_CHECK);
                }
            }
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
