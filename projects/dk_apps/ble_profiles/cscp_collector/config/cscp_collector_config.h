/**
 ****************************************************************************************
 *
 * @file cscp_collector_config.h
 *
 * @brief Application configuration
 *
 * Copyright (C) 2016-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef CSCP_COLLECTOR_CONFIG_H_
#define CSCP_COLLECTOR_CONFIG_H_

/**
 * Wheel's circumference in millimeters
 */
#define CFG_WHEEL_CIRCUMFERENCE 2100

/**
 * Bond with remote after connection establishment
 */
#ifndef CFG_BOND_AFTER_CONNECT
#define CFG_BOND_AFTER_CONNECT  1
#endif

#endif /* CSCP_COLLECTOR_CONFIG_H_ */
