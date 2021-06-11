/**
 ****************************************************************************************
 *
 * @file bms_task.c
 *
 * @brief BMS demo task
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
#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#include "osal.h"
#include "timers.h"
#include "osal.h"
#include "sys_watchdog.h"
#include "ble_att.h"
#include "ble_common.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_service.h"
#include "ble_uuid.h"
#include "bms.h"
#include "util/queue.h"
#include "bms_config.h"

/*
 * BMS Delete all bond notif mask
 */
#define BMS_DELETE_ALL_BOND_NOTIF       (1 << 1)

/*
 * BMS Update connection parameters notif mask
 */
#define BMS_UPDATE_CONN_PARAM_NOTIF     (1 << 2)

/*
 * BMS Demo advertising data
 */
static const uint8_t adv_data[] = {
        0x10, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'B', 'M', 'S', ' ', 'D', 'e', 'm', 'o'
};

/*
 * TASK handle
 */
PRIVILEGED_DATA static OS_TASK current_task;

/*
 * Bond Management Service data
 */

PRIVILEGED_DATA static ble_service_t *bms;

static const bms_config_t bms_config = {
        .supported_delete_bond_op =     BMS_DELETE_BOND_REQ_DEV                 |
                                        BMS_DELETE_BOND_REQ_DEV_AUTH            |
                                        BMS_DELETE_BOND_ALL_DEV                 |
                                        BMS_DELETE_BOND_ALL_DEV_AUTH            |
                                        BMS_DELETE_BOND_ALL_EXCEPT_REQ_DEV      |
                                        BMS_DELETE_BOND_ALL_EXCEPT_REQ_DEV_AUTH,
};

typedef struct {
        void            *next;

        uint16_t        conn_idx; ///< Connection index
        bd_address_t    addr;     ///< Connected device
        bool            unpair;   ///< True if device should be unpaired when disconnected

        OS_TIMER        conn_pause_timer;
} conn_dev_t;

typedef struct {
        void            *next;

        uint16_t        conn_idx; ///< Connection index
} update_param_t;

static queue_t connections;;
static queue_t update_conn_param_q;

static void delete_bond_cb(bms_delete_bond_op_t op, uint16_t conn_idx, uint16_t length,
                                                                         const uint8_t *auth_code);

static const bms_callbacks_t bms_cb = {
        .delete_bond = delete_bond_cb,
};

/*
 * Debug functions
 */

void print_connection_func(void *data, void *user_data)
{
        const conn_dev_t *conn_dev = data;
        int *num = user_data;

        (*num)++;

        printf("%2d | %5d | %17s | %d\r\n", *num, conn_dev->conn_idx,
                                        ble_address_to_string(&conn_dev->addr), conn_dev->unpair);
}

static void print_connections(void)
{
        int num = 0;

        printf("\r\n");
        printf("Active connections:\r\n");
        printf("Nr | Index | Address           | Pending unpair\r\n");
        queue_foreach(&connections, print_connection_func, &num);

        if (!num) {
                printf("(no active connections)\r\n");
        }

        printf("\r\n");
}

static void print_bonded_devices(void)
{
        size_t bonded_length = defaultBLE_MAX_BONDED;
        static gap_device_t devices[defaultBLE_MAX_BONDED];
        uint8_t i;

        ble_gap_get_devices(GAP_DEVICE_FILTER_BONDED, NULL, &bonded_length, devices);

        printf("\r\n");
        printf("Bonded devices:\r\n");

        if (!bonded_length) {
                printf("(no bonded devices)\r\n");
                printf("\r\n");
                return;
        }

        printf("Nr | Address \r\n");

        for (i = 0; i < bonded_length; ++i) {
                printf("%2d | %17s\r\n", (i + 1), ble_address_to_string(&devices[i].address));
        }

        printf("\r\n");
}

static bool conn_dev_match_conn_idx(const void *elem, const void *ud)
{
        conn_dev_t *conn_dev = (conn_dev_t *) elem;
        uint16_t conn_idx = *((uint16_t *) ud);

        return conn_dev->conn_idx == conn_idx;
}

