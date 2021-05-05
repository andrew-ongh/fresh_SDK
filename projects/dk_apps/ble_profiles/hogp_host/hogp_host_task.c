/**
 ****************************************************************************************
 *
 * @file hogp_host_task.c
 *
 * @brief Apple Notification Center Service task
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdarg.h>
#include <stdio.h>
#include "osal.h"
#include "cli.h"
#include "hw_gpio.h"
#include "sys_watchdog.h"
#include "hw_gpio.h"
#include "ble_client.h"
#include "ble_service.h"
#include "ble_uuid.h"
#include "util/queue.h"
#include "gatt_client.h"
#include "hids_client.h"
#include "bas_client.h"
#include "dis_client.h"
#include "scps_client.h"
#include "hogp_host_config.h"
#include "hogp_host_task.h"
#include "debug.h"

/**
 * WKUP and CLI notify mask
 */
#define WKUP_NOTIF              (1 << 1)

#define CLI_NOTIF               (1 << 2)

/**
 * HOGP Host state enum
 */
typedef enum {
        HOGP_HOST_STATE_DISCONNECTED,
        HOGP_HOST_STATE_CONNECTING,
        HOGP_HOST_STATE_CONNECTED,
        HOGP_HOST_STATE_DISCONNECTING,
} hogp_host_state_t;

static const hids_client_config_t hids_config = {
        .mode = CFG_REPORT_MODE,
};

typedef struct {
        uint16_t conn_idx;

        queue_t clients;

        bool svc_changed;
        bool pending_sec;
        bool pending_browse;
        bool pending_init;
} peer_info_t;

/* Current peer information (only 1 peer can be connected) */
__RETAINED_RW static peer_info_t peer_info = {
        .conn_idx = BLE_CONN_IDX_INVALID,
};

/**
 *  Client ID counter
 */
PRIVILEGED_DATA static uint8_t client_id;

/**
 * HOGP Host state
 */
INITIALISED_PRIVILEGED_DATA static hogp_host_state_t hogp_host_state = HOGP_HOST_STATE_DISCONNECTED;

/**
 * TASK handle
 */
PRIVILEGED_DATA static OS_TASK current_task;

static bool match_client_by_ble_client(const void *data, const void *match_data)
{
        const ble_client_t *ble_client = match_data;
        const client_t *client = data;

        return client->client == ble_client;
}

static client_t *get_client_by_ble_client(ble_client_t *client)
{
        return queue_find(&peer_info.clients, match_client_by_ble_client, client);
}

static const char *get_client_name(client_type_t type)
{
        switch (type) {
        case CLIENT_TYPE_HIDS:
                return "HID Client";
        case CLIENT_TYPE_GATT:
                return "GATT Client";
        case CLIENT_TYPE_DIS:
                return "DIS Client";
        case CLIENT_TYPE_BAS:
                return "BAS Client";
        case CLIENT_TYPE_SCPS:
                return "SCPS Client";
        default:
                return "Unknown Client";
        }
}

static void print_client_message(ble_client_t *ble_client, const char *msg)
{
        client_t *client = get_client_by_ble_client(ble_client);

        if (client) {
                printf("%s (ID = %d) %s\r\n", get_client_name(client->type), client->id, msg);
        }
}

static void client_destroy(void *data)
{
        client_t *client = data;

        printf("Removing %s with Id: %d\r\n", get_client_name(client->type), client->id);

        ble_client_remove(client->client);
        ble_client_cleanup(client->client);

        OS_FREE(client);
}

static void client_new(ble_client_t *ble_client, client_type_t type)
{
        client_t *client = OS_MALLOC(sizeof(*client));

        ble_client_add(ble_client);
        client->client = ble_client;
        client->type = type;
        client->id = client_id++;

        queue_push_back(&peer_info.clients, client);

        printf("\t%s initialized with Id: %d\r\n", get_client_name(client->type), client->id);
}

