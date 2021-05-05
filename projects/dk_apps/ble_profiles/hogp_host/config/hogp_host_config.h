/**
 ****************************************************************************************
 *
 * @file hogp_host_config.h
 *
 * @brief Application configuration
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef HOGP_HOST_CONFIG_H_
#define HOGP_HOST_CONFIG_H_

/**
 * HOGP Host protocol mode
 *
 * Set to HIDS_CLIENT_PROTOCOL_MODE_BOOT to enable Boot protocol mode or
 * HIDS_CLIENT_PROTOCOL_MODE_REPORT to enable Report protocol mode
 */
#define CFG_REPORT_MODE         HIDS_CLIENT_PROTOCOL_MODE_BOOT

/**
 * Peripheral device address
 *
 * Modify it to connect specified remote device. Field addr_type (address type)
 * might be one of PUBLIC_ADDRESS or PRIVATE_ADDRESS, addr is remote device address
 * (6 element array of octets)
 */
#define CFG_PERIPH_ADDR                                         \
        {                                                       \
                .addr_type = PUBLIC_ADDRESS,                    \
                .addr = { 0xE6, 0x0D, 0x11, 0x9E, 0x15, 0x00 }, \
        }

/**
 * Connection parameters
 *
 * Connection parameters used to establish connection.
 */
#define CFG_CONN_PARAMS                                                 \
        {                                                               \
                .interval_min = BLE_CONN_INTERVAL_FROM_MS(50),          \
                .interval_max = BLE_CONN_INTERVAL_FROM_MS(70),          \
                .slave_latency = 0,                                     \
                .sup_timeout = BLE_SUPERVISION_TMO_FROM_MS(420),        \
        }

/**
 * Flag indicating if HOST should automatically enable all Input reports
 */
#define CFG_AUTO_ENABLE_NOTIFICATIONS (true)

/**
 * Port and pin which are used to trigger connect/disconnect
 */
#define CFG_TRIGGER_CONNECT_GPIO_PORT (HW_GPIO_PORT_1)
#define CFG_TRIGGER_CONNECT_GPIO_PIN (HW_GPIO_PIN_0)

/**
 * Default Scan Parameters value
 */
#define CFG_SCAN_PARAMS                                         \
        {                                                       \
                .interval = BLE_SCAN_INTERVAL_FROM_MS(30),      \
                .window = BLE_SCAN_WINDOW_FROM_MS(30)           \
        }

#endif /* HOGP_HOST_CONFIG_H_ */
