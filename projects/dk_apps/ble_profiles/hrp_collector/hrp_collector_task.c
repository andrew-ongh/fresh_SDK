/**
 ****************************************************************************************
 *
 * @file hrp_collector_task.c
 *
 * @brief Heart Rate Collector task
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util/queue.h"
#include "osal.h"
#include "cli.h"
#include "ble_bufops.h"
#include "ble_client.h"
#include "ble_storage.h"
#include "ble_uuid.h"
#include "hrs_client.h"
#include "dis_client.h"
#include "hrp_collector_config.h"

/**
 * CLI notify mask
 */
#define CLI_NOTIF               (1 << 1)

#define MAX_FOUND_DEVICES       25

#define HRS_CLIENT_STORAGE_ID   BLE_STORAGE_KEY_APP(0, 0)
#define DIS_CLIENT_STORAGE_ID   BLE_STORAGE_KEY_APP(0, 1)

#define ADDR_RESOLVABLE(bdaddr) (bdaddr.addr_type == PRIVATE_ADDRESS &&      \
                                (bdaddr.addr[5] & 0xc0) == 0x40)

typedef enum {
        PENDING_ACTION_READ_SENSOR_LOCATION = (1 << 1),
        PENDING_ACTION_READ_MANUFACTURER = (1 << 2),
        PENDING_ACTION_READ_MODEL = (1 << 3),
        PENDING_ACTION_READ_FW_VERSION = (1 << 4),
        PENDING_ACTION_READ_SW_VERSION = (1 << 5),
        PENDING_ACTION_ENABLE_NOTIF = (1 << 6),
} pending_action_t;

typedef enum {
        APP_STATE_IDLE,
        APP_STATE_CONNECTING,
        APP_STATE_SCANNING,
} app_state_t;

typedef enum {
        AUTH_TYPE_ENCRYPT,
        AUTH_TYPE_PAIR,
        AUTH_TYPE_BOND,
} auth_type_t;

typedef struct {
        bd_address_t addr;
        bool name_found;
} found_device_t;

typedef struct {
        void *next;

        bd_address_t addr;
        uint16_t conn_idx;

        ble_client_t *hrs_client;
        ble_client_t *dis_client;

        bool busy_init;
        pending_action_t pending_init;

        bool busy_auth;
        bool pending_browse;
        bool notif_state;
} peer_info_t;

/* Application state */
__RETAINED static app_state_t app_state;

/* Current peers informations */
__RETAINED static queue_t peer_info_queue;

/* Scanning state */
__RETAINED static struct {
        bool match_any;
        found_device_t devices[MAX_FOUND_DEVICES];
        size_t num_devices;
} scan_state;

static bool peer_conn_idx_match(const void *data, const void *match_data)
{
        const peer_info_t *peer_info = (peer_info_t *) data;
        const uint16_t conn_idx = *(uint16_t *) match_data;

        return peer_info->conn_idx == conn_idx;
}

static inline void add_peer_info(peer_info_t *peer_info)
{
        queue_push_front(&peer_info_queue, peer_info);
}

static inline peer_info_t *remove_peer_info(uint16_t conn_idx)
{
        return queue_remove(&peer_info_queue, peer_conn_idx_match, &conn_idx);
}

static inline peer_info_t *find_peer_info(uint16_t conn_idx)
{
        return queue_find(&peer_info_queue, peer_conn_idx_match, &conn_idx);
}

static bool start_auth(peer_info_t *peer_info, auth_type_t auth_type, bool mitm);

static void store_client(uint16_t conn_idx, ble_client_t *client, ble_storage_key_t key)
{
        uint8_t *buffer = NULL;
        size_t length;

        if (!client) {
                return;
        }

        /* Get serialized BLE Client length */
        ble_client_serialize(client, NULL, &length);
        buffer = OS_MALLOC(length);
        /* Serialize BLE Client */
        ble_client_serialize(client, buffer, &length);
        /* Put BLE Client to the storage */
        ble_storage_put_buffer(conn_idx, key, length, buffer, OS_FREE_FUNC, true);
}

static void remove_client_data_from_storage(peer_info_t *peer_info)
{
        ble_storage_remove(peer_info->conn_idx, HRS_CLIENT_STORAGE_ID);
        ble_storage_remove(peer_info->conn_idx, DIS_CLIENT_STORAGE_ID);
}

static found_device_t *get_found_device(const bd_address_t *addr, size_t *index)
{
        size_t i;

        for (i = 0; i < scan_state.num_devices; i++) {
                found_device_t *dev = &scan_state.devices[i];

                if (ble_address_cmp(&dev->addr, addr)) {
                        *index = i + 1;
                        return dev;
                }
        }

        return NULL;
}

static inline found_device_t *add_found_device(const bd_address_t *addr, size_t *index)
{
        static found_device_t tmp_dev;
        found_device_t *dev;

        if (scan_state.num_devices >= MAX_FOUND_DEVICES) {
                dev = &tmp_dev;
                *index = 0;
        } else {
                dev = &scan_state.devices[scan_state.num_devices++];
                *index = scan_state.num_devices;
        }

        dev->addr = *addr;
        dev->name_found = false;

        return dev;
}

static bool set_notif_state(peer_info_t *peer_info, bool new_state)
{
        peer_info->notif_state = new_state;

        return hrs_client_set_event_state(peer_info->hrs_client,
                                        HRS_CLIENT_EVENT_HEART_RATE_MEASUREMENT_NOTIF, new_state);
}

