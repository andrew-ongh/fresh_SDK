/**
 ****************************************************************************************
 *
 * @file demo_i2c_spi.c
 *
 * @brief I2C and SPI demo (ad_i2c ad_spi driver)
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdlib.h>
#include <osal.h>
#include <platform_devices.h>
#include <ad_spi.h>
#include <ad_i2c.h>
#include "common.h"
#include "at45db011d.h"
#include "eeprom_24xx256.h"

#if CFG_DEMO_AD_SPI_I2C

static const int TOTAL_LEN = 20000;

/*
 * Function demonstrates synchronous access to I2C and SPI buses.
 * Function copies TOTAL_LEN bytes read from SPI FLASH and writes them to I2C EEPROM.
 * It uses 64 bytes (page size of EEPROM) of heap to store data between reads and writes.
 */
void menu_ad_spi_i2c_copy_sync_func(const struct menu_item *m, bool checked)
{
        uint8_t *buf = OS_MALLOC_NORET(64);
        int offset = 0;
        spi_device src = ad_spi_open(AT45DB011D);
        i2c_device dst = ad_i2c_open(MEM_24LC256);
        OS_TICK_TIME start;
        OS_TICK_TIME stop;

        start = OS_GET_TICK_COUNT();

        /*
         * Continue till all data is written.
         */
        while (offset < TOTAL_LEN) {
                /*
                 * Read and write chunk is limited to EEPORM page size.
                 */
                int chunk = TOTAL_LEN - offset;
                if (chunk > 64) {
                        chunk = 64;
                }

                /*
                 * Synchronously read SPI FLASH.
                 */
                at45db_read(src, offset, buf, (uint16_t) chunk);

                /*
                 * Synchronously write I2C EEPROM.
                 */
                if (!eeprom_24XX256_write(dst, offset, buf, (uint16_t) chunk)) {
                        /*
                         * If there was an error writing EEPROM just abort.
                         */
                        break;
                }
                offset += chunk;
        }

        stop = OS_GET_TICK_COUNT();
        OS_FREE_NORET(buf);

        /*
         * Print copy time.
         */
        uart_printfln(false, "Synchronous copying  %d bytes from FLASH to EEPROM took %u ticks",
                                                                        TOTAL_LEN, stop - start);
        ad_spi_close(src);
        ad_i2c_close(dst);
}

typedef struct {
        uint32_t offset;
        uint16 size;
        uint32 error;
        bool done;
        bool poll_ack;
} transfer_data;

/*
 * Callback for SPI transactions.
 */
static void spi_done_cb(void *user_data)
{
        transfer_data *d = (transfer_data *) user_data;
        d->offset += d->size;
        d->done = true;
}

/*
 * Callback for I2C transactions.
 */
static void i2c_done_cb(void *user_data, HW_I2C_ABORT_SOURCE error)
{
        transfer_data *d = (transfer_data *) user_data;
        if (error == 0) {
                d->offset += d->size;
        }
        d->done = true;
        d->error = error;
}

/*
 * Function demonstrates asynchronous access for I2C and SPI buses.
 * Function copies TOTAL_LEN bytes read from SPI FLASH and writes them to I2C EEPROM.
 * It uses 4 * 64 bytes (4 page of EEPROM) of heap to store data between reads and writes.
 *
 * This code read 64 bytes chunk from FLASH, then as soon as it start writing it asynchronously to
 * EEPROM, it also start reading next chunks from FLASH.
 * This demo does not poll EEPROM to see if next operation can be done it just tries to write,
 * if EEPROM is busy it will just not ACK write transaction and I2C callback will not move offset
 * forward.
 * Following is likely time line: SPI reads - R, I2C writes - W and, I2C failed writes - w
 * SPI FLASH:  RR RR RR RR RR        RR
 * I2C EEPROM:   WWWWWWW w w w WWWWWW w w w WWWWWW w .... WWWWW w w w WWWWWW
 */
