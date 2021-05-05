/**
 ****************************************************************************************
 *
 * @file htp_thermometer_task.c
 *
 * @brief Health Thermometer Profile demo task
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include "FreeRTOS.h"
#include "timers.h"
#include "osal.h"
#include "ble_common.h"
#include "ble_service.h"
#include "sys_watchdog.h"
#include "dis.h"
#include "hts.h"
#include "htp_thermometer_config.h"
#include "hw_wkup.h"
#include "sensor.h"
#include "cli.h"
#include "commands.h"

/*
 * Notification bits reservation
 *
 * Bit #0 is always assigned to BLE event queue notification.
 */
#define ADV_TIMER_NOTIF                 (1 << 1)
#define HTS_SEND_TEMP_MEAS_NOTIF        (1 << 2)
#define HTS_MEAS_TIMER_NOTIF            (1 << 3)
#define CONN_IDLE_TIMER_NOTIF           (1 << 4)
#define SENSOR_NOTIF                    (1 << 5)
#define CLI_NOTIF                       (1 << 6)

/*
 * Masks of indications and notifications
 */
#define TEMP_MEAS_INDICATION_ENABLED            (1 << 1)
#define INTERM_TEMP_NOTIFICATION_ENABLED        (1 << 2)

/* Convert time in miliseconds to advertising interval value */
#define ADV_INTERVAL_MS(MS) ((MS) * 1000 / 625)

/* Task used by application */
PRIVILEGED_DATA static OS_TASK app_task;
/* Health Thermometer Service instance */
PRIVILEGED_DATA static ble_service_t *hts;
/* Timer counting elapsed time */
PRIVILEGED_DATA static OS_TIMER clock_timer;
/* Timer used to send periodical measurements */
PRIVILEGED_DATA static OS_TIMER hts_meas_timer;
/*
 * Timer used to terminate connection if it is idle for more than 5 seconds
 * (this timeout can be configured using #CFG_HTS_CONN_IDLE_TIME)
 */
PRIVILEGED_DATA static OS_TIMER conn_idle_timer;

/* Advertising modes of sensor app */
typedef enum {
        ADV_MODE_OFF,
        ADV_MODE_FAST_CONNECTION,
        ADV_MODE_REDUCED_POWER,
} adv_mode_t;
/* Current advertising mode */
PRIVILEGED_DATA static adv_mode_t adv_mode;
/* Timer used for advertising mode control */
PRIVILEGED_DATA static OS_TIMER adv_timer;

PRIVILEGED_DATA static struct {
        /* stored measurements queue */
        size_t head;
        size_t tail;
        hts_temp_measurement_t meas[CFG_HTS_MAX_MEAS_TO_STORE];

        /* indicated measurement data */
        bool indicating;
        const hts_temp_measurement_t *indicated_meas;
} measurement_data;
/* Temperature Measurement interval */
PRIVILEGED_DATA static uint16_t meas_interval;

/* Peer information */
PRIVILEGED_DATA static struct {
        /* Active connection index */
        uint16_t active_conn_idx;
        /* Status of enabled/disabled notifications */
        uint8_t notif_status;
        /* Peer is bonded */
        bool is_bonded;
} peer_info;

/*
 * HTP advertising and scan response data
 *
 * As per HTP specification, thermometer device should include HTS UUID in advertising data and
 * local name in either advertising data or scan response.
 */
static const uint8_t adv_data[] = {
        0x03, GAP_DATA_TYPE_UUID16_LIST_INC,
        0x09, 0x18, // = 0x1809 (HTS UUID)
};

static const uint8_t scan_rsp[] = {
        0x13, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'T', 'h', 'e', 'r', 'm', 'o', 'm', 'e','t', 'e', 'r'
};

/* Health Thermometer configuration */
static const hts_config_t hts_config = {
        .features = HTS_FEATURE_TEMPERATURE_TYPE |
                    HTS_FEATURE_INTERMEDIATE_TEMP |
                    HTS_FEATURE_MEASUREMENT_INTERVAL |
                    HTS_FEATURE_MEASUREMENT_INTERVAL_WRITABLE |
                    HTS_FEATURE_MEASUREMENT_INTERVAL_INDICATIONS,
        .type = HTS_TEMP_TYPE_GASTRO_TRACT,
        .init_interval = CFG_HTS_MEAS_INTERVAL,
        .interval_bound_low = CFG_HTS_MEAS_INTERVAL_LOW_BOUND,
        .interval_bound_high = CFG_HTS_MEAS_INTERVAL_HIGH_BOUND,
};

