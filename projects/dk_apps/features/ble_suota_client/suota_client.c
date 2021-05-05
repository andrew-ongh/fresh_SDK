/**
 ****************************************************************************************
 *
 * @file suota_client.c
 *
 * @brief SUOTA 1.2 GATT client
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include "osal.h"
#include "ble_bufops.h"
#include "ble_client.h"
#include "ble_gatt.h"
#include "ble_gattc.h"
#include "ble_gattc_util.h"
#include "ble_uuid.h"
#include "suota_client.h"

#define UUID_SUOTA                              0xFEF5
#define UUID_SUOTA_MEM_DEV                      "8082CAA8-41A6-4021-91C6-56F9B954CC34"
#define UUID_SUOTA_PATCH_LEN                    "9D84B9A3-000C-49D8-9183-855B673FDA31"
#define UUID_SUOTA_PATCH_DATA                   "457871E8-D516-4CA1-9116-57D0B17B9CB2"
#define UUID_SUOTA_STATUS                       "5F78DF94-798C-46F5-990A-B3EB6A065C88"
#define UUID_SUOTA_L2CAP_PSM                    "61C8849C-F639-4765-946E-5C3419BEBB2A"
#define UUID_SUOTA_VERSION                      "64B4E8B5-0DE5-401B-A21D-ACC8DB3B913A"
#define UUID_SUOTA_PATCH_DATA_CHAR_SIZE         "42C3DFDD-77BE-4D9C-8454-8F875267FB3B"
#define UUID_SUOTA_MTU                          "B7DE1EEA-823D-43BB-A3AF-C4903DFCE23C"

typedef enum {
        MEM_DEV_OP_NONE,
        MEM_DEV_OP_SET_MEM_DEV,
        MEM_DEV_OP_SEND_REBOOT_CMD,
        MEM_DEV_OP_SEND_END_CMD,
        MEM_DEV_OP_SEND_ABORT_CMD,
} mem_dev_op_t;

typedef struct {
        ble_client_t client;

        const suota_client_callbacks_t *cb;
        suota_client_cap_t caps;

        mem_dev_op_t mem_dev_op;

        uint16_t mem_dev_val_h;
        uint16_t patch_len_val_h;
        uint16_t patch_data_val_h;
        uint16_t status_val_h;
        uint16_t status_ccc_h;
        uint16_t l2cap_psm_val_h;
        uint16_t suota_version_val_h;
        uint16_t patch_data_char_size_val_h;
} suota_client_t;

static void cleanup(ble_client_t *client)
{
        OS_FREE(client);
}

static void dispatch_read_l2cap_psm_completed(ble_client_t *client,
                                                        const ble_evt_gattc_read_completed_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (!suota_client->cb->read_l2cap_psm_completed) {
                return;
        }

        if (evt->status != ATT_ERROR_OK) {
                suota_client->cb->read_l2cap_psm_completed(client, evt->status, 0);
                return;
        }

        if (evt->offset || evt->length < sizeof(uint16_t)) {
                suota_client->cb->read_l2cap_psm_completed(client, ATT_ERROR_UNLIKELY, 0);
                return;
        }

        suota_client->cb->read_l2cap_psm_completed(client, ATT_ERROR_OK, get_u16(evt->value));
}

static void dispatch_get_event_state_completed(ble_client_t *client,
                                                        const ble_evt_gattc_read_completed_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (!suota_client->cb->get_event_state_completed) {
                return;
        }

        if (evt->status != ATT_ERROR_OK) {
                suota_client->cb->get_event_state_completed(client, evt->status,
                                                        SUOTA_CLIENT_EVENT_STATUS_NOTIF, false);
                return;
        }

        if (evt->offset || evt->length < sizeof(uint16_t)) {
                suota_client->cb->get_event_state_completed(client, ATT_ERROR_UNLIKELY,
                                                        SUOTA_CLIENT_EVENT_STATUS_NOTIF, false);
                return;
        }

        suota_client->cb->get_event_state_completed(client, ATT_ERROR_OK,
                                                        SUOTA_CLIENT_EVENT_STATUS_NOTIF,
                                                        get_u16(evt->value) & GATT_CCC_NOTIFICATIONS);
}

static void dispatch_get_suota_version_completed(ble_client_t *client,
                                                        const ble_evt_gattc_read_completed_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (!suota_client->cb->get_suota_version_completed) {
                return;
        }

        suota_client->cb->get_suota_version_completed(client, ATT_ERROR_OK, get_u8(evt->value));
}

static void dispatch_get_patch_data_char_size_completed(ble_client_t *client,
                                                        const ble_evt_gattc_read_completed_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (!suota_client->cb->get_patch_data_char_size_completed) {
                return;
        }

        suota_client->cb->get_patch_data_char_size_completed(client, ATT_ERROR_OK, get_u16(evt->value));
}

static void dispatch_set_event_state_completed(ble_client_t *client,
                                                        const ble_evt_gattc_write_completed_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (!suota_client->cb->set_event_state_completed) {
                return;
        }

        suota_client->cb->set_event_state_completed(client, SUOTA_CLIENT_EVENT_STATUS_NOTIF,
                                                                                        evt->status);
}

static void dispatch_generic_write_completed(ble_client_t *client,
                                                        const ble_evt_gattc_write_completed_t *evt,
                                                        suota_client_generic_write_completed_cb_t cb)
{
        if (!cb) {
                return;
        }

        cb(client, evt->status);
}

static void handle_disconnect_evt(ble_client_t *client, const ble_evt_gap_disconnected_t *evt)
{
        client->conn_idx = BLE_CONN_IDX_INVALID;
        ble_client_remove(client);
}

static void handle_notification_evt(ble_client_t *client, const ble_evt_gattc_notification_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (evt->handle != suota_client->status_val_h ||
                                        !suota_client->cb->status_notif || evt->length < 1) {
                return;
        }

        suota_client->cb->status_notif(client, evt->value[0]);
}

static void handle_read_completed_evt(ble_client_t *client,
                                                        const ble_evt_gattc_read_completed_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (evt->handle == suota_client->l2cap_psm_val_h) {
                dispatch_read_l2cap_psm_completed(client, evt);
        } else if (evt->handle == suota_client->status_ccc_h) {
                dispatch_get_event_state_completed(client, evt);
        } else if (evt->handle == suota_client->suota_version_val_h) {
                dispatch_get_suota_version_completed(client, evt);
        } else if (evt->handle == suota_client->patch_data_char_size_val_h) {
                dispatch_get_patch_data_char_size_completed(client, evt);
        }
}

static void handle_write_completed_evt(ble_client_t *client,
                                                        const ble_evt_gattc_write_completed_t *evt)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        if (evt->handle == suota_client->status_ccc_h) {
                dispatch_set_event_state_completed(client, evt);
        } else if (evt->handle == suota_client->patch_len_val_h) {
                dispatch_generic_write_completed(client, evt,
                                                        suota_client->cb->set_patch_len_completed);
        } else if (evt->handle == suota_client->patch_data_val_h) {
                dispatch_generic_write_completed(client, evt,
                                                        suota_client->cb->send_patch_data_completed);
        } else if (evt->handle == suota_client->mem_dev_val_h) {
                mem_dev_op_t op = suota_client->mem_dev_op;

                suota_client->mem_dev_op = MEM_DEV_OP_NONE;

                switch (op) {
                case MEM_DEV_OP_SET_MEM_DEV:
                        dispatch_generic_write_completed(client, evt,
                                                        suota_client->cb->set_mem_dev_completed);
                        break;
                case MEM_DEV_OP_SEND_REBOOT_CMD:
                        dispatch_generic_write_completed(client, evt,
                                                        suota_client->cb->send_reboot_cmd_completed);
                        break;
                case MEM_DEV_OP_SEND_END_CMD:
                        dispatch_generic_write_completed(client, evt,
                                                        suota_client->cb->send_end_cmd_completed);
                        break;
                case MEM_DEV_OP_SEND_ABORT_CMD:
                        dispatch_generic_write_completed(client, evt,
                                                        suota_client->cb->send_abort_cmd_completed);
                        break;
                default:
                        /* just ignore */
                        break;
                }
        }
}