static bool conn_dev_match_addr(const void *elem, const void *ud)
{
        conn_dev_t *conn_dev = (conn_dev_t *) elem;
        bd_address_t *addr = (bd_address_t *) ud;

        return !memcmp(&conn_dev->addr, addr, sizeof(bd_address_t));
}

/*
 * BMS demo WKUP handler. It is called when user pressed the button.
 */
void bms_wkup_handler(void)
{
        if (current_task) {
                OS_TASK_NOTIFY_FROM_ISR(current_task, BMS_DELETE_ALL_BOND_NOTIF,
                                                                        OS_NOTIFY_SET_BITS);
        }
}

static void unpair(uint16_t skip_conn_idx, bool unpair_connected)
{
        conn_dev_t *conn_dev;
        size_t bonded_length = defaultBLE_MAX_BONDED;
        static gap_device_t devices[defaultBLE_MAX_BONDED];
        uint8_t i;
        bool show_bonded_devices = false;

        ble_gap_get_devices(GAP_DEVICE_FILTER_BONDED, NULL, &bonded_length, devices);

        if (!bonded_length) {
                return;
        }

        for (i = 0; i < bonded_length; ++i) {
                conn_dev = queue_find(&connections, conn_dev_match_addr, &devices[i].address);

                /* Unpair immediately if device is not connected */
                if (!conn_dev) {
                        ble_gap_unpair(&devices[i].address);
                        show_bonded_devices = true;
                        continue;
                }

                /**
                 * If current connection index is equal skip_conn_idx, don't mark device
                 * for unpairing upon disconnection
                 * */
                if (conn_dev->conn_idx == skip_conn_idx) {
                        continue;
                }

                if (unpair_connected) {
                        /* Mark device for unpairing upon disconnection */
                        conn_dev->unpair = true;
                }
        }

        if (show_bonded_devices) {
                print_bonded_devices();
        }
}

static void unpair_all(uint16_t skip_conn_idx)
{
        unpair(skip_conn_idx, true);
}

static void unpair_disconnected(void)
{
        printf("Unpairing non-connected devices\r\n");

        unpair(BLE_CONN_IDX_INVALID, false);
}

static bool check_auth_code(uint16_t length, const uint8_t *auth_code)
{
        static const char demo_auth_code[] = CFG_AUTH_CODE;
        static size_t demo_auth_code_len = sizeof(demo_auth_code) - 1;

        if (length != demo_auth_code_len) {
                return false;
        }

        return !memcmp(auth_code, demo_auth_code, length);
}

static void delete_bond_cb(bms_delete_bond_op_t op, uint16_t conn_idx, uint16_t length,
                                                                          const uint8_t *auth_code)
{
        conn_dev_t *conn_dev;
        bms_delete_bond_status_t status = BMS_DELETE_BOND_STATUS_OK;

        printf("Delete bond\r\n");
        printf("\tConnection index: %d\r\n", conn_idx);
        printf("\tOperation: 0x%02X\r\n", op);
        printf("\tAuthorization code: %.*s\r\n", length, auth_code);

        switch (op) {
        case BMS_DELETE_BOND_REQ_DEV_AUTH:
                if (!check_auth_code(length, auth_code)) {
                        status = BMS_DELETE_BOND_STATUS_INSUFFICIENT_AUTH;
                        break;
                }
                /* no break */
        case BMS_DELETE_BOND_REQ_DEV:
                /* Remove required bonded device */
                conn_dev = queue_find(&connections, conn_dev_match_conn_idx, &conn_idx);
                if (conn_dev) {
                        conn_dev->unpair = true;
                }
                break;

        case BMS_DELETE_BOND_ALL_DEV_AUTH:
                if (!check_auth_code(length, auth_code)) {
                        status = BMS_DELETE_BOND_STATUS_INSUFFICIENT_AUTH;
                        break;
                }
                /* no break */
        case BMS_DELETE_BOND_ALL_DEV:
                /* Remove all bonded device */
                unpair_all(BLE_CONN_IDX_INVALID);
                break;

        case BMS_DELETE_BOND_ALL_EXCEPT_REQ_DEV_AUTH:
                if (!check_auth_code(length, auth_code)) {
                        status = BMS_DELETE_BOND_STATUS_INSUFFICIENT_AUTH;
                        break;
                }
                /* no break */
        case BMS_DELETE_BOND_ALL_EXCEPT_REQ_DEV:
                /* Remove all bonded devices except current device */
                unpair_all(conn_idx);
                break;
        }

        bms_delete_bond_cfm(bms, conn_idx, status);

        print_connections();
}

