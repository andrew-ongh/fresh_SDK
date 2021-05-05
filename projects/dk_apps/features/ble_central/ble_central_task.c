/**
 ****************************************************************************************
 *
 * @file ble_central_task.c
 *
 * @brief BLE central task
 *
 * Copyright (C) 2015-2018 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "osal.h"
#include "hw_uart.h"
#include "sys_watchdog.h"
#include "ble_att.h"
#include "ble_common.h"
#include "ble_config.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_gattc.h"
#include "ble_service.h"
#include "ble_uuid.h"
#include "ble_central_config.h"
#include "util/queue.h"

/*
 * Notification bits reservation
 * bit #0 is always assigned to BLE event queue notification
 */
#define DISCOVER_NOTIF  (1 << 1)
#define RECONNECT_NOTIF (1 << 2)

#if (!CFG_USE_BROWSE_API)
typedef struct {
        void *next;
        uint16_t start_h;
        uint16_t end_h;
} service_t;

typedef struct {
        void *next;
        uint16_t handle;
        uint16_t val_h;
} characteristic_t;

PRIVILEGED_DATA static queue_t services;
PRIVILEGED_DATA static queue_t characteristics;
#endif

PRIVILEGED_DATA static OS_TASK ble_central_task_handle;

PRIVILEGED_DATA static uint16_t devname_val_h;

/* return static buffer with formatted address */
static const char *format_bd_address(const bd_address_t *addr)
{
        static char buf[19];
        int i;

        for (i = 0; i < sizeof(addr->addr); i++) {
                int idx;

                // for printout, address should be reversed
                idx = sizeof(addr->addr) - i - 1;
                sprintf(&buf[i * 3], "%02x:", addr->addr[idx]);
        }

        buf[sizeof(buf) - 2] = '\0';

        return buf;
}

/* return static buffer with formatted UUID */
static const char *format_uuid(const att_uuid_t *uuid)
{
        static char buf[37];

        if (uuid->type == ATT_UUID_16) {
                sprintf(buf, "0x%04x", uuid->uuid16);
        } else {
                int i;
                int idx = 0;

                for (i = ATT_UUID_LENGTH; i > 0; i--) {
                        if (i == 12 || i == 10 || i == 8 || i == 6) {
                                buf[idx++] = '-';
                        }

                        idx += sprintf(&buf[idx], "%02x", uuid->uuid128[i - 1]);
                }
        }

        return buf;
}

/* return static buffer with characteristics properties mask */
static const char *format_properties(uint8_t properties)
{
        static const char props_str[] = "BRXWNISE"; // each letter corresponds to single property
        static char buf[9];
        int i;

        // copy full properties mask
        memcpy(buf, props_str, sizeof(props_str));

        for (i = 0; i < 8; i++) {
                // clear letter from mask if property not present
                if ((properties & (1 << i)) == 0) {
                        buf[i] = '-';
                }
        }

        return buf;
}

/* print buffer in formatted hexdump and ASCII */
static void format_value(uint16_t length, const uint8_t *value)
{
        static char buf1[49]; // buffer for hexdump (16 * 3 chars + \0)
        static char buf2[17]; // buffer for ASCII (16 chars + \0)

        while (length) {
                int i;

                memset(buf1, 0, sizeof(buf1));
                memset(buf2, 0, sizeof(buf2));

                for (i = 0; i < 16 && length > 0; i++, length--, value++) {
                        sprintf(&buf1[i * 3], "%02x ", (int) *value);

                        // any character outside standard ASCII is presented as '.'
                        if (*value < 32 || *value > 127) {
                                buf2[i] = '.';
                        } else {
                                buf2[i] = *value;
                        }
                }

                printf("\t%-49s %-17s\r\n", buf1, buf2);
        }
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        printf("%s: conn_idx=%04x\r\n", __func__, evt->conn_idx);

        // notify main thread, we'll start discovery from there
        OS_TASK_NOTIFY(ble_central_task_handle, DISCOVER_NOTIF, eSetBits);
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        printf("%s: conn_idx=%04x address=%s reason=%d\r\n", __func__, evt->conn_idx,
                                                format_bd_address(&evt->address), evt->reason);

#if (!CFG_USE_BROWSE_API)
        queue_remove_all(&services, OS_FREE_FUNC);
        queue_remove_all(&characteristics, OS_FREE_FUNC);
#endif

        // notify main thread, we'll start reconnect from there
        OS_TASK_NOTIFY(ble_central_task_handle, RECONNECT_NOTIF, eSetBits);
}

