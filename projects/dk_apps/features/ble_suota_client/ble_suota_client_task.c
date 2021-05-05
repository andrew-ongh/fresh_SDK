/**
 ****************************************************************************************
 *
 * @file ble_suota_client_task.c
 *
 * @brief SUOTA 1.2 client application task
 *
 * Copyright (C) 2016-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "osal.h"
#include "timers.h"
#include "sys_watchdog.h"
#include "ble_bufops.h"
#include "ble_common.h"
#include "ble_gattc.h"
#include "ble_gap.h"
#include "ble_l2cap.h"
#include "ble_uuid.h"
#include "cli.h"
#include "suota.h"
#include "ble_client.h"
#include "suota_client.h"
#include "dis_client.h"
#include "ad_nvms.h"

/**
 * SUOTA status
 *
 * As defined by Dialog SUOTA specification.
 *
 */
typedef enum {
        SUOTA_SRV_STARTED      = 0x01,     // Valid memory device has been configured by initiator. No sleep state while in this mode
        SUOTA_CMP_OK           = 0x02,     // SUOTA process completed successfully.
        SUOTA_SRV_EXIT         = 0x03,     // Forced exit of SUOTA service.
        SUOTA_CRC_ERR          = 0x04,     // Overall Patch Data CRC failed
        SUOTA_PATCH_LEN_ERR    = 0x05,     // Received patch Length not equal to PATCH_LEN characteristic value
        SUOTA_EXT_MEM_WRITE_ERR= 0x06,     // External Mem Error (Writing to external device failed)
        SUOTA_INT_MEM_ERR      = 0x07,     // Internal Mem Error (not enough space for Patch)
        SUOTA_INVAL_MEM_TYPE   = 0x08,     // Invalid memory device
        SUOTA_APP_ERROR        = 0x09,     // Application error

        /* SUOTA application specific error codes */
        SUOTA_IMG_STARTED      = 0x10,     // SUOTA started for downloading image (SUOTA application)
        SUOTA_INVAL_IMG_BANK   = 0x11,     // Invalid image bank
        SUOTA_INVAL_IMG_HDR    = 0x12,     // Invalid image header
        SUOTA_INVAL_IMG_SIZE   = 0x13,     // Invalid image size
        SUOTA_INVAL_PRODUCT_HDR= 0x14,     // Invalid product header
        SUOTA_SAME_IMG_ERR     = 0x15,     // Same Image Error
        SUOTA_EXT_MEM_READ_ERR = 0x16,     // Failed to read from external memory device

        /* SUOTA extended status for Apple HomeKit */
        SUOTA_LEGACY_MODE      = 0x18,
        SUOTA_HAP_MODE         = 0x19,
        SUOTA_SIGNED_MODE      = 0x1A,
        SUOTA_ENC_SIGNED_MODE  = 0x1B,
} suota_status_t;

#define CLI_NOTIF               (1 << 15)

#define UUID_SUOTA              0xFEF5

#define L2CAP_CREDITS           5

/*
 * This is the maximum number of bytes the client is allowed to send before receiving a notification from
 * the peer to continue. Setting it too high will increase transfer speed but will also require more heap
 * on peer's side while SUOTA is ongoing.
 */
#define DEFAULT_PATCH_LEN       (1536)

#define MAX_FOUND_DEVICES       25

typedef enum {
        APP_STATE_IDLE,
        APP_STATE_SCANNING,
        APP_STATE_CONNECTING,
        APP_STATE_CONNECTED,
        APP_STATE_UPDATING,
} app_state_t;

typedef enum {
        PENDING_ACTION_ENABLE_NOTIF = (1 << 0),
        PENDING_ACTION_READ_L2CAP_PSM = (1 << 1),
        PENDING_ACTION_READ_MANUFACTURER = (1 << 2),
        PENDING_ACTION_READ_MODEL = (1 << 3),
        PENDING_ACTION_READ_FW_VERSION = (1 << 4),
        PENDING_ACTION_READ_SW_VERSION = (1 << 5),
        PENDING_ACTION_READ_SUOTA_VERSION = (1 << 6),
        PENDING_ACTION_READ_PATCH_DATA_CHAR_SIZE = (1 << 7),
} pending_action_t;

typedef struct {
        bd_address_t addr;
        bool name_found;
} found_device_t;

