/**
 ****************************************************************************************
 *
 * @file at45db011d.c
 *
 * @brief Implementation of access AT45DB011 flash memory
 *
 * Copyright (C) 2015-2018 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <platform_devices.h>
#include <osal.h>
#include <ad_spi.h>
#include "at45db011d.h"

#if SPI_ASYNC_ACTIONS_SIZE < 13
#pragma message SPI_ASYNC_ACTIONS_SIZE must be at least 13 for this driver asynchronous functions
#endif

static bool page_size_256;

/*
 * Prepare address for commands.
 * For 256 byte page size, linear address is just used in addressing.
 * For 264 byte page size, page number starts from 9th bit lower 9 bits should never exceed 264.
 */
static uint32_t at45_page_addr(uint32_t addr)
{
        if (page_size_256) {
                return addr;
        }
        return ((addr / 264) << 9) | (addr % 264);
}

/*
 * Fill buffer with command and address
 */
static void prepare_cmd(uint8_t cmd, uint32_t addr, uint8_t *buf)
{
        const uint32_t page_addr = at45_page_addr(addr);
        buf[0] = cmd;
        buf[1] = page_addr >> 16;
        buf[2] = page_addr >> 8;
        buf[3] = page_addr;
}

uint8_t at45db_read_status_register(spi_device dev)
{
        const uint8_t cmd = AT45DB_READ_STATUS_REGISTER;
        uint8_t status;

        /*
         * Read status is simple one byte command with one byte response.
         */
        ad_spi_transact(dev, &cmd, sizeof(cmd), &status, 1);

        /*
         * Update page_size on every read status.
         */
        page_size_256 = (status & AT45DB_STATUS_PAGE_SIZE_256) != 0;

        return status;
}

void at45db_read(spi_device dev, uint16_t addr, uint8_t *buf, uint16_t size)
{
        /* Buffer for command address and one dummy byte */
        uint8_t cmd[5];

        prepare_cmd(AT45DB_CONTINUOUS_ARRAY_READ, addr, cmd);

        /*
         * Read command used here can read across pages, so there is not need to do
         * anything more that single SPI transaction. This transaction will start with allocation
         * flash memory for this task usage, then allocation of SPI bus, then chip select will be
         * activated, command with address and dummy byte will be send, next data will be read back
         * from flash.
         */
        ad_spi_transact(dev, cmd, sizeof(cmd), buf, size);
}

static inline void cmd_send_cb(void *user_data)
{
        *((bool *) user_data) = true;
}

void at45db_read_async(spi_device dev, uint16_t addr, uint8_t *buf, uint16_t size,
                                                                ad_spi_user_cb cb, void *user_data)
{
        /* Buffer for command address and one dummy byte */
        uint8_t cmd[5];
        volatile bool cmd_sent = false;

        /*
         * Prepare command with address.
         */
        prepare_cmd(AT45DB_CONTINUOUS_ARRAY_READ, addr, cmd);

        /*
         * Build transaction that will send command to flash then fire callback.
         * This in transaction callback is used to stay in this function while command buffer is
         * not sent. After sending command function will return to caller and actual reading
         * will be done in background.
         */
        ad_spi_async_transact(dev, SPI_CSA,
                                SPI_SND(cmd, 5),
                                SPI_CB1(cmd_send_cb, &cmd_sent),
                                SPI_RCV(buf, size),
                                SPI_CB1(cb, user_data),
                                SPI_CSD,
                                SPI_END);
        /*
         * Wait for 5 bytes to be put in FIFO, then command buffer can be discarded and function
         * can return.
         */
        while (!cmd_sent) {
                OS_DELAY(0);
        }
}

void at45db_wait_while_busy(spi_device dev)
{
        /*
         * Read status flash register in loop waiting for ready state.
         */
        while (!(at45db_read_status_register(dev) & AT45DB_STATUS_READY)) {
                /*
                 * Give other tasks some time.
                 */
                OS_DELAY(0);
        }
}

