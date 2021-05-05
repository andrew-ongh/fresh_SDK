/**
 ****************************************************************************************
 *
 * @file ble_task.c
 *
 * @brief Apple Notification Center Service task
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdarg.h>
#include <stdio.h>
#include "osal.h"
#include "sys_watchdog.h"
#include "hw_gpio.h"
#include "ad_nvms.h"
#include "ble_client.h"
#include "ble_uuid.h"
#include "ble_storage.h"
#include "hids_client.h"
#include "dis_client.h"
#include "hid_ble_config.h"

#include "common.h"

#define MSG_QUEUE_NOTIF         (1 << 10)
#define RECONNECT_TIMER_NOTIF   (1 << 11)
#define USB_STATE_NOTIF         (1 << 12)

#define STORAGE_HIDS_CLIENT     BLE_STORAGE_KEY_APP(0, 0)


struct msg_queue_item {
        enum hid_op op;
        uint8_t type;
        uint8_t id;
        uint16_t length;
        uint8_t data[HID_BLE_MAX_REPORT_LENGTH];
};

typedef enum {
        PENDING_ACTION_READ_HID_INFO = (1 << 0),
        PENDING_ACTION_READ_REPORT_MAP = (1 << 1),
        PENDING_ACTION_DISCOVER_REPORTS = (1 << 2),
} pending_action_t;

typedef struct {
        uint16_t conn_idx;

        ble_client_t *hids_client;
        ble_client_t *dis_client;

        bool busy_init;
        pending_action_t pending_init;

        bool busy; /* if true, we're busy writing report and cannot write another one */
        bool force_disconnect; /* if true, we force disconnection (e.g. due to an error) */
} peer_info_t;


static const hids_client_config_t hids_config = {
        .mode = HIDS_CLIENT_PROTOCOL_MODE_REPORT,
};

static const gap_scan_params_t scan_params = {
        .interval = BLE_SCAN_INTERVAL_FROM_MS(60),
        .window = BLE_SCAN_WINDOW_FROM_MS(30),
};


/* Main application task */
__RETAINED static OS_TASK app_task;

/* Peer information (only 1 peer can be connected) */
__RETAINED_RW static peer_info_t peer_info = {
        .conn_idx = BLE_CONN_IDX_INVALID,
};

/* BLE connection state */
__RETAINED static bool ble_available;

/* Message queue from USB */
__RETAINED static OS_QUEUE msg_queue;

/* Reconnection timer */
__RETAINED static OS_TIMER reconnect_timer;

#define pending_init_execute_and_check(FLAG, FUNCTION, ...) \
        ({                                                                      \
                if (peer_info.pending_init & FLAG) {                            \
                        peer_info.busy_init = FUNCTION(__VA_ARGS__);            \
                        if (!peer_info.busy_init) {                             \
                                /* Failed to execute action, clear bit */       \
                                peer_info.pending_init &= ~FLAG;                \
                        }                                                       \
                }                                                               \
                peer_info.busy_init;                                            \
        })

static void set_state(bool new_state)
{
        OS_ENTER_CRITICAL_SECTION();

        ble_available = new_state;

        OS_LEAVE_CRITICAL_SECTION();

        notify_state_changed_to_usb();
}

static void process_pending_actions(void)
{
        if (peer_info.busy_init) {
                return;
        }

        if (pending_init_execute_and_check(PENDING_ACTION_READ_HID_INFO,
                                        hids_client_read_hid_info, peer_info.hids_client)) {
                return;
        }

        if (pending_init_execute_and_check(PENDING_ACTION_READ_REPORT_MAP,
                                        hids_client_read_report_map, peer_info.hids_client)) {
                return;
        }

        if (pending_init_execute_and_check(PENDING_ACTION_DISCOVER_REPORTS,
                                        hids_client_discover_reports, peer_info.hids_client)) {
                return;
        }

        set_state(true);
}

static void add_pending_action(pending_action_t action)
{
        peer_info.pending_init |= action;

        process_pending_actions();
}

