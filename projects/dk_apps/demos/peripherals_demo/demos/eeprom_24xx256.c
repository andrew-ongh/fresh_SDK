/**
 ****************************************************************************************
 *
 * @file eeprom_24xx256.c
 *
 * @brief Implementation of access EEPROM 24xx256
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <platform_devices.h>
#include <osal.h>
#include <ad_i2c.h>
#include "eeprom_24xx256.h"

#define PAGE_SIZE (64)

static void poll_ack(i2c_device dev)
{
        bool no_ack;

        /*
         * For low level functions get hardware id.
         */
        HW_I2C_ID id = ad_i2c_get_hw_i2c_id(dev);

        /*
         * Writing to 24LC256 EEPROM fills internal buffer with data and starts actual write cycle
         * once it receives STOP condition for a write command. Another read or write can be handled
         * by EEPROM only after it completes write cycle and this can be checked by ACK polling
         * (described in datasheet) which basically is sending write command to EEPROM as long as it
         * does not return ACK - once ACK is received, it means write cycle has completed.
         */
        do {
                /*
                 * Device is already acquired, now take bus while EEPROM is probed
                 */
                ad_i2c_bus_acquire(dev);

                /*
                 * Make sure TX abort is reset, this is because at the beginning EEPROM will not ACK
                 * address byte so we'll have a TX abort set in controller.
                 */
                hw_i2c_reset_int_tx_abort(id);

                /*
                 * Clear stop interrupt that will used for waiting for ACK or error.
                 */
                hw_i2c_reset_int_stop_detected(id);

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
                while ((hw_i2c_get_raw_int_state(id) & HW_I2C_INT_STOP_DETECTED) == 0) {
                        /*
                         * Give some time to other tasks, while I2C bus is taken.
                         */
                        OS_DELAY(0);
                }
                /*
                 * Now check it transmission was successful - if EEPROM did not ACK address byte,
                 * appropriate abort source will be set.
                 */
                no_ack = (hw_i2c_get_abort_source(id) & HW_I2C_ABORT_7B_ADDR_NO_ACK);

                /*
                 * Allow other devices to take over I2C bus, device is still held.
                 */
                ad_i2c_bus_release(dev);

        } while (no_ack);
}

int eeprom_24XX256_read(i2c_device dev, uint16_t addr, uint8_t *buf, uint16_t size)
{
        /* Buffer for address */
        uint8_t addr_buf[2] = { addr >> 8, addr };

        /*
         * There is no page boundaries for reading, entire memory can be read in one go.
         */
        return ad_i2c_transact(dev, addr_buf, sizeof(addr_buf), buf, size);
}

static void done_cb(void *user_data, HW_I2C_ABORT_SOURCE error)
{
        *((int *) user_data) = error;
}

bool eeprom_24XX256_write(i2c_device dev, uint32_t addr, const uint8_t *buf, uint16_t size)
{
        uint8_t addr_buf[2];
        size_t write_size = MIN(size, PAGE_SIZE - (addr & 0x3F));
        uint16_t offset = 0;
        volatile int error = 0;

        /*
         * Get exclusive access to EEPROM
         * Other devices on I2C lines can be used in between writes.
         */
        ad_i2c_device_acquire(dev);

        /*
         * Writes must be inside page boundaries. Limit writes to 64 bytes or less.
         */
        while (offset < size && error == 0) {
                addr_buf[0] = (addr + offset) >> 8;
                addr_buf[1] = (uint8_t) (addr + offset);

                /*
                 * Error will be updated in callback to 0 in case of success or to abort
                 * condition from HW_I2C_ABORT_SOURCE
                 */
                error = -1;
                ad_i2c_async_transact(dev, I2C_SND(addr_buf, 2),
                                        I2C_SND_ST(buf + offset, write_size),
                                        I2C_CB1(done_cb, &error),
                                        I2C_END);
                while (error == -1) {
                        OS_DELAY(0);
                }

                /*
                 * Move forward with pointers
                 */
                offset += write_size;
                write_size = MIN(size - offset, PAGE_SIZE);

                if (error == 0) {
                        /*
                         * No errors so far. Wait for EEPROM to write data.
                         */
                        poll_ack(dev);
                }
        }

        /*
         * Release access to EEPROM.
         */
        ad_i2c_device_release(dev);

        return error == 0;
}

void eeprom_24XX256_read_async(i2c_device dev, uint16_t addr, uint8_t *buf, uint16_t size,
                                                                ad_i2c_user_cb cb, void *user_data)
{
        uint8_t addr_buf[2];
        addr_buf[0] = addr >> 8;
        addr_buf[1] = (uint8_t) addr;
        volatile int error = -1;

        /*
         * Prepare transaction with callback after address.
         * This callback will allow function to exit and rest of the write will be done
         * asynchronously. User callback will be called after all was received.
         */
        ad_i2c_async_transact(dev, I2C_SND(addr_buf, 2),
                                I2C_CB1(done_cb, &error),
                                I2C_RCV(buf, size),
                                I2C_CB1(cb, user_data),
                                I2C_END);

        /*
         * Wait for first callback to fire.
         */
        while (error == -1) {
                OS_DELAY(0);
        }
}

void eeprom_24XX256_write_async(i2c_device dev, uint32_t addr, const uint8_t *buf,
                                                uint8_t size, ad_i2c_user_cb cb, void *user_data)
{
        uint8_t addr_buf[2];
        addr_buf[0] = addr >> 8;
        addr_buf[1] = (uint8_t) addr;
        volatile int error = -1;

        /*
         * Prepare transaction with callback after address.
         * This callback will allow function to exit and rest of the write will be done
         * asynchronously. User callback will be called after all was sent.
         */
        ad_i2c_async_transact(dev, I2C_SND(addr_buf, 2),
                                I2C_CB1(done_cb, &error),
                                I2C_SND_ST(buf, size),
                                I2C_CB1(cb, user_data),
                                I2C_END);

        /*
         * Wait for first callback to fire.
         */
        while (error == -1) {
                OS_DELAY(0);
        }
}
