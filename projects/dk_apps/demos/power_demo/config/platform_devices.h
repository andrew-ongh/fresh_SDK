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
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef PLATFORM_DEVICES_H_
#define PLATFORM_DEVICES_H_

#include <ad_uart.h>
#include <ad_gpadc.h>

#ifdef __cplusplus
extern "C" {
#endif

#if dg_configUART_ADAPTER && dg_configUSE_CONSOLE

UART_BUS(UART2, SERIAL_CONSOLE, HW_UART_BAUDRATE_115200, HW_UART_DATABITS_8, HW_UART_PARITY_NONE,
                                HW_UART_STOPBITS_1, 1, 1, HW_DMA_CHANNEL_INVALID, HW_DMA_CHANNEL_INVALID, 0, 0)

#endif /* dg_configUART_ADAPTER && dg_configUSE_CONSOLE */


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
}
#endif

#endif /* PLATFORM_DEVICES_H_ */

/**
 * \}
 * \}
 * \}
 */