static void clear_pending_action(pending_action_t action, att_error_t error)
{
        /* Do nothing if we try to clear action which is not pending */
        if ((peer_info.pending_init & action) == 0) {
                return;
        }

        peer_info.busy_init = false;
        peer_info.pending_init &= ~action;

        process_pending_actions();
}

static void timer_cb(OS_TIMER timer)
{
        uint32_t mask = OS_PTR_TO_UINT(OS_TIMER_GET_TIMER_ID(timer));

        OS_TASK_NOTIFY(app_task, mask, OS_NOTIFY_SET_BITS);
}

static void store_hids_client(uint16_t conn_idx)
{
        uint8_t *buffer = NULL;
        size_t length = 0;
        bool bonded = false;

        ble_gap_is_bonded(conn_idx, &bonded);

        if (!bonded || !peer_info.hids_client) {
                return;
        }

        /* Get serialized BLE Client length */
        ble_client_serialize(peer_info.hids_client, NULL, &length);
        buffer = OS_MALLOC(length);
        /* Serialize BLE Client */
        ble_client_serialize(peer_info.hids_client, buffer, &length);
        /* Put BLE Client to the storage */
        ble_storage_put_buffer(conn_idx, STORAGE_HIDS_CLIENT, length, buffer, OS_FREE_FUNC, true);
}

static void hids_report_map_cb(ble_client_t *hids_client, att_error_t status, uint16_t length,
                                                                        const uint8_t *data)
{
        OS_ASSERT(status == ATT_ERROR_OK);

        clear_pending_action(PENDING_ACTION_READ_REPORT_MAP, ATT_ERROR_OK);

        /*
         * If report map is different that what we expect, disconnect - we can only work with device
         * which has the same report map as we expose on USB, otherwise it is not possible to relay
         * reports between USB and BLE.
         */
        if (length != hid_ble_config.report_map_length ||
                                                memcmp(data, hid_ble_config.report_map, length)) {
                peer_info.force_disconnect = true;
                ble_gap_disconnect(peer_info.conn_idx, BLE_HCI_ERROR_CON_TERM_BY_LOCAL_HOST);
        }
}

static void hids_hid_info_cb(ble_client_t *hids_client, const hids_client_hid_info_t *info)
{
        clear_pending_action(PENDING_ACTION_READ_HID_INFO, ATT_ERROR_OK);
}

static void hids_report_cb(ble_client_t *hids_client, hids_client_report_type_t type,
                                                        uint8_t report_id, att_error_t status,
                                                        uint16_t length, const uint8_t *data)
{
        /* Report data written over BLE, send directly to USB */
        send_to_usb(HID_OP_DATA, type, report_id, length, data);
}

static void hids_client_report_found_cb(ble_client_t *hids_client, att_error_t status,
                                                hids_client_report_type_t type, uint8_t report_id)
{
        /* For input reports need to enable notifications, for other reports do nothing. */
        if (type == HIDS_CLIENT_REPORT_TYPE_INPUT) {
                hids_client_input_report_set_notif_state(hids_client, report_id, true);
        }
}

static void hids_client_discover_reports_complete_cb(ble_client_t *hids_client)
{
        store_hids_client(hids_client->conn_idx);

        clear_pending_action(PENDING_ACTION_DISCOVER_REPORTS, ATT_ERROR_OK);
}

static void hids_client_report_write_complete_cb(ble_client_t *hids_client,
                                                        hids_client_report_type_t type,
                                                        uint8_t report_id, att_error_t status)
{
        /*
         * If report write is finished, we can try to write another one. No need to check if there
         * is any report pending, just notify main task and it will handle this.
         */
        peer_info.busy = false;
        OS_TASK_NOTIFY(app_task, MSG_QUEUE_NOTIF, OS_NOTIFY_SET_BITS);
}

/** HIDS client callbacks */
static const hids_client_callbacks_t hids_callbacks = {
        .report_map = hids_report_map_cb,
        .hid_info = hids_hid_info_cb,
        .report = hids_report_cb,
        .report_found = hids_client_report_found_cb,
        .discover_reports_complete = hids_client_discover_reports_complete_cb,
        .report_write_complete = hids_client_report_write_complete_cb,
};

