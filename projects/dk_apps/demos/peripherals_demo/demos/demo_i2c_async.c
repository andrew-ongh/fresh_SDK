/**
 ****************************************************************************************
 *
 * @file demo_i2c_async.c
 *
 * @brief I2C demo (ad_i2c driver)
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
#include <ad_i2c.h>
#include <resmgmt.h>
#include "common.h"

#if CFG_DEMO_HW_I2C_ASYNC

#define EEPROM_ADDRESS  0x50
#define FM75_ADDRESS    0x4F

#define FM75_REG_TEMP   0x00      /* Temperature register */
#define FM75_REG_CONF   0x01      /* Configuration register */
#define FM75_REG_THYST  0x02      /* THYST (hysteresis) register */
#define FM75_REG_TOS    0x03      /* TOS (over-limit signal) register */

/*
 * When true - task will read temperature sensor and print value on UART.
 */
static bool read_temp_enabled;

/*
 * Handle to temperature sensor used in worker task and in GUI task.
 */
static i2c_device fm75;

/*
 * I2C user data, passed to callback after transfers.
 * It has OS event that will be signaled and error code detected on I2C bus is any.
 */
typedef struct i2c_event
{
        OS_EVENT os_event;
        HW_I2C_ABORT_SOURCE error;
} i2c_event;

/*
 * Signaling event used in operation started from GUI task.
 */
static i2c_event i2c_signal;

/*
 * Signaling event used in operation started from temperature task.
 */
static i2c_event temp_signal;

/*
 * Initialize event structure used in this demo.
 */
static void init_event(i2c_event *event)
{
        event->error = 0;
        OS_EVENT_CREATE(event->os_event);
}

/*
 * Callback passed to i2c_async_transact. It signals event passed as user data.
 */
static void signal_event(void *user_data, HW_I2C_ABORT_SOURCE error)
{
        i2c_event *event = (i2c_event *) user_data;

        /*
         * Store error code.
         */
        event->error = error;

        /*
         * Notify task about finished transfer.
         */
        OS_EVENT_SIGNAL_FROM_ISR(event->os_event);
}

/*
 * Resets event used for synchronization.
 */
static void reset_event(i2c_event *event)
{
        /*
         * Reset error. This will be filled in case of error later.
         */
        event->error = 0;

        /*
         * Make sure event is in un-signaled state.
         */
        OS_EVENT_WAIT(event->os_event, OS_EVENT_NO_WAIT);
}

/*
 * Wait for event to be in signaled state.
 */
static void wait_for_event(i2c_event *event)
{
        OS_EVENT_WAIT(event->os_event, OS_EVENT_FOREVER);
}

/*
 * Write sensor register
 */
static inline void fm75_write_reg_async(i2c_device dev, uint8_t reg, const uint8_t *val,
                                                uint8_t len, ad_i2c_user_cb cb, void *user_data)
{
        /*
         * Writing sensor register. In this case first write register number then register content.
         */
        ad_i2c_async_transact(dev, I2C_SND(&reg, 1),
                                I2C_SND_ST(val, len),
                                I2C_CB1(cb, user_data),
                                I2C_END);
}

/*
 * Write sensor register synchronously with user supplied event
 */
static int fm75_write_reg_event(i2c_device dev, uint8_t reg, const uint8_t *val, uint8_t len,
                                                                                i2c_event *event)
{
        /*
         * Make sure event is not in signal state yet;
         */
        reset_event(event);

        /*
         * Writing sensor register, use callback that will signal event for synchronization.
         */
        fm75_write_reg_async(dev, reg, val, len, signal_event, event);

        /*
         * Wait for transaction to complete.
         */
        wait_for_event(event);

        /*
         * Error code if any is stored in event, return it.
         */
        return event->error;
}

/*
 * Read sensor register asynchronously
 */
