/*!
\file       smart_helmet_uart.h
\brief      UART interface to Wi-SUN module (TXD / RXD)
*/

#ifndef SMART_HELMET_UART_H
#define SMART_HELMET_UART_H

#include <csrtypes.h>
#include <message.h>
#include <stdbool.h>

typedef void (*smart_helmet_uart_rx_cb_t)(const uint8 *data, uint16 len, void *ctx);

/*! Last UART line copied for CC_LOGN %s. Not a firmware string literal. */
extern char rx_str[48];

/*! \brief Configure PIO mux and open Stream UART. */
bool SmartHelmet_UartInit(Task client_task);

/*! \brief Close UART stream. */
void SmartHelmet_UartClose(void);

/*! \brief Transmit bytes to Wi-SUN module. */
bool SmartHelmet_UartSend(const uint8 *data, uint16 len);

/*! \brief Register RX callback (optional; also emits via task MORE_DATA). */
void SmartHelmet_UartSetRxCallback(smart_helmet_uart_rx_cb_t cb, void *ctx);

/*! \brief Handle stream messages (MESSAGE_MORE_DATA / MESSAGE_MORE_SPACE)
 *  and SMART_HELMET_WISUN_LINK_CHECK. */
bool SmartHelmet_UartHandleMessage(Task task, MessageId id, Message message);

void SmartHelmet_ReportStop(void);

enum
{
    /*! WS8856FLS UART identity / link retry. */
    SMART_HELMET_WISUN_LINK_CHECK = 0x5200,
    /*! Periodic UART source drain. Logs RX with %c, not %s. */
    SMART_HELMET_WISUN_RX_POLL = 0x5201
};

typedef enum
{
    smart_helmet_wisun_idle = 0,
    smart_helmet_wisun_tx_only,     /* TX flushed, no RX yet */
    smart_helmet_wisun_rx_unknown,  /* RX bytes, not a WS8856 CLI reply */
    smart_helmet_wisun_module_ok    /* param/ip reply from WS8856 family */
} smart_helmet_wisun_link_t;

typedef struct
{
    smart_helmet_wisun_link_t result;
    bool   tx_ok;
    bool   rx_seen;
    bool   module_seen;   /* Silent Smart CLI: role/status/phy/8856 */
    bool   ip_seen;       /* IPv6 from ip command, not a lone colon */
    bool   online;        /* param status field is 5 (routing node up) */
    bool   reset_seen;    /* boot banner contained "Router start" */
    bool   at_mode;       /* boot banner contained "AT Command mode" */
    bool   at_ok;         /* every probed read command from AT_CommandTXT passed */
    uint8  at_pass;
    uint8  at_fail;
    uint8  status_code;   /* 0xFF if no status digit yet */
    uint8  tries;
    char   at_cmd[12];    /* command in flight, or last finished */
    char   last_line[48];
} smart_helmet_wisun_status_t;

/*! \brief reset, wait for "Router start", then probe read commands from AT_CommandTXT.
 *  Headset power-off does not reset the module. StartVerify sends exit first
 *  so a leftover passthrough session cannot swallow reset. */
void SmartHelmet_UartStartVerify(void);

/*! \brief True only after +++ was accepted. Report must not be sent otherwise. */
bool SmartHelmet_UartInPassthrough(void);

/*! \brief Leave passthrough and stop the link check. Module stays powered. */
void SmartHelmet_UartSleep(void);

/*! \brief Cancel a pending link-check timer. */
void SmartHelmet_UartStopVerify(void);

const smart_helmet_wisun_status_t *SmartHelmet_UartGetStatus(void);

#endif /* SMART_HELMET_UART_H */
