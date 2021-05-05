/**
 ****************************************************************************************
 *
 * @file ble_multi_link_task.c
 *
 * @brief Multi-Link Demo task
 *
 * Copyright (C) 2015-2018 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "util/queue.h"
#include "osal.h"
#include "timers.h"
#include "osal.h"
#include "sys_watchdog.h"
#include "ble_att.h"
#include "ble_common.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_service.h"
#include "dlg_mls.h"

/*
 * Multi-link demo advertising data
 */
static const uint8_t adv_data[] = {
        0x12, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'M', 'u', 'l', 't', 'i', '-', 'l', 'i', 'n', 'k'
};

static const gap_conn_params_t cp = {
        .interval_min  = BLE_CONN_INTERVAL_FROM_MS(50),     // 50.00 ms
        .interval_max  = BLE_CONN_INTERVAL_FROM_MS(70),     // 70.00 ms
        .slave_latency = 0,
        .sup_timeout   = BLE_SUPERVISION_TMO_FROM_MS(420),  // 420.00 ms
};

typedef struct {
        void            *next;

        bd_address_t    addr;
        uint16_t        conn_idx;
} conn_dev_t;

PRIVILEGED_DATA static enum conn_cancel_reason {
        CONN_CANCEL_REASON_ADV_ERR,
        CONN_CANCEL_REASON_DEV_NOT_RESPONDING,
} conn_canceled_reason;

PRIVILEGED_DATA static queue_t connections;

PRIVILEGED_DATA static uint16_t master_dev_conn_idx = BLE_CONN_IDX_INVALID;

PRIVILEGED_DATA static bool connecting = false;

PRIVILEGED_DATA static bd_address_t new_dev_addr;

/*
 * Debug functions
 */

static const char *format_bd_address(const bd_address_t *addr)
{
        static char buf[19];
        int i;

        for (i = 0; i < sizeof(addr->addr); i++) {
                int idx;

                // for printout, address should be reversed
                idx = sizeof(addr->addr) - i - 1;
                sprintf(&buf[i * 3], "%02X:", addr->addr[idx]);
        }

        buf[sizeof(buf) - 2] = '\0';

        return buf;
}

static void print_connection_func(void *data, void *user_data)
{
        const conn_dev_t *conn_dev = data;
        int *num = user_data;

        (*num)++;

        printf("%2d | %5d | %s\r\n", *num, conn_dev->conn_idx,
                                                        format_bd_address(&conn_dev->addr));
}

static void print_connections(void)
{
        int num = 0;

        printf("\r\n");
        printf("Nr | Index | Address\r\n");
        queue_foreach(&connections, print_connection_func, &num);

        if (!num) {
                printf("(no active connections)\r\n");
        }

        printf("\r\n");
}

bool list_elem_match(const void *elem, const void *ud)
{
        conn_dev_t *conn_dev = (conn_dev_t *) elem;
        uint16_t *conn_idx = (uint16_t *) ud;

        return conn_dev->conn_idx == *conn_idx;
}

static void bd_addr_write_cb(ble_service_t *svc, uint16_t conn_idx, const bd_address_t *addr)
{
        ble_error_t status;

        status = ble_gap_connect(addr, &cp);

        if (status != BLE_STATUS_OK) {
                printf("%s: failed. Status=%d\r\n", __func__, status);
        }

        if (status == BLE_ERROR_BUSY) {
                new_dev_addr = *addr;
                conn_canceled_reason = CONN_CANCEL_REASON_DEV_NOT_RESPONDING;

                /*
                 * If the device is not answered after connection, when you are trying to connect to
                 * another device, cancel the last connection to connect to with new one.
                 */
                ble_gap_connect_cancel();
        }

        connecting = (status == BLE_STATUS_OK);
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        conn_dev_t *conn_dev;

        printf("%s: conn_idx=%d peer_address=%s\r\n", __func__, evt->conn_idx,
                                                        format_bd_address(&evt->peer_address));

        conn_dev = OS_MALLOC(sizeof(conn_dev_t));

        conn_dev->addr.addr_type = evt->peer_address.addr_type;
        memcpy(conn_dev->addr.addr, evt->peer_address.addr, sizeof(conn_dev->addr.addr));
        conn_dev->conn_idx = evt->conn_idx;

        queue_push_front(&connections, (void *)conn_dev);

        if (master_dev_conn_idx == BLE_CONN_IDX_INVALID) {
                master_dev_conn_idx = evt->conn_idx;
        }

        connecting = false;

        print_connections();
}