ble_client_t *suota_client_init(const suota_client_callbacks_t *cb,
                                                        const ble_evt_gattc_browse_svc_t *evt)
{
        suota_client_t *suota_client;
        att_uuid_t uuid;
        const gattc_item_t *item;

        if (!cb || !evt) {
                return NULL;
        }

        ble_uuid_create16(UUID_SUOTA, &uuid);
        if (!ble_uuid_equal(&uuid, &evt->uuid)) {
                return NULL;
        }

        suota_client = OS_MALLOC(sizeof(suota_client_t));
        memset(suota_client, 0, sizeof(suota_client_t));
        suota_client->client.conn_idx = evt->conn_idx;
        suota_client->client.cleanup = cleanup;
        suota_client->client.disconnected_evt = handle_disconnect_evt;
        suota_client->client.notification_evt = handle_notification_evt;
        suota_client->client.read_completed_evt = handle_read_completed_evt;
        suota_client->client.write_completed_evt = handle_write_completed_evt;
        suota_client->cb = cb;

        ble_gattc_util_find_init(evt);

        ble_uuid_from_string(UUID_SUOTA_MEM_DEV, &uuid);
        item = ble_gattc_util_find_characteristic(&uuid);
        if (!item || !(item->c.properties & GATT_PROP_WRITE)) {
                goto failed;
        }
        suota_client->mem_dev_val_h = item->c.value_handle;

        ble_uuid_from_string(UUID_SUOTA_PATCH_LEN, &uuid);
        item = ble_gattc_util_find_characteristic(&uuid);
        if (!item || !(item->c.properties & GATT_PROP_WRITE)) {
                goto failed;
        }
        suota_client->patch_len_val_h = item->c.value_handle;

        ble_uuid_from_string(UUID_SUOTA_PATCH_DATA, &uuid);
        item = ble_gattc_util_find_characteristic(&uuid);
        if (!item || !(item->c.properties & GATT_PROP_WRITE_NO_RESP)) {
                goto failed;
        }
        suota_client->patch_data_val_h = item->c.value_handle;

        ble_uuid_from_string(UUID_SUOTA_STATUS, &uuid);
        item = ble_gattc_util_find_characteristic(&uuid);
        if (!item || !(item->c.properties & GATT_PROP_NOTIFY)) {
                goto failed;
        }
        suota_client->status_val_h = item->c.value_handle;

        ble_uuid_create16(UUID_GATT_CLIENT_CHAR_CONFIGURATION, &uuid);
        item = ble_gattc_util_find_descriptor(&uuid);
        if (!item) {
                goto failed;
        }
        suota_client->status_ccc_h = item->handle;

        ble_uuid_from_string(UUID_SUOTA_L2CAP_PSM, &uuid);
        item = ble_gattc_util_find_characteristic(&uuid);
        if (item) {
                suota_client->l2cap_psm_val_h = item->c.value_handle;
                suota_client->caps |= SUOTA_CLIENT_CAP_L2CAP_PSM;
        }

        ble_uuid_from_string(UUID_SUOTA_VERSION, &uuid);
        item = ble_gattc_util_find_characteristic(&uuid);
        if (item) {
                suota_client->suota_version_val_h = item->c.value_handle;
                suota_client->caps |= SUOTA_CLIENT_CAP_SUOTA_VERSION;
        }

        ble_uuid_from_string(UUID_SUOTA_PATCH_DATA_CHAR_SIZE, &uuid);
        item = ble_gattc_util_find_characteristic(&uuid);
        if (item) {
                suota_client->patch_data_char_size_val_h = item->c.value_handle;
        }

        return &suota_client->client;

failed:
        cleanup(&suota_client->client);
        return NULL;
}