static void client_attach(void *data, void *user_data)
{
        client_t *client = data;
        uint16_t *conn_idx = user_data;

        printf("%s attached with Id: %d\r\n", get_client_name(client->type), client->id);

        ble_client_attach(client->client, *conn_idx);
}

static void print_hids_client_cap(client_t *client, hids_client_cap_t cap)
{
        printf("HID Service (%d) Supported characteristics:\r\n", client->id);

        if (cap & HIDS_CLIENT_CAP_PROTOCOL_MODE) {
                printf("\tProtocol Mode characteristic\r\n");
        }

        if (cap & HIDS_CLIENT_CAP_BOOT_MOUSE_INPUT) {
                printf("\tBoot Mouse Input characteristic\r\n");
        }

        if (cap & HIDS_CLIENT_CAP_BOOT_KEYBOARD_INPUT) {
                printf("\tBoot Keyboard Input characteristic\r\n");
        }

        if (cap & HIDS_CLIENT_CAP_BOOT_KEYBOARD_OUTPUT) {
                printf("\tBoot Keyboard Output characteristic\r\n");
        }

        if (cap & HIDS_CLIENT_CAP_HID_INFO) {
                printf("\tHID Info characteristic\r\n");
        }

        if (cap & HIDS_CLIENT_CAP_HID_CONTROL_POINT) {
                printf("\tHID Control Point characteristic\r\n");
        }

        if (cap & HIDS_CLIENT_CAP_REPORT_MAP) {
                printf("\tReport Map characteristic\r\n");
        }

        printf("\r\n");
}

static void client_init(void *data, void *user_data)
{
        client_t *client = data;

        switch (client->type) {
        case CLIENT_TYPE_BAS:
                bas_client_get_event_state(client->client, BAS_CLIENT_EVENT_BATTERY_LEVEL_NOTIFY);
                bas_client_set_event_state(client->client, BAS_CLIENT_EVENT_BATTERY_LEVEL_NOTIFY, true);
                bas_client_read_battery_level(client->client);
                break;
        case CLIENT_TYPE_DIS:
                dis_client_read(client->client, DIS_CLIENT_CAP_PNP_ID);
                break;
        case CLIENT_TYPE_GATT:
                gatt_client_set_event_state(client->client, GATT_CLIENT_EVENT_SERVICE_CHANGED_INDICATE,
                                                                                        true);
                break;
        case CLIENT_TYPE_HIDS:
        {
                hids_client_cap_t cap;

                cap = hids_client_get_capabilities(client->client);
                print_hids_client_cap(client, cap);

                if (cap & HIDS_CLIENT_CAP_PROTOCOL_MODE) {
                        hids_client_set_protocol_mode(client->client);
                }

                if (hids_config.mode == HIDS_CLIENT_PROTOCOL_MODE_REPORT) {
                        hids_client_read_hid_info(client->client);
                        hids_client_read_report_map(client->client);
                        hids_client_discover_external_reports(client->client);
                        hids_client_discover_reports(client->client);
                } else {
#if CFG_AUTO_ENABLE_NOTIFICATIONS
                        if (cap & HIDS_CLIENT_CAP_BOOT_MOUSE_INPUT) {
                                hids_client_boot_report_set_notif_state(client->client,
                                                        HIDS_CLIENT_BOOT_MOUSE_INPUT, true);
                        }

                        if (cap & HIDS_CLIENT_CAP_BOOT_KEYBOARD_INPUT) {
                                hids_client_boot_report_set_notif_state(client->client,
                                                        HIDS_CLIENT_BOOT_KEYBOARD_INPUT, true);
                        }
#endif
                }
                break;
        }
        case CLIENT_TYPE_SCPS:
                scps_client_set_event_state(client->client, SCPS_CLIENT_EVENT_REFRESH_NOTIF, true);
                break;
        case CLIENT_TYPE_NONE:
        default:
                break;
        }
}