#define pending_init_execute_and_check(PEER_INFO, FLAG, FUNCTION, ...)          \
        ({                                                                      \
                if (PEER_INFO->pending_init & FLAG) {                           \
                        PEER_INFO->busy_init = FUNCTION(__VA_ARGS__);           \
                        if (!PEER_INFO->busy_init) {                            \
                                /* Failed to execute action, clear bit */       \
                                PEER_INFO->pending_init &= ~FLAG;               \
                        }                                                       \
                }                                                               \
                PEER_INFO->busy_init;                                           \
        })

static void process_pending_actions(peer_info_t *peer_info)
{
        if (peer_info->busy_init) {
                return;
        }

        if (pending_init_execute_and_check(peer_info, PENDING_ACTION_READ_SENSOR_LOCATION,
                                hrs_client_read_body_sensor_location, peer_info->hrs_client)) {
                return;
        }

        if (pending_init_execute_and_check(peer_info, PENDING_ACTION_READ_MANUFACTURER,
                                                        dis_client_read, peer_info->dis_client,
                                                        DIS_CLIENT_CAP_MANUFACTURER_NAME)) {
                return;
        }

        if (pending_init_execute_and_check(peer_info, PENDING_ACTION_READ_MODEL, dis_client_read,
                                        peer_info->dis_client, DIS_CLIENT_CAP_MODEL_NUMBER)) {
                return;
        }

        if (pending_init_execute_and_check(peer_info, PENDING_ACTION_READ_FW_VERSION,
                                                        dis_client_read, peer_info->dis_client,
                                                        DIS_CLIENT_CAP_FIRMWARE_REVISION)) {
                return;
        }

        if (pending_init_execute_and_check(peer_info, PENDING_ACTION_READ_SW_VERSION,
                                                        dis_client_read, peer_info->dis_client,
                                                        DIS_CLIENT_CAP_SOFTWARE_REVISION)) {
                return;
        }

        if (pending_init_execute_and_check(peer_info, PENDING_ACTION_ENABLE_NOTIF, set_notif_state,
                                                                                peer_info, true)) {
                return;
        }

        printf("Ready.\r\n");
}

static void add_pending_action(peer_info_t *peer_info, pending_action_t action)
{
        peer_info->pending_init |= action;

        process_pending_actions(peer_info);
}

static void clear_pending_action(peer_info_t *peer_info, pending_action_t action, att_error_t error)
{
        /* Do nothing if we try to clear action which is not pending */
        if ((peer_info->pending_init & action) == 0) {
                return;
        }

        /* Try to authenticate if action failed due to unsufficient authentication/ecnryption */
        if ((error == ATT_ERROR_INSUFFICIENT_AUTHENTICATION) ||
                                                (error == ATT_ERROR_INSUFFICIENT_ENCRYPTION)) {
                peer_info->busy_init = false;
                start_auth(peer_info, AUTH_TYPE_PAIR, false);
                return;
        }

        peer_info->busy_init = false;
        peer_info->pending_init &= ~action;

        process_pending_actions(peer_info);
}

static void start_browse(peer_info_t *peer_info)
{
        printf("Browsing...\r\n");
        ble_gattc_browse(peer_info->conn_idx, NULL);
}

static bool start_auth(peer_info_t *peer_info, auth_type_t auth_type, bool mitm)
{
        gap_device_t gap_dev;

        if (peer_info->busy_auth) {
                /* We're already doing authentication, ignore */
                return false;
        }

        if (ble_gap_get_device_by_conn_idx(peer_info->conn_idx, &gap_dev) != BLE_STATUS_OK) {
                return false;
        }

        if ((auth_type == AUTH_TYPE_ENCRYPT) && !gap_dev.paired) {
                return false;
        }

        if (gap_dev.paired) {
                ble_gap_set_sec_level(peer_info->conn_idx, mitm ? GAP_SEC_LEVEL_3 : GAP_SEC_LEVEL_2);
        } else {
                ble_gap_pair(peer_info->conn_idx, auth_type == AUTH_TYPE_BOND);
        }

        peer_info->busy_auth = true;

        return true;
}

static void finish_auth(peer_info_t *peer_info, ble_error_t status)
{
        if (!peer_info->busy_auth) {
                return;
        }

        if (status == BLE_ERROR_ENC_KEY_MISSING) {
                peer_info->pending_browse = true;

                // Remove stored clients and feature from storage
                remove_client_data_from_storage(peer_info);

                // Cleanup clients and feature
                if (peer_info->hrs_client) {
                        ble_client_remove(peer_info->hrs_client);
                        ble_client_cleanup(peer_info->hrs_client);
                        peer_info->hrs_client = NULL;
                }

                if (peer_info->dis_client) {
                        ble_client_remove(peer_info->dis_client);
                        ble_client_cleanup(peer_info->dis_client);
                        peer_info->dis_client = NULL;
                }

                printf("Encrypt key missing. Trying to pair again.\r\n");
                ble_gap_pair(peer_info->conn_idx, true);
                return;
        }

        peer_info->busy_auth = false;

        if (peer_info->pending_browse) {
                start_browse(peer_info);
                return;
        }

        /* Security is completed, check if there are pending init actions to complete */
        process_pending_actions(peer_info);
}

static void clicmd_scan_usage(void)
{
        printf("usage: scan <start|stop> [any]\r\n");
        printf("\t\"any\" will disable filtering devices by HRS UUID, only valid for \"scan start\"\r\n");
}

