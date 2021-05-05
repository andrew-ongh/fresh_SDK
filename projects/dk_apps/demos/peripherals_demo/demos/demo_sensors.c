/**
 ****************************************************************************************
 *
 * @file demo_sensors.c
 *
 * @brief Access sensors on Sensor Board
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
#include <config.h>
#include <platform_devices.h>
#include "common.h"
#if CFG_DEMO_SENSOR_BH1750
#include "bh1750.h"
#endif
#if CFG_DEMO_SENSOR_BME280
#include "contrib/bme280.h"
#endif
#if CFG_DEMO_SENSOR_BMM150
#include "contrib/bmm050.h"
#endif
#if CFG_DEMO_SENSOR_BMG160
#include "contrib/bmg160.h"
#endif

#if CFG_DEMO_SENSOR_BH1750
static int sensor_board_bh1750_read(i2c_device dev)
{
        int err;
        uint16_t value;

        err = bh1750_power_on(dev);
        if (err) {
                return err;
        }

        err = bh1750_read(dev, BH1750_RESOLUTION_HALF_LUX, BH1750_MODE_CONTINUOUS, &value);
        if (err) {
                return err;
        }
        printf(NEWLINE "1/2 lux resolution read value %u", value);

        err = bh1750_read(dev, BH1750_RESOLUTION_ONE_LUX, BH1750_MODE_CONTINUOUS, &value);
        if (err) {
                return err;
        }
        printf(NEWLINE "1 lux resolution read value %u", value);

        err = bh1750_read(dev, BH1750_RESOLUTION_FOUR_LUX, BH1750_MODE_CONTINUOUS, &value);
        if (err) {
                return err;
        }
        printf(NEWLINE "4 lux resolution read value %u", value);

        err = bh1750_power_down(dev);

        return err;
}

void menu_sensor_board_bh1750_read_func(const struct menu_item *m, bool checked)
{
        int err;
        i2c_device dev;

        dev = ad_i2c_open(BH1750);

        err = sensor_board_bh1750_read(dev);
        if (err) {
                printf(NEWLINE "Error accessing BH1750 0x%X", err);
        }

        ad_i2c_close(dev);
}
#endif

#if CFG_DEMO_SENSOR_BME280

/*
 * Bosch driver passes address to I2C access functions but I2C adapter functions need device handle
 * to add this global variable for this.
 */
static i2c_device bme280_dev;

/*
 * Function needed by Bosch Sensortec driver
 */
static void bme280_wrap_delay(BME280_MDELAY_DATA_TYPE time)
{
        OS_DELAY_MS(time);
}

static void bme280_done_cb(void *user_data, HW_I2C_ABORT_SOURCE error)
{
        *((int *) user_data) = error;
}

/*
 * Function needed by Bosch Sensortec driver to write data over I2C bus.
 */
static s8 bme280_wrap_write(u8 device_addr, u8 register_addr, u8 *register_data, u8 wr_len)
{
        volatile int error = -1;

        ad_i2c_async_transact(bme280_dev, I2C_SND(&register_addr, 1),
                I2C_SND_ST(register_data, wr_len),
                I2C_CB1(bme280_done_cb, &error),
                I2C_END);
        while (error == -1) {
        }
        return error ? -1 : 0;
}

/*
 * Function needed by Bosch Sensortec driver to write register address and read register value
 * over I2C bus.
 */
static s8 bme280_wrap_read(u8 device_addr, u8 register_addr, u8 *register_data, u8 wr_len)
{
        return ad_i2c_transact(bme280_dev, &register_addr, 1, register_data, wr_len) ? -1 : 0;
}

/*
 * Structure for functions needed by Bosch Sensortec driver
 */
static struct bme280_t bme280 = {
        .bus_write = bme280_wrap_write,
        .bus_read = bme280_wrap_read,
        .delay_msec = bme280_wrap_delay
};

