/**
 ****************************************************************************************
 *
 * @file ble_peripheral_task.c
 *
 * @brief BLE peripheral task
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
#include "osal.h"
#include "sys_watchdog.h"
#include "ble_att.h"
#include "ble_common.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_service.h"
#include "ble_uuid.h"
#include "ble_peripheral_config.h"
#if CFG_DEBUG_SERVICE
#include "dlg_debug.h"
#endif /* CFG_DEBUG_SERVICE */
#include "bas.h"
#include "cts.h"
#include "dis.h"
#include "scps.h"

/*
 * Notification bits reservation
 * bit #0 is always assigned to BLE event queue notification
 */
#define CTS_SET_TIME_NOTIF (1 << 2)
#define BCS_TIMER_NOTIF (1 << 3)

/*
 * BLE peripheral advertising data
 */
static const uint8_t adv_data[] = {
        0x12, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'P', 'e', 'r', 'i', 'p', 'h', 'e', 'r', 'a', 'l'
};

static OS_TASK ble_peripheral_task_handle;

#if CFG_DEBUG_SERVICE
PRIVILEGED_DATA static ble_service_t *dbgs;
#endif /* CFG_DEBUG_SERVICE */

/*
 * Battery Service data
 */
#if CFG_BAS
#if CFG_BAS_MULTIPLE
static const bas_battery_info_t bas_bat1 = {
        .namespace = 0x01,      // Bluetooth SIG
        .descriptor = 0x0106,   // main
};

static const bas_battery_info_t bas_bat2 = {
        .namespace = 0x01,      // Bluetooth SIG
        .descriptor = 0x0109,   // supplementary
};
#endif

#endif // CFG_BAS && CFG_BAS_MULTIPLE

/*
 * Current Time Service data
 */
#if CFG_CTS
INITIALISED_PRIVILEGED_DATA static cts_current_time_t cts_time = {
        .date_time = {
                .year    = 1970,
                .month   = 1,
                .day     = 1,
                .hours   = 0,
                .minutes = 0,
                .seconds = 0,
        },
        .day_of_week = 4,
        .fractions_256 = 0,
        .adjust_reason = CTS_AR_NO_CHANGE,
};

PRIVILEGED_DATA static OS_TIMER cts_timer;

static void cts_timer_cb(OS_TIMER timer)
{
        cts_time.date_time.seconds++;
        if (cts_time.date_time.seconds > 59) {
                cts_time.date_time.seconds -= 60;
                cts_time.date_time.minutes += 1;
        }
        if (cts_time.date_time.minutes > 59) {
                cts_time.date_time.minutes -= 60;
                cts_time.date_time.hours += 1;
        }
        if (cts_time.date_time.hours > 23) {
                cts_time.date_time.hours -= 24;
        }
}

static void cts_get_time_cb(ble_service_t *svc, uint16_t conn_idx)
{
        cts_current_time_t time = cts_time;

        // for read we return 'no change' adjust reason
        time.adjust_reason = CTS_AR_NO_CHANGE;

        cts_get_time_cfm(svc, conn_idx, ATT_ERROR_OK, &time);
}

static void cts_set_time_cb(ble_service_t *svc, uint16_t conn_idx, const cts_current_time_t *time)
{
#if CFG_DEBUG_SERVICE
        dlgdebug_notify_str(dbgs, conn_idx, "T %04d%02d%02d %02d%02d%02d %d %02x",
                        time->date_time.year, time->date_time.month, time->date_time.day,
                        time->date_time.hours, time->date_time.minutes, time->date_time.seconds,
                        time->day_of_week, time->adjust_reason);
#endif /* CFG_DEBUG_SERVICE */
        if (!cts_is_current_time_valid(time)) {
                cts_set_time_cfm(svc, conn_idx, CTS_ERROR_DATA_FIELD_IGNORED);
                return;
        }

        cts_time = *time;

        cts_set_time_cfm(svc, conn_idx, ATT_ERROR_OK);

        // notify other clients that time has changed
        cts_time.adjust_reason = CTS_AR_MANUAL_TIME_UPDATE;
        OS_TASK_NOTIFY(ble_peripheral_task_handle, CTS_SET_TIME_NOTIF, eSetBits);
}

static void cts_set_local_time_info_cb(ble_service_t *svc, uint16_t conn_idx,
                                                                const cts_local_time_info_t *info)
{
#if CFG_DEBUG_SERVICE
        dlgdebug_notify_str(dbgs, conn_idx, "L tz=%d dst=%d", info->time_zone, info->dst);
#endif /* CFG_DEBUG_SERVICE */
        if (!cts_is_local_time_info_valid(info)) {
                cts_set_local_time_info_cfm(svc, conn_idx, CTS_ERROR_DATA_FIELD_IGNORED);
                return;
        }

        cts_set_local_time_info(svc, info);

        cts_set_local_time_info_cfm(svc, conn_idx, ATT_ERROR_OK);
}