static void hids_report_map_cb(ble_client_t *hids_client, att_error_t status, uint16_t length,
                                                                        const uint8_t *data)
{
        int i;

        print_client_message(hids_client, "Read report map completed");
        printf("\tStatus: 0x%02X\r\n", status);

        if (!status) {
                printf("\tData: ");

                for (i = 0; i < length; i++) {
                        printf("%02x ", data[i]);
                }

                printf("\r\n");
        }
}

static void hids_hid_info_cb(ble_client_t *hids_client, const hids_client_hid_info_t *info)
{
        print_client_message(hids_client, "Read HID info completed");

        printf("\tResource: HID Info characteristic\r\n");
        printf("\tbcdHID: 0x%04x\r\n", info->bcd_hid);
        printf("\tbCountryCode: 0x%02x\r\n", info->country_code);
        printf("\tFlags: 0x%02x\r\n", info->flags);
}

static void hids_report_cb(ble_client_t *hids_client, hids_client_report_type_t type,
                                                        uint8_t report_id, att_error_t status,
                                                        uint16_t length, const uint8_t *data)
{
        int i;

        print_client_message(hids_client, "Report callback");

        if (!status) {
                printf("\tReport type: ");

                switch (type) {
                case HIDS_CLIENT_REPORT_TYPE_INPUT:
                        printf("REPORT_TYPE_INPUT\r\n");
                        break;
                case HIDS_CLIENT_REPORT_TYPE_OUTPUT:
                        printf("REPORT_TYPE_OUTPUT\r\n");
                        break;
                case HIDS_CLIENT_REPORT_TYPE_FEATURE:
                        printf("REPORT_TYPE_FEATURE\r\n");
                        break;
                }

                printf("\tReport Id: %d\r\n", report_id);
                printf("\tData: ");

                for (i = 0; i < length; i++) {
                        printf("%02x ", data[i]);
                }

                printf("\r\n");
        }
}

static void hids_boot_report_cb(ble_client_t *hids_client, hids_client_boot_report_type type,
                                        att_error_t status, uint16_t length, const uint8_t *data)
{
        int i;

        print_client_message(hids_client, "Boot report callback");
        printf("\tReport type: ");

        switch (type) {
        case HIDS_CLIENT_BOOT_KEYBOARD_INPUT:
                printf("BOOT_KEYBOARD_INPUT\r\n");
                break;
        case HIDS_CLIENT_BOOT_KEYBOARD_OUTPUT:
                printf("BOOT_KEYBOARD_OUTPUT\r\n");
                break;
        case HIDS_CLIENT_BOOT_MOUSE_INPUT:
                printf("BOOT_MOUSE_INPUT\r\n");
                break;
        }

        printf("\tData: ");

        for (i = 0; i < length; i++) {
                printf("%02x ", data[i]);
        }

        printf("\r\n");
}

static void hids_get_protocol_mode_cb(ble_client_t *hids_client, att_error_t status,
                                                                hids_client_protocol_mode_t mode)
{
        print_client_message(hids_client, "Get protocol mode callback");

        if (!status) {
                printf("\tMode: %s\r\n", mode == HIDS_CLIENT_PROTOCOL_MODE_BOOT ?
                        "HIDS_CLIENT_PROTOCOL_MODE_BOOT" : "HIDS_CLIENT_PROTOCOL_MODE_REPORT");
        }

        printf("\tStatus: 0x%02X\r\n", status);
}

static void hids_input_report_get_notif_state_cb(ble_client_t *hids_client, uint8_t report_id,
                                                                att_error_t status, bool enabled)
{
        print_client_message(hids_client, "Get report notif state callback");

        if (!status) {
                printf("\tReport Id: %d\r\n", report_id);
                printf("\tNotifications: %s\r\n", enabled ? "enabled" : "disabled");
        }

        printf("\tStatus: 0x%02X\r\n", status);
}