suota_client_cap_t suota_client_get_capabilities(ble_client_t *client)
{
        suota_client_t *suota_client = (suota_client_t *) client;

        return suota_client->caps;
}

bool suota_client_set_event_state(ble_client_t *client, suota_client_event_t event, bool enable)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        uint16_t val;
        ble_error_t ret;

        if (event != SUOTA_CLIENT_EVENT_STATUS_NOTIF) {
                return false;
        }

        val = enable ? GATT_CCC_NOTIFICATIONS : GATT_CCC_NONE;
        ret = ble_gattc_write(client->conn_idx, suota_client->status_ccc_h, 0, sizeof(val),
                                                                                (void *) &val);

        return (ret == BLE_STATUS_OK);
}

bool suota_client_get_event_state(ble_client_t *client, suota_client_event_t event)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        ble_error_t ret;

        if (event != SUOTA_CLIENT_EVENT_STATUS_NOTIF) {
                return false;
        }

        ret = ble_gattc_read(client->conn_idx, suota_client->status_ccc_h, 0);

        return (ret == BLE_STATUS_OK);
}

bool suota_client_read_l2cap_psm(ble_client_t *client)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        ble_error_t ret;

        if (!(suota_client->caps & SUOTA_CLIENT_CAP_L2CAP_PSM)) {
                return false;
        }

        ret = ble_gattc_read(client->conn_idx, suota_client->l2cap_psm_val_h, 0);

        return (ret == BLE_STATUS_OK);
}