static void handle_evt_gap_security_request(ble_evt_gap_security_request_t *evt)
{
        printf("%s: conn_idx=%04x bond=%d\r\n", __func__, evt->conn_idx, evt->bond);

        // trigger pairing
        ble_gap_pair(evt->conn_idx, evt->bond);
}

static void handle_evt_gap_pair_completed(ble_evt_gap_pair_completed_t *evt)
{
        printf("%s: conn_idx=%04x status=%d bond=%d mitm=%d\r\n", __func__, evt->conn_idx,
                                                                evt->status, evt->bond, evt->mitm);
}

#if CFG_USE_BROWSE_API
static void handle_evt_gattc_browse_svc(ble_evt_gattc_browse_svc_t *evt)
{
        uint8_t prop = 0;
        int i;

        printf("%s: conn_idx=%04x start_h=%04x end_h=%04x\r\n", __func__, evt->conn_idx,
                                                                        evt->start_h, evt->end_h);

        printf("\t%04x serv %s\r\n", evt->start_h, format_uuid(&evt->uuid));

        for (i = 0; i < evt->num_items; i++) {
                gattc_item_t *item = &evt->items[i];
                att_uuid_t uuid;

                switch (item->type) {
                case GATTC_ITEM_TYPE_INCLUDE:
                        printf("\t%04x incl %s\r\n", item->handle, format_uuid(&item->uuid));
                        break;
                case GATTC_ITEM_TYPE_CHARACTERISTIC:
                        printf("\t%04x char %s prop=%02x (%s)\r\n", item->handle,
                                                format_uuid(&evt->uuid), item->c.properties,
                                                format_properties(item->c.properties));

                        printf("\t%04x ---- %s\r\n", item->c.value_handle, format_uuid(&item->uuid));

                        // store properties, useful when handling descriptor later
                        prop = item->c.properties;
                        break;
                case GATTC_ITEM_TYPE_DESCRIPTOR:
                        printf("\t%04x desc %s\r\n", item->handle, format_uuid(&item->uuid));

                        ble_uuid_create16(UUID_GATT_CLIENT_CHAR_CONFIGURATION, &uuid);
                        if (ble_uuid_equal(&uuid, &item->uuid)) {
                                if (prop & GATT_PROP_NOTIFY) {
                                        uint16_t ccc = GATT_CCC_NOTIFICATIONS;
                                        ble_gattc_write(evt->conn_idx, item->handle, 0,
                                                                        sizeof(ccc), (uint8_t *) &ccc);
                                }

                                if (prop & GATT_PROP_INDICATE) {
                                        uint16_t ccc = GATT_CCC_INDICATIONS;
                                        ble_gattc_write(evt->conn_idx, item->handle, 0,
                                                                        sizeof(ccc), (uint8_t *) &ccc);
                                }
                        }
                        break;
                default:
                        printf("\t%04x ????\r\n", item->handle);
                        break;
                }
        }
}

static void handle_evt_gattc_browse_completed(ble_evt_gattc_browse_completed_t *evt)
{
        printf("%s: conn_idx=%04x status=%d\r\n", __func__, evt->conn_idx, evt->status);
}
#else
static void handle_evt_gattc_discover_svc(ble_evt_gattc_discover_svc_t *evt)
{
        service_t *service;

        printf("%s: conn_idx=%04x uuid=%s start_h=%04x end_h=%04x\r\n", __func__, evt->conn_idx,
                                                format_uuid(&evt->uuid), evt->start_h, evt->end_h);

        service = OS_MALLOC(sizeof(*service));
        service->start_h = evt->start_h;
        service->end_h = evt->end_h;

        queue_push_back(&services, service);
}

