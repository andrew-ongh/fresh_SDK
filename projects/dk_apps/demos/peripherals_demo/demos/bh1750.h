/**
 * \addtogroup EXAMPLES
 * \{
 * \addtogroup OS
 * \{
 * \addtogroup PERIPHERALS
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file bh1750.h
 *
 * @brief Ambient Light Sensor BH1750 driver.
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef BH1750_H_
#define BH1750_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BH1750_OP_POWER_DOWN            0x00
#define BH1750_OP_POWER_ON              0x01
#define BH1750_OP_RESET                 0x07
#define BH1750_OP_CONTINUOUS_HRES_MODE  0x10
#define BH1750_OP_CONTINUOUS_HRES_MODE2 0x11
#define BH1750_OP_CONTINUOUS_LRES_MODE  0x13
#define BH1750_OP_ONE_TIME_HRES_MODE    0x20
#define BH1750_OP_ONE_TIME_HRES_MODE2   0x21
#define BH1750_OP_ONE_TIME_LRES_MODE    0x23
#define BH1750_OP_CHANGE_MEASURE_TIME_H 0x40
#define BH1750_OP_CHANGE_MEASURE_TIME_L 0x50

#define BH1750_HI_RES_MEASUREMENT_TIME_MS 180
#define BH1750_LO_RES_MEASUREMENT_TIME_MS 24

typedef enum {
        BH1750_RESOLUTION_ONE_LUX = 0,
        BH1750_RESOLUTION_HALF_LUX = 1,
        BH1750_RESOLUTION_FOUR_LUX = 3,
} bh1750_measurement_resolution_t;

typedef enum {
        BH1750_MODE_CONTINUOUS = 0x10,
        BH1750_MODE_ONE_TIME = 0x20,
} bh1750_measurement_mode_t;

/**
 * \brief Enter power down state
 *
 * No measurement is possible in this state, bh1750_power_on() must be called before measurement
 * can be performed. Power down mode is default state after VCC and DVI supply.
 *
 * \param [in] dev device returned from i2c_open()
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 * \sa bh1750_power_on
 *
 */
int bh1750_power_down(i2c_device dev);

/**
 * \brief Enter power on state
 *
 * In this state measurement can be performed.
 *
 * \param [in] dev device returned from i2c_open()
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 * \sa bh1750_power_down
 *
 */
int bh1750_power_on(i2c_device dev);

/**
 * \brief Reset data register
 *
 * This must be done in power on mode.
 *
 * \param [in] dev device returned from i2c_open()
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 * \sa bh1750_power_on
 *
 */
int bh1750_reset(i2c_device dev);

/**
 * \brief Start measurement
 *
 * Starts measurement with given resolution. Resolution can be 1/2, 1, and 4 lux.
 * Mode tells what to do after measurement is read, if mode is BH1750_MODE_CONTINUOUS chip stays
 * in power on mode after readout. If mode is BH1750_MODE_ONE_TIME chip goes to power down mode
 * after readout.
 * After starting measurement chip needs time to prepare value, for 4 lux resolution max
 * BH1750_LO_RES_MEASUREMENT_TIME_MS time is needed before value can be read.
 * For 1/2 and 1 lux resolution BH1750_HI_RES_MEASUREMENT_TIME_MS is needed before call
 * to bh1750_read_measurement().
 *
 *
 * \param [in] dev device returned from i2c_open()
 * \param [in] res requested measurement resolution
 * \param [in] mode requested measurement mode
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 * \sa bh1750_power_on
 * \sa bh1750_read_measurement
 *
 */
int bh1750_start_measurement(i2c_device dev, bh1750_measurement_resolution_t res,
                                                                bh1750_measurement_mode_t mode);

/**
 * \brief Read measurement
 *
 * Read measurement started with bh1750_start_measurement().
 * This function should be called after BH1750_HI_RES_MEASUREMENT_TIME_MS (for 4 lux resolution)
 * or BH1750_HI_RES_MEASUREMENT_TIME_MS for higher resolution selected in bh1750_start_measurement().
 * Output illuminance value should be divided by 1.2 to get value in Lx.
 *
 * \param [in] dev device returned from i2c_open()
 * \param [out] value illuminance value
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 * \sa bh1750_power_on
 * \sa bh1750_read_measurement
 *
 */
int bh1750_read_measurement(i2c_device dev, uint16_t *value);

/**
 * \brief Change measurement time
 *
 * This function allows to change MTReg to adjust measurement result for influence of optical window
 * (refer to datasheet of BH1750FVI for explanation).
 * Default value is 69.
 *
 * \param [in] dev device returned from i2c_open()
 * \param [in] value new value of MTreg
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 *
 */
int bh1750_change_measurement_time(i2c_device dev, uint8_t value);

/**
 * \brief Read measurement value
 *
 * This function start reading measurement waits for result to be ready and reads measurement.
 * Output illuminance value should be divided by 1.2 to get value in Lx.
 * Chip must be in power up mode before this function is called.
 * If mode is BH1750_MODE_ONE_TIME chip goes power down after readout.
 *
 * \param [in] dev device returned from i2c_open()
 * \param [in] res requested measurement resolution
 * \param [in] mode requested measurement mode
 * \param [out] value illuminance value
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 * \sa bh1750_start_measurement
 * \sa bh1750_read_measurement
 *
 */
int bh1750_read(i2c_device dev, bh1750_measurement_resolution_t res,
                                                bh1750_measurement_mode_t mode, uint16_t *value);

#ifdef __cplusplus
}
#endif

#endif /* BH1750_H_ */

/**
 * \}
 * \}
 * \}
 */