static int sensor_board_bme280_read(i2c_device dev)
{
        /* The variable used to read uncompensated temperature */
        s32 v_data_uncomp_tem_s32 = BME280_INIT_VALUE;
        /* The variable used to read uncompensated pressure */
        s32 v_data_uncomp_pres_s32 = BME280_INIT_VALUE;
        /* The variable used to read uncompensated pressure */
        s32 v_data_uncomp_hum_s32 = BME280_INIT_VALUE;
        /* The variable used to read real temperature */
        s32 v_actual_temp_s32 = BME280_INIT_VALUE;
        /* The variable used to read real pressure */
        u32 v_actual_press_u32 = BME280_INIT_VALUE;
        /* The variable used to read real humidity */
        u32 v_actual_humity_u32 = BME280_INIT_VALUE;
        /* result of communication results */
        s32 com_rslt = 0;
        bme280_dev = dev;
        u8 delay;

        /*
         * Initialize driver data, feed Bosch driver with functions needed to access I2C bus.
         */
        com_rslt = bme280_init(&bme280);

        /*
         * For initialization it is required to set the mode of
         * the sensor as "NORMAL"
         * data acquisition/read/write is possible in this mode
         * by using the below API able to set the power mode as NORMAL
         */
        com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);

        /*
         * For reading the pressure, humidity and temperature data it is required to
         * set the OSS setting of humidity, pressure and temperature
         * The "BME280_CTRLHUM_REG_OSRSH" register sets the humidity
         * data acquisition options of the device.
         * changes to this registers only become effective after a write operation to
         * "BME280_CTRLMEAS_REG" register.
         * In the code automated reading and writing of "BME280_CTRLHUM_REG_OSRSH"
         * register first set the "BME280_CTRLHUM_REG_OSRSH" and then read and write
         * the "BME280_CTRLMEAS_REG" register in the function
         */
        com_rslt += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);

        /* set the pressure oversampling */
        com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_2X);

        /* set the temperature oversampling */
        com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_4X);

        /*
         * Normal mode comprises an automated perpetual cycling between an (active)
         * Measurement period and an (inactive) standby period.
         * The standby time is determined by the contents of the register t_sb.
         * Standby time can be set using BME280_STANDBYTIME_125_MS.
         * Usage Hint : bme280_set_standbydur(BME280_STANDBYTIME_125_MS)
         */
        com_rslt += bme280_set_standby_durn(BME280_STANDBY_TIME_1_MS);

        /*
         * Computation time depends on what measurements are selected and which oversampling is
         * used. This helper function will calculate time to wait before data can be read.
         */
        bme280_compute_wait_time(&delay);

        /*
         * Wait for calculated time.
         */
        OS_DELAY_MS(delay);

        /*
         * Read raw ADC value from sensor
         */
        com_rslt += bme280_read_uncomp_pressure_temperature_humidity(&v_data_uncomp_tem_s32,
                                                &v_data_uncomp_pres_s32, &v_data_uncomp_hum_s32);

        printf(NEWLINE "Uncompensated temp=%d, pres=%d, hum=%u", v_data_uncomp_tem_s32,
                v_data_uncomp_pres_s32, v_data_uncomp_hum_s32);

        /*
         * Read the true temperature, humidity and pressure.
         * This function will convert ADC value to true values according to calibration data.
         * Pressure humidity and temperature are fixed point values.
         * Temperature in 0.01C, pressure in Pa, humidity in 0.001%.
         */
        com_rslt += bme280_read_pressure_temperature_humidity(&v_actual_press_u32,
                                                        &v_actual_temp_s32, &v_actual_humity_u32);

        printf(NEWLINE "Compensated pressure=%d.%02d hPa, temp=%d.%02d C, hum=%u.%03d %%",
                v_actual_press_u32 / 100, v_actual_press_u32 % 100,
                v_actual_temp_s32 / 100, v_actual_temp_s32 % 100,
                v_actual_humity_u32 / 1000, v_actual_humity_u32 % 1000);
        /*
         * For de-initialization it is required to set the mode of
         * the sensor as "SLEEP"
         * the device reaches the lowest power consumption only
         * In SLEEP mode no measurements are performed
         * All registers are accessible
         * by using the below API able to set the power mode as SLEEP
         */
        com_rslt += bme280_set_power_mode(BME280_SLEEP_MODE);

        return com_rslt;
}