void at45db_read_memory_to_buffer(spi_device dev, uint32_t addr)
{
        uint8_t cmd[4];

        prepare_cmd(AT45DB_MEMORY_2_BUFFER, addr, cmd);
        ad_spi_write(dev, cmd, 4);
}

void at45db_write(spi_device dev, uint32_t addr, const uint8_t *buf, uint16_t size)
{
        const int ps = page_size_256 ? 256 : 264;
        uint8_t cmd[4];
        const uint8_t *p = buf;
        int n;
        int left = size;

        /*
         * Get exclusive access to flash memory.
         * Other devices on SPI lines can be used in between flash commands.
         */
        ad_spi_device_acquire(dev);

        while (left) {
                /*
                 * Starting offset in page.
                 */
                uint16_t offset = addr % ps;

                /*
                 * Write at most page size at a time. If offset is 0 write exactly page size.
                 */
                n = ps - offset;

                /*
                 * If user wants to write less then page, or less then page is left to write
                 * correct size.
                 */
                if (n > left) {
                        n = left;
                }

                /*
                 * If less then page write is needed. First read current data to flash buffer.
                 */
                if (n < ps) {
                        /*
                         * Read main flash memory into flash internal buffer.
                         * It can take some time so wait for status to return ready flag.
                         */
                        prepare_cmd(AT45DB_MEMORY_2_BUFFER, addr, cmd);
                        ad_spi_write(dev, cmd, 4);
                        at45db_wait_while_busy(dev);
                }

                /*
                 * Write main memory through buffer using complex transaction which
                 * allows sending two chunks of data without copying it to single buffer first.
                 */
                at45db_write_page(dev, addr, p, n);

                /*
                 * Writing can take some time, so wait in OS friendly way for write to complete.
                 */
                at45db_wait_while_busy(dev);

                p += n;
                left -= n;
                addr += n;
        }

        /*
         * Release access to flash memory.
         */
        ad_spi_device_release(dev);
}

void at45db_erase_page(spi_device dev, uint32_t addr)
{
        uint8_t cmd[4];

        prepare_cmd(AT45DB_PAGE_ERASE, addr, cmd);

        ad_spi_write(dev, cmd, sizeof(cmd));
}

void at45db_write_page(spi_device dev, uint32_t addr, const uint8_t *buf, uint16_t size)
{
        uint8_t cmd[4];
        spi_transfer_data transfers[2] = { {0}, {0} };

        prepare_cmd(AT45DB_PAGE_PROGRAM_THROUGH_BUFFER, addr, cmd);
        transfers[0].wbuf = cmd;
        transfers[0].length = 4;
        transfers[1].wbuf = buf;
        transfers[1].length = (size_t) size;
        ad_spi_complex_transact(dev, transfers, 2);

}

void at45db_write_page_async(spi_device dev, uint32_t addr, const uint8_t *buf, uint16_t size,
                                                                ad_spi_user_cb cb, void *user_data)
{
        uint8_t cmd[4];
        volatile bool cmd_sent = false;

        prepare_cmd(AT45DB_PAGE_PROGRAM_THROUGH_BUFFER, addr, cmd);

        /*
         * Prepare async transaction with extra callback after command is send.
         * This extra callback will allow to exit function discarding memory used for command.
         * User callback will be called after all data has been sent.
         */
        ad_spi_async_transact(dev, SPI_CSA,
                                SPI_SND(cmd, sizeof(cmd)),
                                SPI_CB1(cmd_send_cb, &cmd_sent),
                                SPI_SND(buf, size),
                                SPI_CB1(cb, user_data),
                                SPI_CSD,
                                SPI_END);
        /*
         * Wait for 4 bytes to be put in FIFO, then command buffer can be discarded and function
         * can return.
         */
        while (!cmd_sent) {
                OS_DELAY(0);
        }
}
