/**
 ****************************************************************************************
 *
 * @file dlg_mls.h
 *
 * @brief Dialog Multi-Link Service sample implementation API
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef DLG_MLS_H_
#define DLG_MLS_H_

#include "ble_service.h"

typedef void (* bd_addr_write_cb_t) (ble_service_t *svc, uint16_t conn_idx, const bd_address_t *addr);

ble_service_t *dlg_mls_init(bd_addr_write_cb_t addr_write_cb);

#endif /* DLG_MLS_H_ */