static void cts_get_ref_time_info_cb(ble_service_t *svc, uint16_t conn_idx)
{
        cts_ref_time_info_t rti;

        rti.source = CTS_RTS_MANUAL;
        rti.accuracy = 100;
        rti.days_since_update = 0;
        rti.hours_since_update = 1;

        cts_get_ref_time_info_cfm(svc, conn_idx, ATT_ERROR_OK, &rti);
}

static const cts_callbacks_t cts_callbacks = {
        .get_time = cts_get_time_cb,
        .set_time = cts_set_time_cb,
        .set_local_time_info = cts_set_local_time_info_cb,
        .get_ref_time_info = cts_get_ref_time_info_cb,
};
#endif // CFG_CTS

/*
 * Custom service data
 */
#if CFG_USER_SERVICE
static void myservice_init(ble_service_t *include_svc)
{
        att_uuid_t uuid;

        /*
         * This service does absolutely nothing, it just checks that it's possible to use 128-bit
         * UUIDs for services, characteristics and descriptors.
         */

        ble_uuid_from_string("91a7608d-4456-479d-b9b1-4706e8711cf8", &uuid);
        ble_gatts_add_service(&uuid, GATT_SERVICE_PRIMARY, ble_gatts_get_num_attr(1, 1, 1));

        if (include_svc) {
                ble_gatts_add_include(include_svc->start_h, NULL);
        }

        ble_uuid_from_string("25047e64-657c-4856-afcf-e315048a965b", &uuid);
        ble_gatts_add_characteristic(&uuid, GATT_PROP_NONE, ATT_PERM_NONE, 1, 0, NULL, NULL);

        ble_uuid_from_string("6b09fe25-eed7-41fc-8da7-1ec89fab7ecb", &uuid);
        ble_gatts_add_descriptor(&uuid, ATT_PERM_NONE, 1, 0, NULL);

        ble_gatts_register_service(NULL, 0);
}
#endif // CFG_USER_SERVICE

/*
 * Device Information Service data
 */
#if CFG_DIS
#if CFG_DIS_FULL
static const dis_system_id_t dis_sys_id = {
        .oui = { 0x80, 0xEA, 0xCA },    // Dialog Semiconductor Hellas SA
        .manufacturer = { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E },
};

// copied from "Personal Health Devices Transcoding White Paper", available on bluetooth.org
static const uint8_t dis_reg_cert[] = {
        0x00, 0x02, 0x00, 0x12, 0x02, 0x01, 0x00, 0x08, 0x01,
        0x05, 0x00, 0x01, 0x00, 0x02, 0x80, 0x08, 0x02, 0x02,
        0x00, 0x02, 0x00, 0x00,
};

static const dis_pnp_id_t dis_pnp_id = {
        .vid_source = 0x01,     // Bluetooth SIG
        .vid = 0x00D2,          //  Dialog Semiconductor B.V.
        .pid = 0x0001,
        .version = 0x0001,
};
#endif

static const dis_device_info_t dis_info = {
        .manufacturer = "Dialog Semiconductor",
        .model_number = "Dialog BLE",
#if CFG_DIS_FULL
        // sample data
        .serial_number = "SN123456",
        .hw_revision = "Rev.D",
        .fw_revision = "1.0",
        .sw_revision = "1.1",
        .system_id = &dis_sys_id,
        .reg_cert_length = sizeof(dis_reg_cert),
        .reg_cert = dis_reg_cert,
        .pnp_id = &dis_pnp_id,
#endif
};
#endif // CFG_DIS

/*
 * Scan Parameter Service data
 */
#if CFG_SCPS
static const scps_callbacks_t scps_callbacks = {
};
#endif // CFG_SCPS

/*
 * Debug handlers
 */
#if (CFG_BAS && CFG_DEBUG_SERVICE)
static void dbg_bas_set(uint16_t conn_idx, int argc, char **argv, void *ud)
{
        ble_service_t *svc = ud;
        uint8_t level = atoi(argv[0]);

        bas_set_level(svc, level, true);
}
#endif /* (CFG_BAS && CFG_DEBUG_SERVICE) */

#if (CFG_CTS && CFG_DEBUG_SERVICE)
static void dbg_cts_adjust(uint16_t conn_idx, int argc, char **argv, void *ud)
{
        ble_service_t *svc = ud;
        cts_current_time_t time;

        memset(&time, 0, sizeof(time));

        if (!strcmp(argv[0], "manual")) {
                time.adjust_reason = CTS_AR_MANUAL_TIME_UPDATE;
        } else if (!strcmp(argv[0], "tz")) {
                time.adjust_reason = CTS_AR_CHANGE_OF_TIME_ZONE;
        } else if (!strcmp(argv[0], "dst")) {
                time.adjust_reason = CTS_AR_CHANGE_OF_DST;
        } else if (!strcmp(argv[0], "reftime")) {
                time.adjust_reason = CTS_AR_EXTERNAL_REFERENCE_TIME_UPDATE;
        }

        if (time.adjust_reason != CTS_AR_NO_CHANGE) {
                cts_notify_time_all(svc, &time);
        }
}
#endif /* (CFG_CTS && CFG_DEBUG_SERVICE) */