static void update_conn_param(uint16_t conn_idx)
{
        gap_conn_params_t cp;
        ble_error_t ret;

        cp.interval_min = defaultBLE_PPCP_INTERVAL_MIN;
        cp.interval_max = defaultBLE_PPCP_INTERVAL_MAX;
        cp.slave_latency = defaultBLE_PPCP_SLAVE_LATENCY;
        cp.sup_timeout = defaultBLE_PPCP_SUP_TIMEOUT;

        ret = ble_gap_conn_param_update(conn_idx, &cp);
        if (ret != BLE_STATUS_OK) {
                printf("Failed to change connection parameters\r\n");
                printf("\tConnection index: %d\r\n", conn_idx);
                printf("\tStatus: 0x%02X\r\n", ret);
        }
}

static void dump_conn_param(const gap_conn_params_t *conn_params)
{
        uint16_t intv_min = conn_params->interval_min;
        uint16_t intv_max = conn_params->interval_max;

        printf("\tInterval Min: 0x%04x (%d.%02d ms)\r\n", intv_min,
                BLE_CONN_INTERVAL_TO_MS(intv_min), BLE_CONN_INTERVAL_TO_MS(intv_min * 100) % 100);
        printf("\tInterval Max: 0x%04x (%d.%02d ms)\r\n", intv_max,
                BLE_CONN_INTERVAL_TO_MS(intv_max), BLE_CONN_INTERVAL_TO_MS(intv_max * 100) % 100);
        printf("\tSlave Latency: 0x%04x (%d)\r\n", conn_params->slave_latency,
                                                        conn_params->slave_latency);
        printf("\tSupervision Timeout: 0x%04x (%d ms)\r\n", conn_params->sup_timeout,
                                        BLE_SUPERVISION_TMO_TO_MS(conn_params->sup_timeout));
}

static void conn_pause_timer_cb(OS_TIMER timer)
{
        update_param_t *update_param;
        uint16_t conn_idx = (uint32_t) OS_TIMER_GET_TIMER_ID(timer);

        update_param = OS_MALLOC(sizeof(*update_param));
        update_param->conn_idx = conn_idx;

        queue_push_back(&update_conn_param_q, update_param);

        OS_TASK_NOTIFY(current_task, BMS_UPDATE_CONN_PARAM_NOTIF, OS_NOTIFY_SET_BITS);
}

/*
 * Main code
 */

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        conn_dev_t *conn_dev;

        printf("Device connected\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tAddress: %s\r\n", ble_address_to_string(&evt->peer_address));
        dump_conn_param(&evt->conn_params);
        printf("\r\n");

        conn_dev = OS_MALLOC(sizeof(*conn_dev));

        conn_dev->conn_idx = evt->conn_idx;
        conn_dev->unpair = false;
        memcpy(&conn_dev->addr, &evt->peer_address, sizeof(conn_dev->addr));

        conn_dev->conn_pause_timer = OS_TIMER_CREATE("conn_pause_peripheral", OS_MS_2_TICKS(5000),
                                OS_TIMER_FAIL, (uint32_t) conn_dev->conn_idx, conn_pause_timer_cb);

        OS_TIMER_START(conn_dev->conn_pause_timer, OS_TIMER_FOREVER);

        queue_push_back(&connections, conn_dev);

        ble_gap_set_sec_level(evt->conn_idx, GAP_SEC_LEVEL_3);

        print_connections();
}