static void clicmd_scan_handler(int argc, const char *argv[], void *user_data)
{
        ble_error_t status;

        if (argc < 2) {
                clicmd_scan_usage();
                return;
        }

        if (!strcasecmp("start", argv[1])) {
                if (app_state != APP_STATE_IDLE) {
                        printf("ERROR: application need to be in idle state to start scanning\r\n");
                        return;
                }

                status = ble_gap_scan_start(GAP_SCAN_ACTIVE, GAP_SCAN_OBSERVER_MODE,
                                                                BLE_SCAN_INTERVAL_FROM_MS(30),
                                                                BLE_SCAN_WINDOW_FROM_MS(30),
                                                                false, false);

                if (status != BLE_STATUS_OK){
                        printf("ERROR: application already scanning\r\n");
                        return;
                }

                printf("Scanning...\r\n");

                app_state = APP_STATE_SCANNING;

                scan_state.match_any = (argc > 2) && !strcmp(argv[2], "any");
                scan_state.num_devices = 0;
        } else if (!strcasecmp("stop", argv[1])) {
                if (app_state != APP_STATE_SCANNING) {
                        printf("ERROR: application need to be in scanning state to stop "
                                                                                "scanning\r\n");
                        return;
                }

                status = ble_gap_scan_stop();

                if (status != BLE_STATUS_OK) {
                        printf("ERROR: no scan session in progress\r\n");
                }

                printf("Scan stopping...\r\n");
        } else {
                clicmd_scan_usage();
        }
}

static void clicmd_connect_usage(void)
{
        printf("usage: connect <address [public|private] | index>\r\n");
        printf("       connect cancel\r\n");
        printf("\tinstead of address, index of found device can be passed\r\n");
        printf("\tif not specified, public address is assumed\r\n");
        printf("\tuse 'connect cancel' to cancel any outgoing connection attempt\r\n");
}

static void clicmd_connect_handler(int argc, const char *argv[], void *user_data)
{
        static const gap_conn_params_t cp = {
                .interval_min = BLE_CONN_INTERVAL_FROM_MS(50),
                .interval_max = BLE_CONN_INTERVAL_FROM_MS(70),
                .slave_latency = 0,
                .sup_timeout = BLE_SUPERVISION_TMO_FROM_MS(1000),
        };

        bd_address_t addr;
        size_t dev_index;
        ble_error_t status;

        if (argc < 2) {
                clicmd_connect_usage();
                return;
        }

        if (!strcasecmp("cancel", argv[1])) {
                if (app_state != APP_STATE_CONNECTING) {
                        printf("ERROR: application need to be in connecting state to stop "
                                                                                "connecting\r\n");
                        return;
                }

                status = ble_gap_connect_cancel();

                if (status != BLE_STATUS_OK) {
                        printf("ERROR: no active connection attempt to cancel\r\n");
                        return;
                }

                app_state = APP_STATE_IDLE;

                return;
        }

        if (app_state != APP_STATE_IDLE) {
                printf("ERROR: application need to be in idle state to connect\r\n");
                return;
        }

        /* Check if the application achieves max number of connected devices (sensors) */
        if (queue_length(&peer_info_queue) >= BLE_GAP_MAX_CONNECTED) {
                printf("ERROR: max number of connected devices was achieved\r\n");
                return;
        }

        /*
         * If argument cannot be parsed to valid address, check if it can be used as index in
         * found devices cache.
         */
        if (!ble_address_from_string(argv[1], PUBLIC_ADDRESS, &addr)) {
                dev_index = atoi(argv[1]);
                if (dev_index < 1 || dev_index > scan_state.num_devices) {
                        clicmd_connect_usage();
                        return;
                }

                addr = scan_state.devices[dev_index - 1].addr;
        } else {
                if (argc > 2) {
                        /*
                         * If address type argument is present, check for "private" or leave "public"
                         * as set by default.
                         */

                        if (!strcasecmp("private", argv[2])) {
                                addr.addr_type = PRIVATE_ADDRESS;
                        }
                } else {
                        size_t i;

                        /*
                         * If address type is not present try to check for address in found devices
                         * cache, otherwise leave "public".
                         */

                        for (i = 0; i < scan_state.num_devices; i++) {
                                found_device_t *dev = &scan_state.devices[i];

                                if (!memcmp(&dev->addr.addr, &addr.addr, sizeof(addr.addr))) {
                                        addr.addr_type = dev->addr.addr_type;
                                        break;
                                }
                        }
                }
        }

        status = ble_gap_connect(&addr, &cp);

        if (status != BLE_STATUS_OK) {
                printf("ERROR: connection failed\r\n");
                printf("\tStatus: 0x%02x\r\n", status);
                return;
        }

        printf("Connecting to %s ...\r\n", ble_address_to_string(&addr));

        app_state = APP_STATE_CONNECTING;
}

static void clicmd_disconnect_handler(int argc, const char *argv[], void *user_data)
{
        peer_info_t *peer_info;
        uint16_t conn_idx;

        if (argc < 2) {
                peer_info = queue_peek_front(&peer_info_queue);
        } else {
                conn_idx = atoi(argv[1]);
                peer_info = find_peer_info(conn_idx);
        }

        if (!peer_info) {
                printf("ERROR: application has to be connected to disconnect\r\n");
                return;
        }

        ble_gap_disconnect(peer_info->conn_idx, BLE_HCI_ERROR_REMOTE_USER_TERM_CON);

        printf("Disconnected from %s\r\n", ble_address_to_string(&peer_info->addr));
}

static void clicmd_notifications_usage(void)
{
        printf("usage: notifications <conn_idx> <on|off>\r\n");
}

static void clicmd_notifications_handler(int argc, const char *argv[], void *user_data)
{
        peer_info_t *peer_info;
        uint16_t conn_idx;

        if (argc < 3) {
                clicmd_notifications_usage();
                return;
        }

        conn_idx = atoi(argv[1]);
        peer_info = find_peer_info(conn_idx);

        if (!peer_info) {
                printf("ERROR: connection index is not recognized\r\n");
                return;
        }

        if (!strcasecmp("on", argv[2])) {
                set_notif_state(peer_info, true);
        } else if (!strcasecmp("off", argv[2])) {
                set_notif_state(peer_info, false);
        } else {
                clicmd_notifications_usage();
        }
}