static void get_peer_address(bd_address_t *addr)
{
        __RETAINED_RW static bd_address_t peer_addr = { 0xFF };
        nvms_t nvms;

        /*
         * NOTE:
         * This function shall be customized to return address of a bundled HID device, which is
         * intended to work with this dongle application. It can be read from NVMS, OTP, etc. By
         * default it tries to read it from hardcoded offset on parameters partition or sets default
         * address if not found on NVMS.
         */

        if (peer_addr.addr_type != 0xFF) {
                goto done;
        }

        nvms = ad_nvms_open(NVMS_PARAM_PART);
        ad_nvms_read(nvms, 0x0040, (uint8_t *) &peer_addr, sizeof(peer_addr));
        if (peer_addr.addr_type < 0x02) {
                goto done;
        }

        peer_addr.addr_type = defaultBLE_ADDRESS_TYPE;
        memcpy(&peer_addr.addr, &(uint8_t[6]) defaultBLE_STATIC_ADDRESS, 6);

done:
        *addr = peer_addr;
}

static void connect(void)
{
        static const gap_conn_params_t cp = {
                .interval_min = BLE_CONN_INTERVAL_FROM_MS(7.5),
                .interval_max = BLE_CONN_INTERVAL_FROM_MS(7.5),
                .slave_latency = 0,
                .sup_timeout = BLE_SUPERVISION_TMO_FROM_MS(5000),
        };
        bd_address_t peer_addr;

        get_peer_address(&peer_addr);

        ble_gap_connect(&peer_addr, &cp);
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        gap_device_t dev;
        uint16_t len;
        void *buf;
        ble_error_t err;

        hw_gpio_set_active(HW_GPIO_PORT_1, HW_GPIO_PIN_5);

        peer_info.conn_idx = evt->conn_idx;

        /*
         * We check if we are bonded with this device. If yes, we can restore HIDS client state,
         * otherwise do nothing - our device will always send security request and we will start
         * from there.
         */

        err = ble_gap_get_device_by_addr(&evt->peer_address, &dev);
        if (err != BLE_STATUS_OK || !dev.bonded) {
                return;
        }

        err = ble_storage_get_buffer(evt->conn_idx, STORAGE_HIDS_CLIENT, &len, &buf);
        if (err != BLE_STATUS_OK) {
                return;
        }

        peer_info.hids_client = hids_client_init_from_data(evt->conn_idx, &hids_config,
                                                                        &hids_callbacks, buf, len);
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        bool force_disconnect = peer_info.force_disconnect;

        set_state(false);

        hw_gpio_set_inactive(HW_GPIO_PORT_1, HW_GPIO_PIN_5);

        ble_client_cleanup(peer_info.hids_client);
        ble_client_cleanup(peer_info.dis_client);

        memset(&peer_info, 0, sizeof(peer_info));

        peer_info.conn_idx = BLE_CONN_IDX_INVALID;

        /*
         * If this was forced disconnection, do not reconnect immediately. This is to avoid being
         * stuck in a loop in case some invalid device is in neighborhood and we connect it over
         * and over again.
         */
        if (force_disconnect) {
                OS_TIMER_START(reconnect_timer, OS_TIMER_FOREVER);
        } else {
                connect();
        }
}

static void handle_evt_gattc_browse_svc(ble_evt_gattc_browse_svc_t *evt)
{
        /* Ignore non-HIDS services, we don't need them */
        if (evt->uuid.type != ATT_UUID_16 || evt->uuid.uuid16 != UUID_SERVICE_HIDS) {
                return;
        }

        if (peer_info.hids_client) {
                return;
        }

        peer_info.hids_client = hids_client_init(&hids_config, &hids_callbacks, evt);
        if (!peer_info.hids_client) {
                return;
        }

        ble_client_add(peer_info.hids_client);
}