static inline void fm75_read_reg_async(i2c_device dev, uint8_t reg, uint8_t *val, uint8_t len,
                                                                ad_i2c_user_cb cb, void *user_data)
{
        /*
         * Reading sensor register consist of writing register number and reading it's contents.
         * Do this by preparing transaction that will generate write and read.
         * This demo uses callback that will signal event so task can be resumed after read.
         */
        ad_i2c_async_transact(dev, I2C_SND(&reg, 1),
                                I2C_RCV(val, len),
                                I2C_CB1(cb, user_data),
                                I2C_END);
}

/*
 * Read sensor register synchronously with user supplied event
 */
static int fm75_read_reg_event(i2c_device dev, uint8_t reg, uint8_t *val, uint8_t len,
                                                                                i2c_event *event)
{
        /*
         * Make sure event is not in signal state yet;
         */
        reset_event(event);

        /*
         * Reading sensor register consist of writing register number and reading it's contents.
         * Do this by preparing transaction that will generate write and read.
         * This demo uses callback that will signal event so task can be resumed after read.
         */
        fm75_read_reg_async(dev, reg, val, len, signal_event, event);

        /*
         * Wait for transaction to complete.
         */
        wait_for_event(event);

        /*
         * Error code if any is stored in event, return it.
         */
        return event->error;
}