/*
 * Main code
 */
static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        /**
         * Manage connection information
         */
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
}

static void handle_evt_gap_adv_completed(ble_evt_gap_adv_completed_t *evt)
{
        // restart advertising so we can connect again
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
}

void ble_peripheral_task(void *params)
{
        int8_t wdog_id;
#if CFG_CTS
        ble_service_t *cts;
        cts_local_time_info_t cts_lti = {
                /* Example time zone, should be taken from permanent storage or RTC */
                .dst = CTS_DST_DAYLIGHT_TIME,
                .time_zone = cts_get_time_zone(+3, 0), // UTC + 3 Athens
        };
#endif
        ble_service_t *svc;

        // in case services which do not use svc are all disabled, just surpress -Wunused-variable
        (void) svc;

        /* register ble_peripheral task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        ble_peripheral_task_handle = OS_GET_CURRENT_TASK();

        srand(time(NULL));

        ble_peripheral_start();
        ble_register_app();

        ble_gap_device_name_set("Dialog Peripheral", ATT_PERM_READ);

#if CFG_DEBUG_SERVICE
        /* register debug service */
        dbgs = dlgdebug_init(NULL);
#endif /* CFG_DEBUG_SERVICE */

#if CFG_BAS
#if CFG_BAS_MULTIPLE
        /* register BAS (1st instance) */
        svc = bas_init(NULL, &bas_bat1);
#if CFG_DEBUG_SERVICE
        dlgdebug_register_handler(dbgs, "bas", "set", dbg_bas_set, svc);
#endif /* CFG_DEBUG_SERVICE */
        bas_set_level(svc, 90, false);

        /* register BAS (2nd instance) */
        svc = bas_init(NULL, &bas_bat2);
        bas_set_level(svc, 60, false);
#else
        /* register BAS */
        svc = bas_init(NULL, NULL);
#if CFG_DEBUG_SERVICE
        dlgdebug_register_handler(dbgs, "bas", "set", dbg_bas_set, svc);
#endif /* CFG_DEBUG_SERVICE */
        bas_set_level(svc, 90, false);
#endif /* CFG_BAS_MULTIPLE */
#endif /* CFG_BAS */

#if CFG_CTS
        /* register CTS */
        cts = cts_init(&cts_lti, &cts_callbacks);
#if CFG_DEBUG_SERVICE
        dlgdebug_register_handler(dbgs, "cts", "adjust", dbg_cts_adjust, cts);
#endif /* CFG_DEBUG_SERVICE */
#endif

#if CFG_USER_SERVICE
        /* register custom service */
#if CFG_CTS
        myservice_init(cts);
#else
        myservice_init(NULL);
#endif
#endif

#if CFG_DIS
        /* Add DIS */
        dis_init(NULL, &dis_info);
#endif

#if CFG_SCPS
        /* register ScPS */
        scps_init(&scps_callbacks);
#endif

#if CFG_CTS
        /* create timer for CTS, this will be used to update current time every second */
        cts_timer = OS_TIMER_CREATE("cts", portCONVERT_MS_2_TICKS(1000), OS_TIMER_SUCCESS,
                                                (void *) OS_GET_CURRENT_TASK(), cts_timer_cb);
        OS_ASSERT(cts_timer);
        OS_TIMER_START(cts_timer, OS_TIMER_FOREVER);
#endif

        ble_gap_adv_data_set(sizeof(adv_data), adv_data, 0, NULL);
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

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

                        if (ble_service_handle_event(hdr)) {
                                goto handled;
                        }

                        switch (hdr->evt_code) {
                        case BLE_EVT_GAP_CONNECTED:
                                handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_ADV_COMPLETED:
                                handle_evt_gap_adv_completed((ble_evt_gap_adv_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_DISCONNECTED:
                                handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_PAIR_REQ:
                        {
                                ble_evt_gap_pair_req_t *evt = (ble_evt_gap_pair_req_t *) hdr;
                                ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
                                break;
                        }
                        default:
                                ble_handle_event_default(hdr);
                                break;
                        }

handled:
                        OS_FREE(hdr);

no_event:
                        // notify again if there are more events to process in queue
                        if (ble_has_event()) {
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK, eSetBits);
                        }
                }
#if CFG_CTS
                if (notif & CTS_SET_TIME_NOTIF) {
                        cts_notify_time_all(cts, &cts_time);
                }
#endif
        }
}