typedef struct {
        bd_address_t addr;
        uint16_t conn_idx;
        uint16_t mtu;
        uint16_t l2cap_mtu;
        uint16_t patch_data_char_size;

        ble_client_t *dis_client;
        ble_client_t *suota_client;

        pending_action_t pending_init;

        uint16_t psm;
        uint16_t scid;
        size_t pending_bytes;
        bool flow_stop;
} peer_info_t;

typedef struct {
        OS_TICK_TIME start_time;

        bool use_l2cap;

        size_t offset;

        size_t patch_len;
        size_t block_len;

        bool w4_write_completed : 1;    // waiting for completed event for GATT write
        bool w4_write_ack : 1;          // waiting for ACK after writing patch_len
        bool w4_transfer_status : 1;
} update_info_t;

PRIVILEGED_DATA static OS_TASK app_task;

PRIVILEGED_DATA static nvms_t nvms;

PRIVILEGED_DATA static size_t image_size;

PRIVILEGED_DATA static app_state_t app_state;

PRIVILEGED_DATA static bool scan_any;

PRIVILEGED_DATA static peer_info_t peer_info;

PRIVILEGED_DATA static update_info_t update_info;

PRIVILEGED_DATA static struct {
        found_device_t devices[MAX_FOUND_DEVICES];
        size_t num_devices;
} found_devices;

static found_device_t *get_found_device(const bd_address_t *addr, size_t *index)
{
        size_t i;

        for (i = 0; i < found_devices.num_devices; i++) {
                found_device_t *dev = &found_devices.devices[i];

                if (ble_address_cmp(&dev->addr, addr)) {
                        *index = i + 1;
                        return dev;
                }
        }

        return NULL;
}

static inline found_device_t *add_found_device(const bd_address_t *addr, size_t *index)
{
        found_device_t *dev;

        if (found_devices.num_devices >= MAX_FOUND_DEVICES) {
                return NULL;
        }

        dev = &found_devices.devices[found_devices.num_devices++];
        dev->addr = *addr;
        dev->name_found = false;

        *index = found_devices.num_devices;

        return dev;
}