void menu_sensor_board_bme280_read_func(const struct menu_item *m, bool checked)
{
        int err;
        i2c_device dev;

        /*
         * Get handle to sensor device.
         */
        dev = ad_i2c_open(BME280);

        /*
         * Do actual reading.
         */
        err = sensor_board_bme280_read(dev);
        if (err) {
                printf(NEWLINE "Error accessing BME280");
        }

        /*
         * Close device handle.
         */
        ad_i2c_close(dev);
}
#endif

#if CFG_DEMO_SENSOR_ADXL362

#define ADXL362_RESET_CODE      0x52
#define ADXL362_FILTER_CTL_CODE 0x51
#define ADXL362_POWER_CTL_CODE  0x22
#define ADXL362_ACC_RANGE       4       /* g */
#define ADXL362_MAX_11_BIT           0x07FF

typedef enum {
        ADXL362_CMD_WRITE_REG   = 0x0A,
        ADXL362_CMD_READ_REG    = 0x0B,
        ADXL362_CMD_READ_FIFO   = 0x0D
} adxl362_cmd_t;

/* ADXL362 register map */
typedef enum {
        ADXL362_REG_DEVID_AD            = 0x00,
        ADXL362_REG_DEVID_MST           = 0x01,
        ADXL362_REG_PARTID              = 0x02,
        ADXL362_REG_REVID               = 0x03,
        ADXL362_REG_XDATA               = 0x08,
        ADXL362_REG_YDATA               = 0x09,
        ADXL362_REG_ZDATA               = 0x0A,
        ADXL362_REG_STATUS              = 0x0B,
        ADXL362_REG_FIFO_ENTRIES_L      = 0x0C,
        ADXL362_REG_FIFO_ENTRIES_H      = 0x0D,
        ADXL362_REG_XDATA_L             = 0x0E,
        ADXL362_REG_XDATA_H             = 0x0F,
        ADXL362_REG_YDATA_L             = 0x10,
        ADXL362_REG_YDATA_H             = 0x11,
        ADXL362_REG_ZDATA_L             = 0x12,
        ADXL362_REG_ZDATA_H             = 0x13,
        ADXL362_REG_TEMP_L              = 0x14,
        ADXL362_REG_TEMP_H              = 0x15,
        ADXL362_REG_SOFT_RESET          = 0x1F,
        ADXL362_REG_THRESH_ACT_L        = 0x20,
        ADXL362_REG_THRESH_ACT_H        = 0x21,
        ADXL362_REG_TIME_ACT            = 0x22,
        ADXL362_REG_THRESH_INACT_L      = 0x23,
        ADXL362_REG_THRESH_INACT_H      = 0x24,
        ADXL362_REG_TIME_INACT_L        = 0x25,
        ADXL362_REG_TIME_INACT_H        = 0x26,
        ADXL362_REG_ACT_INACT_CTL       = 0x27,
        ADXL362_REG_FIFO_CONTROL        = 0x28,
        ADXL362_REG_FIFO_SAMPLES        = 0x29,
        ADXL362_REG_INTMAP1             = 0x2A,
        ADXL362_REG_INTMAP2             = 0x2B,
        ADXL362_REG_FILTER_CTL          = 0x2C,
        ADXL362_REG_POWER_CTL           = 0x2D,
        ADXL362_REG_SELF_TEST           = 0x2E
} adxl362_reg_t;

typedef enum {
        ADXL362_AXIS_X,
        ADXL362_AXIS_Y,
        ADXL362_AXIS_Z
} adxl362_axis_t;

static bool config_need = true;

static uint16_t create_value12(uint8_t h, uint8_t l)
{
        uint16_t ret = h;

        ret <<= 8;
        ret += l;

        return ret;
}

static inline uint8_t read_register(spi_device dev, adxl362_reg_t reg)
{
        uint8_t cmd[2];
        uint8_t rsp[1];

        cmd[0] = ADXL362_CMD_READ_REG;
        cmd[1] = reg;
        ad_spi_transact(dev, cmd, 2, rsp, 1);

        return rsp[0];
}