static void hids_boot_report_get_notif_state_cb(ble_client_t *hids_client,
                                                                hids_client_boot_report_type type,
                                                                att_error_t status, bool enabled)
{
        print_client_message(hids_client, "Get boot report notif state callback");

        if (!status) {
                printf("\tReport type: ");

                switch (type) {
                case HIDS_CLIENT_BOOT_KEYBOARD_INPUT:
                        printf("BOOT_KEYBOARD_INPUT\r\n");
                        break;
                case HIDS_CLIENT_BOOT_KEYBOARD_OUTPUT:
                        printf("BOOT_KEYBOARD_OUTPUT\r\n");
                        break;
                case HIDS_CLIENT_BOOT_MOUSE_INPUT:
                        printf("BOOT_MOUSE_INPUT\r\n");
                        break;
                }

                printf("\tNotifications: %s\r\n", enabled ? "enabled" : "disabled");
        }

        printf("\tStatus: 0x%02X\r\n", status);
}

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

static void hids_external_report_found_cb(ble_client_t *hids_client, att_error_t status,
                                                                        const att_uuid_t *uuid)
{
        print_client_message(hids_client, "External report found");
        printf("\tStatus: 0x%02X\r\n", status);

        if (!status) {
                printf("\tUUID: %s\r\n", format_uuid(uuid));
        }
}

static void hids_discover_external_reports_complete_cb(ble_client_t *hids_client)
{
        print_client_message(hids_client, "External report discovery complete");
        printf("External reports discovery completed\r\n");
}

static void hids_client_report_found_cb(ble_client_t *hids_client, att_error_t status,
                                                hids_client_report_type_t type, uint8_t report_id)
{
        print_client_message(hids_client, "Report found");
        printf("\tStatus: 0x%02X\r\n", status);

        if (!status) {
                printf("\tReport type: %d\r\n", type);
                printf("\tReport Id: %d\r\n", report_id);
        }

#if CFG_AUTO_ENABLE_NOTIFICATIONS
        if (status == ATT_ERROR_OK && type == HIDS_CLIENT_REPORT_TYPE_INPUT) {
                hids_client_input_report_set_notif_state(hids_client, report_id, true);
        }
#endif
}

static void hids_client_discover_reports_complete_cb(ble_client_t *hids_client)
{
        print_client_message(hids_client, "Reports discovery completed");
}

/**
 * HIDS Client callbacks
 */
static const hids_client_callbacks_t cb = {
        .report_map = hids_report_map_cb,
        .hid_info = hids_hid_info_cb,
        .report = hids_report_cb,
        .boot_report = hids_boot_report_cb,
        .get_protocol_mode = hids_get_protocol_mode_cb,
        .input_report_get_notif_state = hids_input_report_get_notif_state_cb,
        .boot_report_get_notif_state = hids_boot_report_get_notif_state_cb,
        .external_report_found = hids_external_report_found_cb,
        .discover_external_reports_complete = hids_discover_external_reports_complete_cb,
        .report_found = hids_client_report_found_cb,
        .discover_reports_complete = hids_client_discover_reports_complete_cb,
};

static void gatt_set_event_state_completed_cb(ble_client_t *gatt_client, gatt_client_event_t event,
                                                                                att_error_t status)
{
        print_client_message(gatt_client, "Set event state completed");
        printf("\tEvent: 0x%02X\r\n", event);
        printf("\tStatus: 0x%02X\r\n", status);
}

static void gatt_get_event_state_completed_cb(ble_client_t *gatt_client, gatt_client_event_t event,
                                                                att_error_t status, bool enabled)
{
        print_client_message(gatt_client, "Get event state completed");
        printf("\tEvent: 0x%02X\r\n", event);
        printf("\tStatus: 0x%02X\r\n", status);
        printf("\tEnabled: %s\r\n", enabled ? "True" : "False");
}

static void gatt_service_changed_cb(ble_client_t *gatt_client, uint16_t start_handle,
                                                                        uint16_t end_handle)
{
        print_client_message(gatt_client, "Service changed callback");
        printf("\tStart handle: 0x%04X\r\n", start_handle);
        printf("\tEnd handle: 0x%04X\r\n", end_handle);

        if (!peer_info.pending_browse) {
                queue_remove_all(&peer_info.clients, client_destroy);
                peer_info.pending_browse = true;
                peer_info.pending_init = true;

                /**
                 * Start discovery
                 */
                ble_gattc_browse(peer_info.conn_idx, NULL);
        } else {
                peer_info.svc_changed = true;
        }
}

