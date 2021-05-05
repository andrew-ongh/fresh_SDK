/**
 ****************************************************************************************
 *
 * @file power_demo_config.h
 *
 * @brief Application configuration.
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef POWER_DEMO_CONFIG_H_
#define POWER_DEMO_CONFIG_H_

/**
 * Set power configuration over CLI
 */
#define POWER_DEMO_CLI_CONFIGURATION    (1)

/**
 * Set power configuration through GPIO settings
 */
#define POWER_DEMO_GPIO_CONFIGURATION   (0)

/**
 * Set config console write timeout
 */
#define CONFIG_CONSOLE_WRITE_TIMEOUT    (0x20)

#if POWER_DEMO_CLI_CONFIGURATION && POWER_DEMO_GPIO_CONFIGURATION
#error "Set one configuration only, POWER_DEMO_CLI_CONFIGURATION or POWER_DEMO_GPIO_CONFIGURATION"
#endif

#endif /* POWER_DEMO_CONFIG_H_ */
