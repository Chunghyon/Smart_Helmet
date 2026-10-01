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

enum
{
    /*! WS8856FLS UART identity / link retry. */
    SMART_HELMET_WISUN_LINK_CHECK = 0x5200
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
    bool   module_seen;   /* "8856" or param fields */
    bool   ip_seen;       /* IPv6-looking reply after ip */
    uint8  tries;
    char   last_line[48];
} smart_helmet_wisun_status_t;

/*! \brief Send param/ip and log whether WS8856FLS answers. No-op if UART off. */
void SmartHelmet_UartStartVerify(void);

/*! \brief Cancel a pending link-check timer. */
void SmartHelmet_UartStopVerify(void);

const smart_helmet_wisun_status_t *SmartHelmet_UartGetStatus(void);

#endif /* SMART_HELMET_UART_H */
