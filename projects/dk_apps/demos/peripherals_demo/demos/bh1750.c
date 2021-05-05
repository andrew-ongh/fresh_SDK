/**
 ****************************************************************************************
 *
 * @file bh1750.c
 *
 * @brief Ambient Light Sensor BH1750 driver.
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
#include "bh1750.h"

int bh1750_power_down(i2c_device dev)
{
        static const uint8_t op[1] = { BH1750_OP_POWER_DOWN };

        return ad_i2c_write(dev, op, sizeof(op));
}

int bh1750_power_on(i2c_device dev)
{
        static const uint8_t op[1] = { BH1750_OP_POWER_ON };

        return ad_i2c_write(dev, op, sizeof(op));
}

int bh1750_reset(i2c_device dev)
{
        static const uint8_t op[1] = { BH1750_OP_RESET };

        return ad_i2c_write(dev, op, sizeof(op));
}

int bh1750_start_measurement(i2c_device dev, bh1750_measurement_resolution_t res,
                                                                bh1750_measurement_mode_t mode)
{
        uint8_t op[1] = { res | mode };

        return ad_i2c_write(dev, op, sizeof(op));
}

int bh1750_read_measurement(i2c_device dev, uint16_t *value)
{
        uint8_t buf[2];
        int err;

        err = ad_i2c_read(dev, buf, sizeof(buf));
        if (0 == err) {
                *value = (buf[0] << 8) | buf[1];
        }
        return err;
}

int bh1750_change_measurement_time(i2c_device dev, uint8_t value)
{
        uint8_t op[1];
        int err;

        /*
         * Measurement time is 8 bit value that is written in two op codes.
         * To make sure value is consistent both writes must be atomic, for this device must
         * be acquired for exclusive access.
         */
        ad_i2c_device_acquire(dev);

        /*
         * Upper 3 bits in first command.
         */
        op[0] = BH1750_OP_CHANGE_MEASURE_TIME_H | (value >> 5);
        err = ad_i2c_write(dev, op, sizeof(op));
        if (err != 0) {
                goto done;
        }

        /*
         * Lower 5 bits in second command.
         */
        op[0] = BH1750_OP_CHANGE_MEASURE_TIME_L | (value & 0x1F);
        err = ad_i2c_write(dev, op, sizeof(op));

done:
        /*
         * Release device.
         */
        ad_i2c_device_release(dev);

        return err;
}

int bh1750_read(i2c_device dev, bh1750_measurement_resolution_t res,
                                                bh1750_measurement_mode_t mode, uint16_t *value)
{
        int err;

        err = bh1750_start_measurement(dev, res, mode);
        if (0 != err) {
                return err;
        }

        /*
         * Wait time needed according to resolution.
         */
        switch (res) {
        case BH1750_RESOLUTION_ONE_LUX:
        case BH1750_RESOLUTION_HALF_LUX:
                OS_DELAY_MS(BH1750_HI_RES_MEASUREMENT_TIME_MS);
                break;
        default:
                OS_DELAY_MS(BH1750_LO_RES_MEASUREMENT_TIME_MS);
                break;
        }

        /*
         * Read measurement and return.
         */
        return bh1750_read_measurement(dev, value);
}