static void handle_evt_gap_conn_completed(ble_evt_gap_connection_completed_t *evt)
{
        if (master_dev_conn_idx != BLE_CONN_IDX_INVALID) {
                return;
        }

        /* Process only then if the connection was completed not by intended cancellation then  */
        if (evt->status != BLE_ERROR_CANCELED) {
                return;
        }

        if (conn_canceled_reason == CONN_CANCEL_REASON_DEV_NOT_RESPONDING) {
                ble_error_t status;

                printf("%s: The last connection was canceled. Connecting to new device...\r\n",
                                                                                         __func__);

                status = ble_gap_connect(&new_dev_addr, &cp);
                printf("%s: Status=%d\r\n", __func__, status);
        } else if (conn_canceled_reason == CONN_CANCEL_REASON_ADV_ERR) {
                printf("%s: cancel the connect\r\n", __func__);

                ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
                printf("Advertising is on again after error\r\n");
        } else {
                printf("Undefined cancellation reason\r\n");
        }
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        printf("%s: conn_idx=%d address=%s\r\n", __func__, evt->conn_idx,
                                                        format_bd_address(&evt->address));

        conn_dev_t *conn_dev = queue_remove(&connections, list_elem_match, &evt->conn_idx);

        if (conn_dev) {
                OS_FREE(conn_dev);
        }

        print_connections();

        if (master_dev_conn_idx == evt->conn_idx) {
                master_dev_conn_idx = BLE_CONN_IDX_INVALID;

                ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
                printf("Advertising is on again\r\n");
        }
}

static void handle_evt_gap_adv_completed(ble_evt_gap_adv_completed_t *evt)
{
        printf("%s: adv_type=%d status=%d\r\n", __func__, evt->adv_type, evt->status);

        if (connecting && (evt->status == BLE_ERROR_NOT_ALLOWED)) {
                connecting = false;

                conn_canceled_reason = CONN_CANCEL_REASON_ADV_ERR;
                ble_gap_connect_cancel();
        }
}

static void handle_evt_gap_pair_req(ble_evt_gap_pair_req_t *evt)
{
        printf("%s: conn_idx=%d, bond=%d\r\n", __func__, evt->conn_idx, evt->bond);

        ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
}

static void handle_evt_gap_security_request(ble_evt_gap_security_request_t *evt)
{
        ble_error_t status;

        status = ble_gap_pair(evt->conn_idx, evt->bond);

        if (status != BLE_STATUS_OK) {
                printf("%s: failed. Status=%d\r\n", __func__, status);
        }
}

void ble_multi_link_task(void *params)
{
        ble_error_t status;
        int8_t wdog_id;

        /* Register ble_multi_link task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        /* Enable BLE */
        status = ble_enable();

        if (status == BLE_STATUS_OK) {
                /* Set all roles */
                ble_gap_role_set(GAP_PERIPHERAL_ROLE | GAP_CENTRAL_ROLE);
        } else {
                printf("%s: failed. Status=%d\r\n", __func__, status);
        }

        /* Register task to BLE framework to receive BLE event notifications */
        ble_register_app();

        /* Set device name */
        ble_gap_device_name_set("Dialog Multi-link", ATT_PERM_READ);

        /* Add Multi-Link Service */
        dlg_mls_init(bd_addr_write_cb);

        /* Set advertising data and start advertising */
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, 0, NULL);
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

        printf("Advertising is on\r\n");

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
                /* This must block forever, until a task notification is received. So, the
                   return value must be OS_TASK_NOTIFY_SUCCESS */
                OS_ASSERT(ret == OS_TASK_NOTIFY_SUCCESS);

                /* Resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

                /* Notified from BLE Manager? */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        hdr = ble_get_event(false);
                        if (!hdr) {
                                goto no_event;
                        }

                        if (!ble_service_handle_event(hdr)) {
                                switch (hdr->evt_code) {
                                case BLE_EVT_GAP_CONNECTED:
                                        handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_CONNECTION_COMPLETED:
                                        handle_evt_gap_conn_completed((ble_evt_gap_connection_completed_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_DISCONNECTED:
                                        handle_evt_gap_disconnected(
                                                               (ble_evt_gap_disconnected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_ADV_COMPLETED:
                                        handle_evt_gap_adv_completed(
                                                              (ble_evt_gap_adv_completed_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_PAIR_REQ:
                                        handle_evt_gap_pair_req((ble_evt_gap_pair_req_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_SECURITY_REQUEST:
                                        handle_evt_gap_security_request(
                                                           (ble_evt_gap_security_request_t *) hdr);
                                        break;
                                default:
                                        ble_handle_event_default(hdr);
                                        break;
                                }
                        }

                        OS_FREE(hdr);

no_event:
                        /* Notify again if there are more events to process in queue */
                        if (ble_has_event()) {
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK,
                                                                               OS_NOTIFY_SET_BITS);
                        }
                }
        }
}
