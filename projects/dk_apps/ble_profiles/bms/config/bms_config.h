/**
 ****************************************************************************************
 *
 * @file bms_config.h
 *
 * @brief Application configuration
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef BMS_CONFIG_H_
#define BMS_CONFIG_H_

// authentication code used for appropriate operations
#define CFG_AUTH_CODE           "bms_auth_code"

/**
 * Delete bond button pin configuration.
 */
#define CFG_TRIGGER_DELETE_BOND_GPIO_PORT       (HW_GPIO_PORT_1)
#define CFG_TRIGGER_DELETE_BOND_GPIO_PIN        (HW_GPIO_PIN_6)

#endif /* BMS_CONFIG_H_ */