static uint16_t read_val12(spi_device dev, adxl362_axis_t axis)
{
        adxl362_reg_t reg_h, reg_l;
        uint8_t val_h, val_l;

        switch (axis) {
        case ADXL362_AXIS_X:
                reg_h = ADXL362_REG_XDATA_H;
                reg_l = ADXL362_REG_XDATA_L;
                break;
        case ADXL362_AXIS_Y:
                reg_h = ADXL362_REG_YDATA_H;
                reg_l = ADXL362_REG_YDATA_L;
                break;
        case ADXL362_AXIS_Z:
                reg_h = ADXL362_REG_ZDATA_H;
                reg_l = ADXL362_REG_ZDATA_L;
                break;
        default:
                return 0xFFFF;
        }

        val_h = read_register(dev, reg_h);
        val_l = read_register(dev, reg_l);

        return create_value12(val_h, val_l);
}

/* Function converts raw reading to value in g (multiplied by 100) */
static inline void convert_acc_value(int16_t val, bool *sign, uint32_t *conv)
{
        *sign = ((val < 0) ? true : false);
        *conv = abs(val) * ADXL362_ACC_RANGE * 100 / ADXL362_MAX_11_BIT;
}

static void print_converted_values(int16_t x12, int16_t y12, int16_t z12)
{
        bool sign;
        uint32_t conv_val;

        printf(NEWLINE "Axis acceleration (range %d g) - ", ADXL362_ACC_RANGE);

        convert_acc_value(x12, &sign, &conv_val);
        printf("x: %s%lu.%02lu g, ", (sign ? "-" : ""), conv_val / 100, conv_val % 100);

        convert_acc_value(y12, &sign, &conv_val);
        printf("y: %s%lu.%02lu g, ", (sign ? "-" : ""), conv_val / 100, conv_val % 100);

        convert_acc_value(z12, &sign, &conv_val);
        printf("z: %s%lu.%02lu g", (sign ? "-" : ""), conv_val / 100, conv_val % 100);
}

static void sensor_board_adxl362_read(spi_device dev)
{
        int8_t x8, y8, z8;
        int16_t x12, y12, z12;

        /* read and print 8-bit values of acceleration in 3-axis */
        x8 = read_register(dev, ADXL362_REG_XDATA);
        y8 = read_register(dev, ADXL362_REG_YDATA);
        z8 = read_register(dev, ADXL362_REG_ZDATA);
        printf(NEWLINE "Axis acceleration (raw - 8bit) - x: %02X, y: %02X, z: %02X", (uint8_t) x8,
                                                                        (uint8_t) y8, (uint8_t) z8);

        /* read and print 12-bit value of acceleration in 3-axis */
        x12 = read_val12(dev, ADXL362_AXIS_X);
        y12 = read_val12(dev, ADXL362_AXIS_Y);
        z12 = read_val12(dev, ADXL362_AXIS_Z);
        printf(NEWLINE "Axis acceleration (raw - 12bit) - x: %04X, y: %04X, z: %04X",
                                                (uint16_t) x12, (uint16_t) y12, (uint16_t) z12);

        print_converted_values((int16_t) (x8 << 4), (int16_t) (y8 << 4), (int16_t) (z8 << 4));
        print_converted_values(x12, y12, z12);
}

static void config_adxl362(spi_device dev)
{
        uint8_t cmd[3];

        /* reset ADXL362 */
        cmd[0] = ADXL362_CMD_WRITE_REG;
        cmd[1] = ADXL362_REG_SOFT_RESET;
        cmd[2] = ADXL362_RESET_CODE;
        ad_spi_write(dev, cmd, sizeof(cmd));
        OS_DELAY_MS(1);

        /*
         * configure filter control register
         * range: 4g
         * halved bandwidth: true
         * external sampling trigger: false
         * output data rate: 25 Hz
         */
        cmd[0] = ADXL362_CMD_WRITE_REG;
        cmd[1] = ADXL362_REG_FILTER_CTL;
        cmd[2] = ADXL362_FILTER_CTL_CODE;
        ad_spi_write(dev, cmd, sizeof(cmd));

        /*
         * configure power control register
         * external clock: false
         * low noise: ultralow noise mode
         * wake-up mode: false
         * autosleep: false
         * measure mode: measure
         */
        cmd[0] = ADXL362_CMD_WRITE_REG;
        cmd[1] = ADXL362_REG_POWER_CTL;
        cmd[2] = ADXL362_POWER_CTL_CODE;
        ad_spi_write(dev, cmd, sizeof(cmd));

        /* wait for first measurement's value */
        OS_DELAY_MS(1000 / 25 + 1);
}

