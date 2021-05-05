/**
 ****************************************************************************************
 *
 * @file demo_i2c.c
 *
 * @brief I2C demo (hw_i2c driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <osal.h>
#include <platform_devices.h>
#include <hw_i2c.h>
#include <resmgmt.h>
#include "common.h"

#define EEPROM_ADDRESS  0x50
#define FM75_ADDRESS    0x4F

#define FM75_REG_TEMP   0x00      // Temperature register
#define FM75_REG_CONF   0x01      // Configuration register
#define FM75_REG_THYST  0x02      // THYST (hysteresis) register
#define FM75_REG_TOS    0x03      // TOS (over-limit signal) register

static bool read_temp_enabled;
static OS_EVENT event;

static void set_target_address(uint16_t address)
{
        /*
         * Before address of the target will be changed I2C controller must be disabled. After
         * setting the proper address I2C can be reenabled.
         */
        hw_i2c_disable(HW_I2C1);

        hw_i2c_set_target_address(HW_I2C1, address);

        hw_i2c_enable(HW_I2C1);

        /*
         * Setting address in our case means we'll start some transfer with new device. For this
         * reason it's good to reset abort source in order to start in clean state (to avoid having
         * abort source set by previous transfer to other device), otherwise we need to remember
         * about this before every transfer separately.
         */
        hw_i2c_reset_abort_source(HW_I2C1);
}

static void fm75_write_reg(uint8_t reg, const uint8_t *val, uint8_t len)
{
        size_t wr_status = 0;
        HW_I2C_ABORT_SOURCE abrt_src = HW_I2C_ABORT_NONE;

        /*
         * The first writing byte informs to which register rest data will be written.
         */
        hw_i2c_write_byte(HW_I2C1, reg);
        wr_status = hw_i2c_write_buffer_sync(HW_I2C1, val, len, &abrt_src, HW_I2C_F_WAIT_FOR_STOP);
        if ((wr_status < (ssize_t)len) || (abrt_src != HW_I2C_ABORT_NONE)) {
                printf("fm75 write failure: %u" NEWLINE, abrt_src);
        }
}

static void fm75_read_reg(uint8_t reg, uint8_t *val, uint8_t len)
{
        size_t rd_status = 0;
        HW_I2C_ABORT_SOURCE abrt_src = HW_I2C_ABORT_NONE;

        /*
         * Before reading values from sensor registers we need to send one byte information to it
         * to inform which sensor register will be read now.
         */
        hw_i2c_write_byte(HW_I2C1, reg);
        rd_status = hw_i2c_read_buffer_sync(HW_I2C1, val, len, &abrt_src, HW_I2C_F_NONE);
        if ((rd_status < (size_t)len) || (abrt_src != HW_I2C_ABORT_NONE)) {
                printf("fm75 read failure: %u" NEWLINE, abrt_src);
        }
}

void demo_i2c_init(void)
{
        /*
         * Initialize I2C controller in master mode with standard communication speed (100 kb/s) and
         * transfer in 7-bit addressing mode.
         */
        static const i2c_config cfg = {
                .speed = HW_I2C_SPEED_STANDARD,
                .mode = HW_I2C_MODE_MASTER,
                .addr_mode = HW_I2C_ADDRESSING_7B,
        };

        hw_i2c_init(HW_I2C1, &cfg);

        srand(OS_GET_TICK_COUNT());
}

/* Convert temperature returned from FM75 to fixed-point value (1/10000 precision) */
static int convert_temp(uint8_t t_in[2], int *fract)
{
        int t_out;
        uint8_t bit; // current bit of fractional part (msb = 0)

        /* first byte is signed int integer part of temperature */
        t_out = (int8_t) t_in[0];

        /*
         * 2nd byte is fractional part with bits starting from MSB are for 1/2, 1/4, 1/8 and 1/16
         * part of degree. This should be simply added to integer part to have proper temperature
         * value. At most 4 bits are used (for 12-bits precision) and not used bits are always set
         * to 0.
         *
         * It's convenient to use fixed-point arithmetic here since so this can work even on newlib
         * with floating point support disabled. Temperature is expressed as multiply of 1/10000
         * as with precision up to 1/16 (0.0625) this is just enough.
         *
         * In generic case we just need to loop and check all bits of fractional part of result
         * (here: 4 bits).
         */
        t_out *= 10000;
        bit = 0;
        do {
                /* check n-th bit starting from msb */
                if (t_in[1] & (1 << (7 - bit))) {
                        uint8_t div = 1 << (bit + 1); // div=2 for bit=0
                        t_out += 10000 / div;
                }
        } while (++bit < 4);

        /*
         * Since we know exactly that at most 4 bits of fractional part are set, we can just unwind
         * loop and have this simplified:
         *
         * t_out += (t_in[1] & 0x80 ? 5000 : 0);
         * t_out += (t_in[1] & 0x40 ? 2500 : 0);
         * t_out += (t_in[1] & 0x20 ? 1250 : 0);
         * t_out += (t_in[1] & 0x10 ?  625 : 0);
         */
        if (fract) {
                *fract = abs(t_out % 10000);
        }

        return t_out / 10000;
}

