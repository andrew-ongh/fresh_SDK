/**
 ****************************************************************************************
 *
 * @file hogp_device_task.c
 *
 * @brief HOG Profile / HID Device demo application
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include "osal.h"
#include "sys_watchdog.h"
#include "ble_common.h"
#include "ble_service.h"
#include "bas.h"
#include "dis.h"
#include "hids.h"
#include "hogp_device_config.h"
#include "util/queue.h"
#include "hw_wkup.h"

#define HID_TIMER_NOTIF (1 << 1)
#define WKUP_NOTIF      (1 << 2)

/*
 * HOGP Device example is HID Mouse example which moves cursor on square sides.
 * Below are size of square and size of single step.
 */
#define STEP_SIZE               (5)
#define SQUARE_SIZE             (STEP_SIZE * 50)
#define MOUSE_TIMEOUT           (50)
#define MAX_PENDING_REPORTS     (5)

/*
 * HOGP Device advertising data
 *
 * Advertsing data should include HIDS UUID
 */
static const uint8_t adv_data[] = {
        0x03, GAP_DATA_TYPE_UUID16_LIST_INC,
        0x12, 0x18, // = 0x1812 (HID UUID)
        0x03, GAP_DATA_TYPE_APPEARANCE,
#if CFG_DEVICE_MOUSE && CFG_DEVICE_KEYBOARD
        BLE_GAP_APPEARANCE_GENERIC_HID & 0xFF,
        BLE_GAP_APPEARANCE_GENERIC_HID >> 8,
#elif CFG_DEVICE_MOUSE
        BLE_GAP_APPEARANCE_HID_MOUSE & 0xFF,
        BLE_GAP_APPEARANCE_HID_MOUSE >> 8,
#elif CFG_DEVICE_KEYBOARD
        BLE_GAP_APPEARANCE_HID_KEYBOARD & 0xFF,
        BLE_GAP_APPEARANCE_HID_KEYBOARD >> 8,
#else
#error "Set CFG_DEVICE_KEYBOARD or CFG_DEVICE_MOUSE"
#endif
};
#define MOUSE_REPORT_ID         0x01

#if CFG_DEVICE_MOUSE
#define KEYBOARD_REPORT_ID      0x02
#else
#define KEYBOARD_REPORT_ID      0x00
#endif

/*
 * HOGP Device scan response
 *
 * HOGP Device name is included in scan response
 */
static const uint8_t scan_rsp[] = {
        0x13, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'H', 'o', 'G', 'P', ' ', 'D', 'e', 'v', 'i', 'c', 'e'
};
/*
 * Device Information Service data
 *
 * PNP ID characteristic is mandatory for HOGP Device
 *
 */
static const dis_pnp_id_t dis_pnp_info =  {
        .vid_source = 0,
        .vid = 0,
        .pid = 0x0000,
        .version = 0x0001,
};

static const dis_device_info_t dis_info = {
        .manufacturer = "Dialog Semiconductor",
        .model_number = "Dialog BLE",
        .pnp_id = &dis_pnp_info,
};

/*
 * Report Map set as a value in Report Map Characteristic. It describes reports data usage
 */