/**
 * GATT Client callbacks
 */
static const gatt_client_callbacks_t gatt_cb = {
        .set_event_state_completed = gatt_set_event_state_completed_cb,
        .get_event_state_completed = gatt_get_event_state_completed_cb,
        .service_changed = gatt_service_changed_cb,
};

static void bas_read_battery_level_cb(ble_client_t *bas_client, att_error_t status,
                                                                        uint8_t level)
{
        print_client_message(bas_client, "Read battery level");
        printf("\tStatus: 0x%02X\r\n", status);

        if (!status) {
                printf("\tLevel: 0x%02X\r\n", level);
        }
}

static void bas_get_event_state_cb(ble_client_t *bas_client, bas_client_event_t event,
                                                                att_error_t status, bool enabled)
{
        print_client_message(bas_client, "Get event state completed");
        printf("\tEvent: 0x%02X\r\n", event);
        printf("\tStatus: 0x%02X\r\n", status);

        if (!status) {
                printf("\tBattery Level notifications: %s\r\n", enabled ? "enabled" : "disabled");
        }

        printf("\tStatus: 0x%02X\r\n", status);
}

static void bas_battery_level_notif_cb(ble_client_t *bas_client, uint8_t level)
{
        print_client_message(bas_client, "Battery level notification");
        printf("\tLevel: %02d%%\r\n", level);
}

static void bas_set_event_state_cb(ble_client_t *bas_client, bas_client_event_t event,
                                                                        att_error_t status)
{
        print_client_message(bas_client, "Set event state completed");
        printf("\tEvent: 0x%02X\r\n", event);
        printf("\tStatus: 0x%02X\r\n", status);
}

/**
 * Battery Service Client callbacks
 */
static const bas_client_callbacks_t bas_cb = {
        .read_battery_level_completed = bas_read_battery_level_cb,
        .set_event_state_completed = bas_set_event_state_cb,
        .get_event_state_completed = bas_get_event_state_cb,
        .battery_level_notif = bas_battery_level_notif_cb,
};

static void dis_read_completed_cb(ble_client_t *dis_client, att_error_t status,
                                                                dis_client_cap_t capability,
                                                                uint16_t length,
                                                                const uint8_t *value)
{
        print_client_message(dis_client, "Read completed");
        printf("\tCapability: %d\r\n", capability);
        printf("\tStatus: 0x%02X\r\n", status);
        printf("\tLength: 0x%04X", length);

        if (capability == DIS_CLIENT_CAP_PNP_ID && length == sizeof(dis_client_pnp_id_t)) {
                const dis_client_pnp_id_t *pnp_id = (const dis_client_pnp_id_t *) value;

                printf("\tVendor ID Source: 0x%02X\r\n", pnp_id->vid_source);
                printf("\tVendor ID: 0x%04X\r\n", pnp_id->vid_source);
                printf("\tProduct ID: 0x%04X\r\n", pnp_id->pid);
                printf("\tProduct Version: 0x%04X\r\n", pnp_id->version);
        }
}

/**
 *
 */
static const dis_client_callbacks_t dis_cb = {
        .read_completed = dis_read_completed_cb,
};

static void scps_refresh_notif_cb(ble_client_t *scps_client)
{
        const gap_scan_params_t scan_params = CFG_SCAN_PARAMS;

        print_client_message(scps_client, "Refresh characteristic notification");

        scps_client_write_scan_interval_window(scps_client, scan_params.interval,
                                                                        scan_params.window);
}

static void scps_set_event_state_completed_cb(ble_client_t *scps_client, scps_client_event_t event,
                                                                                att_error_t status)
{

        print_client_message(scps_client, "Set event state completed");
        printf("\tEvent: 0x%02X\r\n", event);
        printf("\tStatus: 0x%02X\r\n", status);
}