void task_i2c_get_temp_init(const struct task_item *task)
{
        static const uint8_t conf_reg = 0x00;
        static const uint8_t hyst_reg[2] = {0x4B, 0x00};
        static const uint8_t os_reg[2] = {0x50, 0x00};

        /*
         * Create event to wait on, when task should not read temperature from sensor.
         */
        OS_EVENT_CREATE(event);

        /*
         * Set FM75 with initial values which are at the same time default values of configuration
         * register of the sensor after power up.
         */
        set_target_address(FM75_ADDRESS);
        fm75_write_reg(FM75_REG_CONF, &conf_reg, sizeof(conf_reg));
        fm75_write_reg(FM75_REG_THYST, hyst_reg, sizeof(hyst_reg));
        fm75_write_reg(FM75_REG_TOS, os_reg, sizeof(os_reg));
}

void menu_i2c_fm75_set_resolution_func(const struct menu_item *m, bool checked)
{
        uint8_t conf = 0;
        static const uint8_t res_mask = 0x60;

        resource_acquire(RES_MASK(RES_ID_I2C1), RES_WAIT_FOREVER);

        /*
         * First we need to read currently configuration of FM75.
         */
        fm75_read_reg(FM75_REG_CONF, &conf, sizeof(conf));

        /*
         * Set the new resolution in configuration register of FM75.
         */
        conf &= ~res_mask;
        conf |= ((int) m->param << 5) & res_mask;

        /*
         * Then we set the new value of resolution and send it to sensor.
         */
        fm75_write_reg(FM75_REG_CONF, &conf, sizeof(conf));

        resource_release(RES_MASK(RES_ID_I2C1));
}

void menu_i2c_fm75_set_alarm_limits_func(const struct menu_item *m, bool checked)
{
        uint8_t reg_val[2];

        /*
         * Set predefined temperature boundary values of safe area where alarm is not activated -
         * Over-Limit Signal (OS) output is not actuated.
         *
         * The first is lower limit so we write it to THYST register, the second one is upper limit
         * and it is written to TOS register. Fract values of each limit are set to 0.
         */
        reg_val[0] = ALARM_GET_LOW((int) m->param);
        reg_val[1] = 0;
        fm75_write_reg(FM75_REG_THYST, reg_val, sizeof(reg_val));

        reg_val[0] = ALARM_GET_HIGH((int) m->param);
        reg_val[1] = 0;
        fm75_write_reg(FM75_REG_TOS, reg_val, sizeof(reg_val));
}

void task_i2c_get_temp_func(const struct task_item *task)
{
        uint8_t temp[2];
        int t_out, fract;

        /*
         * Wait in OS friendly way for request to run.
         */
        if (!read_temp_enabled) {
                OS_EVENT_WAIT(event, OS_EVENT_FOREVER);
        }

        /*
         * Require I2C for exclusively reading temperature from FM75 and release it after operation.
         */
        resource_acquire(RES_MASK(RES_ID_I2C1), RES_WAIT_FOREVER);

        set_target_address(FM75_ADDRESS);

        /*
         * Read actual temperature values from FM75.
         */
        fm75_read_reg(FM75_REG_TEMP, temp, sizeof(temp));

        resource_release(RES_MASK(RES_ID_I2C1));

        /*
         * Send results to UART.
         */
        t_out = convert_temp(temp, &fract);
        printf("current temperature: %d.%04d C" NEWLINE, t_out, fract);

        /*
         * Wait 1 second to get temperature again.
         */
        OS_DELAY(1000);
}

void menu_i2c_get_temp_start_func(const struct menu_item *m, bool checked)
{
        /*
         * Set event in signal state if temperature reading operation is enabled. This allows get
         * current temperature.
         */
        read_temp_enabled = !checked;
        if (read_temp_enabled) {
                OS_EVENT_SIGNAL(event);
        }
}

bool menu_i2c_get_temp_start_checked_cb_func(const struct menu_item *m)
{
        /*
         * Return state of reading temperature operation - if it is enabled or not.
         */
        return read_temp_enabled;
}

static void eeprom_poll_ack(HW_I2C_ID id)
{
        /*
         * Writing to 24LC256 EEPROM fills internal buffer with data and starts actual write cycle
         * once it received STOP condition for a write command. Another read or write can be handled
         * by EEPROM only after it completes write cycle and this can be checked by ACK pooling
         * (described in datasheet) which basically is sending write command to EEPROM as long as it
         * does not return ACK - once ACK is received, it means write cycle has completed.
         */
        do {
                /*
                 * Make sure TX abort is reset, this is because at the beginning EEPROM will not ACK
                 * address byte so we'll have a TX abort set in controller.
                 */
                hw_i2c_reset_int_tx_abort(id);
                /*
                 * We only need to check if EEPROM returns ACK for address byte, however we cannot
                 * simply send address byte since it's controller who takes care of this once TX FIFO
                 * is filled with data. A simple solution is to send 'dummy' byte which will be
                 * ignored by EEPROM but will make I2C controller to generate START condition and
                 * send address byte.
                 */
                hw_i2c_write_byte(id, 0xAA);
                /*
                 * Before transmission status can be checked, we have to be sure that controller
                 * finished sending 'dummy' byte and received ACK (or not).
                 */
                while (hw_i2c_is_master_busy(id)) {
                }
                /*
                 * No check it transmission was successful - if EEPROM did not ACK address byte,
                 * appropriate abort source will be set.
                 */
        } while (hw_i2c_get_abort_source(id) & HW_I2C_ABORT_7B_ADDR_NO_ACK);
}