static void initialize_nvms(void)
{
        suota_image_header_t header;
        time_t rawtime;
        struct tm *timeinfo;

        /* Open NVMS partition with firmware */
        nvms = ad_nvms_open(NVMS_BIN_PART);
        if (!nvms) {
                printf("FATAL: NVMS_BIN_PART partition not found\r\n");
                OS_ASSERT(0);
        }

        /* Read header and check if update image is valid */
        ad_nvms_read(nvms, 0, (void *) &header, sizeof(header));

        if (header.signature[0] != SUOTA_1_1_IMAGE_HEADER_SIGNATURE_B1 ||
                        header.signature[1] != SUOTA_1_1_IMAGE_HEADER_SIGNATURE_B2) {
                printf("FATAL: image signature invalid\r\n");
                OS_ASSERT(0);
        }


        /* Store total image size to be transferred */
        image_size = sizeof(header) + header.code_size;

        printf("FW Image size: %u bytes\r\n", image_size);

        /* Dump image info */
        rawtime = header.timestamp;
        timeinfo = gmtime(&rawtime);

        printf("FW Image information:\r\n");
        printf("\tCode Size: %" PRIu32 " bytes\r\n", header.code_size);
        printf("\tVersion: %.*s\r\n", sizeof(header.version), header.version);
        printf("\tTimestamp: %04d-%02d-%02d %02d:%02d:%02d UTC\r\n",
                                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                                timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        printf("\tCRC: 0x%08" PRIx32 "\r\n", header.crc);
        printf("\tFlags: 0x%04" PRIx16 "\r\n", header.flags);
}

static void send_patch_data(void)
{
        const void *data_ptr;
        size_t len = update_info.use_l2cap ? peer_info.l2cap_mtu - 2 : peer_info.mtu - 3;

        /* Check if transfer is completed */
        if (update_info.offset >= image_size) {
                uint32_t time = OS_TICKS_2_MS(OS_GET_TICK_COUNT() - update_info.start_time);
                uint32_t speed = image_size * 1000 / time;

                /* We should have *exactly* the same offset as total_size */
                OS_ASSERT(update_info.offset == image_size);

                printf("Transfer completed\r\n");
                printf("Transferred %u bytes in %lu.%03lu seconds (%lu bytes/sec)\r\n", image_size, time / 1000, time % 1000, speed);

                suota_client_send_end_cmd(peer_info.suota_client);

                /* Now we are waiting for image transfer status */
                update_info.w4_transfer_status = true;
                return;
        }

        if (update_info.offset + len > image_size) {
                len = image_size - update_info.offset;
        }

        ad_nvms_get_pointer(nvms, update_info.offset, len, &data_ptr);

        if (update_info.use_l2cap) {
                ble_l2cap_send(peer_info.conn_idx, peer_info.scid, len, data_ptr);
                peer_info.pending_bytes = len;
                /* Offset will be advanced once data are sent over the air */
        } else {
                /* For GATT we cannot send more than patch_data_char_size bytes */
                if (len > peer_info.patch_data_char_size) {
                        len = peer_info.patch_data_char_size;
                }

                /* For GATT transfer we also need to check if data fits in patch_len */
                if (update_info.block_len + len > update_info.patch_len) {
                        len = update_info.patch_len - update_info.block_len;
                }

                suota_client_send_patch_data(peer_info.suota_client, len, data_ptr);

                update_info.offset += len;
                update_info.block_len += len;

                /* Wait for write completed event */
                update_info.w4_write_completed = true;

                /* After writing full patch_len, wait also for remote to ACK */
                if (update_info.block_len == update_info.patch_len) {
                        update_info.w4_write_ack = true;
                }
        }

        printf("Sent %d bytes\r\n", update_info.offset);
}

static void send_patch_data_gatt(void)
{
        /*
         * Simply return if waiting for either write completed or ACK - we will be back here when
         * flag is cleared.
         *
         * Note that this is a bit redundant as long as SUOTA uses Write Command to send image
         * blocks, but having this will also allow to work properly in case Write Request is used.
         */
        if (update_info.w4_write_completed || update_info.w4_write_ack) {
                return;
        }

        /* If full image block was written, we need to check if patch_len should be updated */
        if (update_info.block_len == update_info.patch_len) {
                update_info.block_len = 0;

                if (image_size - update_info.offset < update_info.patch_len) {
                        update_info.patch_len = image_size - update_info.offset;
                        suota_client_set_patch_len(peer_info.suota_client, update_info.patch_len);

                        printf("patch_len changed to %u\r\n", update_info.patch_len);

                        /* New block write will be triggered when patch_len is updated */
                        return;
                }
        }

        send_patch_data();
}

static void pending_clear_and_check(pending_action_t action)
{
        peer_info.pending_init &= ~action;

        /* If all flags are cleared, we're ready to start SUOTA */
        if (!peer_info.pending_init) {
                printf("Ready.\r\n");
        }
}

static void clicmd_scan_usage(void)
{
        printf("usage: scan <start|stop> [any]\r\n");
        printf("\t\"any\" will disable filtering devices by SUOTA UUID, only valid for \"scan start\"\r\n");
}

static void clicmd_scan_handler(int argc, const char *argv[], void *user_data)
{
        if (argc < 2) {
                clicmd_scan_usage();
                return;
        }

        if (!strcasecmp("start", argv[1])) {
                if (app_state == APP_STATE_IDLE) {
                        scan_any = (argc > 2) && !strcmp(argv[2], "any");

                        found_devices.num_devices = 0;
                        ble_gap_scan_start(GAP_SCAN_ACTIVE, GAP_SCAN_OBSERVER_MODE,
                                                                BLE_SCAN_INTERVAL_FROM_MS(30),
                                                                BLE_SCAN_WINDOW_FROM_MS(30),
                                                                false, false);

                        printf("Scanning...\r\n");

                        app_state = APP_STATE_SCANNING;
                } else {
                        printf("ERROR: application has to be in idle state to start scanning\r\n");
                }
        } else if (!strcasecmp("stop", argv[1])) {
                if (app_state == APP_STATE_SCANNING) {
                        ble_gap_scan_stop();

                        printf("Scan stopped\r\n");

                        app_state = APP_STATE_IDLE;
                } else {
                        printf("ERROR: no scan session in progress\r\n");
                }
        } else {
                clicmd_scan_usage();
        }
}

static void clicmd_connect_usage(void)
{
        printf("usage: connect <address|index> [public|random]\r\n");
        printf("\tinstead of address, index of found device can be passed\r\n");
        printf("\tif not specified, public address is assumed\r\n");
}

static void clicmd_connect_handler(int argc, const char *argv[], void *user_data)
{
        static const gap_conn_params_t cp = {
                .interval_min = BLE_CONN_INTERVAL_FROM_MS(7.5),
                .interval_max = BLE_CONN_INTERVAL_FROM_MS(7.5),
                .slave_latency = 0,
                .sup_timeout = BLE_SUPERVISION_TMO_FROM_MS(1000),
        };

        bd_address_t addr;
        size_t dev_index;

        if (argc < 2) {
                clicmd_connect_usage();
                return;
        }

        if (app_state != APP_STATE_IDLE) {
                printf("ERROR: application has to be in idle state to connect\r\n");
                return;
        }

        /*
         * If argument cannot be parsed to valid address, check if it can be used as index in
         * found devices cache.
         */
        if (!ble_address_from_string(argv[1], PUBLIC_ADDRESS, &addr)) {
                dev_index = atoi(argv[1]);
                if (dev_index < 1 || dev_index > found_devices.num_devices) {
                        clicmd_connect_usage();
                        return;
                }

                addr = found_devices.devices[dev_index - 1].addr;
        } else {
                if (argc > 2) {
                        /*
                         * If address type argument is present, check for "random" or leave "public"
                         * as set by default.
                         */

                        if (!strcasecmp("random", argv[2])) {
                                addr.addr_type = PRIVATE_ADDRESS;
                        }
                } else {
                        size_t i;

                        /*
                         * If address type is not present try to check for address in found devices
                         * cache, otherwise leave "public".
                         */

                        for (i = 0; i < found_devices.num_devices; i++) {
                                found_device_t *dev = &found_devices.devices[i];

                                if (!memcmp(&dev->addr.addr, &addr.addr, sizeof(addr.addr))) {
                                        addr.addr_type = dev->addr.addr_type;
                                        break;
                                }
                        }
                }
        }

        ble_gap_connect(&addr, &cp);

        printf("Connecting to %s...\r\n", ble_address_to_string(&addr));

        app_state = APP_STATE_CONNECTING;
}

static void clicmd_update_handler(int argc, const char *argv[], void *user_data)
{
        bool force_gatt;

        if (app_state < APP_STATE_CONNECTED) {
                printf("ERROR: not connected\r\n");
                return;
        }

        force_gatt = (argc > 1 && !strcasecmp("gatt", argv[1]));

        printf("Updating...\r\n");

        memset(&update_info, 0, sizeof(update_info));
        update_info.use_l2cap = peer_info.psm && !force_gatt;

        suota_client_set_mem_dev(peer_info.suota_client, SUOTA_CLIENT_MEM_DEV_SPI_FLASH, 0);

        app_state = APP_STATE_UPDATING;
}

static void clicmd_default_handler(int argc, const char *argv[], void *user_data)
{
        printf("Valid commands:\r\n");
        printf("\tscan <start|stop> [any]\r\n");
        printf("\tconnect <address> [public|random]\r\n");
        printf("\tupdate [gatt]\r\n");
}

static const cli_command_t clicmd[] = {
        { .name = "scan",       .handler = clicmd_scan_handler, },
        { .name = "connect",    .handler = clicmd_connect_handler, },
        { .name = "update",     .handler = clicmd_update_handler, },
        {},
};

void suota_set_event_state_completed_cb(ble_client_t *client, suota_client_event_t event,
                                                                                att_error_t status)
{
        pending_clear_and_check(PENDING_ACTION_ENABLE_NOTIF);
}

static void suota_status_notif_cb(ble_client_t *client, uint8_t status)
{
        printf("SUOTA status: 0x%02x\r\n", status);

        if (update_info.w4_write_ack) {
                update_info.w4_write_ack = false;

                if (status == SUOTA_CMP_OK) {
                        send_patch_data_gatt();
                } else {
                        printf("ERROR: remote did not ACK data block\r\n");
                }
        }

        if (update_info.w4_transfer_status) {
                update_info.w4_transfer_status = false;

                if (status == SUOTA_CMP_OK) {
                        printf("Rebooting remote...\r\n");
                        suota_client_send_reboot_cmd(peer_info.suota_client);
                }
        }
}

static void suota_read_l2cap_psm_completed_cb(ble_client_t *client, att_error_t status, uint16_t psm)
{
        peer_info.psm = psm;

        if (status != ATT_ERROR_OK) {
                printf("\tL2CAP PSM: failed to read (0x%02x)\r\n", status);
        } else {
                printf("\tL2CAP PSM: 0x%04x\r\n", psm);
        }

        pending_clear_and_check(PENDING_ACTION_READ_L2CAP_PSM);
}


static void suota_get_suota_version_completed_cb(ble_client_t *client, att_error_t status, uint8_t suota_version)
{
        if (status != ATT_ERROR_OK) {
                printf("ERROR: failed to get SUOTA version (code=0x%02x)\r\n", status);
                return;
        }

        printf("\tSUOTA version: v%u\r\n", suota_version);

        /*
         * Query version specific characteristics
         */
        if (suota_version >= SUOTA_VERSION_1_3) {
                peer_info.pending_init |= PENDING_ACTION_READ_PATCH_DATA_CHAR_SIZE;
                suota_client_get_patch_data_char_size(client);
        }

        pending_clear_and_check(PENDING_ACTION_READ_SUOTA_VERSION);
}

static void suota_get_patch_data_char_size_completed_cb(ble_client_t *client, att_error_t status, uint16_t patch_data_char_size)
{
        if (status != ATT_ERROR_OK) {
                printf("ERROR: failed to get patch data characteristic size (code=0x%02x)\r\n", status);
                return;
        }

        printf("\tPatch data characteristic size: %u bytes\r\n", patch_data_char_size);

        peer_info.patch_data_char_size = patch_data_char_size;

        pending_clear_and_check(PENDING_ACTION_READ_PATCH_DATA_CHAR_SIZE);
}

static void suota_set_mem_dev_completed_cb(ble_client_t *client, att_error_t status)
{
        if (status != ATT_ERROR_OK) {
                printf("ERROR: failed to configure device (code=0x%02x)\r\n", status);
                return;
        }

        if (update_info.use_l2cap) {
                printf("Starting update via L2CAP CoC...\r\n");

                /* Connect L2CAP channel, this will trigger image transfer */
                ble_l2cap_connect(peer_info.conn_idx, peer_info.psm, L2CAP_CREDITS,
                                                                                &peer_info.scid);
        } else {
                printf("Starting update via GATT...\r\n");
                update_info.start_time = OS_GET_TICK_COUNT();

                /* Write patch len, this will trigger image transfer */
                update_info.patch_len = DEFAULT_PATCH_LEN;

                printf("Setting patch len to %u bytes...\r\n", update_info.patch_len);

                suota_client_set_patch_len(peer_info.suota_client, update_info.patch_len);
        }
}

static void suota_send_end_cmd_completed_cb(ble_client_t *client, att_error_t status)
{
        if (status != ATT_ERROR_OK) {
                printf("ERROR: SPOTAR_IMAGE_END failed on remote\r\n");
        }
}

static void suota_set_patch_len_completed_cb(ble_client_t *client, att_error_t status)
{
        if (status != ATT_ERROR_OK) {
                printf("ERROR: Failed to set new patch block length (code=0x%02x)\r\n", status);
                return;
        }

        send_patch_data_gatt();
}

static void suota_send_patch_data_completed_cb(ble_client_t *client, att_error_t status)
{
        if (status != ATT_ERROR_OK) {
                printf("ERROR: Failed to send patch data\r\n");
                return;
        }

        if (update_info.w4_write_completed) {
                update_info.w4_write_completed = false;
                send_patch_data_gatt();
        }
}

static const suota_client_callbacks_t suota_callbacks = {
                .set_event_state_completed = suota_set_event_state_completed_cb,
                .status_notif = suota_status_notif_cb,
                .read_l2cap_psm_completed = suota_read_l2cap_psm_completed_cb,
                .get_suota_version_completed = suota_get_suota_version_completed_cb,
                .get_patch_data_char_size_completed = suota_get_patch_data_char_size_completed_cb,
                .set_mem_dev_completed = suota_set_mem_dev_completed_cb,
                .send_end_cmd_completed = suota_send_end_cmd_completed_cb,
                .set_patch_len_completed = suota_set_patch_len_completed_cb,
                .send_patch_data_completed = suota_send_patch_data_completed_cb,
};

static void dis_read_completed_cb(ble_client_t *dis_client, att_error_t status,
                                                        dis_client_cap_t capability,
                                                        uint16_t length, const uint8_t *value)
{
        switch (capability) {
        case DIS_CLIENT_CAP_MANUFACTURER_NAME:
                printf("\tManufacturer: %.*s\r\n", length, value);
                pending_clear_and_check(PENDING_ACTION_READ_MANUFACTURER);
                break;
        case DIS_CLIENT_CAP_MODEL_NUMBER:
                printf("\tModel: %.*s\r\n", length, value);
                pending_clear_and_check(PENDING_ACTION_READ_MODEL);
                break;
        case DIS_CLIENT_CAP_FIRMWARE_REVISION:
                printf("\tFirmware version: %.*s\r\n", length, value);
                pending_clear_and_check(PENDING_ACTION_READ_FW_VERSION);
                break;
        case DIS_CLIENT_CAP_SOFTWARE_REVISION:
                printf("\tSoftware version: %.*s\r\n", length, value);
                pending_clear_and_check(PENDING_ACTION_READ_SW_VERSION);
                break;
        default:
                break;
        }
}

static const dis_client_callbacks_t dis_callbacks = {
        .read_completed = dis_read_completed_cb,
};

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
        if (!dev && scan_any) {
                new_device = true;
                dev = add_found_device(&evt->address, &dev_index);
        }

        for (p = evt->data; p < evt->data + evt->length; p += (ad_len - 1)) {
                ad_len = *p++;
                ad_type = *p++;

                /* Device not found so we look for UUID */
                if (!dev && (ad_type == GAP_DATA_TYPE_UUID16_LIST ||
                                                        ad_type == GAP_DATA_TYPE_UUID16_LIST_INC)) {
                        size_t idx;

                        for (idx = 0; idx < ad_len; idx += sizeof(uint16_t)) {
                                if (get_u16(p + idx) == UUID_SUOTA) {
                                        new_device = true;
                                        dev = add_found_device(&evt->address, &dev_index);
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
                printf("[%02d] Device found: %s (%.*s)\r\n", dev_index,
                                                                ble_address_to_string(&evt->address),
                                                                dev_name_len, dev_name);
        } else if (new_device) {
                printf("[%02d] Device found: %s\r\n", dev_index, ble_address_to_string(&evt->address));
        }
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        printf("Device connected\r\n");
        printf("\tAddress: %s\r\n", ble_address_to_string(&evt->peer_address));
        printf("\tConnection index: %d\r\n", evt->conn_idx);

        peer_info.addr = evt->peer_address;
        peer_info.conn_idx = evt->conn_idx;
        peer_info.patch_data_char_size = 120;
        peer_info.pending_init = 0;

        app_state = APP_STATE_CONNECTED;

        ble_gattc_exchange_mtu(evt->conn_idx);

        printf("Exchanging MTU...\r\n");
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        printf("Device disconnected\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tReason: 0x%02x\r\n", evt->reason);

        if (peer_info.suota_client) {
                ble_client_cleanup(peer_info.suota_client);
        }

        if (peer_info.dis_client) {
                ble_client_cleanup(peer_info.dis_client);
        }

        memset(&peer_info, 0, sizeof(peer_info));
        peer_info.conn_idx = BLE_CONN_IDX_INVALID;

        app_state = APP_STATE_IDLE;
}

static void handle_evt_gattc_mtu_changed(ble_evt_gattc_mtu_changed_t *evt)
{
        printf("MTU set to %u bytes\r\n", evt->mtu);

        peer_info.mtu = evt->mtu;

        printf("Browsing...\r\n");

        ble_gattc_browse(evt->conn_idx, NULL);
}

static void handle_evt_gattc_browse_svc(ble_evt_gattc_browse_svc_t *evt)
{
        /* Both SUOTA and DIS have 16-bit UUIDs, don't care about 128-bit UUIDs */
        if (evt->uuid.type != ATT_UUID_16) {
                return;
        }

        switch (evt->uuid.uuid16) {
        case UUID_SUOTA:
                if (peer_info.suota_client) {
                        return;
                }

                peer_info.suota_client = suota_client_init(&suota_callbacks, evt);
                if (!peer_info.suota_client) {
                        return;
                }

                ble_client_add(peer_info.suota_client);
                break;
        case UUID_SERVICE_DIS:
                if (peer_info.dis_client) {
                        return;
                }

                peer_info.dis_client = dis_client_init(&dis_callbacks, evt);
                if (!peer_info.dis_client) {
                        return;
                }

                ble_client_add(peer_info.dis_client);
                break;
        }
}

static void handle_evt_gattc_browse_completed(ble_evt_gattc_browse_completed_t *evt)
{
        printf("Browse completed\r\n");

        if (!peer_info.suota_client) {
                printf("\tSUOTA service not found\r\n");

                printf("Disconnecting...\r\n");
                ble_gap_disconnect(evt->conn_idx, BLE_HCI_ERROR_REMOTE_USER_TERM_CON);
                return;
        }

        printf("\tSUOTA service found\r\n");

        if (peer_info.dis_client) {
                dis_client_cap_t cap = dis_client_get_capabilities(peer_info.dis_client);

                printf("\tDevice Information Service found\r\n");

                /* Read manufacturer name (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_MANUFACTURER_NAME) {
                        peer_info.pending_init |= PENDING_ACTION_READ_MANUFACTURER;
                        dis_client_read(peer_info.dis_client, DIS_CLIENT_CAP_MANUFACTURER_NAME);
                }

                /* Read model number (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_MODEL_NUMBER) {
                        peer_info.pending_init |= PENDING_ACTION_READ_MODEL;
                        dis_client_read(peer_info.dis_client, DIS_CLIENT_CAP_MODEL_NUMBER);
                }

                /* Read firmware version (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_FIRMWARE_REVISION) {
                        peer_info.pending_init |= PENDING_ACTION_READ_FW_VERSION;
                        dis_client_read(peer_info.dis_client, DIS_CLIENT_CAP_FIRMWARE_REVISION);
                }

                /* Read software version (if supported by DIS server) */
                if (cap & DIS_CLIENT_CAP_SOFTWARE_REVISION) {
                        peer_info.pending_init |= PENDING_ACTION_READ_SW_VERSION;
                        dis_client_read(peer_info.dis_client, DIS_CLIENT_CAP_SOFTWARE_REVISION);
                }
        }

        /* Enable state notifications (write CCC) */
        peer_info.pending_init |= PENDING_ACTION_ENABLE_NOTIF;
        suota_client_set_event_state(peer_info.suota_client, SUOTA_CLIENT_EVENT_STATUS_NOTIF, true);

        /* Read L2CAP PSM (if supported by SUOTA server) */
        if (suota_client_get_capabilities(peer_info.suota_client) & SUOTA_CLIENT_CAP_L2CAP_PSM) {
                peer_info.pending_init |= PENDING_ACTION_READ_L2CAP_PSM;
                suota_client_read_l2cap_psm(peer_info.suota_client);
        }

        if (suota_client_get_capabilities(peer_info.suota_client) & SUOTA_CLIENT_CAP_SUOTA_VERSION) {
                peer_info.pending_init |= PENDING_ACTION_READ_SUOTA_VERSION;
                suota_client_get_suota_version(peer_info.suota_client);
        }

        printf("Querying device...\r\n");
}

static void handle_evt_l2cap_connected(ble_evt_l2cap_connected_t *evt)
{
        printf("Data channel connected\r\n");
        update_info.start_time = OS_GET_TICK_COUNT();

        peer_info.l2cap_mtu = evt->mtu;

        send_patch_data();
}

static void handle_evt_l2cap_connection_failed(ble_evt_l2cap_connection_failed_t *evt)
{
        printf("Data channel connection failed\r\n");
        printf("\tStatus: %d\r\n", evt->status);
}

static void handle_evt_l2cap_disconnected(ble_evt_l2cap_disconnected_t *evt)
{
        printf("Data channel disconnected (reason=0x%04x)\r\n", evt->reason);

        if (app_state == APP_STATE_UPDATING) {
                app_state = APP_STATE_CONNECTED;
                return;
        }
}

static void handle_evt_l2cap_sent(ble_evt_l2cap_sent_t *evt)
{
        switch (evt->status) {
        case BLE_STATUS_OK:
                /* Mark data as sent */
                update_info.offset += peer_info.pending_bytes;
                peer_info.pending_bytes = 0;
                break;
        case BLE_ERROR_L2CAP_MTU_EXCEEDED:
                /*
                 * This should not happen because we obey MTU set on channel. To recover we can set
                 * "usable" MTU to minimul value and retry.
                 */
                peer_info.l2cap_mtu = 23;

                OS_ASSERT(0);
                break;
        case BLE_ERROR_L2CAP_NO_CREDITS:
                /* Do nothing, this will just stop flow and wait for credits from remote */
                break;
        default:
                /* This means wither channel or connection no longer exist, nothing more to do.  */
                return;
        }

        if (evt->remote_credits > 0) {
                send_patch_data();
                return;
        }

        peer_info.flow_stop = true;
}

static void handle_evt_l2cap_remote_credits_changed(ble_evt_l2cap_credit_changed_t *evt)
{
        if (!peer_info.flow_stop || evt->remote_credits == 0) {
                return;
        }

        send_patch_data();

        peer_info.flow_stop = false;
}

static void handle_evt_l2cap_data_ind(ble_evt_l2cap_data_ind_t *evt)
{
        ble_l2cap_add_credits(evt->conn_idx, evt->scid, evt->local_credits_consumed);
}

void ble_suota_client_task(void *params)
{
        int8_t wdog_id;
        cli_t *cli;

        app_task = OS_GET_CURRENT_TASK();

        /* Register task in watchdog subsystem */
        wdog_id = sys_watchdog_register(false);

        /* Register CLI handlers */
        cli = cli_register(CLI_NOTIF, clicmd, clicmd_default_handler);

        /* Initialize NVMS data */
        initialize_nvms();

        /* Setup application in BLE Manager */
        ble_register_app();
        ble_central_start();

        ble_gap_mtu_size_set(512);

        ble_gap_device_name_set("Black Orca SUOTA Client", ATT_PERM_READ);

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                /* Notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);

                /* Suspend watchdog while blocking on OS_TASK_NOTIFY_WAIT() */
                sys_watchdog_suspend(wdog_id);

                /*
                 * Wait on any of the notification bits, then clear them all.
                 * Blocks forever waiting for task notification. The return value must be OS_OK.
                 */
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                OS_ASSERT(ret == OS_OK);

                /* Resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

                /* Notified from BLE manager, can get event */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        hdr = ble_get_event(false);
                        if (!hdr) {
                                goto no_event;
                        }

                        switch (hdr->evt_code) {
                        case BLE_EVT_GAP_ADV_REPORT:
                                handle_evt_gap_adv_report((ble_evt_gap_adv_report_t *) hdr);
                                break;
                        case BLE_EVT_GAP_CONNECTED:
                                handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_DISCONNECTED:
                                /* skip now, handled after processing ble_clients */
                                break;
                        case BLE_EVT_GATTC_MTU_CHANGED:
                                handle_evt_gattc_mtu_changed((ble_evt_gattc_mtu_changed_t *) hdr);
                                break;
                        case BLE_EVT_GATTC_BROWSE_SVC:
                                handle_evt_gattc_browse_svc((ble_evt_gattc_browse_svc_t *) hdr);
                                break;
                        case BLE_EVT_GATTC_BROWSE_COMPLETED:
                                handle_evt_gattc_browse_completed((ble_evt_gattc_browse_completed_t *) hdr);
                                break;
                        case BLE_EVT_L2CAP_CONNECTED:
                                handle_evt_l2cap_connected((ble_evt_l2cap_connected_t *) hdr);
                                break;
                        case BLE_EVT_L2CAP_CONNECTION_FAILED:
                                handle_evt_l2cap_connection_failed((ble_evt_l2cap_connection_failed_t *) hdr);
                                break;
                        case BLE_EVT_L2CAP_DISCONNECTED:
                                handle_evt_l2cap_disconnected((ble_evt_l2cap_disconnected_t *) hdr);
                                break;
                        case BLE_EVT_L2CAP_SENT:
                                handle_evt_l2cap_sent((ble_evt_l2cap_sent_t *) hdr);
                                break;
                        case BLE_EVT_L2CAP_REMOTE_CREDITS_CHANGED:
                                handle_evt_l2cap_remote_credits_changed((ble_evt_l2cap_credit_changed_t *) hdr);
                                break;
                        case BLE_EVT_L2CAP_DATA_IND:
                                handle_evt_l2cap_data_ind((ble_evt_l2cap_data_ind_t *) hdr);
                                break;
                        default:
                                ble_handle_event_default(hdr);
                                break;
                        }

                        ble_client_handle_event(hdr);

                        if (hdr->evt_code == BLE_EVT_GAP_DISCONNECTED) {
                                handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                        }

                        OS_FREE(hdr);

no_event:
                        /* Notify again if there are more events to process in queue */
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
