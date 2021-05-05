/**
 ****************************************************************************************
 *
 * @file demo_qspi.c
 *
 * @brief QSPI demo (hw_qspi driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_qspi.h>
#include "common.h"
#include "config.h"
#include "w25q16dv.h"

/*
 * This is function that does some computation. Function does not use any global variable
 * and will be compiled as position independent code. This demo will copy this function
 * to flash memory to perform some test on code running from flash.
 */
int function_to_put_in_flash(int a1, int b2)
{
        if (a1 > 0) {
              a1 = (a1 << 8 | a1 >> 24);
        }
        if (b2 < 0) {
                b2 ^= a1;
        }
        return b2 ^ 0x1524 ^ a1;
}

static void qspi_init(void)
{
        /*
         * Set hardware characteristic according to W25Q16DV specification.
         */
        static const qspi_config cfg = {
                .address_size = HW_QSPI_ADDR_SIZE_24,
                .idle_clock = HW_QSPI_POL_HIGH,
                .sampling_edge = HW_QSPI_SAMPLING_EDGE_NEGATIVE
        };

        /*
         * Initialize controller.
         * It will start in manual mode.
         */
        hw_qspi_init(&cfg);

        /*
         * Make sure flash is not in continuous read mode.
         */
        w25_reset_continuous_mode();

        /*
         * In power down flash does not return its ID.
         * Read Id and if it is wrong try to leave power down mode.
         */
        if (w25q_read_id() != W25Q16DV_ID) {
                w25q_release_power_down();
        }
}

/*
 * Set to true if QSPI controller was setup.
 */
static bool config_selected = false;

void menu_qspi_set_quad_func(const struct menu_item *m, bool checked)
{
        printf(NEWLINE "Setting up fastest QUAD mode");

        config_selected = true;

        /*
         * Enable qspi controller.
         */
        qspi_init();

        /*
         * QSPI allows to execute code from SPI flash. QSPI controller will generate SPI signals
         * in response to read signals from AHB bus.
         * To enable this feature SPI flash commands must be stored into QSPI controller.
         * QUAD or DUAL SPI flash can use 1-4 bus lines in different phases of command.
         * This example uses WINBOND W25Q16DV flash.
         */

        /*
         * First disable auto mode so AHB access will not for duration of programming QSPI
         * controller generate any traffic on SPI bus.
         */
        hw_qspi_set_automode(false);

        /*
         * Exit continuous read mode.
         * W25Q16DV can operate in continuous read mode when read instruction is not transmitted
         * at every read, just once. There are two ways to exit this mode, one is to setup
         * certain bit in extra byte transmitted after address. Second is to send reset sequence
         * that will switch flash to command mode again.
         */
        w25_reset_continuous_mode();

        /*
         * This flash is not using D2 and D3 lines for transmission unless command that is
         * sent in single mode allows it.
         * Since read instruction to use uses quad mode, flash needs to be notified that all
         * lines will be used for transmission.
         */
        w25q_quad_enable();

        /*
         * Setup read instruction that will be used during bus access.
         * Only command byte in this case is sent in single mode, all other bits (address,
         * data and dummy/extra bytes are sent in quad mode.
         */
        hw_qspi_set_read_instruction(
                W25Q_FAST_READ_QUAD_IO, /* Read command sent in single mode */
                1,                      /* Send command only once, later access to memory will not
                                           send command and will start from address field */
                2,                      /* Dummy byte count, this number is 2 for this command */
                HW_QSPI_BUS_MODE_SINGLE,/* Command is sent in single (MISO/MOSI only) */
                HW_QSPI_BUS_MODE_QUAD,  /* Address is sent on 4 lines */
                HW_QSPI_BUS_MODE_QUAD,  /* Dummy phase uses 4 lines */
                HW_QSPI_BUS_MODE_QUAD); /* Data is read using 4 lines */

        /*
         * For this particular flash it's possible to further reduce SPI traffic by telling memory
         * that next read instruction will not require command to be sent at all. This will
         * reduce command by 8 clock cycles.
         * With this setting it is important to call w25_reset_continues_mode() when exiting
         * auto mode.
         */
        hw_qspi_set_extra_byte(0x20, HW_QSPI_BUS_MODE_QUAD, 1);

        /*
         * Finally tell QSPI controller to translate AHB signals to programmed instructions.
         */
        hw_qspi_set_automode(true);
}

