/**
 ****************************************************************************************
 *
 * @file common.h
 *
 * @brief Common interface between BLE and USB
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

enum hid_op {
        HID_OP_DATA,
        HID_OP_GET_REPORT,
        HID_OP_SET_REPORT,
};

void send_to_ble(enum hid_op op, uint8_t type, uint8_t id, uint16_t length, const uint8_t *data);

void send_to_usb(enum hid_op op, uint8_t type, uint8_t id, uint16_t length, const uint8_t *data);

int usb_on_get_report(bool has_ble, uint8_t type, uint8_t id, uint32_t *length, uint8_t *data);

int usb_on_set_report(bool has_ble, uint8_t type, uint8_t id, uint32_t length, const uint8_t *data);

int usb_on_data(bool has_ble, uint8_t type, uint8_t id, uint32_t length, const uint8_t *data);

void notify_state_changed_to_ble(void);

void notify_state_changed_to_usb(void);

bool get_ble_state(void);

bool get_usb_state(void);
