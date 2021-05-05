/**
 ****************************************************************************************
 *
 * @file power_demo_task.c
 *
 * @brief Power Demo task
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "osal.h"
#include "hw_gpio.h"
#include "cli.h"
#include "sys_watchdog.h"
#include "ble_common.h"
#include "ble_gap.h"
#include "power_demo_settings.h"

#define CLI_NOTIF (1 << 15)

#define WKUP_NOTIF (1 << 16)

/*
 * Power demo advertising data
 */
static const uint8_t adv_data[] = {
        0x12, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'P', 'o', 'w', 'e', 'r', ' ', 'D', 'e', 'm', 'o',
};

typedef void (* config_handler_t) (int idx);

/* Flag to check if application has stopped advertising */
PRIVILEGED_DATA static bool restart_advertising;

/* Active connection index */
INITIALISED_PRIVILEGED_DATA static uint16_t active_conn_idx = BLE_CONN_IDX_INVALID;

/* Application id */
PRIVILEGED_DATA static OS_TASK app_task;

void button_trigger_cb(void)
{
        if (app_task) {
                OS_TASK_NOTIFY_FROM_ISR(app_task, WKUP_NOTIF, OS_NOTIFY_SET_BITS);
        }
}

static void handle_evt_gap_pair_req(ble_evt_gap_pair_req_t *evt)
{
        ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        active_conn_idx = evt->conn_idx;

        printf("Connected\r\n");
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        active_conn_idx = BLE_CONN_IDX_INVALID;

        printf("Disconnected\r\n");

        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
        restart_advertising = false;

        printf("Advertising started...\r\n");
}

static void handle_evt_gap_adv_completed(ble_evt_gap_adv_completed_t *evt)
{
        printf("Advertising stopped\r\n");

        /*
         * Start advertising with new advertising parameters which had been set earlier if
         * advertising had been stopped by the application before.
         */
        if (restart_advertising) {
                ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
                restart_advertising = false;

                printf("Advertising started...\r\n");
        }
}

static void set_adv_interval(int idx)
{
        const adv_interval_config_t *config = get_adv_interval_config(idx);

        if (!config) {
                printf("No advertising interval configuration with idx: %d\r\n", idx);
                return;
        }

        ble_gap_adv_stop();
        restart_advertising = true;

        printf("Set advertising interval\r\n");
        printf("\tInterval min: 0x%04X ms\r\n", BLE_ADV_INTERVAL_TO_MS(config->interval_min));
        printf("\tInterval max: 0x%04X ms\r\n", BLE_ADV_INTERVAL_TO_MS(config->interval_max));

        ble_gap_adv_intv_set(config->interval_min, config->interval_max);
}

static void set_adv_channel_map(int idx)
{
        uint8_t chnl_map;

        if (!get_channel_map_config(idx, &chnl_map)) {
                printf("No channel map configuration with idx: %d\r\n", idx);
                return;
        }

        ble_gap_adv_stop();
        restart_advertising = true;

        printf("Set advertising channel map to: 0x%02X\r\n", chnl_map);

        ble_gap_adv_chnl_map_set(chnl_map);
}

static void set_recharge_period(int idx)
{
        uint16_t period;

        if (!get_recharge_period_config(idx, &period)) {
                printf("No recharge period configuration with idx: %d\r\n", idx);
                return;
        }

        printf("Set recharge period to: 0x%04X\r\n", period);

        hw_cpm_set_recharge_period(period);
}

static void conn_param_update(int idx)
{
        const gap_conn_params_t *params = get_conn_params_config(idx);

        if (!params) {
                printf("No connection params configuration with idx: %d\r\n", idx);
                return;
        }

        if (active_conn_idx == BLE_CONN_IDX_INVALID) {
                printf("No active connection\r\n");
                return;
        }

        printf("Connection param update\r\n");
        printf("\tInterval min: 0x%04X ms\r\n", BLE_CONN_INTERVAL_TO_MS(params->interval_min));
        printf("\tInterval max: 0x%04X ms\r\n", BLE_CONN_INTERVAL_TO_MS(params->interval_max));
        printf("\tSlave latency: 0x%04X ms\r\n", params->slave_latency);
        printf("\tSupervision timeout: 0x%04X ms\r\n", BLE_SUPERVISION_TMO_TO_MS(params->sup_timeout));

        ble_gap_conn_param_update(active_conn_idx, params);
}

static void clicmd_config_handler(int argc, const char *argv[], void *user_data)
{
        config_handler_t func = user_data;
        int idx;

        if (argc < 2) {
                printf("Missing configuration index\r\n");
                return;
        }

        idx = atoi(argv[1]);

        func(idx);
}

static void clicmd_default_handler(int argc, const char *argv[], void *user_data)
{
        printf("Valid commands:\r\n");
        printf("\tset_adv_interval <configuration_idx>\r\n");
        printf("\tset_adv_channel_map <configuration_idx>\r\n");
        printf("\tset_recharge_period <configuration_idx>\r\n");
        printf("\tconn_param_update <configuration_idx>\r\n");
}

static const cli_command_t clicmd[] = {
        { "set_adv_interval", clicmd_config_handler, set_adv_interval },
        { "set_adv_channel_map", clicmd_config_handler, set_adv_channel_map },
        { "set_recharge_period", clicmd_config_handler, set_recharge_period },
        { "conn_param_update", clicmd_config_handler, conn_param_update },
        {},
};

static void handle_button_press(void)
{
        config_type_t type = get_config_type();
        int idx = get_config_index();

        switch (type) {
        case CONFIG_TYPE_ADV_INTERVAL:
                set_adv_interval(idx);
                break;
        case CONFIG_TYPE_CHANNEL_MAP:
                set_adv_channel_map(idx);
                break;
        case CONFIG_TYPE_RECHARGE_PERIOD:
                set_recharge_period(idx);
                break;
        case CONFIG_TYPE_CONN_PARAM_UPDATE:
                conn_param_update(idx);
                break;
        }
}


void power_demo_task(void *params)
{
        int8_t wdog_id;
        cli_t *cli;

        /* register power_demo task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        cli = cli_register(CLI_NOTIF, clicmd, clicmd_default_handler);

        app_task = OS_GET_CURRENT_TASK();

        ble_peripheral_start();
        ble_register_app();
        ble_gap_device_name_set("Dialog Power Demo", ATT_PERM_READ);

        /*
         * Set advertising data and scan response, then start advertising.
         */
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, 0, NULL);
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

        printf("Advertising started...\r\n");

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

                        switch (hdr->evt_code) {
                        case BLE_EVT_GAP_CONNECTED:
                                handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_DISCONNECTED:
                                handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                break;
                        case BLE_EVT_GAP_PAIR_REQ:
                                handle_evt_gap_pair_req((ble_evt_gap_pair_req_t *) hdr);
                                break;
                        case BLE_EVT_GAP_ADV_COMPLETED:
                                handle_evt_gap_adv_completed((ble_evt_gap_adv_completed_t *) hdr);
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
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK, eSetBits);
                        }
                }

                if (notif & CLI_NOTIF) {
                        cli_handle_notified(cli);
                }

                if (notif & WKUP_NOTIF) {
                        handle_button_press();
                }
        }
}