static void handle_evt_gattc_discover_char(ble_evt_gattc_discover_char_t *evt)
{
        characteristic_t *characteristic;
        att_uuid_t uuid;

        printf("%s: conn_idx=%04x uuid=%s handle=%04x value_handle=%04x properties=%02x (%s)\r\n",
                __func__, evt->conn_idx, format_uuid(&evt->uuid), evt->handle, evt->value_handle,
                evt->properties, format_properties(evt->properties));

        ble_uuid_create16(0x2a00, &uuid); // Device Name
        if (ble_uuid_equal(&uuid, &evt->uuid)) {
                // store handle if Device Name is writable - once read is completed we'll write new
                // value there and read again
                if (evt->properties & GATT_PROP_WRITE) {
                        devname_val_h = evt->value_handle;
                }

                ble_gattc_read(evt->conn_idx, evt->value_handle, 0);
        }

        characteristic = OS_MALLOC(sizeof(*characteristic));
        characteristic->handle = evt->handle;
        characteristic->val_h = evt->value_handle;

        queue_push_back(&characteristics, characteristic);
}

static void handle_evt_gattc_discover_desc(ble_evt_gattc_discover_desc_t *evt)
{
        printf("%s: conn_idx=%04x uuid=%s handle=%04x\r\n", __func__, evt->conn_idx,
                                                        format_uuid(&evt->uuid), evt->handle);
}

static void handle_evt_gattc_discover_completed(ble_evt_gattc_discover_completed_t *evt)
{
        service_t *service;

        printf("%s: conn_idx=%04x type=%d status=%d\r\n", __func__, evt->conn_idx, evt->type,
                                                                                        evt->status);


        service = queue_peek_front(&services);

        if (evt->type == GATTC_DISCOVERY_TYPE_SVC && service) {
                ble_gattc_discover_char(evt->conn_idx, service->start_h, service->end_h, NULL);
        } else if (evt->type == GATTC_DISCOVERY_TYPE_CHARACTERISTICS && service) {
                characteristic_t *charac, *next = NULL;

                for (charac = queue_peek_front(&characteristics); charac; charac = next) {
                        next = charac->next;

                        /*
                         * Check if there is enough room for at least one descriptor.
                         * Range start from next handle after characteristic value handle,
                         * ends before next characteristic or service's end handle
                         */
                        if (charac->val_h < (next ? next->handle - 1 : service->end_h)) {
                                ble_gattc_discover_desc(evt->conn_idx, charac->val_h + 1, next ?
                                                        next->handle - 1 : service->end_h);
                        }
                }

                queue_remove_all(&characteristics, OS_FREE_FUNC);
                queue_pop_front(&services);
                OS_FREE(service);

                service = queue_peek_front(&services);
                if (service) {
                        ble_gattc_discover_char(evt->conn_idx, service->start_h,
                                                                        service->end_h, NULL);
                }
        }
}
#endif

static void handle_evt_gattc_read_completed(ble_evt_gattc_read_completed_t *evt)
{
#if CFG_UPDATE_NAME
        static bool devname_written = false;
#endif

        printf("%s: conn_idx=%04x handle=%04x status=%d\r\n", __func__, evt->conn_idx, evt->handle,
                                                                                        evt->status);

        if (evt->status == ATT_ERROR_OK) {
                format_value(evt->length, evt->value);
        }

#if CFG_UPDATE_NAME
        if (evt->handle == devname_val_h && !devname_written) {
                static const uint8_t name[] = "DA1468x was here!";
                devname_written = true;
                ble_gattc_write(evt->conn_idx, evt->handle, 0, sizeof(name) - 1, name);
        }
#endif
}

static void handle_evt_gattc_write_completed(ble_evt_gattc_write_completed_t *evt)
{
        printf("%s: conn_idx=%04x handle=%04x status=%d\r\n", __func__, evt->conn_idx, evt->handle,
                                                                                        evt->status);

        if (evt->handle == devname_val_h) {
                ble_gattc_read(evt->conn_idx, evt->handle, 0);
        }
}