static const uint8_t report_map[] = {
        0x05, 0x01,                     // Usage Page (Generic Desktop Ctrls)
#if CFG_DEVICE_MOUSE
        0x09, 0x02,                     // Usage (Mouse)
        0xA1, 0x01,                     // Collection (Application)
        0x09, 0x01,                     //   Usage (Pointer)
        0xA1, 0x00,                     //   Collection (Physical)
        0X85, MOUSE_REPORT_ID,          //     Report ID
        0x05, 0x09,                     //     Usage Page (Button)
        0x19, 0x01,                     //     Usage Minimum (0x01)
        0x29, 0x08,                     //     Usage Maximum (0x08)
        0x15, 0x00,                     //     Logical Minimum (0)
        0x25, 0x01,                     //     Logical Maximum (1)
        0x95, 0x08,                     //     Report Count (8)
        0x75, 0x01,                     //     Report Size (1)
        0x81, 0x02,                     //     Input (data, variable, absolute)
        0x05, 0x01,                     //     Usage Page (Generic Desktop Ctrls)
        0x09, 0x30,                     //     Usage (X)
        0x09, 0x31,                     //     Usage (Y)
        0x15, 0x80,                     //     Logical Minimum (128)
        0x25, 0x7F,                     //     Logical Maximum (127)
        0x75, 0x08,                     //     Report Size (8)
        0x95, 0x02,                     //     Report Count (2)
        0x81, 0x06,                     //     Input (data, variable, absolute)
        0x09, 0x38,                     //     Usage (Wheel)
        0x15, 0x80,                     //     Logical Minimum (128)
        0x25, 0x7F,                     //     Logical Maximum (127)
        0x75, 0x08,                     //     Report Size (8)
        0x95, 0x01,                     //     Report Count (1)
        0x81, 0x06,                     //     Input (data, variable, absolute)
        0xC0,                           //   End Collection
        0xC0,                           // End Collection
#endif
#if CFG_DEVICE_KEYBOARD
        0x09, 0x06,                     // Usage (Keyboard)
        0xA1, 0x01,                     // Collection (Application)
#if CFG_DEVICE_MOUSE
        0X85, KEYBOARD_REPORT_ID,       //   Report ID
#endif
        0x05, 0x07,                     //   Usage page (Key Codes)
        0x19, 0xE0,                     //   Usage minimum (224)
        0x29, 0xE7,                     //   Usage maximum (231)
        0x15, 0x00,                     //   Logical minimum (0)
        0x25, 0x01,                     //   Logical maximum (1)
        0x75, 0x01,                     //   Report size (1)
        0x95, 0x08,                     //   Report count (8)
        0x81, 0x02,                     //   Input (data, variable, absolute)
        0x95, 0x01,                     //   Report count (1)
        0x75, 0x08,                     //   Report size (8)
        0x81, 0x01,                     //   Input (constant)
        0x95, 0x06,                     //   Report count (6)
        0x75, 0x08,                     //   Report size (8)
        0x15, 0x00,                     //   Logical minimum (0)
        0x25, 0x65,                     //   Logical maximum (101)
        0x05, 0x07,                     //   Usage page (key codes)
        0x19, 0x00,                     //   Usage minimum (0)
        0x29, 0x65,                     //   Usage maximum (101)
        0x81, 0x00,                     //   Input (data, array)
        0xC0,                           // End Collection
#endif
};

/*
 * Array of report characteristics
 *
 * Include only one Input Report declared in report map
 */
static const hids_report_t reports[] = {
#if CFG_DEVICE_MOUSE
        {
                .type = HIDS_REPORT_TYPE_INPUT,
                .report_id = MOUSE_REPORT_ID,
                .length = 0x04,
        },
#endif
#if CFG_DEVICE_KEYBOARD
        {
                .type = HIDS_REPORT_TYPE_INPUT,
                .report_id = KEYBOARD_REPORT_ID,
                .length = 0x08,
        },
#endif
};

/*
 * HID Service config
 *
 * Declare support for boot device mouse and set reports, report map and HID info value
 */
static const hids_config_t hids_config = {
#if CFG_DEVICE_MOUSE && CFG_DEVICE_KEYBOARD
        .boot_device = HIDS_BOOT_DEVICE_COMBO,
#elif CFG_DEVICE_MOUSE
        .boot_device = HIDS_BOOT_DEVICE_MOUSE,
#elif CFG_DEVICE_KEYBOARD
        .boot_device = HIDS_BOOT_DEVICE_KEYBOARD,
#else
#error "Set CFG_DEVICE_KEYBOARD or CFG_DEVICE_MOUSE"
#endif
        .num_reports = sizeof(reports) / sizeof(reports[0]),
        .reports = reports,
        .report_map = report_map,
        .report_map_length = sizeof(report_map),
        .hids_info.bcd_hid = 0x0000,
        .hids_info.country_code = 0x00,
        .hids_info.flags = 0x00,
};

/* HID Service instance */
static ble_service_t *hids;

/* Control Point Value */
PRIVILEGED_DATA static hids_cp_command_t cp_state;
/* Protocol mode value */
PRIVILEGED_DATA static hids_protocol_mode_t protocol_mode;
/* Timer used for cursor movement */
PRIVILEGED_DATA static OS_TIMER hid_timer;
/* Values used to calculate report value */
PRIVILEGED_DATA static int32_t side_length;
PRIVILEGED_DATA static uint8_t side;
PRIVILEGED_DATA static OS_TASK current_task;
PRIVILEGED_DATA static int reports_depth;

/* Timer used to send mouse reports */
#if CFG_DEVICE_MOUSE
static void hid_timer_cb(OS_TIMER timer)
{
        OS_TASK task = (OS_TASK) OS_TIMER_GET_TIMER_ID(timer);

        OS_TASK_NOTIFY(task, HID_TIMER_NOTIF, eSetBits);
}
#endif