static void clicmd_reset_ee_usage(void)
{
        printf("usage: reset_ee <conn_idx>\r\n");
}

static void clicmd_reset_ee_handler(int argc, const char *argv[], void *user_data)
{
        peer_info_t *peer_info;
        uint16_t conn_idx;

        if (argc < 2) {
                clicmd_reset_ee_usage();
                return;
        }

        conn_idx = atoi(argv[1]);
        peer_info = find_peer_info(conn_idx);

        if (!peer_info) {
                printf("ERROR: connection index is not recognized\r\n");
                return;
        }

        if (!hrs_client_reset_energy_expended(peer_info->hrs_client)) {
                printf("ERROR: failed to reset energy expended\r\n");
                return;
        }
}

static void show_devices(gap_device_filter_t filter)
{
        size_t i, dev_count = BLE_GAP_MAX_CONNECTED;
        gap_device_t devices[BLE_GAP_MAX_CONNECTED];

        ble_gap_get_devices(filter, NULL, &dev_count, devices);

        printf("%s devices (%u)\r\n", GAP_DEVICE_FILTER_BONDED == filter ? "Bonded" : "Connected",
                                                                                        dev_count);

        for (i = 0; i < dev_count; i++) {
                if (filter == GAP_DEVICE_FILTER_BONDED) {
                        printf("\tAddress: %s %s\r\n",
                                devices[i].address.addr_type == PUBLIC_ADDRESS ?
                                "public " : "private",
                                ble_address_to_string(&devices[i].address));
                } else {
                        printf("\tAddress: %s %s conn_idx: %d\r\n",
                                devices[i].address.addr_type == PUBLIC_ADDRESS ?
                                "public" : "private",
                                ble_address_to_string(&devices[i].address), devices[i].conn_idx);
                }
        }
}

static void clicmd_show_usage(void)
{
        printf("usage: show [connected|bonded]\r\n");
}

static void clicmd_show_handler(int argc, const char *argv[], void *user_data)
{
        if (argc < 2) {
                clicmd_show_usage();
                return;
        }

        if (!strcasecmp("connected", argv[1])) {
                show_devices(GAP_DEVICE_FILTER_CONNECTED);
        } else if (!strcasecmp("bonded", argv[1])) {
                show_devices(GAP_DEVICE_FILTER_BONDED);
        } else {
                clicmd_show_usage();
        }
}

static void clicmd_unbond_usage(void)
{
        printf("usage: unbond [[public|private] <address> | all]\r\n");
        printf("\tpublic    set address type public\r\n");
        printf("\tprivate   set address type private\r\n");
        printf("\taddress   address of bonded device\r\n");
        printf("\tall       unbond all bonded devices\r\n");
}

static void print_unbond_info(ble_error_t status, bd_address_t *address)
{
        printf("Unbond device\r\n");
        printf("\tStatus: 0x%02x\r\n", status);
        printf("\tAddress: %s\r\n", address ? ble_address_to_string(address) : "not found");
}

static void unbond_all(void)
{
        uint8_t i, length;
        bd_address_t *bonded_devices;
        ble_error_t status;

        ble_gap_get_bonded(&length, &bonded_devices);

        if (!length) {
                print_unbond_info(BLE_ERROR_NOT_FOUND, NULL);
        }

        for (i = 0; i < length; i++) {
                status = ble_gap_unpair(&bonded_devices[i]);
                print_unbond_info(status, &bonded_devices[i]);
        }

        OS_FREE(bonded_devices);
}

static void unbond_by_address(bd_address_t *address)
{
        ble_error_t status;

        status = ble_gap_unpair(address);
        print_unbond_info(status, address);
}

static void clicmd_unbond_handler(int argc, const char *argv[], void *user_data)
{
        bd_address_t address;

        if (argc < 2) {
                clicmd_unbond_usage();
                return;
        }

        if (!strcasecmp("all", argv[1])) {
                unbond_all();
                return;
        }

        if (!strcasecmp("public", argv[1])) {
                if (argc < 3 || !ble_address_from_string(argv[2], PUBLIC_ADDRESS, &address)) {
                        clicmd_unbond_usage();
                        return;
                }
        } else if (!strcasecmp("private", argv[1])) {
                if (argc < 3 || !ble_address_from_string(argv[2], PRIVATE_ADDRESS, &address)) {
                        clicmd_unbond_usage();
                        return;
                }
        } else if (!ble_address_from_string(argv[1], PUBLIC_ADDRESS, &address)) {
                clicmd_unbond_usage();
                return;
        }

        unbond_by_address(&address);
}

static void clicmd_default_handler(int argc, const char *argv[], void *user_data)
{
        printf("Valid commands:\r\n");
        printf("\tscan <start|stop> [any]\r\n");
        printf("\tconnect <address [public|private] | index>\r\n");
        printf("\tconnect cancel\r\n");
        printf("\tnotifications <conn_idx> <on|off>\r\n");
        printf("\treset_ee <conn_idx>\r\n");
        printf("\tdisconnect [<conn_idx>]\r\n");
        printf("\tshow [connected|bonded]\r\n");
        printf("\tunbond [[public|private] <address> | all]\r\n");
}

static const cli_command_t clicmd[] = {
        { .name = "scan",               .handler = clicmd_scan_handler, },
        { .name = "connect",            .handler = clicmd_connect_handler, },
        { .name = "notifications",      .handler = clicmd_notifications_handler, },
        { .name = "reset_ee",           .handler = clicmd_reset_ee_handler, },
        { .name = "disconnect",         .handler = clicmd_disconnect_handler, },
        { .name = "show",               .handler = clicmd_show_handler, },
        { .name = "unbond",             .handler = clicmd_unbond_handler, },
        {},
};