/*
 * \brief Device Information Service data
 *
 * Manufacturer Name String, Model Number String and System ID are mandatory for devices supporting
 * HTP.
 */
static const dis_system_id_t dis_sys_id = {
        .oui = { 0x80, 0xEA, 0xCA },    // Dialog Semiconductor Hellas SA
        .manufacturer = { 0x0A, 0x0B, 0x0C, 0x0D, 0x0E },
};

static const dis_device_info_t dis_info = {
        .manufacturer = "Dialog Semiconductor",
        .model_number = "Dialog BLE",
        .system_id = &dis_sys_id,
};

INITIALISED_PRIVILEGED_DATA static svc_date_time_t hts_time = {
        .year    = 1970,
        .month   = 1,
        .day     = 1,
        .hours   = 0,
        .minutes = 0,
        .seconds = 0,
};

static inline size_t get_measurement_count(void)
{
        if (measurement_data.tail >= measurement_data.head) {
                return measurement_data.tail - measurement_data.head;
        } else {
                return measurement_data.tail + CFG_HTS_MAX_MEAS_TO_STORE - measurement_data.head + 1;
        }
}

static void clock_timer_cb(OS_TIMER timer)
{
        hts_time.seconds++;
        if (hts_time.seconds > 59) {
                hts_time.seconds -= 60;
                hts_time.minutes += 1;
        }
        if (hts_time.minutes > 59) {
                hts_time.minutes -= 60;
                hts_time.hours += 1;
        }
        if (hts_time.hours > 23) {
                hts_time.hours -= 24;
        }
}

static void set_adv_mode(adv_mode_t mode)
{
        const char *mode_str;
        uint16_t intv_min = 0;
        uint16_t intv_max = 0;
        unsigned timeout = 0;

        /* If request mode is the same, just restart timer */
        if (mode == adv_mode) {
                OS_TIMER_START(adv_timer, OS_TIMER_FOREVER);
                return;
        }

        switch (mode) {
        case ADV_MODE_OFF:
                // leave all-zero
                mode_str = "off";
                break;
        case ADV_MODE_FAST_CONNECTION:
                intv_min = 20;
                intv_max = 30;
                timeout = 30000;
                mode_str = "fast connection";
                break;
        case ADV_MODE_REDUCED_POWER:
                intv_min = 1000;
                intv_max = 2500;
                timeout = 90000;
                mode_str = "reduced power";
                break;
        default:
                return;
        }

        /* Always try to stop advertising - we need to change parameters and then start again */
        OS_TIMER_STOP(adv_timer, OS_TIMER_FOREVER);
        ble_gap_adv_stop();

        /* If both min and max intervals are non-zero, set them and start advertising */
        if (intv_min && intv_max) {
                ble_gap_adv_intv_set(ADV_INTERVAL_MS(intv_min), ADV_INTERVAL_MS(intv_max));
                ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

                if (timeout) {
                        OS_TIMER_CHANGE_PERIOD(adv_timer, OS_MS_2_TICKS(timeout), OS_TIMER_FOREVER);
                        OS_TIMER_START(adv_timer, OS_TIMER_FOREVER);
                }
        }

        adv_mode = mode;

        printf("Advertising mode changed to *%s*\r\n", mode_str);
}

static void notif_timer_cb(OS_TIMER timer)
{
        uint32_t notif = OS_PTR_TO_UINT(OS_TIMER_GET_TIMER_ID(timer));

        OS_TASK_NOTIFY(app_task, notif, OS_NOTIFY_SET_BITS);
}

static void send_measurement(void);
static void complete_indication(bool success);

static void temp_meas_indication_changed_cb(uint16_t conn_idx, bool enabled);
static void temp_meas_indication_sent_cb(uint16_t conn_idx, bool success);
static void interm_temp_notification_changed_cb(uint16_t conn_idx, bool enabled);
static void interm_temp_notification_sent_cb(uint16_t conn_idx, bool success);
static void meas_interval_set_cb(uint16_t conn_idx, uint16_t interval);

/* Callbacks from HTS implementation */
static const hts_callbacks_t hts_cb = {
        .temp_meas_indication_changed = temp_meas_indication_changed_cb,
        .temp_meas_indication_sent = temp_meas_indication_sent_cb,
        .interm_temp_notification_changed = interm_temp_notification_changed_cb,
        .interm_temp_notification_sent = interm_temp_notification_sent_cb,
        .meas_interval_set = meas_interval_set_cb,
};