static void scps_get_event_state_completed_cb(ble_client_t *scps_client, scps_client_event_t event,
                                                                att_error_t status, bool enabled)
{
        print_client_message(scps_client, "Get event state completed");
        printf("\tEvent: 0x%02X\r\n", event);
        printf("\tStatus: 0x%02X\r\n", status);
        printf("\tEnabled: %s\r\n", enabled ? "true" : "false");
}

static const scps_client_callbacks_t scps_cb = {
        .refresh_notif = scps_refresh_notif_cb,
        .set_event_state_completed = scps_set_event_state_completed_cb,
        .get_event_state_completed = scps_get_event_state_completed_cb,
};

void hogp_host_wkup_handler(void)
{
        if (!hw_gpio_get_pin_status(CFG_TRIGGER_CONNECT_GPIO_PORT, CFG_TRIGGER_CONNECT_GPIO_PIN)) {
                OS_TASK_NOTIFY_FROM_ISR(current_task, WKUP_NOTIF, OS_NOTIFY_SET_BITS);
        }
}

static void handle_disconnected(ble_evt_gap_disconnected_t *evt)
{
        printf("Device disconnected\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tAddress: %s\r\n", ble_address_to_string(&evt->address));

        if (peer_info.conn_idx == evt->conn_idx) {
                if (peer_info.pending_browse) {
                        /* List of services is incomplete, so remove clients */
                        queue_remove_all(&peer_info.clients, client_destroy);
                }

                hogp_host_state = HOGP_HOST_STATE_DISCONNECTED;

                peer_info.conn_idx = BLE_CONN_IDX_INVALID;
                peer_info.pending_browse = false;
                peer_info.pending_sec = false;
                peer_info.svc_changed = false;
        }
}

static void initialize_clients(void)
{
        if (peer_info.pending_sec) {
                return;
        }

        if (peer_info.pending_browse) {
                return;
        }

        // Init was previously performed
        if (!peer_info.pending_init) {
                return;
        }

        queue_foreach(&peer_info.clients, client_init, NULL);

        peer_info.pending_init = false;
}

static void handle_connected(ble_evt_gap_connected_t *evt)
{
        const bd_address_t addr = CFG_PERIPH_ADDR;

        printf("Device connected\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tAddress: %s\r\n", ble_address_to_string(&evt->peer_address));

        if (hogp_host_state == HOGP_HOST_STATE_CONNECTING && !memcmp(&evt->peer_address, &addr,
                                                                                sizeof(addr))) {
                bool bonded;

                peer_info.conn_idx = evt->conn_idx;
                hogp_host_state = HOGP_HOST_STATE_CONNECTED;

                ble_gap_is_bonded(peer_info.conn_idx, &bonded);
                if (bonded) {
                        ble_gap_set_sec_level(peer_info.conn_idx, GAP_SEC_LEVEL_2);
                } else {
                        ble_gap_pair(peer_info.conn_idx, true);
                }

                peer_info.pending_sec = true;

                if (queue_length(&peer_info.clients) == 0) {
                        /**
                         * Start discovery
                         */
                        ble_gattc_browse(peer_info.conn_idx, NULL);
                        peer_info.pending_browse = true;
                } else {
                        queue_foreach(&peer_info.clients, client_attach, &peer_info.conn_idx);
                }

                peer_info.pending_init = true;
        }
}