static void handle_evt_gattc_browse_completed(ble_evt_gattc_browse_completed_t *evt)
{
        hids_client_cap_t cap;

        /*
         * We only work in report mode - other setting would be a configuration error so we can
         * assert here.
         */
        OS_ASSERT(hids_config.mode == HIDS_CLIENT_PROTOCOL_MODE_REPORT);

        cap = hids_client_get_capabilities(peer_info.hids_client);

        if ((cap & HIDS_CLIENT_CAP_PROTOCOL_MODE) == 0) {
                /* "Our" device supports protocol mode, so this is some other device - disconnect */
                peer_info.force_disconnect = true;
                ble_gap_disconnect(evt->conn_idx, BLE_HCI_ERROR_CON_TERM_BY_LOCAL_HOST);
                return;
        }

        add_pending_action(PENDING_ACTION_READ_HID_INFO);
        add_pending_action(PENDING_ACTION_READ_REPORT_MAP);
        add_pending_action(PENDING_ACTION_DISCOVER_REPORTS);
}

static void handle_evt_gap_security_request(ble_evt_gap_security_request_t *evt)
{
        gap_device_t dev;
        size_t len = 1;
        ble_error_t err;

        /*
         * We receive security request if:
         * - we are not bonded
         * - we are bonded, but encryption failed to be enabled
         * In both cases we need to re-bond, but first try to remove any existing bonding so it is
         * possible to bond new device.
         */

        err = ble_gap_get_devices(GAP_DEVICE_FILTER_BONDED, NULL, &len, &dev);
        if (err == BLE_STATUS_OK && len > 0) {
                ble_gap_unpair(&dev.address);
        }

        ble_gap_pair(evt->conn_idx, true);
}

static void handle_evt_gap_sec_level_changed(ble_evt_gap_sec_level_changed_t *evt)
{
        att_uuid_t uuid;

        /*
         * If encryption is enabled but HIDS client is missing, it means we were not bonded and need
         * to browse for HID service now.
         */

        if (peer_info.hids_client) {
                return;
        }

        ble_uuid_create16(UUID_SERVICE_HIDS, &uuid);
        ble_gattc_browse(evt->conn_idx, &uuid);
}

static void handle_evt_gap_pair_completed(ble_evt_gap_pair_completed_t *evt)
{
        /*
         * We can't do much if pairing failed, so just disconnect immediately (and wait for another
         * connection).
         */

        if (evt->status != BLE_STATUS_OK) {
                peer_info.force_disconnect = true;
                ble_gap_disconnect(evt->conn_idx, BLE_HCI_ERROR_CON_TERM_BY_LOCAL_HOST);
                return;
        }
}

static void process_msg_queue(void)
{
        static struct msg_queue_item item;

        while (!peer_info.busy && OS_QUEUE_GET(msg_queue, &item, OS_QUEUE_NO_WAIT) == OS_QUEUE_OK) {
                switch (item.op) {
                case HID_OP_DATA:
                        hids_client_report_write(peer_info.hids_client, item.type, item.id, false,
                                                                        item.length, item.data);
                        break;
                case HID_OP_GET_REPORT:
                        hids_client_report_read(peer_info.hids_client, item.type, item.id);
                        break;

                case HID_OP_SET_REPORT:
                        hids_client_report_write(peer_info.hids_client, item.type, item.id, true,
                                                                        item.length, item.data);
                        peer_info.busy = true;
                        break;
                default:
                        break;
                }
        }
}