static void temp_meas_indication_changed_cb(uint16_t conn_idx, bool enabled)
{
        if (enabled) {
                peer_info.notif_status |= TEMP_MEAS_INDICATION_ENABLED;
                /*
                 * If there is something in the temperature measurement queue then some data were
                 * stored and should be sent to the user when the indications are enabled.
                 */
                send_measurement();
        } else {
                peer_info.notif_status &= ~TEMP_MEAS_INDICATION_ENABLED;
        }
}

static void reset_idle_timer(void)
{
        /*
         * Reset termination timer to inform that the connection is not in idle state.
         */
        if (OS_TIMER_IS_ACTIVE(conn_idle_timer)) {
                OS_TIMER_RESET(conn_idle_timer, OS_TIMER_FOREVER);
        }
}

static void temp_meas_indication_sent_cb(uint16_t conn_idx, bool success)
{
        complete_indication(success);

        reset_idle_timer();

        if (success) {
                printf("Measurement indicated successfully, %d in queue\r\n",
                                                                        get_measurement_count());
                send_measurement();
        } else {
                printf("Measurement not indicated successfully, %d in queue\r\n",
                                                                        get_measurement_count());
        }
}

static void interm_temp_notification_changed_cb(uint16_t conn_idx, bool enabled)
{
        if (enabled) {
                peer_info.notif_status |= INTERM_TEMP_NOTIFICATION_ENABLED;
        } else {
                peer_info.notif_status &= ~INTERM_TEMP_NOTIFICATION_ENABLED;
        }
}

static void interm_temp_notification_sent_cb(uint16_t conn_idx, bool success)
{
        reset_idle_timer();
}

static void temp_measurements_cb(void)
{
        OS_TASK_NOTIFY_FROM_ISR(app_task, HTS_MEAS_TIMER_NOTIF, OS_NOTIFY_SET_BITS);

#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        hw_wkup_reset_counter();
#endif
        hw_wkup_reset_interrupt();
}

static inline void enable_measurement_trigger_button(void)
{
        /*
         * Register wake-up interrupt for handling button pressing action. It will be used for
         * generating non-periodic measurements.
         */
        hw_wkup_register_interrupt(temp_measurements_cb, 1);
}

static inline void disable_measurement_trigger_button(void)
{
        /*
         * Unregistering wake-up interrupt means that button will be disabled and non-periodic
         * measurements will be not generated.
         */
        hw_wkup_unregister_interrupt();
}

static void remove_oldest_measurement(void)
{
        /* need to remove reference to indicated measurement in case it's removed from queue */
        if (&measurement_data.meas[measurement_data.head] == measurement_data.indicated_meas) {
                measurement_data.indicated_meas = NULL;
        }

        measurement_data.head++;

        if (measurement_data.head >= CFG_HTS_MAX_MEAS_TO_STORE) {
                measurement_data.head = 0;
        }
}

static void store_new_measurement(const hts_temp_measurement_t *meas)
{
        measurement_data.meas[measurement_data.tail++] = *meas;

        if (measurement_data.tail >= CFG_HTS_MAX_MEAS_TO_STORE) {
                measurement_data.tail = 0;
        }

        /* Need to remove oldest measurement if the queue is full */
        if (measurement_data.head == measurement_data.tail) {
                remove_oldest_measurement();
        }

        printf("Measurement stored, %d in queue\r\n", get_measurement_count());
}

static bool has_pending_measurement(void)
{
        return measurement_data.head != measurement_data.tail;
}

static const hts_temp_measurement_t *setup_next_indication(void)
{
        /* Indication is pending, disallow another one */
        if (measurement_data.indicating) {
                return NULL;
        }

        /* Check for pending measurements in the queue */
        if (!has_pending_measurement()) {
                return NULL;
        }

        measurement_data.indicating = true;
        measurement_data.indicated_meas = &measurement_data.meas[measurement_data.head];

        return measurement_data.indicated_meas;
}

static void complete_indication(bool success)
{
        if (success && (&measurement_data.meas[measurement_data.head] ==
                                                                measurement_data.indicated_meas)) {
                remove_oldest_measurement();
        }

        measurement_data.indicating = false;
        measurement_data.indicated_meas = NULL;
}