static void handle_gattc_browse_svc(ble_evt_gattc_browse_svc_t *evt)
{
        ble_client_t *client = NULL;
        client_type_t type;
        att_uuid_t uuid;

        if (evt->conn_idx != peer_info.conn_idx || peer_info.svc_changed) {
                return;
        }

        printf("Service found during browsing\r\n");
        printf("\tStart handle: 0x%04X\r\n", evt->start_h);
        printf("\tEnd handle: 0x%04X\r\n", evt->end_h);

        ble_uuid_create16(UUID_SERVICE_HIDS, &uuid);
        if (ble_uuid_equal(&uuid, &evt->uuid)) {
                client = hids_client_init(&hids_config, &cb, evt);
                type = CLIENT_TYPE_HIDS;
                goto done;
        }

        ble_uuid_create16(UUID_SERVICE_GATT, &uuid);
        if (ble_uuid_equal(&uuid, &evt->uuid)) {
                client = gatt_client_init(&gatt_cb, evt);
                type = CLIENT_TYPE_GATT;
                goto done;
        }

        ble_uuid_create16(UUID_SERVICE_BAS, &uuid);
        if (ble_uuid_equal(&uuid, &evt->uuid)) {
                client = bas_client_init(&bas_cb, evt);
                type = CLIENT_TYPE_BAS;
                goto done;
        }

        ble_uuid_create16(UUID_SERVICE_DIS, &uuid);
        if (ble_uuid_equal(&uuid, &evt->uuid)) {
                client = dis_client_init(&dis_cb, evt);
                type = CLIENT_TYPE_DIS;
                goto done;
        }

        ble_uuid_create16(UUID_SERVICE_SCPS, &uuid);
        if (ble_uuid_equal(&uuid, &evt->uuid)) {
                client = scps_client_init(&scps_cb, evt);
                type = CLIENT_TYPE_SCPS;
                goto done;
        }

done:
        if (client) {
                client_new(client, type);
        }
}

static void handle_gattc_browse_cmpl(ble_evt_gattc_browse_completed_t *evt)
{
        printf("Browsing procedure completed\r\n");

        if (evt->conn_idx != peer_info.conn_idx) {
                return;
        }

        if (peer_info.svc_changed) {
                peer_info.svc_changed = false;

                /**
                 * Remove hids clients and start discovery
                 */
                queue_remove_all(&peer_info.clients, client_destroy);
                peer_info.pending_browse = true;

                /**
                 * Restart discovery
                 */
                ble_gattc_browse(peer_info.conn_idx, NULL);
        } else {
                peer_info.pending_browse = false;
        }

        initialize_clients();
}

static void security_done(uint16_t conn_idx)
{
        if (conn_idx != peer_info.conn_idx) {
                return;
        }

        peer_info.pending_sec = false;

        initialize_clients();
}

static void handle_gap_sec_level_changed(ble_evt_gap_sec_level_changed_t *evt)
{
        security_done(evt->conn_idx);
}

static void handle_gap_set_sec_level_failed(ble_evt_gap_set_sec_level_failed_t *evt)
{
        security_done(evt->conn_idx);
}

static void handle_gap_pair_completed(ble_evt_gap_pair_completed_t *evt)
{
        security_done(evt->conn_idx);
}

static void handle_security_request(ble_evt_gap_security_request_t *evt)
{
        /**
         * Send pairing request to remote device or enable encryption with existing keys
         */
        if (ble_gap_pair(evt->conn_idx, evt->bond) == BLE_ERROR_ALREADY_DONE) {
                ble_gap_set_sec_level(evt->conn_idx, GAP_SEC_LEVEL_2);
        }
}

static void handle_button_press(void)
{
        const bd_address_t addr = CFG_PERIPH_ADDR;
        const gap_conn_params_t cp = CFG_CONN_PARAMS;

        switch (hogp_host_state) {
        case HOGP_HOST_STATE_DISCONNECTED:
                /**
                 * Connect to remote device
                 */
                ble_gap_connect(&addr, &cp);
                hogp_host_state = HOGP_HOST_STATE_CONNECTING;
                break;
        case HOGP_HOST_STATE_CONNECTING:
                /**
                 * Cancel connection to remote device
                 */
                ble_gap_connect_cancel();
                hogp_host_state = HOGP_HOST_STATE_DISCONNECTED;
                break;
        case HOGP_HOST_STATE_CONNECTED:
                ble_gap_disconnect(peer_info.conn_idx, BLE_HCI_ERROR_REMOTE_USER_TERM_CON);
                hogp_host_state = HOGP_HOST_STATE_DISCONNECTING;
                break;
        case HOGP_HOST_STATE_DISCONNECTING:
        default:
                break;
        }
}