void hrs_heart_rate_measurement_notif_cb(ble_client_t *client, hrs_client_measurement_t *measurement)
{
        printf("Heart Rate Measurement notification received\r\n");
        printf("\tConnection index: %u\r\n", client->conn_idx);

        printf("\tValue: %d bpm\r\n", measurement->bpm);

        if (measurement->contact_supported) {
                printf("\tSensor Contact: supported, %s\r\n",
                                        measurement->contact_detected ? "detected" : "not detected");
        } else {
                printf("\tSensor Contact: not supported\r\n");
        }

        if (measurement->has_energy_expended) {
                printf("\tEnergy Expended: %d kJ\r\n", measurement->energy_expended);
        } else {
                printf("\tEnergy Expended: not supported\r\n");
        }

        if (measurement->rr_num) {
                int i;

                printf("\tRR-Intervals: ");

                for (i = 0; i < measurement->rr_num; i++) {
                        printf("%d ", measurement->rr[i]);
                }

                printf("\r\n");
        } else {
                printf("\tRR-Intervals: not supported\r\n");
        }

        printf("\r\n");
        OS_FREE(measurement);
}

void hrs_set_event_state_completed_cb(ble_client_t *client, hrs_client_event_t event, att_error_t status)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(client->conn_idx);

        if (!peer_info) {
                printf("ERROR: failed to set notifications - unknown device\r\n");
                printf("\tConnection index: %u\r\n", client->conn_idx);
                return;
        }

        /*
         * This is the only event supported right now, but more can be added in future versions so
         * need to check here.
         */
        if (event != HRS_CLIENT_EVENT_HEART_RATE_MEASUREMENT_NOTIF) {
                return;
        }

        if (status == ATT_ERROR_OK) {
                printf("Heart Rate Measurement notifications %s\r\n", peer_info->notif_state ?
                                                                        "enabled" : "disabled");
        } else {
                printf("ERROR: failed to set notifications (0x%02x)\r\n", status);
        }
        printf("\tConnection index: %u\r\n", client->conn_idx);

        clear_pending_action(peer_info, PENDING_ACTION_ENABLE_NOTIF, status);
}

static char *body_sensor_location_to_string(hrs_client_body_sensor_location_t location)
{
        switch (location) {
        case HRS_CLIENT_BODY_SENSOR_LOC_OTHER:
                return "other";
        case HRS_CLIENT_BODY_SENSOR_LOC_CHEST:
                return "chest";
        case HRS_CLIENT_BODY_SENSOR_LOC_WRIST:
                return "wrist";
        case HRS_CLIENT_BODY_SENSOR_LOC_FINGER:
                return "finger";
        case HRS_CLIENT_BODY_SENSOR_LOC_HAND:
                return "hand";
        case HRS_CLIENT_BODY_SENSOR_LOC_EAR_LOBE:
                return "ear lobe";
        case HRS_CLIENT_BODY_SENSOR_LOC_FOOT:
                return "foot";
        }

        return "unknown";
}

void hrs_read_body_sensor_location_completed_cb(ble_client_t *client, att_error_t status,
                                                        hrs_client_body_sensor_location_t location)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(client->conn_idx);
        if (!peer_info) {
                printf("\tFailed to read Body Sensor Location - unknown device\r\n");
                return;
        }

        if (status == ATT_ERROR_OK) {
                printf("\tBody Sensor Location: %s\r\n", body_sensor_location_to_string(location));
        } else {
                printf("\tFailed to read Body Sensor Location (%#x)\r\n", status);
        }

        printf("\tConnection index: %u\r\n", client->conn_idx);

        clear_pending_action(peer_info, PENDING_ACTION_READ_SENSOR_LOCATION, status);
}

void hrs_reset_energy_expended_completed_cb(ble_client_t *client, att_error_t status)
{
        printf("Reset Energy Expended\r\n");
        printf("\tConnection index: %d\r\n", client->conn_idx);
        printf("\tStatus: 0x%02x\r\n", status);
}

static const hrs_client_callbacks_t hrs_callbacks = {
        .heart_rate_measurement_notif = hrs_heart_rate_measurement_notif_cb,
        .set_event_state_completed = hrs_set_event_state_completed_cb,
        .read_body_sensor_location_completed = hrs_read_body_sensor_location_completed_cb,
        .reset_energy_expended_completed =  hrs_reset_energy_expended_completed_cb,
};

static void dis_read_completed_cb(ble_client_t *dis_client, att_error_t status,
                                                        dis_client_cap_t capability,
                                                        uint16_t length, const uint8_t *value)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(dis_client->conn_idx);
        if (!peer_info) {
                return;
        }

        printf("Read DIS informations\r\n");
        printf("\tConnection index: %u\r\n", dis_client->conn_idx);

        switch (capability) {
        case DIS_CLIENT_CAP_MANUFACTURER_NAME:
                if (status == ATT_ERROR_OK) {
                        printf("\tManufacturer: %.*s\r\n", length, value);
                } else {
                        printf("\tFailed to read Manufacturer information (%#x)\r\n", status);
                }
                clear_pending_action(peer_info, PENDING_ACTION_READ_MANUFACTURER, status);
                break;
        case DIS_CLIENT_CAP_MODEL_NUMBER:
                if (status == ATT_ERROR_OK) {
                        printf("\tModel: %.*s\r\n", length, value);
                } else {
                        printf("\tFailed to read Model information (%#x)\r\n", status);
                }
                clear_pending_action(peer_info, PENDING_ACTION_READ_MODEL, status);
                break;
        case DIS_CLIENT_CAP_FIRMWARE_REVISION:
                if (status == ATT_ERROR_OK) {
                        printf("\tFirmware version: %.*s\r\n", length, value);
                } else {
                        printf("\tFailed to read FW Version information (%#x)\r\n", status);
                }
                clear_pending_action(peer_info, PENDING_ACTION_READ_FW_VERSION, status);
                break;
        case DIS_CLIENT_CAP_SOFTWARE_REVISION:
                if (status == ATT_ERROR_OK) {
                        printf("\tSoftware version: %.*s\r\n", length, value);
                } else {
                        printf("\tFailed to read SW Version information (%#x)\r\n", status);
                }
                clear_pending_action(peer_info, PENDING_ACTION_READ_SW_VERSION, status);
                break;
        default:
                break;
        }
}