static void send_measurement(void)
{
        const hts_temp_measurement_t *meas;

        if (peer_info.active_conn_idx == BLE_CONN_IDX_INVALID || !peer_info.is_bonded) {
                return;
        }

        /* Check if Temperature Measurement Indications are enabled */
        if (!(peer_info.notif_status & TEMP_MEAS_INDICATION_ENABLED)) {
                return;
        }

        /* Retrieve measurement from queue which should be indicated now, if any */
        meas = setup_next_indication();
        if (!meas) {
                return;
        }

        if (!hts_indicate_temperature(hts, peer_info.active_conn_idx, meas)) {
                complete_indication(false);
        }
}

static void handle_sensor_notif(void)
{
        sensor_event_t event;
        hts_temp_measurement_t meas;

        memset(&meas, 0, sizeof(meas));
        meas.unit = HTS_TEMP_UNIT_CELSIUS;

        while (sensor_get_event(&event)) {
                meas.temperature = event.value;

                /* Send intermediate results immediately (or discard) */
                if (event.type == SENSOR_EVENT_INTERMEDIATE) {
                        if (peer_info.active_conn_idx != BLE_CONN_IDX_INVALID &&
                                                                        peer_info.is_bonded) {
                                hts_notify_interm_temperature(hts, peer_info.active_conn_idx, &meas);
                        }
                        continue;
                }

                if (event.type == SENSOR_EVENT_MEASUREMENT) {
                        printf("Measurement done\r\n");
                        printf("\tValue.Mantissa: %ld\r\n", event.value.mantissa);
                        printf("\tValue.Exponent: %d\r\n", event.value.exp);
                } else {
                        printf("Measurement failed\r\n");
                }

                /*
                 * Even if the measurement was not taken successfully, the result from the sensor
                 * should be NaN so it can still be send over the air.
                 */
                meas.has_time_stamp = true;
                meas.time_stamp = hts_time;
                store_new_measurement(&meas);

                if ((peer_info.active_conn_idx == BLE_CONN_IDX_INVALID) &&
                                                                        adv_mode == ADV_MODE_OFF) {
                        set_adv_mode(ADV_MODE_FAST_CONNECTION);
                } else {
                        send_measurement();
                }
        }
}

static void hts_terminate_connection(void)
{
        OS_TIMER_STOP(conn_idle_timer, OS_TIMER_FOREVER);

        printf("Connection is idle for %d seconds\r\n", CFG_HTS_CONN_IDLE_TIME);

        printf("Disconnecting from client (connection index: %d)\r\n", peer_info.active_conn_idx);
        ble_gap_disconnect(peer_info.active_conn_idx, BLE_HCI_ERROR_REMOTE_USER_TERM_CON);

        printf("Stop advertising\r\n");
        set_adv_mode(ADV_MODE_OFF);
}

att_error_t handle_interval_value(uint16_t interval)
{
        uint8_t ret;

        if (interval == 0) {
                OS_TIMER_STOP(hts_meas_timer, OS_TIMER_FOREVER);

                enable_measurement_trigger_button();
        } else {
                disable_measurement_trigger_button();

                ret = OS_TIMER_CHANGE_PERIOD(hts_meas_timer, OS_MS_2_TICKS(1000 * interval),
                                                                                OS_TIMER_FOREVER);

                if (ret != OS_TIMER_SUCCESS) {
                        return ATT_ERROR_APPLICATION_ERROR;
                }

                OS_TIMER_START(hts_meas_timer, OS_TIMER_FOREVER);
        }

        if (meas_interval != interval) {
                hts_set_measurement_interval(hts, interval);
                meas_interval = interval;
                hts_indicate_measurement_interval(hts, peer_info.active_conn_idx);
        }

        return ATT_ERROR_OK;
}