void menu_qspi_set_single_func(const struct menu_item *m, bool checked)
{
        printf(NEWLINE "Setting up slow single mode");

        config_selected = true;

        /*
         * Enable qspi controller.
         */
        qspi_init();

        /*
         * QSPI allows to execute code from SPI flash. QSPI controller will generate SPI signals
         * in response to read signals from AHB bus.
         * To enable this feature SPI flash commands must be stored into QSPI controller.
         * QUAD or DUAL SPI flash can use 1-4 bus lines in different phases of command.
         * This example uses WINBOND W25Q16DV flash.
         */

        /*
         * First disable auto mode so AHB access will not generate any traffic on SPI bus for
         * duration of programming QSPI controller.
         */
        hw_qspi_set_automode(false);

        /*
         * Exit continuous read mode.
         * W25Q16DV can operate in continuous read mode when read instruction is not transmitted
         * at every read, just once. There are two ways to exit this mode, one is to setup
         * certain bit in extra byte transmitted after address. Second is to send reset sequence
         * that will switch flash to command mode again.
         */
        w25_reset_continuous_mode();

        /*
         * Setup read instruction that will be used during bus access.
         * This command uses single SPI mode for all phases.
         */
        hw_qspi_set_read_instruction(
                W25Q_READ_DATA,         /* Command to use */
                0,                      /* 0 command is sent at every memory access */
                0,                      /* No dummy bytes after address for this command */
                HW_QSPI_BUS_MODE_SINGLE,/* Single mode for command phase */
                HW_QSPI_BUS_MODE_SINGLE,/* Single mode for address */
                HW_QSPI_BUS_MODE_SINGLE,/* Single mode for dummy */
                HW_QSPI_BUS_MODE_SINGLE);/* Single mode for data phase */
        /*
         * Finally tell QSPI controller to translate AHB signals to programmed instructions.
         */
        hw_qspi_set_automode(true);
}

/* Starting address in flash used in this demo */
static const uint32_t start_address = 0x0020000;

/* Ending address in flash accessed in this demo */
static const uint32_t end_address   = 0x0023000;
static const uint32_t sector_size   = 0x0001000;

static bool flash_prepared = false;
static uint8_t buf[1024];

static void qspi_program_flash(void)
{
        size_t i;
        /*
         * W25Q16DV is 2MB flash memory, with 256 byte page.
         * One page can be programmed at a time.
         * Pages can be erased in groups of 16 (1 sector = 16 pages), 128 pages (block) or
         * whole memory.
         *
         * In this example ...
         */
        uint32_t address;

        /*
         * Disable auto mode.
         */
        hw_qspi_set_automode(false);

        /*
         * Exit continuous read mode.
         * W25Q16DV can operate in continuous read mode when read instruction is not transmitted
         * at every read, just once. There are two ways to exit this mode, one is to setup
         * certain bit in extra byte transmitted after address. Second is to send reset sequence
         * that will switch flash to command mode again.
         */
        w25_reset_continuous_mode();

        /*
         * This memory requires write enable bit in status register to be set before each erase or
         * write, w25_write_page and w25q_erase_sector do by calling w25q_write_enable when needed.
         * So it this example erase region 0x100000-0x180000. Write some data at 0x110000 and
         * copy some position independent function into flash to execute.
         */
        for (address = start_address; address < end_address; address += sector_size) {
                /*
                 * Erase 16 pages.
                 */
                w25q_erase_sector(address);

                /*
                 * Wait for erase to finish.
                 */
                while (w25q_read_status_register_1() & W25Q_STATUS1_BUSY) {
                        /*
                         * Let other tasks do their job
                         */
                        OS_DELAY(1);
                }
        }

        /*
         * Now copy function from ram to flash.
         * Function is short enough to fit in one page.
         * Pointer can be from thumb mode in which case LSB is set to 1, so for copy ignore
         * LSB.
         */
        w25q_write_page(start_address, (const uint8_t *) (((int) function_to_put_in_flash) & ~1),
                                                                                        256);

        /*
         * Wait for write to finish.
         */
        while (w25q_read_status_register_1() & W25Q_STATUS1_BUSY) {
                /*
                 * Let other tasks do their job
                 */
                OS_DELAY(1);
        }

        /*
         * Now fill rest of memory with some pattern.
         * Let's each page have all bytes but starting from page address.
         * So page 0 would have bytes 0-255
         * Page 1 would have 1-255, 0
         * Page 2 would have 2-255, 0-1
         */
        for (address = start_address + 256; address < end_address; address += sector_size) {
                for (i = 0; i < 256; ++i) {
                        buf[i] = (uint8_t) (i + address);
                }
                /*
                 * Write page.
                 */
                w25q_write_page(address, buf, 256);
                /*
                 * Wait for erase to finish.
                 */
                while (w25q_read_status_register_1() & W25Q_STATUS1_BUSY) {
                        /*
                         * Let other tasks do their job
                         */
                        OS_DELAY(1);
                }
        }
        /*
         * Enable auto mode
         */
        hw_qspi_set_automode(true);
}

/*
 * Helper function that will allow to call memmove on memory several times, to check performance.
 */
static void stress_memcpy(const void *src, size_t size, size_t count)
{

        if (size > sizeof(buf))
                size = sizeof(buf);

        while (count > 0) {
                memmove(buf, src, size);
                count--;
        }
}

