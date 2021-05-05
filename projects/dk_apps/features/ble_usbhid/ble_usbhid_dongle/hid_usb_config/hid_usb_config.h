/**
 ****************************************************************************************
 *
 * @file hid_usb_config.h
 *
 * @brief Global configuration for HID Device
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef HID_USB_CONFIG_H_
#define HID_USB_CONFIG_H_

/*
 * USB device descriptor configuration
 *
 * These values configure device descriptor used for enumeration.
 */
#define HID_USB_VID              (0x2DCF)                       /* Vendor ID */
#define HID_USB_PID              (0x6004)                       /* Product ID */
#define HID_USB_VENDOR_NAME     "Dialog Semiconductor"          /* Manufacturer name */
#define HID_USB_PRODUCT_NAME    "Dialog BLE-USB HID dongle"     /* Product name */
#define HID_USB_SERIAL_NUMBER   "00000001"                      /* Serial number */

#endif /* HID_USB_CONFIG_H_ */