static void meas_interval_set_cb(uint16_t conn_idx, uint16_t interval)
{
        att_error_t status;

        status = handle_interval_value(interval);

        hts_set_meas_interval_cfm(hts, conn_idx, status);
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        ble_error_t status;
        gap_device_t gap_device;

        printf("Device connected\r\n");
        printf("Connection index: %d\r\n", evt->conn_idx);
        printf("Address: %s\r\n", ble_address_to_string(&evt->peer_address));

        peer_info.active_conn_idx = evt->conn_idx;
        ble_gap_pair(evt->conn_idx, true);

        status = ble_gap_get_device_by_conn_idx(evt->conn_idx, &gap_device);

        if (status == BLE_STATUS_OK) {
                peer_info.is_bonded = gap_device.bonded;
        }

        OS_TIMER_START(conn_idle_timer, OS_TIMER_FOREVER);
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        printf("Device disconnected\r\n");
        printf("Connection index: %d\r\n", evt->conn_idx);
        printf("Address: %s\r\n", ble_address_to_string(&evt->address));
        printf("Reason: %d\r\n", evt->reason);

        if (evt->conn_idx != peer_info.active_conn_idx) {
                return;
        }

        if (OS_TIMER_IS_ACTIVE(conn_idle_timer)) {
                OS_TIMER_STOP(conn_idle_timer, OS_TIMER_FOREVER);
        }

        if (has_pending_measurement()) {
                set_adv_mode(ADV_MODE_FAST_CONNECTION);
        }

        /* mark indication as failed, if any pending */
        complete_indication(false);

        peer_info.active_conn_idx = BLE_CONN_IDX_INVALID;
}

static void handle_evt_gap_adv_completed(ble_evt_gap_adv_completed_t *evt)
{
        /* Update mode if advertising is called due to something else than our stop request */
        if (evt->status != BLE_ERROR_CANCELED) {
                set_adv_mode(ADV_MODE_OFF);
        }
}

static void handle_evt_gap_pair_req(ble_evt_gap_pair_req_t *evt)
{
        printf("Pair request\r\n");
        printf("Connection Index: %d\r\n", evt->conn_idx);
        printf("Bond: %d\r\n", evt->bond);

        peer_info.is_bonded = evt->bond;
        ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
}

static void handle_adv_timer_notif(void)
{
        switch (adv_mode) {
        case ADV_MODE_OFF:
                /* ignore */
                break;
        case ADV_MODE_FAST_CONNECTION:
                set_adv_mode(ADV_MODE_REDUCED_POWER);
                break;
        case ADV_MODE_REDUCED_POWER:
                set_adv_mode(ADV_MODE_OFF);
                break;
        default:
                OS_ASSERT(0);
                return;
        }
}

static void setup_timers(void)
{
        /*
         * Create timer for controlling advertising mode. We need to set any non-zero period
         * (i.e. 1) but this will be changed later, when timer is started.
         */
        adv_timer = OS_TIMER_CREATE("adv", /* don't care */ 1, OS_TIMER_FAIL,
                                                OS_UINT_TO_PTR(ADV_TIMER_NOTIF), notif_timer_cb);
        OS_ASSERT(adv_timer);

        /*
         * Create timer for virtual clock, this will be used to update current time every second
         */
        clock_timer = OS_TIMER_CREATE("clock", OS_MS_2_TICKS(1000), OS_TIMER_SUCCESS,
                                                OS_UINT_TO_PTR(app_task), clock_timer_cb);
        OS_ASSERT(clock_timer);

        /*
         * Create timer which terminates the connection if it is idle for more than 5 seconds
         * (e.g. 30 seconds - this can be changed by setting #CFG_HTS_CONN_IDLE_TIME)
         */
        conn_idle_timer = OS_TIMER_CREATE("conn_idle", OS_MS_2_TICKS(CFG_HTS_CONN_IDLE_TIME * 1000),
                                                OS_TIMER_SUCCESS,
                                                        OS_UINT_TO_PTR(CONN_IDLE_TIMER_NOTIF),
                                                                                notif_timer_cb);
        OS_ASSERT(conn_idle_timer);

        /*
         * Create timer for HTS to send periodic measurements. Set any value now, it will be
         * adjusted later when starting timer depending on interval set (it may be zero now).
         */
        hts_meas_timer = OS_TIMER_CREATE("hts_meas", /* don't care */ 1, OS_TIMER_SUCCESS,
                                                        OS_UINT_TO_PTR(HTS_MEAS_TIMER_NOTIF),
                                                                                notif_timer_cb);
        OS_ASSERT(hts_meas_timer);

        /* Start timers which should be running from the beginning */
        OS_TIMER_START(clock_timer, OS_TIMER_FOREVER);
        if (hts_config.init_interval != 0) {
                OS_TIMER_CHANGE_PERIOD(hts_meas_timer,
                                                OS_MS_2_TICKS(hts_config.init_interval * 1000),
                                                                                OS_TIMER_FOREVER);
                OS_TIMER_START(hts_meas_timer, OS_TIMER_FOREVER);
        }
}