void ble_task(void *params)
{
        int8_t wdog_id;

        app_task = OS_GET_CURRENT_TASK();
        wdog_id = sys_watchdog_register(false);

        OS_QUEUE_CREATE(msg_queue, sizeof(struct msg_queue_item), 10);

        ble_enable();
        ble_gap_role_set(GAP_CENTRAL_ROLE);
        ble_register_app();

        reconnect_timer = OS_TIMER_CREATE("reconnect", OS_MS_2_TICKS(3000), false,
                                                OS_UINT_TO_PTR(RECONNECT_TIMER_NOTIF), timer_cb);

        /* Set mandatory GAP characteristics values */
        ble_gap_appearance_set(BLE_GAP_APPEARANCE_GENERIC_COMPUTER, ATT_PERM_READ);
        ble_gap_device_name_set("Dialog HID Dongle", ATT_PERM_READ);

        ble_gap_scan_params_set(&scan_params);

        connect();

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                /* notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);

                /* suspend watchdog while blocking on OS_TASK_NOTIFY_WAIT() */
                sys_watchdog_suspend(wdog_id);

                /* Wait on any of the notification bits, then clear them all */
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                /* Guaranteed to succeed, since we're waiting forever until a notification is received */
                OS_ASSERT(ret == OS_OK);

                /* resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

                /* notified from BLE manager, can get event */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        hdr = ble_get_event(false);
                        if (!hdr) {
                                goto no_event;
                        }

                        ble_client_handle_event(hdr);

                        switch (hdr->evt_code) {
                        case BLE_EVT_GAP_CONNECTED:
                                handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_DISCONNECTED:
                                handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                break;
                        case BLE_EVT_GATTC_BROWSE_SVC:
                                handle_evt_gattc_browse_svc((ble_evt_gattc_browse_svc_t *) hdr);
                                break;
                        case BLE_EVT_GATTC_BROWSE_COMPLETED:
                                handle_evt_gattc_browse_completed((ble_evt_gattc_browse_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_SECURITY_REQUEST:
                                handle_evt_gap_security_request((ble_evt_gap_security_request_t *) hdr);
                                break;
                        case BLE_EVT_GAP_SEC_LEVEL_CHANGED:
                                handle_evt_gap_sec_level_changed((ble_evt_gap_sec_level_changed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_PAIR_COMPLETED:
                                handle_evt_gap_pair_completed((ble_evt_gap_pair_completed_t *) hdr);
                                break;
                        default:
                                ble_handle_event_default(hdr);
                                break;
                        }

                        /* Free event buffer (it's not needed anymore) */
                        OS_FREE(hdr);

no_event:
                        /*
                         * If there are more events waiting in queue, application should process
                         * them now.
                         */
                        if (ble_has_event()) {
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK,
                                                                                OS_NOTIFY_SET_BITS);
                        }
                }

                /* Notified by queue, process it */
                if (notif & MSG_QUEUE_NOTIF) {
                        process_msg_queue();
                }

                /* Notified by reconnection timer, connect */
                if (notif & RECONNECT_TIMER_NOTIF) {
                        connect();
                }

                /* Notified by USB state changed, handle it */
                if (notif & USB_STATE_NOTIF) {
                        /*
                         * If USB state change to 'available', we initiate connection.
                         * Otherwise we either disconnect existing connection or cancel connection
                         * attept if any - there is no point in having BLE activity if there is no
                         * target to send it to.
                         */
                        if (get_usb_state()) {
                                connect();
                        } else {
                                if (peer_info.conn_idx != BLE_CONN_IDX_INVALID) {
                                        ble_gap_disconnect(peer_info.conn_idx,
                                                       BLE_HCI_ERROR_CON_TERM_BY_LOCAL_HOST);
                                } else {
                                        ble_gap_connect_cancel();
                                }
                        }
                }
        }
}

void send_to_ble(enum hid_op op, uint8_t type, uint8_t id, uint16_t length, const uint8_t *data)
{
        struct msg_queue_item item;

        if (app_task == 0) {
                return;
        }

        item.op = op;
        item.type = type;
        item.id = id;
        item.length = length;
        if (data) {
                memcpy(item.data, data, length);
        }

        OS_QUEUE_PUT(msg_queue, &item, OS_QUEUE_NO_WAIT);
        OS_TASK_NOTIFY(app_task, MSG_QUEUE_NOTIF, OS_NOTIFY_SET_BITS);
}

void notify_state_changed_to_ble(void)
{
        OS_TASK_NOTIFY(app_task, USB_STATE_NOTIF, OS_NOTIFY_SET_BITS);
}

bool get_ble_state(void)
{
        bool state;

        OS_ENTER_CRITICAL_SECTION();

        state = ble_available;

        OS_LEAVE_CRITICAL_SECTION();

        return state;
}
