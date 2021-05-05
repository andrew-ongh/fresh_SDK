/**
 ****************************************************************************************
 *
 * @file common.h
 *
 * @brief Common interface
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdint.h>
#include "ble_service.h"

extern ble_service_t *hids;

/**
 * Initialize device-specific code
 *
 * This function is called when application starts. Any initialization code can be placed here.
 *
 */
void device_init(void);

/**
 * Device connected handler
 *
 * This function is called when HID host is connected and set up, i.e. after security is completed.
 *
 */
void device_connected(void);

/**
 * Device disconnected handler
 *
 * This function is called when HID host is disconnected.
 *
 */
void device_disconnected(void);

/**
 * Protocol mode change handler
 *
 * This function is called when HID host requested to switch protocol mode. It is also called just
 * after link with HID host is established to allow initialize protocol mode properly on new
 * connection (in this case it can be called *before* device connected handler).
 *
 * \param [in] mode  new protocol mode
 *
 */
void device_protocol_mode_set(hids_protocol_mode_t mode);

/**
 * Boot keyboard report written handler
 *
 * This function is called when boot keyboard report characteristic has been written by HID host.
 *
 * \param [in] length  data length
 * \param [in] data    report data
 *
 */
void device_boot_keyboard_report_written(uint16_t length, const uint8_t *data);

/**
 * Report written handler
 *
 * This function is called when any available report (except for boot keyboard
 * report, see device_protocol_mode_set()) has been written by HID host.
 *
 * \param [in] type    report type
 * \param [in] id      report id
 * \param [in] length  data length
 * \param [in] data    report data
 *
 */
void device_report_written(hids_report_type_t type, uint8_t id,
                                                        uint16_t length, const uint8_t *data);

/**
 * Device suspend handler
 *
 * This function is called when HID host entered sleep mode.
 *
 */
void device_suspend(void);

/**
 * Device wake up handler
 *
 * This function is called when HID host woken up.
 *
 */
void device_wakeup(void);

/**
 * Task notification
 *
 * This function is called when main task received notification targeted for device-specific code.
 *
 * \param [in] notif  notification mask
 *
 */
void device_task_notif(uint32_t notif);
