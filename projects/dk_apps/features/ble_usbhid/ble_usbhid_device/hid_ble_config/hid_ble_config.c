/**
 ****************************************************************************************
 *
 * @file hid_ble_config.c
 *
 * @brief HID device configuration (BLE part)
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include "hid_ble_config.h"

/*
 * Do not modify this file directly - see hid_ble_config.h
 */

#define HID_REPORT(_type, _id, _length) \
        {                                               \
                .type = HIDS_REPORT_TYPE_##_type,       \
                .report_id = _id,                       \
                .length = _length,                      \
        }

static const uint8_t report_map[] = {
        HID_BLE_REPORT_MAP
};

static const hids_report_t reports[] = {
        HID_BLE_REPORTS
};

const hids_config_t hid_ble_config = {
        .report_map = report_map,
        .report_map_length = sizeof(report_map),
        .reports = reports,
        .num_reports = sizeof(reports) / sizeof(reports[0]),

        .boot_device = HID_BLE_BOOT_DEVICE_TYPE,

        .hids_info = {
                .bcd_hid = 0x0110,
                .country_code = HID_BLE_INFO_COUNTRY_CODE,
                .flags = 0x00,
        },
};