void menu_sensor_board_adxl362_read_func(const struct menu_item *m, bool checked)
{
        spi_device dev;

        dev = ad_spi_open(ADXL362);

        if (config_need) {
                config_adxl362(dev);
                config_need = false;
        }

        sensor_board_adxl362_read(dev);
        ad_spi_close(dev);
}

#endif

#if CFG_DEMO_SENSOR_BMM150
#define BMM150_MAX_12_BIT       0x0FFF
#define BMM150_MAX_14_BIT       0x3FFF
#define BMM150_XY_MAX_VAL       1300    /* uT */
#define BMM150_Z_MAX_VAL        2500    /* uT */

/*
 * Bosch driver passes address to I2C access functions but I2C adapter functions need device handle
 * to add this global variable for this.
 */
static i2c_device bmm150_dev;

/*
 * Function needed by Bosch Sensortec driver
 */
static void bmm150_wrap_delay(BMM050_MDELAY_DATA_TYPE time)
{
        OS_DELAY_MS(time);
}

static void bmm150_done_cb(void *user_data, HW_I2C_ABORT_SOURCE error)
{
        *((int *) user_data) = error;
}

/*
 * Function needed by Bosch Sensortec driver to write data over I2C bus.
 */
static s8 bmm150_wrap_write(u8 device_addr, u8 register_addr, u8 *register_data, u8 wr_len)
{
        volatile int error = -1;

        ad_i2c_async_transact(bmm150_dev, I2C_SND(&register_addr, 1),
                I2C_SND_ST(register_data, wr_len),
                I2C_CB1(bmm150_done_cb, &error),
                I2C_END);
        while (error == -1) {
        }
        return error ? -1 : 0;
}

/*
 * Function needed by Bosch Sensortec driver to write register address and read register value
 * over I2C bus.
 */
static s8 bmm150_wrap_read(u8 device_addr, u8 register_addr, u8 *register_data, u8 wr_len)
{
        return ad_i2c_transact(bmm150_dev, &register_addr, 1, register_data, wr_len) ? -1 : 0;
}

/*
 * Structure for functions needed by Bosch Sensortec driver
 */
static struct bmm050_t bmm150 = {
        .bus_write = bmm150_wrap_write,
        .bus_read = bmm150_wrap_read,
        .delay_msec = bmm150_wrap_delay
};

/* Function converts raw reading to value in uT (multiplied by 100) - X and Y axes */
static inline void convert_xy_value(int16_t val, bool *sign, uint32_t *conv)
{
        *sign = ((val < 0) ? true : false);
        *conv = abs(val) * BMM150_XY_MAX_VAL * 100 / BMM150_MAX_12_BIT;
}

/* Function converts raw reading to value in uT (multiplied by 100) - Z axis */
static inline void convert_z_value(int16_t val, bool *sign, uint32_t *conv)
{
        *sign = ((val < 0) ? true : false);
        *conv = abs(val) * BMM150_Z_MAX_VAL * 100 / BMM150_MAX_14_BIT;
}