static void handle_evt_gattc_notification(ble_evt_gattc_notification_t *evt)
{
        printf("%s: conn_idx=%04x handle=%04x length=%d\r\n", __func__, evt->conn_idx, evt->handle,
                                                                                       evt->length);

        format_value(evt->length, evt->value);
}

static void handle_evt_gattc_indication(ble_evt_gattc_indication_t *evt)
{
        printf("%s: conn_idx=%04x handle=%04x length=%d\r\n", __func__, evt->conn_idx, evt->handle,
                                                                                       evt->length);

        format_value(evt->length, evt->value);
}

void ble_central_task(void *params)
{
        int8_t wdog_id;
        const bd_address_t addr = {
                .addr_type = PUBLIC_ADDRESS,
                .addr = defaultBLE_STATIC_ADDRESS,    // Default BD address
        };
        const gap_conn_params_t cp = {
                .interval_min = BLE_CONN_INTERVAL_FROM_MS(50),   // 50.00ms
                .interval_max = BLE_CONN_INTERVAL_FROM_MS(70),   // 70.00ms
                .slave_latency = 0,
                .sup_timeout = BLE_SUPERVISION_TMO_FROM_MS(420), // 420ms
        };

        ble_central_task_handle = OS_GET_CURRENT_TASK();

        /* register ble_central task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

#if (!CFG_USE_BROWSE_API)
        queue_init(&services);
        queue_init(&characteristics);
#endif

        ble_central_start();
        ble_register_app();

        printf("BLE Central application started\r\n");

        ble_gap_connect(&addr, &cp);

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
                /* Blocks forever waiting for task notification. The return value must be OS_OK */
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

                        if (!ble_service_handle_event(hdr)) {
                                switch (hdr->evt_code) {
                                case BLE_EVT_GAP_CONNECTED:
                                        handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_DISCONNECTED:
                                        handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_SECURITY_REQUEST:
                                        handle_evt_gap_security_request((ble_evt_gap_security_request_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_PAIR_COMPLETED:
                                        handle_evt_gap_pair_completed((ble_evt_gap_pair_completed_t *) hdr);
                                        break;
#if CFG_USE_BROWSE_API
                                case BLE_EVT_GATTC_BROWSE_SVC:
                                        handle_evt_gattc_browse_svc((ble_evt_gattc_browse_svc_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_BROWSE_COMPLETED:
                                        handle_evt_gattc_browse_completed((ble_evt_gattc_browse_completed_t *) hdr);
                                        break;
#else
                                case BLE_EVT_GATTC_DISCOVER_SVC:
                                        handle_evt_gattc_discover_svc((ble_evt_gattc_discover_svc_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_DISCOVER_CHAR:
                                        handle_evt_gattc_discover_char((ble_evt_gattc_discover_char_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_DISCOVER_DESC:
                                        handle_evt_gattc_discover_desc((ble_evt_gattc_discover_desc_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_DISCOVER_COMPLETED:
                                        handle_evt_gattc_discover_completed((ble_evt_gattc_discover_completed_t *) hdr);
                                        break;
#endif
                                case BLE_EVT_GATTC_READ_COMPLETED:
                                        handle_evt_gattc_read_completed((ble_evt_gattc_read_completed_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_WRITE_COMPLETED:
                                        handle_evt_gattc_write_completed((ble_evt_gattc_write_completed_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_NOTIFICATION:
                                        handle_evt_gattc_notification((ble_evt_gattc_notification_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_INDICATION:
                                        handle_evt_gattc_indication((ble_evt_gattc_indication_t *) hdr);
                                        break;
                                default:
                                        ble_handle_event_default(hdr);
                                        break;
                                }
                        }

                        OS_FREE(hdr);

no_event:
                        // notify again if there are more events to process in queue
                        if (ble_has_event()) {
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK, eSetBits);
                        }
                }

                if (notif & DISCOVER_NOTIF) {
#if CFG_USE_BROWSE_API
                        ble_gattc_browse(0, NULL);
#else
                        ble_gattc_discover_svc(0, NULL);
#endif
                }

                if (notif & RECONNECT_NOTIF) {
                        ble_gap_connect(&addr, &cp);
                }

        }
}
