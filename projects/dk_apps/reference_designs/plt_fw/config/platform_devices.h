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

#include <ad_gpadc.h>
#include <ad_uart.h>

#ifdef __cplusplus
extern "C" {
#endif

UART_BUS(UART2, DGTL_UART, HW_UART_BAUDRATE_115200, HW_UART_DATABITS_8, HW_UART_PARITY_NONE,
                                HW_UART_STOPBITS_1, CONFIG_USE_HW_FLOW_CONTROL, 1, HW_DMA_CHANNEL_1, HW_DMA_CHANNEL_0, 0, 0)

GPADC_SOURCE(TEMP_SENSOR, HW_GPADC_CLOCK_INTERNAL, HW_GPADC_INPUT_MODE_SINGLE_ENDED,
                                HW_GPADC_INPUT_SE_TEMPSENS, 5, false, HW_GPADC_OVERSAMPLING_1_SAMPLE,
                                                                HW_GPADC_INPUT_VOLTAGE_UP_TO_1V2)

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_DEVICES_H_ */

/**
 * \}
 * \}
 * \}
 */
