/**
 ****************************************************************************************
 *
 * @file device_task.c
 *
 * @brief HID Device demo application
 *
 * Copyright (C) 2017 Dialog Semiconductor.
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
#include "hw_wkup.h"
#include "ble_common.h"
#include "ble_uuid.h"
#include "ble_service.h"
#include "bas.h"
#include "dis.h"
#include "hids.h"
#include "hid_ble_config.h"
#include "common.h"

#define ADV_TIMER_TMO_NOTIF     (1 << 2)        /* advertising timeout */
#define WKUP_TIMER_NOTIF        (1 << 3)        /* wakeup timer */
#define DEVICE_NOTIF_MASK       (0xFFFF0000)    /* mask for device_handler notifications */

/** Advertising Data and Scan Response definitions */
static const uint8_t adv_data[] = {
        0x03, GAP_DATA_TYPE_UUID16_LIST_INC, UUID_SERVICE_HIDS & 0xFF, UUID_SERVICE_HIDS >> 8,
        0x03, GAP_DATA_TYPE_APPEARANCE, (HID_BLE_APPEARANCE) & 0xFF, (HID_BLE_APPEARANCE) >> 8,
};

static const uint8_t scan_rsp[] = {
        0x0C, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'H', 'o', 'G', 'P',
};

/** Device Information Service configuration */
static const dis_pnp_id_t dis_pnp_info =  {
        .vid_source = HID_BLE_PNP_VID_SOURCE,
        .vid = HID_BLE_PNP_VID,
        .pid = HID_BLE_PNP_PID,
        .version = HID_BLE_PNP_VERSION,
};

static const dis_device_info_t dis_info = {
        .manufacturer = "Dialog Semiconductor",
        .model_number = "Dialog BLE",
        .pnp_id = &dis_pnp_info,
};

__RETAINED static OS_TIMER adv_timer;

__RETAINED static OS_TASK app_task;

__RETAINED ble_service_t *hids;

__RETAINED static struct {
        hids_cp_command_t hids_cp_state;
        bool connected;
} peer_state;

static void start_adv(gap_conn_mode_t mode)
{
        size_t len;
        gap_device_t dev;
        ble_error_t err;
        bool is_bonded;

        len = 1;
        err = ble_gap_get_devices(GAP_DEVICE_FILTER_BONDED, NULL, &len, &dev);
        is_bonded = err == BLE_STATUS_OK && len > 0;

        /*
         * Directed advertising can be only used if we are already bonded, otherwise fall-back to
         * undirected advertising.
         */
        if (is_bonded && mode == GAP_CONN_MODE_DIRECTED) {
                ble_gap_adv_direct_address_set(&dev.address);
        } else {
                mode = GAP_CONN_MODE_UNDIRECTED;

                /* Values for min/max advertising interval and advertising duration are set as
                 * recommended by HoGP specification (section 5.1.2 and 5.1.3).
                 */

                if (is_bonded) {
                        ble_gap_adv_intv_set(BLE_ADV_INTERVAL_FROM_MS(20),
                                                                BLE_ADV_INTERVAL_FROM_MS(30));
                        OS_TIMER_CHANGE_PERIOD(adv_timer, OS_MS_2_TICKS(30000), OS_TIMER_FOREVER);
                } else {
                        ble_gap_adv_intv_set(BLE_ADV_INTERVAL_FROM_MS(30),
                                                                BLE_ADV_INTERVAL_FROM_MS(50));
                        OS_TIMER_CHANGE_PERIOD(adv_timer, OS_MS_2_TICKS(180000), OS_TIMER_FOREVER);
                }

                OS_TIMER_START(adv_timer, OS_TIMER_FOREVER);
        }

        ble_gap_adv_stop();
        ble_gap_adv_start(mode);
}

static void adv_timer_cb(OS_TIMER timer)
{
        OS_TASK_NOTIFY(app_task, ADV_TIMER_TMO_NOTIF, OS_NOTIFY_SET_BITS);
}

void wkup_cb(void)
{
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        hw_wkup_reset_counter();
#endif
        hw_wkup_reset_interrupt();

        OS_TASK_NOTIFY_FROM_ISR(app_task, WKUP_TIMER_NOTIF, eSetBits);
}

static void hids_set_protocol_mode_cb(ble_service_t *svc, hids_protocol_mode_t mode)
{
        device_protocol_mode_set(mode);
}

static void hids_control_point_cb(ble_service_t *svc, hids_cp_command_t command)
{
        if (peer_state.hids_cp_state == command) {
                return;
        }

        peer_state.hids_cp_state = command;

        if (peer_state.hids_cp_state == HIDS_CONTROL_POINT_EXIT_SUSPEND) {
                device_wakeup();
        } else {
                device_suspend();
        }
}

static void hids_report_write_cb(ble_service_t *svc, hids_report_type_t type, uint8_t id,
                                                        uint16_t length, const uint8_t *data)
{
        device_report_written(type, id, length, data);
}

static void hids_boot_keyboard_output_write_cb(ble_service_t *svc, uint16_t length,
                                                                        const uint8_t *data)
{
        device_boot_keyboard_report_written(length, data);
}

/* HIDS callbacks */
static const hids_callbacks_t hids_callbacks = {
        .set_protocol_mode = hids_set_protocol_mode_cb,
        .control_point = hids_control_point_cb,
        .report_write = hids_report_write_cb,
        .boot_keyboard_write = hids_boot_keyboard_output_write_cb,

};