static void handle_evt_gap_address_resolved(ble_evt_gap_address_resolved_t *evt)
{
        conn_dev_t *conn_dev;
        char addr[19];

        /*
         * we can't just call format_bd_address() twice in printf since it returns pointer to static
         * buffer so one address would overwrite another - instead, we format one of addresses into
         * temporary buffer and print from there
         */
        strcpy(addr, ble_address_to_string(&evt->address));

        printf("Address resolved\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tAddress: %s\r\n", addr);
        printf("\tResolved address: %s\r\n", ble_address_to_string(&evt->resolved_address));

        conn_dev = queue_find(&connections, conn_dev_match_conn_idx, &evt->conn_idx);
        if (conn_dev) {
                memcpy(&conn_dev->addr, &evt->resolved_address, sizeof(conn_dev->addr));
        }

        print_connections();
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        conn_dev_t *conn_dev;

        printf("Device disconnected\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tAddress: %s\r\n", ble_address_to_string(&evt->address));
        printf("\tReason: 0x%02x\r\n", evt->reason);
        printf("\r\n");

        /*
         * If we had defaultBLE_MAX_CONNECTIONS devices connected, advertising has not been
         * restarted in advertising complete event, restart it here
         */
        if (queue_length(&connections) == defaultBLE_MAX_CONNECTIONS) {
                printf("Start advertising...\r\n");
                ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
        }

        conn_dev = queue_remove(&connections, conn_dev_match_conn_idx, &evt->conn_idx);

        if (conn_dev) {
                if (conn_dev->unpair) {
                        ble_gap_unpair(&conn_dev->addr);
                        printf("Device unpaired\r\n");
                        printf("\tAddress: %s\r\n", ble_address_to_string(&conn_dev->addr));
                        print_bonded_devices();
                } else {
                        printf("Device NOT unpaired\r\n");
                        printf("\tAddress: %s\r\n", ble_address_to_string(&conn_dev->addr));
                }

                OS_TIMER_DELETE(conn_dev->conn_pause_timer, OS_TIMER_FOREVER);

                OS_FREE(conn_dev);
        }

        print_connections();
}

static void handle_evt_gap_adv_completed(ble_evt_gap_adv_completed_t *evt)
{
        printf("Advertisement completed\r\n");
        printf("\tStatus: 0x%02x\r\n", evt->status);

        if (queue_length(&connections) < defaultBLE_MAX_CONNECTIONS) {
                printf("Start advertising...\r\n");
                // restart advertising so we can connect again
                ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
        }
}

static void handle_evt_gap_pair_req(ble_evt_gap_pair_req_t *evt)
{
        ble_error_t status;

        printf("Pair request\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tBond: %d\r\n", evt->bond);

        status = ble_gap_pair_reply(evt->conn_idx, true, true);
        if (status == BLE_ERROR_INS_RESOURCES) {
                printf("Cannot pair, number of max bonded devices has been reached\r\n");
                printf("\tConnection index: %d\r\n", evt->conn_idx);

                ble_gap_pair_reply(evt->conn_idx, false, false);
        }
}

static void handle_evt_gap_passkey_notify(ble_evt_gap_passkey_notify_t * evt)
{
        printf("Passkey notify\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tPasskey=%06" PRIu32 "\r\n", evt->passkey);
}

static void handle_evt_gap_pair_completed(ble_evt_gap_pair_completed_t *evt)
{
        if (evt->status) {
                printf("Pairing failed\r\n");
                printf("\tConnection Index: %d\r\n", evt->conn_idx);
                printf("\tStatus: 0x%02X\r\n", evt->status);
        } else {
                printf("Pairing completed\r\n");
                printf("\tConnection Index: %d\r\n", evt->conn_idx);
                printf("\tBond: %s\r\n", evt->bond ? "true" : "false");
                printf("\tMITM: %s\r\n", evt->mitm ? "true" : "false");
        }

        if (evt->bond) {
                print_bonded_devices();
        }
}

static void handle_evt_gap_sec_level_changed(ble_evt_gap_sec_level_changed_t *evt)
{
        printf("Security Level changed\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tLevel: %d\r\n", evt->level + 1);
        printf("\r\n");
}