static int sensor_board_bmm150_read(i2c_device dev)
{
        /* Structure used for read the mag xyz data*/
        struct bmm050_mag_data_s16_t data;
        /* result of communication results*/
        s32 com_rslt = ERROR;
        bmm150_dev = dev;
        bool sign;
        uint32_t conv_val;

        /*
         * Initialize driver data, feed Bosch driver with functions needed to access I2C bus.
         */
        com_rslt = bmm050_init(&bmm150);

        /*
         * For initialization it is required to set the state of
         * the sensor as "NORMAL MODE"
         * data acquisition/read/write is possible in this mode
         * by using the below API able to set the power mode as NORMAL
         */
        com_rslt += bmm050_set_functional_state(BMM050_NORMAL_MODE);

        /*
         * Set data rate
         */
        com_rslt += bmm050_set_data_rate(BMM050_DATA_RATE_30HZ);

        /* Wait for data ready */
        OS_DELAY(1000 / 30 + 1);

        /* accessing the bmm050_mdata parameter by using data*/
        com_rslt += bmm050_read_mag_data_XYZ(&data);

        /* print raw values */
        printf(NEWLINE "Compensated magnetometer data (raw 16 bit) - x: %04X, y: %04X, z: %04X",
                        (uint16_t) data.datax, (uint16_t) data.datay, (uint16_t) data.dataz);
        /* convert and print values in uT (microtesla unit) */
        printf(NEWLINE "Compensated magnetometer data  - ");
        convert_xy_value(data.datax, &sign, &conv_val);
        printf("x: %s%lu.%02lu uT, ", (sign ? "-" : ""), conv_val / 100, conv_val % 100);
        convert_xy_value(data.datay, &sign, &conv_val);
        printf("y: %s%lu.%02lu uT, ", (sign ? "-" : ""), conv_val / 100, conv_val % 100);
        convert_z_value(data.dataz, &sign, &conv_val);
        printf("z: %s%lu.%02lu uT", (sign ? "-" : ""), conv_val / 100, conv_val % 100);

        /* For de-initialization it is required to set the mode of
         * the sensor as "SUSPEND"
         * the SUSPEND mode set from the register 0x4B bit BMM050_INIT_VALUE should be disabled
         * by using the below API able to set the power mode as SUSPEND
         */
        com_rslt += bmm050_set_functional_state(BMM050_SUSPEND_MODE);

        return com_rslt;
}

void menu_sensor_board_bmm150_read_func(const struct menu_item *m, bool checked)
{
        int err;
        i2c_device dev;

        /*
         * Get handle to sensor device.
         */
        dev = ad_i2c_open(BMM150);

        /*
         * Do actual reading.
         */
        err = sensor_board_bmm150_read(dev);
        if (err) {
                printf(NEWLINE "Error accessing BMM150");
        }

        /*
         * Close device handle.
         */
        ad_i2c_close(dev);
}
#endif

#if CFG_DEMO_SENSOR_BMG160
#define BMG160_MAX_15_BIT       0x7FFF
#define BMG160_RANGE            500    /* deg/s */
/*
 * Bosch driver passes address to I2C access functions but I2C adapter functions need device handle
 * to add this global variable for this.
 */
static i2c_device bmg160_dev;

/*
 * Function needed by Bosch Sensortec driver
 */
static void bmg160_wrap_delay(BMG160_MDELAY_DATA_TYPE time)
{
        OS_DELAY_MS(time + 3);
}

static void bmg160_done_cb(void *user_data, HW_I2C_ABORT_SOURCE error)
{
        *((int *) user_data) = error;
}

/*
 * Function needed by Bosch Sensortec driver to write data over I2C bus.
 */
static s8 bmg160_wrap_write(u8 device_addr, u8 register_addr, u8 *register_data, u8 wr_len)
{
        volatile int error = -1;

        ad_i2c_async_transact(bmg160_dev, I2C_SND(&register_addr, 1),
                I2C_SND_ST(register_data, wr_len),
                I2C_CB1(bmg160_done_cb, &error),
                I2C_END);
        while (error == -1) {
        }
        return error ? -1 : 0;
}

/*
 * Function needed by Bosch Sensortec driver to write register address and read register value
 * over I2C bus.
 */
static s8 bmg160_wrap_read(u8 device_addr, u8 register_addr, u8 *register_data, u8 wr_len)
{
        return ad_i2c_transact(bmg160_dev, &register_addr, 1, register_data, wr_len) ? -1 : 0;
}

/*
 * Structure for functions needed by Bosch Sensortec driver
 */