/* Convert temperature returned from FM75 to fixed-point value (1/10000 precision) */
static int convert_temp(uint8_t t_in[2], int *fract)
{
        int t_out;
        uint8_t bit; // current bit of fractional part (msb = 0)

        /* first byte is signed integer part of temperature */
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

/*
 * Function will be called once at task initialization.
 */
void task_i2c_async_get_temp_init(const struct task_item *task)
{
        static const uint8_t conf_reg[] = {0x00};
        static const uint8_t hyst_reg[] = {0x4B, 0x00};
        static const uint8_t os_reg[] = {0x50, 0x00};

        /*
         * Get handle to temperature sensor device.
         * This call does not configure I2C controller yet.
         */
        fm75 = ad_i2c_open(FM75);

        /*
         * Create event that will be used by GUI to wait for I2C transactions to finish.
         * This event will be used for access temperature sensor or EEPROM from GUI task.
         */
        init_event(&i2c_signal);

        /*
         * Create event that will be used by temperature reading task.
         */
        init_event(&temp_signal);

        /*
         * Set FM75 with initial values which are at the same time default values of configuration
         * register of the sensor after power up.
         * While it's not strictly necessary temperature sensor will be lock for configuration time.
         * Between calls to i2c_device_acquire and i2c_device_release no other task will access
         * sensor, other I2C device can be used in between.
         */
        ad_i2c_device_acquire(fm75);

        fm75_write_reg_event(fm75, FM75_REG_CONF, conf_reg, sizeof(conf_reg), &i2c_signal);
        fm75_write_reg_event(fm75, FM75_REG_THYST, hyst_reg, sizeof(hyst_reg), &i2c_signal);
        fm75_write_reg_event(fm75, FM75_REG_TOS, os_reg, sizeof(os_reg), &i2c_signal);

        /*
         * Release access to sensor for other tasks.
         */
        ad_i2c_device_release(fm75);
}

/*
 * Set resolution returned by MF75 sensor.
 */
void menu_i2c_async_fm75_set_resolution_func(const struct menu_item *m, bool checked)
{
        uint8_t conf = 0;
        static const uint8_t res_mask = 0x60;

        /*
         * Make change atomic by locking access to sensor.
         */
        ad_i2c_device_acquire(fm75);

        /*
         * Read current configuration of FM75.
         */
        fm75_read_reg_event(fm75, FM75_REG_CONF, &conf, sizeof(conf), &i2c_signal);

        /*
         * Set the new resolution in configuration register of FM75.
         */
        conf &= ~res_mask;
        conf |= ((int) m->param << 5) & res_mask;

        /*
         * Then we set the new value of resolution and send it to sensor.
         */
        fm75_write_reg_event(fm75, FM75_REG_CONF, &conf, sizeof(conf), &i2c_signal);

        /*
         * Release device for other tasks.
         */
        ad_i2c_device_release(fm75);
}

/*
 * Set value for over limit signals.
 * Sensor can change pin state when temperature is outside of boundaries.
 */
void menu_i2c_async_fm75_set_alarm_limits_func(const struct menu_item *m, bool checked)
{
        uint8_t reg_val[2];

        /*
         * Lock access to device for setting time.
         */
        ad_i2c_device_acquire(fm75);

        /*
         * Set predefined temperature boundary values of safe area where alarm is not activated -
         * Over-Limit Signal (OS) output is not actuated.
         *
         * The first is lower limit so we write it to THYST register, the second one is upper limit
         * and it is written to TOS register. Fract values of each limit are set to 0.
         */
        reg_val[0] = ALARM_GET_LOW((int) m->param);
        reg_val[1] = 0;
        fm75_write_reg_event(fm75, FM75_REG_THYST, reg_val, sizeof(reg_val), &i2c_signal);

        reg_val[0] = ALARM_GET_HIGH((int) m->param);
        reg_val[1] = 0;
        fm75_write_reg_event(fm75, FM75_REG_TOS, reg_val, sizeof(reg_val), &i2c_signal);

        /*
         * Release device for other tasks.
         */
        ad_i2c_device_release(fm75);
}

/*
 * Function called constantly from task main loop.
 */
void task_i2c_async_get_temp_func(const struct task_item *task)
{
        uint8_t temp[2];
        int t_out, fract;
        static int cnt = 0;

        /*
         * No read requested, wait some time.
         */
        if (!read_temp_enabled) {
                OS_DELAY(1000);
                return;
        }

        /*
         * Read actual temperature values from FM75.
         */
        fm75_read_reg_event(fm75, FM75_REG_TEMP, temp, sizeof(temp), &temp_signal);

        /*
         * Send results to UART.
         * Results are sent once for 1000 ticks.
         * It would be possible to just wait 1000 tick in OS_DELAY(1000).
         * This demo however is written to demonstrate I2C devices access and having such
         * frequent reads generate a lot of traffic on I2C bus.
         */
        t_out = convert_temp(temp, &fract);
        if (++cnt > 1000) {
                printf("current temperature: %d.%04d C" NEWLINE, t_out, fract);
                cnt = 0;
        }

        /*
         * Wait some time get temperature again.
         */
        OS_DELAY(1);
}

/*
 * Function changes global variable that is checked by task.
 */
void menu_i2c_async_get_temp_start_func(const struct menu_item *m, bool checked)
{
        /*
         * Set variable to check-box state.
         */
        read_temp_enabled = !checked;
}

/*
 * Function called from menu to check/uncheck menu item.
 */
bool menu_i2c_async_get_temp_start_checked_cb_func(const struct menu_item *m)
{
        /*
         * Return state of reading temperature operation - if it is enabled or not.
         */
        return read_temp_enabled;
}

/*
 * Wait while EEPROM is writing.
 * Don't call it if write operation returned error.
 */
static void eeprom_wait_while_busy(i2c_device dev, struct i2c_event *event)
{
        static const uint8_t buf[1] = {0x00};

        /*
         * Writing to 24LC256 EEPROM fills internal buffer with data and starts actual write cycle
         * once it received STOP condition for a write command. Another read or write can be handled
         * by EEPROM only after it completes write cycle and this can be checked by ACK pooling
         * (described in datasheet) which basically is sending write command to EEPROM as long as it
         * does not return ACK - once ACK is received, it means write cycle has completed.
         */
        do {
                /*
                 * Make sure event is not signaled.
                 */
                reset_event(event);

                /*
                 * We only need to check if EEPROM returns ACK for address byte, however we cannot
                 * simply send address byte since it's controller who takes care of this once TX FIFO
                 * is filled with data. A simple solution is to send 'dummy' byte which will be
                 * ignored by EEPROM but will make I2C controller to generate START condition and
                 * send address byte.
                 * This write will wait for stop condition before callback is executed.
                 * If stop condition was detected and there is no error EEPROM finished write.
                 */
                i2c_async_write(dev, buf, 1, signal_event, event);

                /*
                 * Wait for write to finish.
                 */
                wait_for_event(event);

                /*
                 * Check if EEPROM ACK'ed address byte, if not keep in a loop.
                 */
        } while (event->error & HW_I2C_ABORT_7B_ADDR_NO_ACK);
}

void menu_i2c_async_write_to_eeprom_func(const struct menu_item *m, bool checked)
{
        int i;
        static uint8_t write_buffer[65];        // 64 bytes of data + '\0'
        static const uint8_t mem_addr[2] = {0x00, 0x00};
        i2c_device dev;

        /*
         * Get temperature sensor handle.
         */
        dev = ad_i2c_open(MEM_24LC256);

        /*
         * Generate random data (some pattern).
         */
        for (i = 0; i < sizeof(write_buffer) - 1; i++) {
                write_buffer[i] = rand() % 96 + 32;
        }

        /*
         * Acquire device so no other task accesses EEPROM. First two bytes determine memory address
         * from which data will be written. First byte is high byte address and the second one is
         * low byte address. In that case data will be stored starting from address 0. Next bytes
         * are data. Despite 3 separate write calls below, writing is still goes as one transfer
         * on the bus because it is done via FIFO and as long as FIFO is non-empty controller will
         * continue to send data and will generate stop condition only when FIFO is empty.
         */
        ad_i2c_device_acquire(dev);

        /*
         * Make sure event is not signaled.
         */
        reset_event(&i2c_signal);

        /*
         * Perform write transaction with 2 buffers, one for command and address and one for data.
         */
        ad_i2c_async_transact(dev, I2C_SND(mem_addr, sizeof(mem_addr)),
                                I2C_SND_ST(write_buffer, sizeof(write_buffer) - 1),
                                I2C_CB1(signal_event, &i2c_signal),
                                I2C_END);

        /*
         * Wait for write transaction to finish.
         * Callback will be executed after address and data are sent or error is detected.
         */
        wait_for_event(&i2c_signal);

        /*
         * If there was no error on I2C bus, wait until EEPROM is ready again.
         */
        if (!i2c_signal.error) {
                eeprom_wait_while_busy(dev, &i2c_signal);
        } else {
                /*
                 * Print error condition detected by I2C controller.
                 */
                printf("write to EEPROM:  failed %x" NEWLINE, i2c_signal.error);
        }

        /*
         * Release device so it can be used by other tasks.
         */
        ad_i2c_device_release(dev);

        ad_i2c_close(dev);
}

void menu_i2c_async_read_from_eeprom_func(const struct menu_item *m, bool checked)
{
        static uint8_t read_buffer[65];        // 64 bytes of data + '\0'
        static const uint8_t mem_addr[2] = {0x00, 0x00};
        i2c_device dev;

        /*
         * Get handle to temperature sensor.
         */
        dev = ad_i2c_open(MEM_24LC256);

        /*
         * Make sure event is not signaled.
         */
        reset_event(&i2c_signal);

        /*
         * Before reading data we need to set address from which reading will start. This is done
         * by writing two bytes indicating address in EEPROM before reading from it. I2C controller
         * will automatically generate proper write and read commands on I2C bus.
         * There is no need to lock device or bus manually, it will be handled inside transaction.
         */
        ad_i2c_async_transact(dev, I2C_SND(mem_addr, sizeof(mem_addr)),
                                I2C_RCV(read_buffer, sizeof(read_buffer) - 1),
                                I2C_CB1(signal_event, &i2c_signal),
                                I2C_END);

        /*
         * Just wait for transaction to finish.
         */
        wait_for_event(&i2c_signal);

        /*
         * Close handle.
         */
        ad_i2c_close(dev);

        /*
         * Print on UART what was read from EEPROM.
         */
        if (i2c_signal.error) {
                printf("read from EEPROM:  failed %x" NEWLINE, i2c_signal.error);
        } else {
                printf("read from EEPROM:  %s" NEWLINE, read_buffer);
        }
}

#endif /* CFG_DEMO_HW_I2C_ASYNC */
