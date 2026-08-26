/*!
\file       smart_helmet_i2c.h
\brief      Dual I2C master interface (Bitserial) for Smart Helmet sensors
*/

#ifndef SMART_HELMET_I2C_H
#define SMART_HELMET_I2C_H

#include <csrtypes.h>
#include <stdbool.h>

typedef enum
{
    smart_helmet_i2c_bus_0 = 0,  /*!< SCL/SDA */
    smart_helmet_i2c_bus_1 = 1,  /*!< SCL1/SDA1 */
    smart_helmet_i2c_bus_count
} smart_helmet_i2c_bus_t;

/*! \brief Initialise enabled I2C buses (PIO mux + Bitserial open). */
bool SmartHelmet_I2cInit(void);

/*! \brief Close I2C buses. */
void SmartHelmet_I2cClose(void);

/*!
 * \brief Combined write-then-read on a 7-bit addressed slave (blocking).
 * \param bus       I2C0 or I2C1
 * \param addr7     7-bit slave address
 * \param tx        bytes to write first (may be NULL if tx_len==0)
 * \param tx_len    write length
 * \param rx        buffer for read (may be NULL if rx_len==0)
 * \param rx_len    read length
 * \return TRUE on success
 */
bool SmartHelmet_I2cTransfer(smart_helmet_i2c_bus_t bus,
                             uint8 addr7,
                             const uint8 *tx, uint16 tx_len,
                             uint8 *rx, uint16 rx_len);

/*! \brief Write only. */
bool SmartHelmet_I2cWrite(smart_helmet_i2c_bus_t bus,
                          uint8 addr7,
                          const uint8 *tx, uint16 tx_len);

/*! \brief Read only (no reg address phase). */
bool SmartHelmet_I2cRead(smart_helmet_i2c_bus_t bus,
                         uint8 addr7,
                         uint8 *rx, uint16 rx_len);

/*! \brief Write 8-bit register address then read. */
bool SmartHelmet_I2cReadReg(smart_helmet_i2c_bus_t bus,
                            uint8 addr7, uint8 reg,
                            uint8 *rx, uint16 rx_len);

/*! \brief Write 8-bit register address + payload. */
bool SmartHelmet_I2cWriteReg(smart_helmet_i2c_bus_t bus,
                             uint8 addr7, uint8 reg,
                             const uint8 *tx, uint16 tx_len);

#endif /* SMART_HELMET_I2C_H */
