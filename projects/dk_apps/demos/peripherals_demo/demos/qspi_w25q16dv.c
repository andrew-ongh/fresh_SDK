/**
 ****************************************************************************************
 *
 * @file qspi_w25q16dv.c
 *
 * @brief Implementation of access W25Q16DV flash memory using QSPI
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <hw_qspi.h>
#include "w25q16dv.h"

/**
 * \brief Send and receive data over SPI
 *
 * \param [in] wbuf data to write
 * \parma [in] wlen number of bytes to write
 * \param [out] rbuf buffer to read data
 * \param [in] rlen size of buffer
 *
 */
void qspi_transact(const uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen)
{
        size_t i;

        hw_qspi_cs_enable();

        for (i = 0; i < wlen; ++i) {
                hw_qspi_write8(wbuf[i]);
        }

        for (i = 0; i < rlen; ++i) {
                rbuf[i] = hw_qspi_read8();
        }

        hw_qspi_cs_disable();
}

/**
 * \brief Send data over SPI
 *
 * \param [in] wbuf data to write
 * \parma [in] wlen number of bytes to write
 *
 */
void qspi_write(const uint8_t *wbuf, size_t wlen)
{
        size_t i;

        hw_qspi_cs_enable();

        for (i = 0; i < wlen; ++i) {
                hw_qspi_write8(wbuf[i]);
        }

        hw_qspi_cs_disable();
}

uint8_t w25q_read_status_register_1(void)
{
        uint8_t status;
        static const uint8_t cmd[] = { W25Q_READ_STATUS_REGISTER1 };

        qspi_transact(cmd, 1, &status, 1);

        return status;
}

uint8_t w25q_read_status_register_2(void)
{
        static const uint8_t cmd[] = { W25Q_READ_STATUS_REGISTER2 };
        uint8_t status;

        qspi_transact(cmd, 1, &status, 1);

        return status;
}

void w25q_write_enable(void)
{
        static const uint8_t cmd[] = { W25Q_WRITE_ENABLE };

        qspi_write(cmd, 1);
}

void w25q_read(uint32_t addr, uint8_t *buf, uint16_t size)
{
        uint8_t cmd[5] = { W25Q_FAST_READ, addr >> 16, addr >>  8, addr, 0 };

        qspi_transact(cmd, 5, buf, size);
}

void w25q_erase_sector(uint32_t addr)
{
        uint8_t cmd[4] = { W25Q_SECTOR_ERASE, addr >> 16, addr >>  8, addr };

        w25q_write_enable();

        qspi_write(cmd, 4);
}

void w25q_write_page(uint32_t addr, const uint8_t *buf, uint16_t size)
{
        size_t i;

        w25q_write_enable();

        hw_qspi_cs_enable();

        hw_qspi_write8(W25Q_PAGE_PROGRAM);
        hw_qspi_write8((uint8_t) (addr >> 16));
        hw_qspi_write8((uint8_t) (addr >>  8));
        hw_qspi_write8((uint8_t) (addr >>  0));

        for (i = 0; i < size; ++i) {
                hw_qspi_write8(buf[i]);
        }

        hw_qspi_cs_disable();
}

void w25q_quad_enable(void)
{
        uint8_t r1;
        uint8_t r2;

        r1 = w25q_read_status_register_1();
        r2 = w25q_read_status_register_2();

        /*
         * Check if quad is already enabled, if so no need to do it again
         */
        if ((r2 & W25Q_STATUS2_QE) == 0) {
                w25q_write_enable();

                hw_qspi_cs_enable();
                hw_qspi_write8(W25Q_WRITE_STATUS_REGISTER);
                hw_qspi_write8(r1);
                hw_qspi_write8(r2 | W25Q_STATUS2_QE);
                hw_qspi_cs_disable();
        }
}

void w25_reset_continuous_mode(void)
{
        hw_qspi_cs_enable();
        hw_qspi_write16(0xFFFF);
        hw_qspi_cs_disable();
}

void w25q_power_down(void)
{
        hw_qspi_cs_enable();
        hw_qspi_write8(W25Q_POWER_DOWN);
        hw_qspi_cs_disable();
}

void w25q_release_power_down(void)
{
        hw_qspi_cs_enable();
        hw_qspi_write8(W25Q_RELEASE_POWER_DOWN);
        hw_qspi_cs_disable();
}

uint16_t w25q_read_id(void)
{
        uint16_t id;

        hw_qspi_cs_enable();
        hw_qspi_write8(W25Q_MANUFACTURER_ID);
        hw_qspi_write16(0);
        hw_qspi_write8(0);
        id = hw_qspi_read16();
        hw_qspi_cs_disable();

        return id;
}
