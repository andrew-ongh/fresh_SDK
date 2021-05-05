/**
 ****************************************************************************************
 *
 * @file hrp_sensor_config.h
 *
 * @brief Application configuration.
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef CONFIG_HRP_SENSOR_CONFIG_H_
#define CONFIG_HRP_SENSOR_CONFIG_H_

/**
 * Port and pin configuration for 16-bit HRP measurement value trigger
 */
#define CFG_SEND_16_BIT_VALUE_TRIGGER_GPIO_PORT   (HW_GPIO_PORT_1)
#define CFG_SEND_16_BIT_VALUE_TRIGGER_GPIO_PIN    (HW_GPIO_PIN_2)

/**
 * Port and pin configuration for starting advertising if it is turned off
 */
#define CFG_START_ADVERTISING_TRIGGER_GPIO_PORT   (HW_GPIO_PORT_1)
#define CFG_START_ADVERTISING_TRIGGER_GPIO_PIN    (HW_GPIO_PIN_6)

/**
 * Pair after connect flag
 */
#ifndef CFG_PAIR_AFTER_CONNECT
#define CFG_PAIR_AFTER_CONNECT  0
#endif

#endif /* CONFIG_HRP_SENSOR_CONFIG_H_ */