void htp_thermometer_task(void *params)
{
        cli_t cli;
        int8_t wdog_id;
        static const ble_service_config_t service_config = {
                .service_type = GATT_SERVICE_PRIMARY,
                .sec_level = GAP_SEC_LEVEL_2,
                .num_includes = 0,
        };

        peer_info.active_conn_idx = BLE_CONN_IDX_INVALID;

        /* Register CLI command handlers */
        cli = register_command_handlers(CLI_NOTIF);

        /* Store application task handle for task notifications */
        app_task = OS_GET_CURRENT_TASK();

        printf("Health Thermometer application started\r\n");

        /* Register htp_thermometer_task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        /* Start BLE device as peripheral */
        ble_peripheral_start();

        /* Register task to BLE framework to receive BLE event notifications */
        ble_register_app();

        /* Set device name, advertising/scan response data and IO capabilities */
        ble_gap_device_name_set("Dialog Thermometer", ATT_PERM_READ);
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, sizeof(scan_rsp), scan_rsp);

        /* Add Health Thermometer Service */
        hts = hts_init(&service_config, &hts_config, &hts_cb);
        OS_ASSERT(hts);

        /* Add Device Information Service */
        dis_init(&service_config, &dis_info);

        /* Setup various timers */
        setup_timers();

        /*
         * For non-periodic measurements, the K1 button will be used to generate and send dummy
         * measurements, otherwise a timer routine will be doing that periodically with predefined
         * interval value.
         */
        if (hts_config.init_interval == 0) {
                enable_measurement_trigger_button();
        }

        /* Initialize temperature sensor */
        sensor_init(SENSOR_NOTIF);

        /* Start advertising */
        set_adv_mode(ADV_MODE_FAST_CONNECTION);

        for (;;) {
                BaseType_t ret;
                uint32_t notif;

                /* Notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);

                /* Suspend watchdog while blocking on OS_TASK_NOTIFY_WAIT() */
                sys_watchdog_suspend(wdog_id);

                /*
                 * Wait on any of the notification bits, then clear them all
                 */
                ret = OS_TASK_NOTIFY_WAIT(0, (uint32_t) -1, &notif, OS_TASK_NOTIFY_FOREVER);
                /* Blocks forever waiting for the task notification. Therefore, the return value must
                 * always be OS_OK
                 */
                OS_ASSERT(ret == OS_OK);

                /* Resume watchdog */
                sys_watchdog_resume(wdog_id);

                /* Notified from BLE Manager? */
                if (notif & BLE_APP_NOTIFY_MASK) {
                        ble_evt_hdr_t *hdr;

                        hdr = ble_get_event(false);
                        if (!hdr) {
                                goto no_event;
                        }

                        /*
                         * The application will first attempt to handle the event using the
                         * BLE service framework.
                         * If no handler is specified in the BLE service framework, the
                         * event may be handled in the switch statement that follows.
                         * If no handler is specified in the switch statement, the event will be
                         * handled by the default event handler.
                         */
                        if (!ble_service_handle_event(hdr)) {
                                switch (hdr->evt_code) {
                                case BLE_EVT_GAP_CONNECTED:
                                        handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_DISCONNECTED:
                                        handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_ADV_COMPLETED:
                                        handle_evt_gap_adv_completed((ble_evt_gap_adv_completed_t *) hdr);
                                        break;
                                case BLE_EVT_GAP_PAIR_REQ:
                                        handle_evt_gap_pair_req((ble_evt_gap_pair_req_t *) hdr);
                                        break;
                                default:
                                        ble_handle_event_default(hdr);
                                        break;
                                }
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

                if (notif & ADV_TIMER_NOTIF) {
                        handle_adv_timer_notif();
                }

                if (notif & HTS_MEAS_TIMER_NOTIF) {
                        if (meas_interval != 0) {
                                disable_measurement_trigger_button();
                        } else {
                                enable_measurement_trigger_button();
                        }

                        sensor_do_measurement();

                        printf("Measurement started\r\n");

                }

                if (notif & CONN_IDLE_TIMER_NOTIF) {
                        hts_terminate_connection();
                }

                if (notif & SENSOR_NOTIF) {
                        handle_sensor_notif();
                }

                if (notif & CLI_NOTIF) {
                        cli_handle_notified(cli);
                }
        }
}
