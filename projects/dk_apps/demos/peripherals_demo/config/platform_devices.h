/**
 * \addtogroup BSP
 * \{
 * \addtogroup ADAPTERS
 * \{
 * \addtogroup CONFIGURATION
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file platform_devices.h
 *
 * @brief Configuration of devices connected to board
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef PLATFORM_DEVICES_H_
#define PLATFORM_DEVICES_H_

/* SPI adapter fine-tuning */
#define CONFIG_SPI_ONE_DEVICE_ON_BUS            (0)
#define CONFIG_SPI_EXCLUSIVE_OPEN               (0)

/* I2C adapter fine-tuning */
# define CONFIG_I2C_ENABLE_CRITICAL_SECTION     (1)

#include "config.h"
#include <ad_spi.h>
#include <ad_i2c.h>
#include <ad_uart.h>
#include <ad_gpadc.h>
#include <ad_temp_sens.h>

#ifdef __cplusplus
extern "C" {
#endif

#if dg_configUART_ADAPTER

        UART_BUS(UART1, SERIAL1, HW_UART_BAUDRATE_115200, HW_UART_DATABITS_8, HW_UART_PARITY_NONE,
                HW_UART_STOPBITS_1, 0, 0, HW_DMA_CHANNEL_1, HW_DMA_CHANNEL_0, 0, 0)

        UART_BUS(UART2, SERIAL2, HW_UART_BAUDRATE_115200, HW_UART_DATABITS_8, HW_UART_PARITY_NONE,
                HW_UART_STOPBITS_1, 0, 1, HW_DMA_CHANNEL_3, HW_DMA_CHANNEL_2, 0, 0)

#endif /* dg_configUART_ADAPTER */


#if dg_configSPI_ADAPTER

        /*
         * Define devices connected to SPI bus
         *
         * Following section describes devices connected to SPI.
         * Each SPI_SLAVE_DEVICE entry describes parameters that must be configured to access
         * device. Some of them can be found in data sheet of device (bus width, max speed allowed,
         * polarity and phase of the clock). Other parameters describe connection specifics (GPIO
         * port and pin number for chip select).
         */
        SPI_BUS(SPI1)
        #ifdef CONFIG_AT45DB011D
                SPI_SLAVE_DEVICE(SPI1, AT45DB011D, HW_GPIO_PORT_2, HW_GPIO_PIN_0, HW_SPI_WORD_8BIT,
                                        HW_SPI_POL_LOW, HW_SPI_PHA_MODE_0, HW_SPI_FREQ_DIV_14, 0);
        #endif
        #if CFG_DEMO_SENSOR_ADXL362
                SPI_SLAVE_DEVICE(SPI1, ADXL362, HW_GPIO_PORT_3, HW_GPIO_PIN_6, HW_SPI_WORD_8BIT,
                                                HW_SPI_POL_LOW, HW_SPI_PHA_MODE_0, HW_SPI_FREQ_DIV_4,
                                                                                        HW_DMA_CHANNEL_4);
        #endif //CFG_ADXL362
        #ifdef CONFIG_SPI_SLAVE
                SPI_SLAVE_TO_EXT_MASTER(SPI1, SLAVE1, false, HW_SPI_WORD_8BIT, HW_SPI_POL_LOW,
                                                                                HW_SPI_PHA_MODE_0, -1);
        #endif
        SPI_BUS_END

        SPI_BUS(SPI2)
        #if CONFIG_SOME_SLAVE_DEVICE
                SPI_SLAVE_DEVICE(SPI2, SP1_SLAVE, HW_GPIO_PORT_3, HW_GPIO_PIN_1, HW_SPI_WORD_8BIT,
                                                HW_SPI_POL_LOW, HW_SPI_PHA_MODE_0, HW_SPI_FREQ_DIV_14, -1);
        #endif
        SPI_BUS_END

#endif /* dg_configSPI_ADAPTER */


#if dg_configI2C_ADAPTER

        /*
         * Define devices connected to I2C bus
         *
         * Following section describes devices connected to I2C.
         * Each I2C_SLAVE_DEVICE entry describes parameters that must be configured to access
         * the device. Some of them can be found in data sheet of device (max speed allowed, address length
         * address or some of it bits). Device address can be also determined by some pins allowing to
         * connect several devices of same kind.
         */
        I2C_BUS(I2C1)
        #if CFG_DEMO_SENSOR_BME280
                /* Combined humidity and pressure sensor */
                I2C_SLAVE_DEVICE(I2C1, BME280, 0x76, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_STANDARD);
        #endif

        #if CFG_DEMO_SENSOR_BMM150
                /* Geomagnetic Sensor */
                I2C_SLAVE_DEVICE(I2C1, BMM150, 0x10, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_STANDARD);
        #endif

        #if CFG_DEMO_SENSOR_BMG160
                /* Digital, triaxial gyroscope sensor */
                I2C_SLAVE_DEVICE(I2C1, BMG160, 0x68, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_STANDARD);
        #endif

        #if CFG_DEMO_SENSOR_BH1750
                /* Light Sensor */
                I2C_SLAVE_DEVICE(I2C1, BH1750, 0x23, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_FAST);
        #endif

        #ifdef CONFIG_24LC256
                /* Example EEPROM */
                I2C_SLAVE_DEVICE(I2C1, MEM_24LC256, 0x50, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_STANDARD);
        #endif

        #ifdef CONFIG_FM75
                /* Example temperature sensor FM75 */
                I2C_SLAVE_DEVICE(I2C1, FM75, 0x4F, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_STANDARD);
        #endif

        #ifdef CONFIG_MPL3115A2
                /* Example pressure sensor */
                I2C_SLAVE_DEVICE(I2C1, MPL3115A2, 0x60, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_STANDARD);
        #endif

        #ifdef CONFIG_ADT7420
                /* Example temperature sensor */
                I2C_SLAVE_DEVICE(I2C1, ADT7420, 0x48, HW_I2C_ADDRESSING_7B, HW_I2C_SPEED_STANDARD);
        #endif
        I2C_BUS_END

        I2C_BUS(I2C2)
        I2C_BUS_END

#endif /* dg_configI2C_ADAPTER */


#if dg_configGPADC_ADAPTER

        /*
         * Define measurements connected to GPADC
         */
        GPADC_SOURCE(REF_POINT_3V3, HW_GPADC_CLOCK_DIGITAL, HW_GPADC_INPUT_MODE_SINGLE_ENDED,
                        HW_GPADC_INPUT_SE_V33, 5, true, HW_GPADC_OVERSAMPLING_1_SAMPLE,
                        HW_GPADC_INPUT_VOLTAGE_UP_TO_3V6)


        GPADC_SOURCE(BATTERY_LEVEL, HW_GPADC_CLOCK_INTERNAL, HW_GPADC_INPUT_MODE_SINGLE_ENDED,
                        HW_GPADC_INPUT_SE_VBAT, 15, true, HW_GPADC_OVERSAMPLING_4_SAMPLES,
                        HW_GPADC_INPUT_VOLTAGE_UP_TO_1V2)

        GPADC_SOURCE(TEMP_SENSOR, HW_GPADC_CLOCK_INTERNAL, HW_GPADC_INPUT_MODE_SINGLE_ENDED,
                        HW_GPADC_INPUT_SE_TEMPSENS, 5, false, HW_GPADC_OVERSAMPLING_1_SAMPLE,
                        HW_GPADC_INPUT_VOLTAGE_UP_TO_1V2)

        GPADC_SOURCE(LIGHT_SENSOR, HW_GPADC_CLOCK_DIGITAL, HW_GPADC_INPUT_MODE_SINGLE_ENDED,
                        HW_GPADC_INPUT_SE_P12, 5, true, HW_GPADC_OVERSAMPLING_64_SAMPLES,
                        HW_GPADC_INPUT_VOLTAGE_UP_TO_3V6)

        GPADC_SOURCE(ENCODER_SENSOR, HW_GPADC_CLOCK_DIGITAL, HW_GPADC_INPUT_MODE_DIFFERENTIAL,
                        HW_GPADC_INPUT_DIFF_P12_P14, 5, true, HW_GPADC_OVERSAMPLING_1_SAMPLE,
                        HW_GPADC_INPUT_VOLTAGE_UP_TO_3V6)

#endif /* dg_configGPADC_ADAPTER */

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_DEVICES_H_ */

/**
 * \}
 * \}
 * \}
 */