struct client_id_type {
        client_type_t type;
        uint8_t id;
};

static bool client_match_id_type(const void *data, const void *match_data)
{
        const struct client_id_type *client_data = match_data;
        const client_t *client = data;

        if (client->id != client_data->id) {
                return false;
        }

        return (client->type == client_data->type);
}

client_t *get_client(uint8_t id, client_type_t type)
{
        struct client_id_type data;

        data.type = type;
        data.id = id;

        return queue_find(&peer_info.clients, client_match_id_type, &data);
}

void hogp_connect(void)
{
        if (hogp_host_state != HOGP_HOST_STATE_DISCONNECTED) {
                return;
        }

        handle_button_press();
}

void hogp_disconnect(void)
{
         if (hogp_host_state != HOGP_HOST_STATE_DISCONNECTED) {
                 handle_button_press();
         }
}

void hogp_host_task(void *params)
{
        int8_t wdog_id;
        const gap_scan_params_t scan_params = CFG_SCAN_PARAMS;
        cli_t cli;

        /* register hogp_host task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        ble_enable();
        ble_gap_role_set(GAP_CENTRAL_ROLE);
        ble_register_app();

        queue_init(&peer_info.clients);
        current_task = OS_GET_CURRENT_TASK();

        ble_gap_appearance_set(BLE_GAP_APPEARANCE_GENERIC_HID, ATT_PERM_READ);
        ble_gap_device_name_set("Dialog HOGP Host", ATT_PERM_READ);
        ble_gap_scan_params_set(&scan_params);

        cli = register_debug(CLI_NOTIF);

        printf("HOGP Host application started\r\n");

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
                ret = OS_TASK_NOTIFY_WAIT(0, (uint32_t) -1, &notif, portMAX_DELAY);
                /* Guaranteed to succeed since we're waiting forever for a notification */
                OS_ASSERT(ret == OS_TASK_NOTIFY_SUCCESS);

                /* resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

                /* notified from BLE manager, can get event */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        /*
                         * no need to wait for event, should be already there since we were notified
                         * from manager
                         */
                        hdr = ble_get_event(false);
                        OS_ASSERT(hdr);

                        ble_client_handle_event(hdr);

                        if (!ble_service_handle_event(hdr)) {
                                switch (hdr->evt_code) {
                                case BLE_EVT_GAP_CONNECTED:
                                        handle_connected((ble_evt_gap_connected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_DISCONNECTED:
                                        handle_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_SECURITY_REQUEST:
                                        handle_security_request((ble_evt_gap_security_request_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_BROWSE_SVC:
                                        handle_gattc_browse_svc((ble_evt_gattc_browse_svc_t *) hdr);
                                        break;
                                case BLE_EVT_GATTC_BROWSE_COMPLETED:
                                        handle_gattc_browse_cmpl((ble_evt_gattc_browse_completed_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_SEC_LEVEL_CHANGED:
                                        handle_gap_sec_level_changed((ble_evt_gap_sec_level_changed_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_SET_SEC_LEVEL_FAILED:
                                        handle_gap_set_sec_level_failed((ble_evt_gap_set_sec_level_failed_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_PAIR_COMPLETED:
                                        handle_gap_pair_completed((ble_evt_gap_pair_completed_t *) hdr);
                                        break;
                                default:
                                        ble_handle_event_default(hdr);
                                        break;
                                }
                        }

                        OS_FREE(hdr);

                        // notify again if there are more events to process in queue
                        if (ble_has_event()) {
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK, eSetBits);
                        }
                }

                if (notif & WKUP_NOTIF) {
                        handle_button_press();
                }

                if (notif & CLI_NOTIF) {
                        cli_handle_notified(cli);
                }
        }
}