void memmove_performance_test(void) {
        OS_TICK_TIME ram_start;
        OS_TICK_TIME ram_stop;
        OS_TICK_TIME flash_start;
        OS_TICK_TIME flash_stop;
        OS_TICK_TIME rom_start;
        OS_TICK_TIME rom_stop;
        const size_t size = 1024;
        const size_t cnt = 15;

        /*
         * Measure FLASH->RAM memmov performance.
         */
        flash_start = OS_GET_TICK_COUNT();
        stress_memcpy((void *) MEMORY_QSPIF_BASE, size, cnt);
        flash_stop = OS_GET_TICK_COUNT();

        /*
         * Measure RAM->RAM memmov performance.
         */
        ram_start = OS_GET_TICK_COUNT();
        stress_memcpy((void *) MEMORY_SYSRAM_BASE, size, cnt);
        ram_stop = OS_GET_TICK_COUNT();

        /*
         * Measure ROM->RAM memmov performance.
         */
        rom_start = OS_GET_TICK_COUNT();
        stress_memcpy((void *) MEMORY_ROM_BASE, size, cnt);
        rom_stop = OS_GET_TICK_COUNT();

        /*
         * Print results on uart.
         */
        printf(NEWLINE "Memory copy RAM->RAM took   %d", (int) (ram_stop - ram_start));
        printf(NEWLINE "Memory copy ROM->RAM took   %d", (int) (rom_stop - rom_start));
        printf(NEWLINE "Memory copy FLASH->RAM took %d", (int) (flash_stop - flash_start));
}

/*
 * This function executes function passed in argument on memory.
 * Both memories can be from RAM or QSPI.
 */
static void execute_code(int (*p)(int a, int b), int *data, size_t size)
{
        size_t i;
        int r = 0;

        for (i = 0; i < size; ++i) {
                r = p(r, data[i]);
        }
}

/*
 * This function measures execution time of code run from RAM with data from RAM,
 * Code run from QSPI with data from RAM, and finally code running form QSPI memory
 * reading QSPI memory.
 */
void code_performance_test(void)
{
        const size_t cnt = 5000;
        OS_TICK_TIME start1;
        OS_TICK_TIME stop1;
        OS_TICK_TIME start2;
        OS_TICK_TIME stop2;
        OS_TICK_TIME start3;
        OS_TICK_TIME stop3;
        int (*fun)(int a1, int b2);

        /*
         * Lets fun point to function form RAM.
         */
        fun = function_to_put_in_flash;

        /*
         * Measure code in RAM data in RAM.
         */
        start1 = OS_GET_TICK_COUNT();
        execute_code(fun, (void *) MEMORY_SYSRAM_BASE, cnt);
        stop1 = OS_GET_TICK_COUNT();

        /*
         * Switch to same function copied to QSPI memory.
         * Keep LSB from original function to allow this code to run in thumb and arm mode.
         */
        fun = (int (*)(int, int)) (start_address + MEMORY_QSPIF_BASE
                                                        + (((int) function_to_put_in_flash) & 1));

        /*
         * Measure code in FLASH data in RAM.
         */
        start2 = OS_GET_TICK_COUNT();
        execute_code(fun, (void *) MEMORY_SYSRAM_BASE, cnt);
        stop2 = OS_GET_TICK_COUNT();

        /*
         * Measure code in FLASH data in FLASH.
         */
        start3 = OS_GET_TICK_COUNT();
        execute_code(fun, (void *) MEMORY_QSPIF_BASE, cnt);
        stop3 = OS_GET_TICK_COUNT();

        /*
         * Print results.
         */
        printf(NEWLINE "Code execution from RAM data from RAM took   %5d", (int) (stop1 - start1));
        printf(NEWLINE "Code execution from QSPI data from RAM took  %5d", (int) (stop2 - start2));
        printf(NEWLINE "Code execution from QSPI data from QSPI took %5d", (int) (stop3 - start3));
}

void menu_qspi_test_performance_func(const struct menu_item *m, bool checked)
{
        /*
         * Enable qspi controller.
         */
        qspi_init();

        /*
         * If user has not selected configuration yet, select one.
         */
        if (!config_selected) {
                menu_qspi_set_single_func(NULL, false);
        }

        /*
         * Skip programming flash memory if it was already programmed during this test.
         */
        if (!flash_prepared) {
                qspi_program_flash();
                flash_prepared = true;
        }

        /*
         * Enable auto mode.
         */
        hw_qspi_set_automode(true);

        memmove_performance_test();
        code_performance_test();
}

void menu_qspi_div_func(const struct menu_item *m, bool checked)
{
        HW_QSPI_DIV div = (HW_QSPI_DIV) m->param;

        if (!checked) {
                hw_qspi_set_div(div);
        }
}

bool menu_qspi_div_checked_cb_func(const struct menu_item *m)
{
        HW_QSPI_DIV div = (HW_QSPI_DIV) m->param;

        return hw_qspi_get_div() == div;
}
