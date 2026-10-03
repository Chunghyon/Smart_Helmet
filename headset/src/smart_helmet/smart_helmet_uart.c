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
static uint8 sh_wisun_step;          /* 0 idle, 1 reset wait, 2 AT cmd, 3 done */
static uint8 sh_at_idx;
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

static void shLogBytes(const char *tag, const uint8 *data, uint16 len)
{
    uint16 off = 0;

    UNUSED(tag);
    if (!data)
    {
        return;
    }
    if (len > 36)
    {
        len = 36;
    }
    /* CC_LOGN formats %s later, so a stack string is already gone.
     * Print raw bytes as numbers. */
    while (off < len)
    {
        uint8 b[8];
        uint16 i;
        uint16 n = (uint16)(len - off);

        if (n > 8)
        {
            n = 8;
        }
        for (i = 0; i < 8; i++)
        {
            b[i] = (i < n) ? data[off + i] : 0;
        }
        CC_LOGN("SmartHelmet UART bytes +%u n=%u %u %u %u %u %u %u %u %u",
                off, n, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
        off = (uint16)(off + n);
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

    /* Keep CR/LF as '|' so the log viewer does not cut the line at a space
     * or stop on the first newline. Silent Smart replies are multi-line. */
    for (i = 0; i < len && n + 1 < sizeof(sh_wisun.last_line); i++)
    {
        uint8 c = data[i];
        if (c == '\r' || c == '\n')
        {
            sh_wisun.last_line[n++] = '|';
        }
        else if (c == ' ')
        {
            sh_wisun.last_line[n++] = '_';
        }
        else
        {
            sh_wisun.last_line[n++] = (c >= 32 && c < 127) ? (char)c : '.';
        }
    }
    sh_wisun.last_line[n] = '\0';
}

static bool shIpv6Like(const uint8 *data, uint16 len)
{
    uint16 i;
    uint8 colons = 0;
    uint8 hex = 0;

    for (i = 0; i < len; i++)
    {
        uint8 c = data[i];
        if (c == ':')
        {
            colons++;
        }
        else if ((c >= '0' && c <= '9') ||
                 (c >= 'a' && c <= 'f') ||
                 (c >= 'A' && c <= 'F'))
        {
            hex++;
        }
    }
    return colons >= 2 && hex >= 4;
}

static void shNoteStatus(const uint8 *data, uint16 len)
{
    uint16 i;

    for (i = 0; i + 6 < len; i++)
    {
        if (shContainsFold(data + i, (uint16)(len - i), "status"))
        {
            uint16 j = (uint16)(i + 6);
            while (j < len && (data[j] < '0' || data[j] > '9'))
            {
                j++;
                if (j > i + 12)
                {
                    break;
                }
            }
            if (j < len && data[j] >= '0' && data[j] <= '9')
            {
                sh_wisun.status_code = (uint8)(data[j] - '0');
                if (sh_wisun.status_code == 5)
                {
                    sh_wisun.online = TRUE;
                }
            }
            return;
        }
    }
}

static void shWisunNoteRx(const uint8 *data, uint16 len)
{
    if (!data || !len)
    {
        return;
    }
    sh_wisun.rx_seen = TRUE;
    shCopyPreview(data, len);

    /* WS8856FLS / WS8854 family CLI: "param" returns role, status, PHY.
     * status 5 means the routing node is online. "ip" returns an IPv6. */
    if (shContainsFold(data, len, "8856") ||
        shContainsFold(data, len, "role") ||
        shContainsFold(data, len, "status") ||
        shContainsFold(data, len, "phy") ||
        shContainsFold(data, len, "domain"))
    {
        sh_wisun.module_seen = TRUE;
        shNoteStatus(data, len);
    }
    if (sh_wisun_step == 2 && shIpv6Like(data, len))
    {
        sh_wisun.ip_seen = TRUE;
    }
    if (shContainsFold(data, len, "udpr"))
    {
        CC_LOGN("SmartHelmet WS8856: udpr data line=%s", sh_wisun.last_line);
    }
}

static void shWisunLogResult(void)
{
    if (sh_wisun.module_seen)
    {
        sh_wisun.result = smart_helmet_wisun_module_ok;
        CC_LOGN("SmartHelmet WS8856 proto ok tx=%u ip=%u online=%u st=%u try=%u line=%s",
                sh_wisun.tx_ok, sh_wisun.ip_seen, sh_wisun.online,
                sh_wisun.status_code, sh_wisun.tries, sh_wisun.last_line);
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
    uint8 frame[16];
    uint16 len = 0;

    while (cmd[len] != '\0' && len + 2 < sizeof(frame))
    {
        frame[len] = (uint8)cmd[len];
        len++;
    }
    frame[len++] = '\r';
    frame[len++] = '\n';
    /* One flush. A split "param" then CR-LF is parsed as
     * "Command have no CR-LF" / "invalid cmd". */
    sh_wisun.tx_ok = SmartHelmet_UartSend(frame, len);
    CC_LOGN("SmartHelmet WS8856: send %s tx=%u try=%u baud=115200 8N1",
            cmd, sh_wisun.tx_ok, sh_wisun.tries);
    shLogBytes("TX", frame, len);
}

/* Read-only probes from AT_CommandTXT. expect=NULL accepts any non-invalid reply. */
static const struct
{
    const char *cmd;
    const char *expect;
} sh_at_cmds[] =
{
    { "version",  0 },
    { "role",     "router" },
    { "param",    "status" },
    { "mac",      ":" },
    { "ip",       0 },
    { "fstat",    0 },
    { "domain",   0 },
    { "cca",      0 },
    { "txpower",  0 },
    { "pan",      0 },
    { "chrate",   0 },
    { "chconfig", 0 },
    { "neighbor", 0 }
};

#define SH_AT_CMD_COUNT  ((uint8)(sizeof(sh_at_cmds) / sizeof(sh_at_cmds[0])))
#define SH_STEP_RESET    (1)
#define SH_STEP_CMD      (2)
#define SH_STEP_DONE     (3)

static void shAtCopyCmd(const char *cmd)
{
    uint8 i = 0;

    while (cmd[i] != '\0' && i + 1 < sizeof(sh_wisun.at_cmd))
    {
        sh_wisun.at_cmd[i] = cmd[i];
        i++;
    }
    sh_wisun.at_cmd[i] = '\0';
}

static bool shAtReplyOk(void)
{
    const char *cmd = sh_at_cmds[sh_at_idx].cmd;
    const char *expect = sh_at_cmds[sh_at_idx].expect;
    uint16 i;
    uint16 printable = 0;
    uint16 cmd_len = 0;

    if (sh_at_idx >= SH_AT_CMD_COUNT)
    {
        return FALSE;
    }
    if (shContainsFold(sh_rx_asm, sh_rx_asm_len, "invalid"))
    {
        return FALSE;
    }
    /* WS8856 answers "<OK" before or with the payload. Echo alone is not enough. */
    if (shContainsFold(sh_rx_asm, sh_rx_asm_len, "ok"))
    {
        return TRUE;
    }
    if (!strcmp(cmd, "ip"))
    {
        return shIpv6Like(sh_rx_asm, sh_rx_asm_len);
    }
    if (!strcmp(cmd, "role"))
    {
        return shContainsFold(sh_rx_asm, sh_rx_asm_len, "router") ||
               shContainsFold(sh_rx_asm, sh_rx_asm_len, "border");
    }
    if (expect && !shContainsFold((const uint8 *)cmd, (uint16)strlen(cmd), expect))
    {
        return shContainsFold(sh_rx_asm, sh_rx_asm_len, expect);
    }
    while (cmd[cmd_len] != '\0')
    {
        cmd_len++;
    }
    for (i = 0; i < sh_rx_asm_len; i++)
    {
        uint8 c = sh_rx_asm[i];
        if (c >= 32 && c < 127)
        {
            printable++;
        }
    }
    return printable > (uint16)(cmd_len + 2);
}

static void shAtFinish(void)
{
    sh_wisun_step = SH_STEP_DONE;
    sh_wisun.at_ok = (sh_wisun.at_fail == 0 && sh_wisun.at_pass > 0 &&
                      (!SMART_HELMET_WISUN_AT_RESET_FIRST || sh_wisun.reset_seen));
    if (sh_wisun.at_ok || sh_wisun.module_seen)
    {
        sh_wisun.result = smart_helmet_wisun_module_ok;
    }
    CC_LOGN("SmartHelmet AT: done reset=%u mode=%u pass=%u fail=%u at_ok=%u line=%s",
            sh_wisun.reset_seen, sh_wisun.at_mode, sh_wisun.at_pass,
            sh_wisun.at_fail, sh_wisun.at_ok, sh_wisun.last_line);
    shWisunLogResult();
    if (sh_uart_task)
    {
        MessageCancelAll(sh_uart_task, SMART_HELMET_WISUN_LINK_CHECK);
    }
}

static void shAtSendCurrent(void)
{
    if (sh_at_idx >= SH_AT_CMD_COUNT)
    {
        shAtFinish();
        return;
    }
    sh_rx_asm_len = 0;
    shAtCopyCmd(sh_at_cmds[sh_at_idx].cmd);
    sh_wisun_step = SH_STEP_CMD;
    shWisunSendProbe(sh_at_cmds[sh_at_idx].cmd);
    shWisunArm(SMART_HELMET_WISUN_LINK_TIMEOUT_MS);
}

static void shAtNoteResult(bool pass)
{
    CC_LOGN("SmartHelmet AT: %s %s line=%s",
            sh_wisun.at_cmd, pass ? "PASS" : "FAIL", sh_wisun.last_line);
    if (pass)
    {
        if (sh_wisun.at_pass < 0xff)
        {
            sh_wisun.at_pass++;
        }
    }
    else if (sh_wisun.at_fail < 0xff)
    {
        sh_wisun.at_fail++;
    }
    sh_at_idx++;
    shAtSendCurrent();
}

static void shResetNoteBanner(void)
{
    if (shContainsFold(sh_rx_asm, sh_rx_asm_len, "AT Command mode"))
    {
        sh_wisun.at_mode = TRUE;
    }
    if (shContainsFold(sh_rx_asm, sh_rx_asm_len, "Router start"))
    {
        sh_wisun.reset_seen = TRUE;
        CC_LOGN("SmartHelmet AT: Router start (mode=%u) — probing AT_CommandTXT",
                sh_wisun.at_mode);
        sh_at_idx = 0;
        shAtSendCurrent();
    }
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
    sh_wisun.status_code = 0xFF;
    sh_at_idx = 0;
    sh_rx_asm_len = 0;
#if SMART_HELMET_WISUN_AT_RESET_FIRST
    sh_wisun_step = SH_STEP_RESET;
    CC_LOGN("SmartHelmet AT: reset, then wait for Router start");
    shWisunArm(200);
#else
    CC_LOGN("SmartHelmet AT: skip reset, probe AT_CommandTXT");
    shAtSendCurrent();
#endif
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
    if (sh_wisun_step == SH_STEP_RESET)
    {
        if (sh_wisun.reset_seen)
        {
            return;
        }
        if (sh_wisun.tries == 0)
        {
            sh_wisun.tries = 1;
            sh_rx_asm_len = 0;
            shWisunSendProbe("reset");
            shWisunArm(SMART_HELMET_WISUN_RESET_WAIT_MS);
            return;
        }
        if (sh_wisun.tries < SMART_HELMET_WISUN_RESET_RETRIES)
        {
            sh_wisun.tries++;
            CC_LOGN("SmartHelmet AT: no Router start, reset retry %u",
                    sh_wisun.tries);
            sh_rx_asm_len = 0;
            shWisunSendProbe("reset");
            shWisunArm(SMART_HELMET_WISUN_RESET_WAIT_MS);
            return;
        }
        CC_LOGN("SmartHelmet AT: FAIL no Router start after reset");
        sh_wisun.at_fail++;
        shAtFinish();
        return;
    }

    if (sh_wisun_step == SH_STEP_CMD)
    {
        shAtNoteResult(shAtReplyOk());
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
            shLogBytes("RX", ptr, size);
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
            if (sh_wisun_step == SH_STEP_RESET)
            {
                shResetNoteBanner();
            }
            else if (sh_wisun_step == SH_STEP_CMD && shAtReplyOk())
            {
                shAtNoteResult(TRUE);
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