static const dis_client_callbacks_t dis_callbacks = {
        .read_completed = dis_read_completed_cb,
};

static void resolve_found_device(found_device_t *dev)
{
        // Check if device address is resolvable
        if (ADDR_RESOLVABLE(dev->addr)) {
                ble_gap_address_resolve(dev->addr);
        }
}

static void handle_evt_gap_adv_report(const ble_evt_gap_adv_report_t *evt)
{
        found_device_t *dev;
        size_t dev_index = 0;
        const uint8_t *p;
        uint8_t ad_len, ad_type;
        bool new_device = false;
        const char *dev_name = NULL;
        size_t dev_name_len = 0;

        dev = get_found_device(&evt->address, &dev_index);
        if (dev && dev->name_found) {
                return;
        }

        /* Add device if 'any' was specified as scan argument */
        if (!dev && scan_state.match_any) {
                new_device = true;
                dev = add_found_device(&evt->address, &dev_index);

                if (dev_index > 0) {
                        resolve_found_device(dev);
                }
        }

        for (p = evt->data; p < evt->data + evt->length; p += ad_len) {
                ad_len = (*p++) - 1; /* ad_len is length of value only, without type */
                ad_type = *p++;

                /* Device not found so we look for UUID */
                if (!dev && (ad_type == GAP_DATA_TYPE_UUID16_LIST ||
                                                        ad_type == GAP_DATA_TYPE_UUID16_LIST_INC)) {
                        size_t idx;

                        for (idx = 0; idx < ad_len; idx += sizeof(uint16_t)) {
                                if (get_u16(p + idx) == UUID_SERVICE_HRS) {
                                        new_device = true;
                                        dev = add_found_device(&evt->address, &dev_index);

                                        if (dev_index > 0) {
                                                resolve_found_device(dev);
                                        }

                                        break;
                                }
                        }

                        continue;
                }

                /* Look for name and store it to use later, if proper UUID is found */
                if (ad_type == GAP_DATA_TYPE_SHORT_LOCAL_NAME ||
                                                        ad_type == GAP_DATA_TYPE_LOCAL_NAME) {
                        dev_name = (const char *) p;
                        dev_name_len = ad_len;

                        if (dev) {
                                /* Already have device, no need to look further */
                                break;
                        }
                }
        }

        /*
         * If we have both device and device name, print as new device found with name.
         * For new device and no name, just print address for now.
         */
        if (dev && dev_name) {
                dev->name_found = true;
                printf("[%02d] Device found: %s %s (%.*s)\r\n", dev_index,
                                evt->address.addr_type == PUBLIC_ADDRESS ? "public " : "private",
                                ble_address_to_string(&evt->address),
                                dev_name_len, dev_name);
        } else if (new_device) {
                printf("[%02d] Device found: %s %s\r\n", dev_index,
                                evt->address.addr_type == PUBLIC_ADDRESS ? "public " : "private",
                                ble_address_to_string(&evt->address));
        }
}

static ble_client_t *get_stored_client(uint16_t conn_idx, ble_storage_key_t key)
{
        ble_error_t err;
        uint16_t len = 0;
        void *buffer;

        err = ble_storage_get_buffer(conn_idx, key, &len, &buffer);
        if (err) {
                return NULL;
        }

        switch (key) {
        case HRS_CLIENT_STORAGE_ID:
                return hrs_client_init_from_data(conn_idx, &hrs_callbacks, buffer, len);
        case DIS_CLIENT_STORAGE_ID:
                return dis_client_init_from_data(conn_idx, &dis_callbacks, buffer, len);
        default:
                return NULL;
        }
}

static void handle_evt_gap_connected(const ble_evt_gap_connected_t *evt)
{
        peer_info_t *peer_info;
        bool bonded;

        printf("Device connected\r\n");
        printf("\tAddress: %s %s\r\n",
                        evt->peer_address.addr_type == PUBLIC_ADDRESS ? "public " : "private",
                        ble_address_to_string(&evt->peer_address));
        printf("\tConnection index: %u\r\n", evt->conn_idx);
        printf("\tConnected devices: %u\r\n", queue_length(&peer_info_queue) + 1);

        app_state = APP_STATE_IDLE;

        peer_info = OS_MALLOC(sizeof(*peer_info));
        memset(peer_info, 0, sizeof(*peer_info));

        peer_info->addr = evt->peer_address;
        peer_info->conn_idx = evt->conn_idx;

        add_peer_info(peer_info);

        ble_gap_is_bonded(peer_info->conn_idx, &bonded);
        if (bonded) {
                peer_info->hrs_client = get_stored_client(peer_info->conn_idx, HRS_CLIENT_STORAGE_ID);
                peer_info->dis_client = get_stored_client(peer_info->conn_idx, DIS_CLIENT_STORAGE_ID);
        }

        /*
         * Try to start encryption first. If cannot be started it means we are not bonded and can
         * got directly to browse. Service data can be already cached then no need to do it again.
         */
        if (start_auth(peer_info, AUTH_TYPE_ENCRYPT, false)) {
                if (!peer_info->hrs_client) {
                        peer_info->pending_browse = true;
                }
                return;
        }

#if CFG_BOND_AFTER_CONNECT
        if (start_auth(peer_info, AUTH_TYPE_BOND, false)) {
                peer_info->pending_browse = true;
                return;
        }
#endif

        start_browse(peer_info);
}