#if CFG_DEVICE_KEYBOARD
void hogp_device_wkup_handler(void)
{
        OS_TASK_NOTIFY_FROM_ISR(current_task, WKUP_NOTIF, eSetBits);
}
#endif

typedef enum {
        REPORT_TYPE_MOUSE,
        REPORT_TYPE_KEYBOARD,
} report_type_t;

typedef struct {
        void *next;
        report_type_t report_type;
        uint16_t length;
        uint8_t data[];
} notification_t;

static bool send_report(uint16_t length, const uint8_t *data, report_type_t report_type)
{
        bool status = false;

        if (reports_depth >= MAX_PENDING_REPORTS) {
                return false;
        }

        if (report_type == REPORT_TYPE_MOUSE) {
                if (protocol_mode == HIDS_PROTOCOL_MODE_REPORT) {
                        status = hids_notify_input_report(hids, MOUSE_REPORT_ID, length, data);
                } else {
                        /* Boot report for mouse contains 3 bytes: buttons, x, y */
                        status = hids_notify_boot_mouse_input_report(hids, 3, data);
                }
        } else {
                if (protocol_mode == HIDS_PROTOCOL_MODE_REPORT) {
                        status = hids_notify_input_report(hids, KEYBOARD_REPORT_ID, length, data);
                } else {
                        status = hids_notify_boot_keyboard_input_report(hids, length, data);
                }
        }

        if (status) {
                reports_depth++;
        }

        return status;
}

/*
 * This function updates local variables and sends formatted mouse reports.
 *
 * Cursor moves on square sides. Variable side indicates on which side cursor moves.
 * side_length is updated with STEP_SIZE and once it reaches SQUARE_SIZE, cursor
 * changes direction (writes next side of square).
 */
static void update_coords(void)
{
        int8_t report[4];
        int8_t x, y;

        if (cp_state == HIDS_CONTROL_POINT_SUSPEND) {
                return;
        }

        if (side == 0) {
                x = STEP_SIZE;
                y = 0;
        } else if (side == 1) {
                x = 0;
                y = STEP_SIZE;
        } else if (side == 2) {
                x = -STEP_SIZE;
                y = 0;
        } else {
                x = 0;
                y = -STEP_SIZE;
        }

        side_length += abs(x) + abs(y);
        if (side_length == SQUARE_SIZE) {
                side_length = 0;

                if (side < 3) {
                        side++;
                } else {
                        side = 0;
                }
        }

        /* No pressed buttons */
        report[0] = 0;
        /* X axis */
        report[1] = x;
        /* Y axis */
        report[2] = y;
        /* Scroll */
        report[3] = 0;

        send_report(sizeof(report), (uint8_t *) report, REPORT_TYPE_MOUSE);
}

/* This function sends input keyboard reports with random key codes in range a-z and 0-9 */
static void update_keyboard_buttons(void)
{
        uint8_t report[8];

        memset(report, 0, 8);

        /*
         * Gereral report format:
         *
         * report[0]     = modifier keys bit mask
         * report[1]     = padding
         * report[2]-[7] = keycode array
         *
         * We get random one key press in range 'a' - '9' and random SHIFT modifier.
         * Values are taken from HID Usage Table
         */
        report[2] = (rand() % 0x24) + 0x04;
        /* set modifier key to LEFT SHIFT (1st bit) */
        report[0] = (rand() % 2) ? 0x02 : 0x00;

        send_report(sizeof(report), report, REPORT_TYPE_KEYBOARD);
        /* Clear buttons */
        memset(report, 0, 8);
        send_report(sizeof(report), report, REPORT_TYPE_KEYBOARD);
}

static void set_protocol_mode_cb(ble_service_t *svc, hids_protocol_mode_t mode)
{
        protocol_mode = mode;
}

static void control_point_cb(ble_service_t *svc, hids_cp_command_t command)
{
        if (cp_state == command) {
                return;
        }

        cp_state = command;

        if (cp_state == HIDS_CONTROL_POINT_EXIT_SUSPEND) {
                OS_TIMER_START(hid_timer, OS_TIMER_FOREVER);
        } else {
                OS_TIMER_STOP(hid_timer, OS_TIMER_FOREVER);
        }
}


static void notify_boot_mouse_input_report_completed_cb(ble_service_t *svc, bool success)
{
        reports_depth--;
}

static void notify_boot_keyboard_input_report_completed_cb(ble_service_t *svc, bool success)
{
        reports_depth--;
}

static void notify_input_report_completed_cb(ble_service_t *svc, uint8_t report_id, bool success)
{
        reports_depth--;
}