void menu_ad_spi_i2c_copy_async_func(const struct menu_item *m, bool checked)
{
        const int BUF_SIZE = 4 * 64;
        uint8_t *buf = OS_MALLOC_NORET(BUF_SIZE);
        volatile transfer_data spi_data = {0};
        volatile transfer_data i2c_data = {0};
        spi_device src = ad_spi_open(AT45DB011D);
        i2c_device dst = ad_i2c_open(MEM_24LC256);
        OS_TICK_TIME start;
        OS_TICK_TIME stop;

        start = OS_GET_TICK_COUNT();
        i2c_data.done = true; /* true means that next transaction on I2C can start */
        spi_data.done = true; /* true means that next transaction on I2C can start */

        /*
         * Loop till all is written.
         */
        while (i2c_data.offset < TOTAL_LEN) {
                /*
                 * Is I2C is not busy writing and there is some data not written data.
                 */
                if (i2c_data.done && i2c_data.offset < spi_data.offset) {
                        i2c_data.done = false;
                        /*
                         * Compute chunk size, no more than EEPROM page size.
                         */
                        i2c_data.size = spi_data.offset - i2c_data.offset;
                        if (i2c_data.size > 64) {
                                i2c_data.size = 64;
                        }

                        i2c_data.error = 0;
                        /*
                         * Write asynchronously chunk.
                         */
                        eeprom_24XX256_write_async(dst, i2c_data.offset,
                                                buf + (i2c_data.offset & 63), i2c_data.size,
                                                                i2c_done_cb, (void *) &i2c_data);
                }
                /*
                 * Check if previous SPI transaction is done, and there is space in cirular buffer.
                 */
                if (spi_data.done && spi_data.offset < TOTAL_LEN &&
                                                spi_data.offset < i2c_data.offset + BUF_SIZE) {
                        spi_data.done = false;
                        /*
                         * Reduce read to 64 bytes.
                         */
                        spi_data.size = TOTAL_LEN - spi_data.offset;
                        if (spi_data.size > 64) {
                                spi_data.size = 64;
                        }
                        /*
                         * Asynchronously read chunk.
                         */
                        at45db_read_async(src, spi_data.offset, buf + (spi_data.offset & 63),
                                                spi_data.size, spi_done_cb, (void *) &spi_data);
                }
                /*
                 * If both SPI and I2C buses are busy, give other task some time.
                 */
                while (!(spi_data.done || i2c_data.done)) {
                        OS_DELAY(0);
                }
        }

        stop = OS_GET_TICK_COUNT();
        OS_FREE_NORET(buf);

        uart_printfln(false, "Asynchronous copying %d bytes from FLASH to EEPROM took %u ticks",
                                                                        TOTAL_LEN, stop - start);
        ad_spi_close(src);
        ad_i2c_close(dst);
}

/*
 * Function demonstrates synchronous access for I2C and SPI buses.
 * Function copies TOTAL_LEN bytes read from I2C EEPROM and writes them to SPI FLASH.
 * It uses 256 or 264 bytes (page of FLASH) of heap to store data between reads and writes.
 */
void menu_ad_i2c_spi_copy_sync_func(const struct menu_item *m, bool checked)
{
        int offset = 0;
        spi_device dst = ad_spi_open(AT45DB011D);
        i2c_device src = ad_i2c_open(MEM_24LC256);
        OS_TICK_TIME start;
        OS_TICK_TIME stop;
        /* FLASH page size can be 256 or 264 bytes, check it before */
        int page_size = at45db_read_status_register(dst) & AT45DB_STATUS_PAGE_SIZE_256 ? 256 : 264;
        uint8_t *buf = OS_MALLOC_NORET(page_size);

        start = OS_GET_TICK_COUNT();

        /*
         * Till all is written.
         */
        while (offset < TOTAL_LEN) {
                int chunk = TOTAL_LEN - offset;
                /*
                 * Chunk size is no more then page size of FLASH.
                 */
                if (chunk > page_size) {
                        chunk = page_size;
                }

                /*
                 * Read from EEPROM.
                 */
                eeprom_24XX256_read(src, offset, buf, (uint16_t) chunk);

                /*
                 * Write to FLASH.
                 */
                at45db_write(dst, offset, buf, (uint16_t) chunk);

                /*
                 * Move forward.
                 */
                offset += page_size;
        }

        stop = OS_GET_TICK_COUNT();
        OS_FREE_NORET(buf);

        uart_printfln(false, "Synchronous copying  %d bytes from EEPROM to FLASH took %u ticks",
                                                                        TOTAL_LEN, stop - start);
        ad_spi_close(dst);
        ad_i2c_close(src);
}