static void handle_evt_gap_connection_completed(const ble_evt_gap_connection_completed_t *evt)
{
        /* Successful connections are handled in separate event */
        if (evt->status == BLE_STATUS_OK) {
                return;
        }

        printf("Connection failed\r\n");
        printf("\tStatus: 0x%02x\r\n", evt->status);

        app_state = APP_STATE_IDLE;
}

static void handle_evt_gap_address_resolved(ble_evt_gap_address_resolved_t *evt)
{
        found_device_t *dev;
        size_t index;

        if (evt->address.addr_type == PUBLIC_ADDRESS) {
                // Address is already known
                return;
        }

        dev = get_found_device(&evt->address, &index);
        if (dev) {
                printf("[%02d] Identity Address: %s %s\r\n", index,
                        evt->resolved_address.addr_type == PUBLIC_ADDRESS ? "public" : "private",
                        ble_address_to_string(&evt->resolved_address));
        } else {
                printf("Private address: %s %s, ",
                                evt->address.addr_type == PUBLIC_ADDRESS ? "public" : "private",
                                ble_address_to_string(&evt->address));
                printf("Identity Address: %s %s\r\n",
                        evt->resolved_address.addr_type == PUBLIC_ADDRESS ? "public" : "private",
                        ble_address_to_string(&evt->resolved_address));
        }
}

static void handle_evt_gap_disconnected(const ble_evt_gap_disconnected_t * evt)
{
        peer_info_t *peer_info;

        printf("Device disconnected\r\n");
        printf("\tConnection index: %u\r\n", evt->conn_idx);
        printf("\tReason: 0x%02x\r\n", evt->reason);

        peer_info = remove_peer_info(evt->conn_idx);

        printf("\tConnected devices: %u\r\n", queue_length(&peer_info_queue));

        if (!peer_info) {
                return;
        }

        if (peer_info->hrs_client) {
                ble_client_cleanup(peer_info->hrs_client);
        }
        if (peer_info->dis_client) {
                ble_client_cleanup(peer_info->dis_client);
        }

        OS_FREE(peer_info);
}

static void handle_evt_gap_scan_completed(const ble_evt_gap_scan_completed_t *evt)
{
        printf("Scan stopped\r\n");

        app_state = APP_STATE_IDLE;
}

static void handle_evt_gap_security_request(const ble_evt_gap_security_request_t *evt)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(evt->conn_idx);
        OS_ASSERT(peer_info);

        printf("Security Request received\r\n");
        printf("\tConnection index: %u\r\n", evt->conn_idx);

        start_auth(peer_info, evt->bond ? AUTH_TYPE_BOND : AUTH_TYPE_PAIR, evt->mitm);
}

static void handle_evt_gap_passkey_notify(const ble_evt_gap_passkey_notify_t *evt)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(evt->conn_idx);
        OS_ASSERT(peer_info);

        printf("Passkey notify\r\n");
        printf("\tPasskey: %06" PRIu32 "\r\n", evt->passkey);
        printf("\tConnection index: %u\r\n", evt->conn_idx);
}

static void handle_evt_gap_pair_completed(const ble_evt_gap_pair_completed_t *evt)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(evt->conn_idx);
        OS_ASSERT(peer_info);

        if (evt->status == BLE_STATUS_OK) {
                OS_ASSERT(evt->conn_idx == peer_info->conn_idx);
                printf("Pairing completed (%s, %s)\r\n", evt->bond ? "bond" : "no bond",
                                                                evt->mitm ? "MITM" : "no MITM");
                if (evt->bond) {
                        store_client(evt->conn_idx, peer_info->hrs_client, HRS_CLIENT_STORAGE_ID);
                        store_client(evt->conn_idx, peer_info->dis_client, DIS_CLIENT_STORAGE_ID);
                }
        } else {
                printf("Pairing failed (0x%02x)\r\n", evt->status);
        }
        printf("\tConnection index: %u\r\n", evt->conn_idx);

        finish_auth(peer_info, evt->status);
}

static void handle_evt_gap_sec_level_changed(const ble_evt_gap_sec_level_changed_t *evt)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(evt->conn_idx);
        OS_ASSERT(peer_info);

        printf("Security Level changed to %d\r\n", evt->level + 1);
        printf("\tConnection index: %u\r\n", evt->conn_idx);

        finish_auth(peer_info, BLE_STATUS_OK);
}

static void handle_evt_gap_set_sec_level_failed(const ble_evt_gap_set_sec_level_failed_t *evt)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(evt->conn_idx);
        OS_ASSERT(peer_info);

        printf("Failed to set security level (0x%02x)\r\n", evt->status);

        finish_auth(peer_info, evt->status);
}