static void handle_evt_gap_adv_completed(ble_evt_gap_adv_completed_t *evt)
{
        OS_TIMER_STOP(adv_timer, OS_TIMER_FOREVER);

        /* If finished with timeout, this will be switch from directed to undirected adv */
        if (evt->status == BLE_ERROR_TIMEOUT) {
                start_adv(GAP_CONN_MODE_UNDIRECTED);
        }
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        peer_state.connected = true;

        /*
         * HIDS can be used exclusively by one connection and it's required to attach particular
         * connection to service instance. In our case we can have only one device connected at any
         * time so we can just attach whatever comes in.
         *
         * Connection is automatically detached when disconnected.
         */
        hids_attach_connection(hids, evt->conn_idx);

        device_protocol_mode_set(HIDS_PROTOCOL_MODE_REPORT);

        /* Send a security request after connection */
        ble_gap_set_sec_level(evt->conn_idx, GAP_SEC_LEVEL_2);
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        peer_state.connected = false;

        device_disconnected();

        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
}

static void handle_evt_gap_pair_req(ble_evt_gap_pair_req_t *evt)
{
        ble_error_t err;

        err = ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
        if (err != BLE_STATUS_OK) {
                /*
                 * Reply may be rejected by stack due to e.g. insufficient resources in case we are
                 * already bonded but different remote tries to bond again. In this case we reject
                 * pairing and user needs to remove bonding in order to pair with other host.
                 */
                ble_gap_pair_reply(evt->conn_idx, false, evt->bond);
        }
}

static void handle_evt_gap_sec_level_changed(ble_evt_gap_sec_level_changed_t *evt)
{
        /*
         * We use sec level 2 thus it only makes sense to enable device code when encryption is
         * enabled - in other case, we do not sent anything anyway due to insufficient security.
         */
        device_connected();
}

void device_task(void *params)
{
        int8_t wdog_id;
        ble_service_config_t service_config = {
                .sec_level = GAP_SEC_LEVEL_2,
                .num_includes = 0,
        };

        app_task = OS_GET_CURRENT_TASK();
        wdog_id = sys_watchdog_register(false);

        ble_peripheral_start();
        ble_register_app();

        /* Initialize device-specific part */
        device_init();

        /* Set mandatory GAP characteristics values */
        ble_gap_device_name_set(HID_BLE_DEVICE_NAME, ATT_PERM_READ);
        ble_gap_appearance_set(HID_BLE_APPEARANCE, ATT_PERM_READ);

        /*
         * Add BLE services
         */

        /* Add BAS */
        bas_init(&service_config, NULL);

        /* Add DIS */
        dis_init(&service_config, &dis_info);

        /* Add HIDS */
        hids = hids_init(&service_config, &hid_ble_config, &hids_callbacks);

        peer_state.hids_cp_state = HIDS_CONTROL_POINT_EXIT_SUSPEND;

        /*
         * If device is started with P1.6 (K1 button) pressed, remove bonding information to allow
         * new host to be bonded.
         */
        if (!hw_gpio_get_pin_status(HW_GPIO_PORT_1, HW_GPIO_PIN_6)) {
                gap_device_t dev;
                size_t len = 1;
                ble_error_t err;

                err = ble_gap_get_devices(GAP_DEVICE_FILTER_BONDED, NULL, &len, &dev);
                if (err == BLE_STATUS_OK && len > 0) {
                        ble_gap_unpair(&dev.address);
                }
        }

        /* Configure advertising */
        adv_timer = OS_TIMER_CREATE("adv", 1, false, NULL, adv_timer_cb);
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, sizeof(scan_rsp), scan_rsp);
        start_adv(GAP_CONN_MODE_DIRECTED);

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                /* notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);

                /* suspend watchdog while blocking on OS_TASK_NOTIFY_WAIT() */
                sys_watchdog_suspend(wdog_id);

                /* Wait on any of the notification bits, then clear them all */
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                /* Guaranteed to succeed, since we're waiting forever until a notification is received */
                OS_ASSERT(ret == OS_OK);

                /* resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

                /* Notified from BLE Manager? */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        hdr = ble_get_event(false);
                        if (!hdr) {
                                goto no_event;
                        }

                        if (ble_service_handle_event(hdr)) {
                                goto event_handled;
                        }

                        switch (hdr->evt_code) {
                        case BLE_EVT_GAP_ADV_COMPLETED:
                                handle_evt_gap_adv_completed((ble_evt_gap_adv_completed_t *) hdr);
                                break;
                        case BLE_EVT_GAP_CONNECTED:
                                handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_DISCONNECTED:
                                handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_PAIR_REQ:
                                handle_evt_gap_pair_req((ble_evt_gap_pair_req_t *) hdr);
                                break;
                        case BLE_EVT_GAP_SEC_LEVEL_CHANGED:
                                handle_evt_gap_sec_level_changed((ble_evt_gap_sec_level_changed_t *) hdr);
                                break;
                        default:
                                ble_handle_event_default(hdr);
                                break;
                        }

event_handled:
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

                if (notif & ADV_TIMER_TMO_NOTIF) {
                        /* Stop any advertising on timeout */
                        ble_gap_adv_stop();
                }

                if (notif & DEVICE_NOTIF_MASK) {
                        device_task_notif(notif & DEVICE_NOTIF_MASK);
                }

                if (notif & WKUP_TIMER_NOTIF) {
                        if (!peer_state.connected) {
                                start_adv(GAP_CONN_MODE_DIRECTED);
                        }
                }
        }
}
