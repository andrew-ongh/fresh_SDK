/**
 ****************************************************************************************
 *
 * @file demo_spi_os.c
 *
 * @brief SPI demo for accessing flash memory (using ad_spi adapter)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <osal.h>
#include <ad_spi.h>
#include <platform_devices.h>
#include "common.h"
#include "at45db011d.h"

#if CFG_DEMO_AD_SPI

static uint8_t buf[10];

/*
 * Function called from task's main loop.
 */
void task_ad_spi_at45_worker_func(const struct task_item *task)
{
        /*
         * Handle to flash device.
         */
        spi_device dev;
        uint8_t sum;
        uint8_t prev_sum = 0;
        int i, j;

        /*
         * Open device connected to SPI bus.
         * This will not start any transmission yet.
         */
        dev = ad_spi_open(AT45DB011D);

        for (;;) {
                OS_DELAY(2000);
                sum = 0;
                /*
                 * Calculate checksum of 1000 bytes starting from address 0x8000.
                 * This could be done in one call to at45db_read but this would required to have
                 * 1000 bytes in buffer. For purpose of demonstration only 10 bytes buffer will
                 * be used.
                 * To have accurate checksum flash memory will be locked for all read transactions.
                 */
                ad_spi_device_acquire(dev);

                for (i = 0; i < 1000; i += sizeof(buf)) {
                        /*
                         * Read some part of memory, and calculate checksum.
                         */
                        at45db_read(dev, 0x8000 + i, buf, sizeof(buf));
                        for (j = 0; j < sizeof(buf); ++j) {
                                sum += buf[j];
                        }
                }

                /*
                 * Device access finished, let other task use it.
                 */
                ad_spi_device_release(dev);

                /*
                 * If something changed in memory from last checksum computation notify user
                 * and update checksum.
                 */
                if (sum != prev_sum) {
                        uart_printfln_s("Memory updated, checksum now 0x%X", sum);
                        prev_sum = sum;
                }
        }

        /*
         * This task will not use flash for time being, close it.
         */
        ad_spi_close(dev);
}

/*
 * Write pointer
 */
static int wp = 0;

void menu_ad_spi_at45_write_func(const struct menu_item *m, bool checked)
{
        char buf[32] = {0};

        /*
         * Handle to flash device.
         */
        spi_device dev;

        /*
         * Open device connected to SPI bus.
         * This will not start any transmission yet.
         */
        dev = ad_spi_open(AT45DB011D);

        /*
         * Prepare record to write, it will have tick time in text form.
         * Store it in circular buffer.
         */
        sprintf(buf, "Dialog ticks %d", (int) OS_GET_TICK_COUNT());
        at45db_write(dev, 0x8000 + wp, (const uint8_t *) buf, 32);
        wp += 32;
        if (wp >= 1024) {
                wp = 0;
        }

        uart_printfln(false, "Added log entry.");

        /*
         * This task will not use flash for time being, close it.
         */
        ad_spi_close(dev);
}

void menu_ad_spi_at45_erase_func(const struct menu_item *m, bool checked)
{
        /*
         * Handle to flash device.
         */
        spi_device dev;

        /*
         * Open device connected to SPI bus.
         * This will not start any transmission yet.
         */
        dev = ad_spi_open(AT45DB011D);

        at45db_erase_page(dev, 0x8000);
        wp = 0;

        uart_printfln(false, "One page erased.");

        /*
         * This task will not use flash for time being, close it.
         */
        ad_spi_close(dev);
}

void menu_ad_spi_at45_print_log_func(const struct menu_item *m, bool checked)
{
        char buf[32];
        int i;

        /*
         * Handle to flash device.
         */
        spi_device dev;

        /*
         * Open device connected to SPI bus.
         * This will not start any transmission yet.
         */
        dev = ad_spi_open(AT45DB011D);

        uart_printfln(false, "Start of log:");
        /*
         * Read records from memory, record is empty when first byte if 0xFF.
         * Stop reading on first empty record.
         */
        for (i = 0; i < 1024; i += 32) {
                /*
                 * Read just one record at a time.
                 */
                at45db_read(dev, 0x8000 + i, (uint8_t *) buf, 32);
                if (buf[0] == (char) 0xFF) {
                        break;
                }
                uart_printfln(false, buf);
        }

        uart_printfln(false, "End of log");

        /*
         * This task will not use flash for time being, close it.
         */
        ad_spi_close(dev);
}

#endif /* CFG_DEMO_AD_SPI */
