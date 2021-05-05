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
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef PLATFORM_DEVICES_H_
#define PLATFORM_DEVICES_H_

#define CONFIG_SPI_ONE_DEVICE_ON_BUS 0

#define CONFIG_SPI_EXCLUSIVE_OPEN 0

#include <ad_spi.h>
#include <ad_i2c.h>
#include <ad_uart.h>
#include <ad_temp_sens.h>
#include <ad_battery.h>

#ifdef __cplusplus
extern "C" {
#endif

#if dg_configUART_ADAPTER

UART_BUS(UART2, SERIAL_CONSOLE, HW_UART_BAUDRATE_115200, HW_UART_DATABITS_8, HW_UART_PARITY_NONE,
                                HW_UART_STOPBITS_1, 1, 1, HW_DMA_CHANNEL_3, HW_DMA_CHANNEL_2, 0, 0)

#endif /* dg_configUART_ADAPTER */

#if dg_configGPADC_ADAPTER

/*
 * Define sources connected to GPADC
 */

GPADC_SOURCE(TEMP_SENSOR, HW_GPADC_CLOCK_INTERNAL, HW_GPADC_INPUT_MODE_SINGLE_ENDED,
                                HW_GPADC_INPUT_SE_TEMPSENS, 5, false, HW_GPADC_OVERSAMPLING_1_SAMPLE,
                                                                HW_GPADC_INPUT_VOLTAGE_UP_TO_1V2)

GPADC_SOURCE(BATTERY_LEVEL, HW_GPADC_CLOCK_INTERNAL, HW_GPADC_INPUT_MODE_SINGLE_ENDED,
                                HW_GPADC_INPUT_SE_VBAT, 15, true, HW_GPADC_OVERSAMPLING_4_SAMPLES,
                                HW_GPADC_INPUT_VOLTAGE_UP_TO_1V2)

#endif /* dg_configGPADC_ADAPTER */

#ifdef __cplusplus
extern }
#endif

#endif /* PLATFORM_DEVICES_H_ */

/**
 * \}
 * \}
 * \}
 */