bool suota_client_get_suota_version(ble_client_t *client)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        ble_error_t ret;

        if (!(suota_client->caps & SUOTA_CLIENT_CAP_SUOTA_VERSION)) {
                return false;
        }

        ret = ble_gattc_read(client->conn_idx, suota_client->suota_version_val_h, 0);

        return (ret == BLE_STATUS_OK);
}

bool suota_client_get_patch_data_char_size(ble_client_t *client)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        ble_error_t ret;

        ret = ble_gattc_read(client->conn_idx, suota_client->patch_data_char_size_val_h, 0);

        return (ret == BLE_STATUS_OK);
}

bool suota_client_set_mem_dev(ble_client_t *client, suota_client_mem_dev_t dev, uint32_t base_address)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        uint32_t val;
        ble_error_t ret;

        if (suota_client->mem_dev_op != MEM_DEV_OP_NONE) {
                return false;
        }

        val = (dev << 24) | (base_address & 0x00ffffff);

        ret = ble_gattc_write(client->conn_idx, suota_client->mem_dev_val_h, 0, sizeof(val),
                                                                                (void *) &val);

        if (ret == BLE_STATUS_OK) {
                suota_client->mem_dev_op = MEM_DEV_OP_SET_MEM_DEV;
        }

        return (ret == BLE_STATUS_OK);
}

bool suota_client_set_patch_len(ble_client_t *client, uint16_t patch_len)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        ble_error_t ret;

        ret = ble_gattc_write(client->conn_idx, suota_client->patch_len_val_h, 0,
                                                        sizeof(patch_len), (void *) &patch_len);

        return (ret == BLE_STATUS_OK);
}

bool suota_client_send_patch_data(ble_client_t *client, size_t length, const void *data)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        ble_error_t ret;

        ret = ble_gattc_write_no_resp(client->conn_idx, suota_client->patch_data_val_h,
                                                                        false, length, data);

        return (ret == BLE_STATUS_OK);
}

bool suota_client_send_reboot_cmd(ble_client_t *client)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        uint32_t val;
        ble_error_t ret;

        if (suota_client->mem_dev_op != MEM_DEV_OP_NONE) {
                return false;
        }

        val = 0xFD000000;
        ret = ble_gattc_write(client->conn_idx, suota_client->mem_dev_val_h, 0, sizeof(val),
                                                                                (void *) &val);

        if (ret == BLE_STATUS_OK) {
                suota_client->mem_dev_op = MEM_DEV_OP_SEND_REBOOT_CMD;
        }

        return (ret == BLE_STATUS_OK);

}

bool suota_client_send_end_cmd(ble_client_t *client)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        uint32_t val;
        ble_error_t ret;

        if (suota_client->mem_dev_op != MEM_DEV_OP_NONE) {
                return false;
        }

        val = 0xFE000000;
        ret = ble_gattc_write(client->conn_idx, suota_client->mem_dev_val_h, 0, sizeof(val),
                                                                                (void *) &val);

        if (ret == BLE_STATUS_OK) {
                suota_client->mem_dev_op = MEM_DEV_OP_SEND_END_CMD;
        }

        return (ret == BLE_STATUS_OK);

}

bool suota_client_send_abort_cmd(ble_client_t *client)
{
        suota_client_t *suota_client = (suota_client_t *) client;
        uint32_t val;
        ble_error_t ret;

        if (suota_client->mem_dev_op != MEM_DEV_OP_NONE) {
                return false;
        }

        val = 0xFF000000;
        ret = ble_gattc_write(client->conn_idx, suota_client->mem_dev_val_h, 0, sizeof(val),
                                                                                (void *) &val);

        if (ret == BLE_STATUS_OK) {
                suota_client->mem_dev_op = MEM_DEV_OP_SEND_ABORT_CMD;
        }

        return (ret == BLE_STATUS_OK);

}