/* HIDS callbacks */
static const hids_callbacks_t hids_cb = {
        .set_protocol_mode = set_protocol_mode_cb,
        .control_point = control_point_cb,
        .notify_boot_mouse_input_report_completed =
                notify_boot_mouse_input_report_completed_cb,
        .notify_boot_keyboard_input_report_completed =
                notify_boot_keyboard_input_report_completed_cb,
        .notify_input_report_completed = notify_input_report_completed_cb,
};

static void handle_disconnected(ble_evt_gap_disconnected_t *evt)
{
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
}

static void handle_pair_request(ble_evt_gap_pair_req_t *evt)
{
        ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
}

static void handle_connected(ble_evt_gap_connected_t *evt)
{
        /*
         * HIDS can be used exclusively by one connection and it's required to attach particular
         * connection to service instance. In our case we can have only one device connected at any
         * time so we can just attach whatever comes in.
         *
         * Connection is automatically detached when disconnected.
         */
        hids_attach_connection(hids, evt->conn_idx);

        reports_depth = 0;

        ble_gap_set_sec_level(evt->conn_idx, GAP_SEC_LEVEL_2);
}

void hogp_device_task(void *params)
{
        int8_t wdog_id;
        ble_service_t *hids_includes[2];
        ble_service_config_t service_config = {
                .sec_level = GAP_SEC_LEVEL_2,
                .num_includes = 0,
        };

        /* Register hogp_device task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        current_task = OS_GET_CURRENT_TASK();

        ble_peripheral_start();
        ble_register_app();

        /* Set HOGP Device Name and Appearance */
        ble_gap_device_name_set("Dialog HoGP Device", ATT_PERM_READ);
        ble_gap_appearance_set(BLE_GAP_APPEARANCE_GENERIC_HID, ATT_PERM_READ);

        /* Add Battery Service */
        hids_includes[0] = bas_init(&service_config, NULL);

        /* Add Device Information Service */
        hids_includes[1] = dis_init(&service_config, &dis_info);

        /* Add HID Service */
        service_config.includes = hids_includes;
        service_config.num_includes = sizeof(hids_includes) / sizeof(hids_includes[0]);

        hids = hids_init(&service_config, &hids_config, &hids_cb);

        /* Set initial values of control point and protocol mode */
        cp_state = HIDS_CONTROL_POINT_EXIT_SUSPEND;
        protocol_mode = HIDS_PROTOCOL_MODE_REPORT;

#if CFG_DEVICE_MOUSE
        /* Create and start timer for updating coordinates */
        hid_timer = OS_TIMER_CREATE("hid", MOUSE_TIMEOUT / OS_PERIOD_MS, OS_TIMER_SUCCESS,
                                                (void *) OS_GET_CURRENT_TASK(), hid_timer_cb);
        OS_TIMER_START(hid_timer, OS_TIMER_FOREVER);
#endif

        /* Configure and start advertising */
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, sizeof(scan_rsp), scan_rsp);
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                /* notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);

                /* suspend watchdog while blocking on OS_TASK_NOTIFY_WAIT() */
                sys_watchdog_suspend(wdog_id);

                /*
                 * Wait on any of the notification bits, then clear them all
                 */
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                /* Guaranteed to succeed, since we're waiting forever until a notification is received */
                OS_ASSERT(ret == OS_OK);

                /* resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

                /* Notified from BLE Manager? */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        hdr = ble_get_event(false);
                        if (!hdr) {
                                goto no_event;
                        }

                        /*
                         * First, application needs to try pass event through ble_framework.
                         * Then it can handle it itself and finally pass to default event handler.
                         */
                        if (!ble_service_handle_event(hdr)) {
                                switch (hdr->evt_code) {
                                case BLE_EVT_GAP_CONNECTED:
                                        handle_connected((ble_evt_gap_connected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_DISCONNECTED:
                                        handle_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_PAIR_REQ:
                                        handle_pair_request((ble_evt_gap_pair_req_t *) hdr);
                                        break;
                                default:
                                        ble_handle_event_default(hdr);
                                        break;
                                }
                        }

                        /* Free event buffer (it's not needed anymore) */
                        OS_FREE(hdr);

no_event:
                        /*
                         * If there are more events waiting in queue, application should process
                         * them now.
                         */
                        if (ble_has_event()) {
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK, eSetBits);
                        }
                }
                /* Notified from HID timer */
                if (notif & HID_TIMER_NOTIF) {
                        /* Update coords and notify client */
                        update_coords();
                }

                /* WKUP callback */
                if (notif & WKUP_NOTIF) {
                        update_keyboard_buttons();
                }
        }
}
