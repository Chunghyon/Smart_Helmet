/*!
\copyright  Copyright (c) 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      SPI SRAM interfaces - example of streaming data to/from
            a peripheral device using Bitserial traps.
*/

#ifndef SPI_SRAM_23LC1024_H
#define SPI_SRAM_23LC1024_H

typedef void (*spi_sram_cb_t)(void *cb_context, bool result);

bool spi_sram_23lc1024_init(void);
void spi_sram_23lc1024_close(void);

bool spi_sram_23lc1024_write(size_t offset, size_t size, const uint8 *data_ptr,
                             spi_sram_cb_t cb, void *cb_context);
bool spi_sram_23lc1024_read(size_t offset, size_t size, uint8 *data_ptr,
                            spi_sram_cb_t cb, void *cb_context);

#endif /* SPI_SRAM_23LC1024_H */