static struct bmg160_t bmg160 = {
        .bus_write = bmg160_wrap_write,
        .bus_read = bmg160_wrap_read,
        .delay_msec = bmg160_wrap_delay
};

/* Function converts raw reading to value in deg/s (multiplied by 100) */
static inline void convert_gyro_value(int16_t val, bool *sign, uint32_t *conv)
{
        *sign = ((val < 0) ? true : false);
        *conv = abs(val) * BMG160_RANGE * 100 / BMG160_MAX_15_BIT;
}

static int sensor_board_bmg160_read(i2c_device dev)
{
        /* variable used for read the sensor data*/
        s16 v_gyro_datax_s16, v_gyro_datay_s16, v_gyro_dataz_s16 = BMG160_INIT_VALUE;
        /* result of communication results*/
        s32 com_rslt = ERROR;
        bool sign;
        uint32_t conv_val;
        bmg160_dev = dev;

        /*
         * Initialize driver data, feed Bosch driver with functions needed to access I2C bus.
         */
        com_rslt = bmg160_init(&bmg160);

        /* Wait for data ready */
        OS_DELAY_MS(9);

        /*
         * For initialization it is required to set the mode of
         * the sensor as "NORMAL"
         * data acquisition/read/write is possible in this mode
         * by using the below API able to set the power mode as NORMAL
         */
        com_rslt += bmg160_set_power_mode(BMG160_MODE_NORMAL);

        /* Wait for data ready */
        OS_DELAY_MS(9);

        /* Set range */
        com_rslt += bmg160_set_range_reg(BMG160_RANGE_500);

        /*
         * Set bandwidth of 230Hz
         */
        com_rslt += bmg160_set_bw(C_BMG160_BW_230HZ_U8X);

        /* Wait for data ready */
        OS_DELAY_MS(9);

        /* Read Gyro data xyz */
        com_rslt += bmg160_get_data_X(&v_gyro_datax_s16);
        com_rslt += bmg160_get_data_Y(&v_gyro_datay_s16);
        com_rslt += bmg160_get_data_Z(&v_gyro_dataz_s16);

        /* print raw values */
        printf(NEWLINE "Gyroscope data (raw 16 bit) - x: %04X, y: %04X, z: %04X",
                                        (uint16_t) v_gyro_datax_s16, (uint16_t) v_gyro_datay_s16,
                                                                (uint16_t) v_gyro_dataz_s16);
        /* convert and print values in  deg/s (degree per second)*/
        printf(NEWLINE "Gyroscope data - ");
        convert_gyro_value(v_gyro_datax_s16, &sign, &conv_val);
        printf("x: %s%lu.%02lu deg/s, ", (sign ? "-" : ""), conv_val / 100, conv_val % 100);
        convert_gyro_value(v_gyro_datay_s16, &sign, &conv_val);
        printf("y: %s%lu.%02lu deg/s, ", (sign ? "-" : ""), conv_val / 100, conv_val % 100);
        convert_gyro_value(v_gyro_dataz_s16, &sign, &conv_val);
        printf("z: %s%lu.%02lu deg/s", (sign ? "-" : ""), conv_val / 100, conv_val % 100);

        /*
         * For de-initialization it is required to set the mode of
         * the sensor as "DEEPSUSPEND"
         * the device reaches the lowest power consumption only
         * interface selection is kept alive
         * No data acquisition is performed
         * The DEEPSUSPEND mode set from the register 0x11 bit 5
         * by using the below API able to set the power mode as DEEPSUSPEND
         * For the read/ write operation it is required to provide least 450us
         * micro second delay
         */
        com_rslt += bmg160_set_power_mode(BMG160_MODE_DEEPSUSPEND);

        return com_rslt;
}

void menu_sensor_board_bmg160_read_func(const struct menu_item *m, bool checked)
{
        int err;
        i2c_device dev;

        /*
         * Get handle to sensor device.
         */
        dev = ad_i2c_open(BMG160);

        /*
         * Do actual reading.
         */
        err = sensor_board_bmg160_read(dev);
        if (err) {
                printf(NEWLINE "Error accessing BMG160");
        }

        /*
         * Close device handle.
         */
        ad_i2c_close(dev);
}
#endif

