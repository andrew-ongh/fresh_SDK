/**
 ****************************************************************************************
 *
 * @file hid_ble_config.h
 *
 * @brief HID device configuration (BLE part)
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef HID_BLE_CONFIG_H_
#define HID_BLE_CONFIG_H_

#include "hids.h"

/*
 * GAP characteristics values
 *
 * These symbols define values for Device Name and Appearance characteristics of GAP Service.
 *
 * References:
 * - Bluetooth Core Specification 4.2, Vol 3, Part C, Section 12
 */
#define HID_BLE_DEVICE_NAME             "Dialog HoGP"
#define HID_BLE_APPEARANCE              (BLE_GAP_APPEARANCE_GENERIC_HID)

/*
 * Device Information PnP ID characteristic values *
 *
 * These symbols define value for PnP ID characteristic.
 *
 * References:
 * - Device Information Service 1.1, section 3.9.
 */
#define HID_BLE_PNP_VID_SOURCE          (0x01)   /* Bluetooth SIG assigned Company Identifier */
#define HID_BLE_PNP_VID                 (0x00D2) /* Dialog Semiconductor B.V. */
#define HID_BLE_PNP_PID                 (0x0000) /* 0x0000 - managed by vendor */
#define HID_BLE_PNP_VERSION             (0x0100) /* 1.0.0 - managed by vendor */

/*
 * HID report map
 *
 * This symbol defines raw report map used by device when operating in report mode. The report map
 * depends on type and capabilities of device and is defined by vendor.
 *
 * The default report map defines reports for mouse and keyboard (i.e. combo device) and they have
 * the same structure as used in boot protocol mode. The report ids for mouse and keyboard are 0x01
 * and 0x02 respectively.
 *
 * References:
 * - HID Service specification 1.0, section 2.6
 * - USB Device Class Definition for HID, section 6
 * - USB Device Class Definition for HID, Appendix D
 */
#define HID_BLE_REPORT_MAP \
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, \
        0xA1, 0x00, 0x85, 0x01, 0x05, 0x09, 0x19, 0x01, \
        0x29, 0x08, 0x15, 0x00, 0x25, 0x01, 0x95, 0x08, \
        0x75, 0x01, 0x81, 0x02, 0x05, 0x01, 0x09, 0x30, \
        0x09, 0x31, 0x15, 0x80, 0x25, 0x7F, 0x75, 0x08, \
        0x95, 0x02, 0x81, 0x06, 0xC0, 0xC0, 0x09, 0x06, \
        0xA1, 0x01, 0x85, 0x02, 0x05, 0x07, 0x19, 0xE0, \
        0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, \
        0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, \
        0x81, 0x01, 0x95, 0x03, 0x75, 0x01, 0x05, 0x08, \
        0x19, 0x01, 0x29, 0x03, 0x91, 0x02, 0x95, 0x05, \
        0x75, 0x01, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, \
        0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, \
        0x29, 0x65, 0x81, 0x00, 0xC0,                   \

/*
 * HID reports list
 *
 * This symbol defines list of reports included in a report map. Reports listed here shall match
 * report map defined by \p HID_BLE_REPORT_MAP to allow service register proper characteristics.
 *
 * References:
 * * HID Service specification 1.0, section 2.5
 */
#define HID_BLE_REPORTS \
        HID_REPORT(INPUT,  0x01, 3), \
        HID_REPORT(INPUT,  0x02, 8), \
        HID_REPORT(OUTPUT, 0x02, 1), \

/*
 * Maximum length of HID report
 *
 * This value should be (at least) the length of largest report defined in report map.
 */
#define HID_BLE_MAX_REPORT_LENGTH       (8)

/*
 * Type of boot device
 *
 * This symbol defines the type of device when working in boot protocol mode. Allowed values are as
 * in * ::hids_boot_device_t.
 *
 * References:
 * * HID Service specification 1.0, section 2.7-2.9
 */
#define HID_BLE_BOOT_DEVICE_TYPE        (HIDS_BOOT_DEVICE_COMBO)

/*
 * Country code
 *
 * This value defines country code as defined by HID specification.
 *
 * References:
 * * HID Service specification 1.0, section 2.11
 */
#define HID_BLE_INFO_COUNTRY_CODE       (0x00)

/*
 * BLE HID Device configuration
 *
 * This is prototype for common BLE HID Device configuration. It is configured automatically using
 * values above and should not be modified manually.
 *
 */
extern const hids_config_t hid_ble_config;

#endif /* HID_BLE_CONFIG_H_ */