static void handle_evt_gap_conn_param_updated(ble_evt_gap_conn_param_updated_t *evt)
{

        printf("Connection parameters updated\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        dump_conn_param(&evt->conn_params);
        printf("\r\n");
}

void bms_task(void *params)
{
        int8_t wdog_id;

        /* Register bms_task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        /* Initialize queues needed to track connection parameter updates on peers */
        queue_init(&connections);
        queue_init(&update_conn_param_q);

        /* Start BLE device as peripheral */
        ble_peripheral_start();

        /* Register task to BLE framework to receive BLE event notifications */
        ble_register_app();

        current_task = OS_GET_CURRENT_TASK();

        /* Set device name */
        ble_gap_device_name_set("Dialog BMS Demo", ATT_PERM_READ);

        /* Add BMS */
        bms = bms_init(NULL, &bms_config, &bms_cb);

        /* Set input/output capabilities */
        ble_gap_set_io_cap(GAP_IO_CAP_DISP_ONLY);


        print_bonded_devices();

        /* Set advertising data and start advertising */
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, 0, NULL);
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

        printf("Start advertising...\r\n");

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                /* Notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);

                /* Suspend watchdog while blocking on OS_TASK_NOTIFY_WAIT() */
                sys_watchdog_suspend(wdog_id);

                /*
                 * Wait on any of the notification bits, then clear them all
                 */
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                /* Since it will block forever, it is guaranteed to have a valid notification when
                   it gets here */
                OS_ASSERT(ret == OS_OK);

                /* Resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

                /* Notified from BLE Manager? */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        hdr = ble_get_event(false);
                        if (!hdr) {
                                goto no_event;
                        }

                        if (ble_service_handle_event(hdr)) {
                                goto handled;
                        }

                        switch (hdr->evt_code) {
                        case BLE_EVT_GAP_CONNECTED:
                                handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_ADDRESS_RESOLVED:
                                handle_evt_gap_address_resolved((ble_evt_gap_address_resolved_t *) hdr);
                                break;
                        case BLE_EVT_GAP_DISCONNECTED:
                                handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_ADV_COMPLETED:
                                handle_evt_gap_adv_completed((ble_evt_gap_adv_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_PAIR_REQ:
                                handle_evt_gap_pair_req((ble_evt_gap_pair_req_t *) hdr);
                                break;
                        case BLE_EVT_GAP_PASSKEY_NOTIFY:
                                handle_evt_gap_passkey_notify((ble_evt_gap_passkey_notify_t *) hdr);
                                break;
                        case BLE_EVT_GAP_PAIR_COMPLETED:
                                handle_evt_gap_pair_completed((ble_evt_gap_pair_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_SEC_LEVEL_CHANGED:
                                handle_evt_gap_sec_level_changed((ble_evt_gap_sec_level_changed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_CONN_PARAM_UPDATED:
                                handle_evt_gap_conn_param_updated((ble_evt_gap_conn_param_updated_t *) hdr);
                                break;
                        default:
                                ble_handle_event_default(hdr);
                                break;
                        }

handled:
                        OS_FREE(hdr);

no_event:
                        /* Notify again if there are more events to process in queue */
                        if (ble_has_event()) {
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK,
                                                                                OS_NOTIFY_SET_BITS);
                        }
                }

                if (notif & BMS_DELETE_ALL_BOND_NOTIF) {
                        /* Remove bond information of disconnected devices */
                        unpair_disconnected();
                }

                if (notif & BMS_UPDATE_CONN_PARAM_NOTIF) {
                        update_param_t *update_param;

                        update_param = queue_pop_front(&update_conn_param_q);

                        if (update_param) {
                                update_conn_param(update_param->conn_idx);

                                OS_FREE(update_param);
                        }

                        if (queue_length(&update_conn_param_q)) {
                                OS_TASK_NOTIFY(current_task, BMS_UPDATE_CONN_PARAM_NOTIF,
                                                                               OS_NOTIFY_SET_BITS);
                        }
                }
        }
}