void menu_i2c_write_to_eeprom_func(const struct menu_item *m, bool checked)
{
        int i;
        size_t wr_status = 0;
        HW_I2C_ABORT_SOURCE abrt_src = HW_I2C_ABORT_NONE;
        uint8_t addr[2] = {0x00, 0x00};
        static uint8_t write_buffer[65];        // 64 bytes of data + '\0'

        set_target_address(EEPROM_ADDRESS);
        /*
         * Generate random data (some pattern).
         */
        for (i = 0; i < sizeof(write_buffer) - 1; i++) {
                write_buffer[i] = rand() % 96 + 32;
        }

        /*
         * Acquire I2C resource and write data to EEPROM. First two bytes determine memory address
         * from which data will be written. First byte is high byte address and the second one is
         * low byte address. In that case data will be stored starting from address 0. Next bytes
         * are data. Despite 2 separate write calls below, writing is still goes as one transfer
         * on the bus because it is done via FIFO and as long as FIFO is non-empty controller will
         * continue to send data and will generate stop condition only when FIFO is empty. Notice
         * also the use of flags between the 2 calls that ensure that no STOP condition will be
         * generated between them.
         * After writing operation release I2C resource.
         */
        resource_acquire(RES_MASK(RES_ID_I2C1), RES_WAIT_FOREVER);

        wr_status = hw_i2c_write_buffer_sync(HW_I2C1, addr, sizeof(addr), &abrt_src, HW_I2C_F_NONE);
        if ((wr_status < sizeof(addr)) || (abrt_src != HW_I2C_ABORT_NONE)) {
                printf("EEPROM address write during write failed: %u" NEWLINE, abrt_src);
        }
        else {
                wr_status = hw_i2c_write_buffer_sync(HW_I2C1, write_buffer, sizeof(write_buffer) - 1, NULL,
                                                     HW_I2C_F_WAIT_FOR_STOP);
                if ((wr_status < (ssize_t)sizeof(write_buffer)) || (abrt_src != HW_I2C_ABORT_NONE)) {
                        printf("EEPROM write failure: %u" NEWLINE, abrt_src);
                }
                else {
                        /*
                         * Wait for ACK from EEPROM.
                         */
                        eeprom_poll_ack(HW_I2C1);

                        /*
                         * Print on UART what was generated and written in EEPROM.
                         */
                        printf("written to EEPROM: %s" NEWLINE, write_buffer);
                }
        }


        resource_release(RES_MASK(RES_ID_I2C1));
}

void menu_i2c_read_from_eeprom_func(const struct menu_item *m, bool checked)
{
        size_t rd_status, wr_status;
        HW_I2C_ABORT_SOURCE abrt_src = HW_I2C_ABORT_NONE;
        uint8_t addr[2] = {0x00, 0x00};
        static uint8_t read_buffer[65];        // 64 bytes of data + '\0'

        set_target_address(EEPROM_ADDRESS);
        /*
         * Acquire I2C resource and read data back from EEPROM and release it after operation.
         * Before reading data we need to set address from which reading will start. This is done
         * by writing two bytes indicating address in EEPROM before reading from it. I2C controller
         * will automatically generate proper write and read commands on I2C bus.
         */
        resource_acquire(RES_MASK(RES_ID_I2C1), RES_WAIT_FOREVER);

        wr_status = hw_i2c_write_buffer_sync(HW_I2C1, addr, sizeof(addr), &abrt_src, HW_I2C_F_NONE);
        if ((wr_status < sizeof(addr)) || (abrt_src != HW_I2C_ABORT_NONE)) {
                printf("EEPROM address write during read failed: %u" NEWLINE, abrt_src);
        }
        else {
                rd_status = hw_i2c_read_buffer_sync(HW_I2C1, read_buffer, sizeof(read_buffer) - 1, &abrt_src, HW_I2C_F_NONE);

                /*
                 * Print on UART what was read from EEPROM.
                 */
                if ((rd_status < sizeof(read_buffer)) || (abrt_src != HW_I2C_ABORT_NONE)) {
                        printf("EEPROM read failure: %u" NEWLINE, abrt_src);
                }
                else {
                        printf("read from EEPROM:  %s" NEWLINE, read_buffer);
                }
        }

        resource_release(RES_MASK(RES_ID_I2C1));
}

