/*!
\file       smart_helmet.h
\brief      Smart Helmet sensor-hub interface (I2C x2, ADC x4, Wi-SUN UART)

Schematic: SMART_HELMET_260816_1.DSN
  - I2C0 SCL/SDA : CJMCU-8118, LIS3DH, GY-906-BAA, SSD1315(opt)
  - I2C1 SCL1/SDA1 : SSD1315(opt)
  - ADC : SENS_IN, CO, NH3, NO2
  - UART : Wi-SUN TXD/RXD
*/

#ifndef SMART_HELMET_H
#define SMART_HELMET_H

#include <message.h>
#include <stdbool.h>
#include "smart_helmet_config.h"
#include "smart_helmet_i2c.h"
#include "smart_helmet_adc.h"
#include "smart_helmet_uart.h"
#include "smart_helmet_vitals.h"

/*! \brief One-time bring-up of all enabled interfaces. */
bool SmartHelmet_Init(Task client_task);

/*! \brief Tear down interfaces. */
void SmartHelmet_Close(void);

/*! \brief Start Wi-SUN and sensor polling. Headset power-on. */
void SmartHelmet_PowerOn(void);

/*! \brief Sleep sensors, stop border-router reports. */
void SmartHelmet_PowerOff(void);

/*! \brief Redraw OLED for the current headset state. */
void SmartHelmet_UiRefresh(void);

/*!
 * \brief Message pump — call from the owner task handler.
 * Consumes ADC + UART messages when they belong to this module.
 */
bool SmartHelmet_HandleMessage(Task task, MessageId id, Message message);

/*! \brief Trigger ADC scan + optional I2C sensor poll. */
void SmartHelmet_PollSensors(void);

/*! \brief Send a frame to the Wi-SUN module. */
bool SmartHelmet_WisunSend(const uint8 *data, uint16 len);

/*! \brief Run vitals proxy once (after accel/SENS samples were pushed). */
void SmartHelmet_VitalsTick(void);

/*! \brief Start the 5 s passthrough report. Called after +++ is accepted. */
void SmartHelmet_ReportStart(void);

enum
{
    SMART_HELMET_REPORT_TICK = 0x5300
};

#endif /* SMART_HELMET_H */