/*
 * Function demonstrates asynchronous access for I2C and SPI buses.
 * Function copies TOTAL_LEN bytes read from I2C EEPROM and writes them to SPI FLASH.
 * It uses 2 * page size of FLASH of heap to store data between reads and writes.
 *
 * This code read 264 bytes chunk from EEPROM, then as soon as it start writing it asynchronously to
 * FLASH, it also start reading next chunks from EEPROM.
 * After writing page, FLASH is not able to process commands for some time, to see if flash can be
 * used again, synchronous read status register is performed.
 *
 * Following is likely time line, I2C reads - R, SPI writes - W, SPI status reads - s
 * I2C EEPROM:  RRRRR RRRRR  RRRRR RRRRR RRRRR
 * SPI FLASH:        WW s s WW s s s WW s s WW s s WWWWW
 */
void menu_ad_i2c_spi_copy_async_func(const struct menu_item *m, bool checked)
{
        volatile transfer_data src_data = {0};
        volatile transfer_data dst_data = {0};
        spi_device dst = ad_spi_open(AT45DB011D);
        i2c_device src = ad_i2c_open(MEM_24LC256);
        OS_TICK_TIME start;
        OS_TICK_TIME stop;
        int page_size = at45db_read_status_register(dst) & AT45DB_STATUS_PAGE_SIZE_256 ? 256 : 264;
        uint8_t *buf = OS_MALLOC_NORET(page_size * 2);
        bool ready = false;

        start = OS_GET_TICK_COUNT();
        dst_data.done = false;
        src_data.done = true;

        /*
         * Till all is written, unless there was error on I2C bus.
         */
        while (dst_data.offset < TOTAL_LEN && src_data.error == 0) {
                /*
                 * If data was written, flash can be busy, read status and update ready flag.
                 */
                if (dst_data.done && !ready) {
                        ready = (at45db_read_status_register(dst) & AT45DB_STATUS_READY) != 0;
                }

                /*
                 * If flash is ready, and there is something to write.
                 */
                if (ready && dst_data.done && dst_data.offset < src_data.offset) {
                        ready = false;
                        dst_data.done = false;
                        dst_data.size = src_data.offset - dst_data.offset;
                        /*
                         * No more that page_size at a time.
                         */
                        if (dst_data.size > page_size) {
                                dst_data.size = page_size;
                        }
                        /*
                         * Start asynchronous write.
                         */
                        at45db_write_page_async(dst, dst_data.offset,
                                                buf + dst_data.offset % (page_size * 2),
                                                dst_data.size, spi_done_cb, (void *) &dst_data);
                }

                /*
                 * Set first SPI done after first page was read from I2C.
                 */
                if (src_data.done && src_data.offset == page_size) {
                        dst_data.done = true;
                }

                /*
                 * If previous read is finished and there is space in buffer, start next read.
                 */
                if (src_data.done && src_data.offset < TOTAL_LEN &&
                                                src_data.offset < dst_data.offset + page_size * 2) {
                        src_data.done = false;
                        src_data.size = TOTAL_LEN - src_data.offset;
                        if (src_data.size > page_size) {
                                src_data.size = page_size;
                        }
                        eeprom_24XX256_read_async(src, src_data.offset,
                                                        buf + src_data.offset % (page_size * 2),
                                                        src_data.size, i2c_done_cb,
                                                                                (void *) &src_data);
                }
                /*
                 * If both buses are busy, give other tasks some time.
                 */
                while (!(src_data.done || dst_data.done)) {
                        OS_DELAY(1);
                }
        }

        stop = OS_GET_TICK_COUNT();
        OS_FREE_NORET(buf);

        uart_printfln(false, "Asynchronous copying %d bytes from EEPROM to FLASH took %u ticks",
                                                                        TOTAL_LEN, stop - start);
        ad_i2c_close(src);
        ad_spi_close(dst);
}

#endif /* CFG_DEMO_AD_SPI_I2C */
