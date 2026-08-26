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

/*! \brief Handle stream messages (MESSAGE_MORE_DATA / MESSAGE_MORE_SPACE). */
bool SmartHelmet_UartHandleMessage(Task task, MessageId id, Message message);

#endif /* SMART_HELMET_UART_H */
