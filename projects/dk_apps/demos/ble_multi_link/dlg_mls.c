/**
 ****************************************************************************************
 *
 * @file dlg_mls.c
 *
 * @brief Dialog Multi-Link Service sample implementation
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "osal.h"
#include "ble_gatts.h"
#include "ble_uuid.h"
#include "svc_defines.h"
#include "dlg_mls.h"

#define UUID_DLG_MLS                        "3292546e-0a42-4348-aa38-33aab6f9af93"
#define UUID_DLG_MLS_PERIPHERAL_ADDR        "3292546e-0a42-4348-aa38-33aab6f9af94"

typedef struct {
        ble_service_t svc;
        bd_addr_write_cb_t addr_write_cb;
        uint16_t periph_addr_val_h;
} ml_service_t;

static att_error_t handle_peripheral_address_write(ml_service_t *dlg_mls, uint16_t conn_idx,
                                            uint16_t offset, uint16_t length, const uint8_t *value)
{
        if (!dlg_mls->addr_write_cb) {
                return ATT_ERROR_WRITE_NOT_PERMITTED;
        }

        if (length != sizeof(bd_address_t) || !value) {
                return ATT_ERROR_INVALID_VALUE_LENGTH;
        }

        if (value[0] != PUBLIC_ADDRESS && value[0] != PRIVATE_ADDRESS) {
                return ATT_ERROR_APPLICATION_ERROR;
        }

        dlg_mls->addr_write_cb(&dlg_mls->svc, conn_idx, (bd_address_t *) value);

        return ATT_ERROR_OK;
}

static void handle_write_req(ble_service_t *svc, const ble_evt_gatts_write_req_t *evt)
{
        ml_service_t *dlg_mls = (ml_service_t *) svc;
        att_error_t status = ATT_ERROR_ATTRIBUTE_NOT_FOUND;

        if (evt->handle == dlg_mls->periph_addr_val_h) {
                status = handle_peripheral_address_write(dlg_mls, evt->conn_idx, evt->offset,
                                                                          evt->length, evt->value);
        }

        ble_gatts_write_cfm(evt->conn_idx, evt->handle, status);
}

ble_service_t *dlg_mls_init(bd_addr_write_cb_t addr_write_cb)
{
        uint16_t num_attr;
        ml_service_t *dlg_mls;
        att_uuid_t uuid;

        dlg_mls = OS_MALLOC(sizeof(ml_service_t));
        memset(dlg_mls, 0, sizeof(ml_service_t));

        num_attr = ble_gatts_get_num_attr(0, 1, 0);

        ble_uuid_from_string(UUID_DLG_MLS, &uuid);
        ble_gatts_add_service(&uuid, GATT_SERVICE_PRIMARY, num_attr);

        ble_uuid_from_string(UUID_DLG_MLS_PERIPHERAL_ADDR, &uuid);
        ble_gatts_add_characteristic(&uuid, GATT_PROP_WRITE_NO_RESP, ATT_PERM_WRITE,
                                                              7, 0, NULL, &dlg_mls->periph_addr_val_h);

        ble_gatts_register_service(&dlg_mls->svc.start_h, &dlg_mls->periph_addr_val_h, 0);

        dlg_mls->svc.end_h = dlg_mls->svc.start_h + num_attr;
        dlg_mls->svc.write_req = handle_write_req;
        dlg_mls->addr_write_cb = addr_write_cb;

        ble_service_add(&dlg_mls->svc);

        return &dlg_mls->svc;
}