#if CFG_DEMO_SENSOR_BOARD
volatile static bool sensor_cyclic_read_enabled;

void task_sensor_board_worker_func(const struct task_item *task)
{
        for (;;) {
                OS_DELAY(OS_MS_2_TICKS(5000));

                if (sensor_cyclic_read_enabled) {
#if CFG_DEMO_SENSOR_BH1750
                        printf(NEWLINE NEWLINE "Light Sensor");
                        menu_sensor_board_bh1750_read_func(NULL, false);
#endif
#if CFG_DEMO_SENSOR_BME280
                        printf(NEWLINE NEWLINE "Environmental Sensor");
                        menu_sensor_board_bme280_read_func(NULL, false);
#endif
#if CFG_DEMO_SENSOR_ADXL362
                        printf(NEWLINE NEWLINE "Accelerometer");
                        menu_sensor_board_adxl362_read_func(NULL, false);
#endif
#if CFG_DEMO_SENSOR_BMM150
                        printf(NEWLINE NEWLINE "Geomagnetic Sensor");
                        menu_sensor_board_bmm150_read_func(NULL, false);
#endif
#if CFG_DEMO_SENSOR_BMG160
                        printf(NEWLINE NEWLINE "Gyroscope Sensor");
                        menu_sensor_board_bmg160_read_func(NULL, false);
#endif
                }
        }
}

void menu_sensor_board_cyclic_read_func(const struct menu_item *m, bool checked)
{
        sensor_cyclic_read_enabled ^= true;
}

bool menu_sensor_board_cyclic_read_checked_cb_func(const struct menu_item *m)
{
        return sensor_cyclic_read_enabled;
}
#endif

#if CFG_DEMO_AD_TEMPSENS

typedef struct {
        OS_EVENT event;
        int temp_val;
} user_data_t;

volatile static bool cyclic_read_enabled;

void menu_temperature_sensor_read_func(const struct menu_item *m, bool checked)
{
        int temp_val;
        tempsens_source src;

        src = ad_tempsens_open();

        temp_val = ad_tempsens_read(src);

        ad_tempsens_close(src);

        printf(NEWLINE "Temperature: %d[C]", temp_val);
}

static void temp_sens_read_cb(void *user_data, int value)
{
        user_data_t *ud = (user_data_t *) user_data;

        ud->temp_val = hw_tempsens_convert_to_temperature(value);

        OS_EVENT_SIGNAL_FROM_ISR(ud->event);
}

void menu_temperature_sensor_read_async_func(const struct menu_item *m, bool checked)
{
        user_data_t user_data;
        tempsens_source src;

        OS_EVENT_CREATE(user_data.event);

        src = ad_tempsens_open();

        ad_tempsens_read_async(src, temp_sens_read_cb, &user_data);

        printf(NEWLINE "Waiting for the result ...");

        OS_EVENT_WAIT(user_data.event, OS_EVENT_FOREVER);

        ad_tempsens_close(src);

        printf(NEWLINE "Temperature: %d[C]", user_data.temp_val);
}

void task_temperature_sensor_worker_func(const struct task_item *task)
{
        tempsens_source src;
        int temp_val;

        /*
         * Open temperature sensor.
         * This will not start any measurement yet.
         */
        src = ad_tempsens_open();

        for (;;) {
                OS_DELAY(OS_MS_2_TICKS(5000));

                if (cyclic_read_enabled) {
                        temp_val = ad_tempsens_read(src);

                        printf(NEWLINE "Temperature: %d[C]", temp_val);
                }
        }

        /*
         * This task will not use temperature sensor for time being, close it.
         */
        ad_tempsens_close(src);
}

void menu_temperature_sensor_read_cyclic_func(const struct menu_item *m, bool checked)
{
        cyclic_read_enabled ^= true;
}

bool menu_temperature_sensor_read_cyclic_checked_cb_func(const struct menu_item *m)
{
        return cyclic_read_enabled;
}

#endif // CFG_DEMO_AD_TEMPSENS