static void handle_evt_gattc_browse_svc(const ble_evt_gattc_browse_svc_t *evt)
{
        peer_info_t *peer_info;
        bool bonded;

        peer_info = find_peer_info(evt->conn_idx);
        OS_ASSERT(peer_info);

        /* We are not interested in any service with 128-bit UUID */
        if (evt->uuid.type != ATT_UUID_16) {
                return;
        }

        ble_gap_is_bonded(evt->conn_idx, &bonded);

        switch (evt->uuid.uuid16) {
        case UUID_SERVICE_HRS:
                if (peer_info->hrs_client) {
                        return;
                }

                peer_info->hrs_client = hrs_client_init(&hrs_callbacks, evt);
                if (!peer_info->hrs_client) {
                        return;
                }

                ble_client_add(peer_info->hrs_client);

                if (bonded) {
                        store_client(evt->conn_idx, peer_info->hrs_client, HRS_CLIENT_STORAGE_ID);
                }

                break;
        case UUID_SERVICE_DIS:
                if (peer_info->dis_client) {
                        return;
                }

                peer_info->dis_client = dis_client_init(&dis_callbacks, evt);
                if (!peer_info->dis_client) {
                        return;
                }

                ble_client_add(peer_info->dis_client);

                if (bonded) {
                        store_client(evt->conn_idx, peer_info->dis_client, DIS_CLIENT_STORAGE_ID);
                }

                break;
        }
}

static void handle_evt_gattc_browse_completed(const ble_evt_gattc_browse_completed_t *evt)
{
        peer_info_t *peer_info;

        peer_info = find_peer_info(evt->conn_idx);
        OS_ASSERT(peer_info);

        if (evt->status == BLE_STATUS_OK) {
                OS_ASSERT(evt->conn_idx == peer_info->conn_idx);
        }

        printf("Browse completed\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);

        if (!peer_info->hrs_client) {
                printf("\tHeart Rate service not found\r\n");

                printf("Disconnecting...\r\n");
                ble_gap_disconnect(evt->conn_idx, BLE_HCI_ERROR_REMOTE_USER_TERM_CON);
                return;
        }

        printf("\tHeart Rate service found\r\n");

        if (peer_info->dis_client) {
                dis_client_cap_t cap = dis_client_get_capabilities(peer_info->dis_client);

                printf("\tDevice Information Service found\r\n");

                /* Read manufacturer name (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_MANUFACTURER_NAME) {
                        add_pending_action(peer_info, PENDING_ACTION_READ_MANUFACTURER);
                }

                /* Read model number (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_MODEL_NUMBER) {
                        add_pending_action(peer_info, PENDING_ACTION_READ_MODEL);
                }

                /* Read firmware version (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_FIRMWARE_REVISION) {
                        add_pending_action(peer_info, PENDING_ACTION_READ_FW_VERSION);
                }

                /* Read software version (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_SOFTWARE_REVISION) {
                        add_pending_action(peer_info, PENDING_ACTION_READ_SW_VERSION);
                }
        } else {
                printf("\tDevice Information Service not found\r\n");
        }

        /* Read body sensor location */
        if (hrs_client_get_capabilities(peer_info->hrs_client)
                                                        & HRS_CLIENT_CAP_BODY_SENSOR_LOCATION) {
                add_pending_action(peer_info, PENDING_ACTION_READ_SENSOR_LOCATION);
        }

        /* Enable measurement notifications */
        add_pending_action(peer_info, PENDING_ACTION_ENABLE_NOTIF);

        printf("Querying device...\r\n");
}

void hrp_collector_task(void *params)
{
        cli_t cli;

        /* Register CLI */
        cli = cli_register(CLI_NOTIF, clicmd, clicmd_default_handler);

        /* Set device name */
        ble_gap_device_name_set("Black Orca HR Collector", ATT_PERM_READ);

        /* Setup application in BLE Manager */
        ble_register_app();
        ble_central_start();

        /* Initial peer information queue*/
        queue_init(&peer_info_queue);

        /*
         * We have keyboard support (for CLI) but since support for passkey entry is missing, let's
         * just declare "Display Only" capabilities for now.
         */
        ble_gap_set_io_cap(GAP_IO_CAP_DISP_ONLY);

        printf("HRP Collector ready.\r\n");

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                OS_ASSERT(ret == OS_OK);

                /* Notified from BLE manager, can get event */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        /*
                         * No need to wait for event, should be already there since we were notified
                         * from manager
                         */
                        hdr = ble_get_event(false);

                        if (!hdr) {
                                goto no_event;
                        }

                        ble_client_handle_event(hdr);

                        switch (hdr->evt_code) {
                        case BLE_EVT_GAP_ADV_REPORT:
                                handle_evt_gap_adv_report((ble_evt_gap_adv_report_t *) hdr);
                                break;
                        case BLE_EVT_GAP_CONNECTED:
                                handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_CONNECTION_COMPLETED:
                                handle_evt_gap_connection_completed((ble_evt_gap_connection_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_DISCONNECTED:
                                handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_SCAN_COMPLETED:
                                handle_evt_gap_scan_completed((ble_evt_gap_scan_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_SECURITY_REQUEST:
                                handle_evt_gap_security_request((ble_evt_gap_security_request_t *) hdr);
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
                        case BLE_EVT_GAP_SET_SEC_LEVEL_FAILED:
                                handle_evt_gap_set_sec_level_failed((ble_evt_gap_set_sec_level_failed_t *) hdr);
                                break;
                        case BLE_EVT_GATTC_BROWSE_SVC:
                                handle_evt_gattc_browse_svc((ble_evt_gattc_browse_svc_t *) hdr);
                                break;
                        case BLE_EVT_GATTC_BROWSE_COMPLETED:
                                handle_evt_gattc_browse_completed((ble_evt_gattc_browse_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_ADDRESS_RESOLVED:
                                handle_evt_gap_address_resolved((ble_evt_gap_address_resolved_t *) hdr);
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

                if (notif & CLI_NOTIF) {
                        cli_handle_notified(cli);
                }
        }
}
