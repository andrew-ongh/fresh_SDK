/**
 ****************************************************************************************
 *
 * @file blp_sensor_task.c
 *
 * @brief Blood Pressure Sensor Profile task
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include "osal.h"
#include "hw_wkup.h"
#include "sys_watchdog.h"
#include "ble_common.h"
#include "ble_gap.h"
#include "ble_service.h"
#include "util/queue.h"
#include "bls.h"
#include "dis.h"
#include "sensor.h"
#include "blp_sensor_config.h"

/* Notification from advertising mode control timer */
#define ADV_TIMER_NOTIF                         (1 << 1)
#define BLS_MEASUREMENT_NOTIF                   (1 << 2)
#define SENSOR_NOTIF                            (1 << 3)

/* Advertising modes of BLP Sensor application */
typedef enum {
        ADV_MODE_OFF,
        ADV_MODE_FAST_CONNECTION,
        ADV_MODE_REDUCED_POWER,
} adv_mode_t;

/*
 * BLP advertising and scan response data
 */
static const uint8_t adv_data[] = {
        0x03, GAP_DATA_TYPE_UUID16_LIST_INC,
        0x10, 0x18, // = 0x1810 (Blood Pressure Service UUID)
};

static const uint8_t scan_rsp[] = {
        0x12, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'B', 'L', 'P', ' ', 'S', 'e', 'n', 's', 'o', 'r',
};

/* Callbacks from BLS implementation */
static void meas_indication_changed_cb(ble_service_t *service, uint16_t conn_idx, bool enabled);
static void meas_indication_sent_cb(uint16_t conn_idx, bool status);
static void intem_cuff_pressure_notif_changed_cb(ble_service_t *service, uint16_t conn_idx,
                                                                                bool enabled);

static const bls_callbacks_t bls_cb = {
        .meas_indication_changed                = meas_indication_changed_cb,
        .meas_indication_sent                   = meas_indication_sent_cb,
        .interm_cuff_pressure_notif_changed     = intem_cuff_pressure_notif_changed_cb,
        .interm_cuff_pressure_notif_sent        = NULL,
};

static const ble_service_config_t bls_service_config = {
        .service_type                           = GATT_SERVICE_PRIMARY,
        .sec_level                              = GAP_SEC_LEVEL_2,
        .num_includes                           = 0,
        .includes                               = NULL,
};

static const dis_device_info_t dis_config = {
        .manufacturer                           = "Dialog Semiconductor",
        .model_number                           = "Dialog BLE",
};

static const bls_config_t bls_config = {
        .feature_supp   = BLS_FEATURE_BODY_MOVEMENT_DETECTION |
                          BLS_FEATURE_CUFF_FIT_DETECTION |
                          BLS_FEATURE_IRREGULAR_PULSE_DETECTION |
                          BLS_FEATURE_PULSE_RATE_RANGE_DETECTION |
                          BLS_FEATURE_MEASUREMENT_POS_DETECTION,
        .supported_char = BLS_SUPPORTED_CHAR_INTERM_CUFF_PRESSURE,
};

/*
 * Queue element for storing measurements
 */
typedef struct {
        void                            *next;
        bls_measurement_t               measurement;
} measurement_t;

/*
 * Structure for all information about BLP sensor
 */
typedef struct {
        /* Connection index */
        uint16_t                        conn_idx;
        /* BLP Sensor is bonded */
        bool                            is_bonded;
        /* Queue of blood pressure measurements */
        queue_t                         measurements;
        /* Measurement sending is in progress */
        bool                            is_sending;
        /* Current time */
        svc_date_time_t                 blp_time;
} blp_sensor_info_t;

/* Task used by application */
PRIVILEGED_DATA static OS_TASK                  app_task;
/* Blood Pressure Service instance */
PRIVILEGED_DATA static ble_service_t            *bls;
/* Timer used to update current time */
PRIVILEGED_DATA static OS_TIMER                 blp_timer;
/* Timer used for advertising mode control */
PRIVILEGED_DATA static OS_TIMER                 adv_timer;
/* Current advertising mode */
PRIVILEGED_DATA static adv_mode_t               adv_mode;
/* BLP sensor info */
INITIALISED_PRIVILEGED_DATA blp_sensor_info_t   blp_sensor_info = {
        .blp_time = {
                .year    = 2017,
                .month   = 6,
                .day     = 9,
                .hours   = 12,
                .minutes = 30,
                .seconds = 0,
        },
};

static void set_adv_mode(adv_mode_t mode)
{
        uint16_t intv_min = 0;
        uint16_t intv_max = 0;
        unsigned timeout = 0;

        /* If request mode is the same, just restart timer */
        if (mode == adv_mode) {
                OS_TIMER_START(adv_timer, OS_TIMER_FOREVER);
                return;
        }

        printf("Set advertising mode: ");

        switch (mode) {
        case ADV_MODE_OFF:
                /* Leave all-zero */
                printf("Off\r\n");
                break;
        case ADV_MODE_FAST_CONNECTION:
                intv_min = 20;
                intv_max = 30;
                timeout = 30000;
                printf("Fast mode\r\n");
                break;
        case ADV_MODE_REDUCED_POWER:
                intv_min = 1000;
                intv_max = 2500;
                printf("Reduced power mode\r\n");
                break;
        default:
                return;
        }

        /* Always try to stop advertising - we need to change parameters and then start again */
        OS_TIMER_STOP(adv_timer, OS_TIMER_FOREVER);
        ble_gap_adv_stop();

        /* If both min and max intervals are non-zero, set them and start advertising */
        if (intv_min && intv_max) {
                ble_gap_adv_intv_set(BLE_ADV_INTERVAL_FROM_MS(intv_min),
                                                               BLE_ADV_INTERVAL_FROM_MS(intv_max));
                ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

                if (timeout) {
                        OS_TIMER_CHANGE_PERIOD(adv_timer, OS_MS_2_TICKS(timeout), OS_TIMER_FOREVER);
                        OS_TIMER_START(adv_timer, OS_TIMER_FOREVER);
                }
        }

        adv_mode = mode;
}

static void adv_timer_cb(OS_TIMER timer)
{
        OS_TASK task = (OS_TASK) OS_TIMER_GET_TIMER_ID(timer);

        OS_TASK_NOTIFY(task, ADV_TIMER_NOTIF, OS_NOTIFY_SET_BITS);
}

static void blp_timer_cb(OS_TIMER timer)
{
        blp_sensor_info.blp_time.seconds++;

        if (blp_sensor_info.blp_time.seconds > 59) {
                blp_sensor_info.blp_time.seconds = 0;
                blp_sensor_info.blp_time.minutes++;
        }
        if (blp_sensor_info.blp_time.minutes > 59) {
                blp_sensor_info.blp_time.minutes = 0;
                blp_sensor_info.blp_time.hours++;
        }
        if (blp_sensor_info.blp_time.hours > 23) {
                blp_sensor_info.blp_time.hours = 0;
        }
}

static void handle_adv_timer_notif(void)
{
        if (adv_mode == ADV_MODE_FAST_CONNECTION) {
                set_adv_mode(ADV_MODE_REDUCED_POWER);
        }
}

void button_interrupt_cb(void)
{
        OS_TASK_NOTIFY(app_task, BLS_MEASUREMENT_NOTIF, OS_NOTIFY_SET_BITS);
}

static char *ieee11703_to_string(const svc_ieee11073_float_t *value)
{
        static char buf[10];

        int mantissa = value->mantissa;
        int exp = value->exp;

        int decimal = mantissa;
        int rest = 0;
        int rest_index = 1;

        while (exp > 0) {
            exp--;
            decimal *= 10;
        }

        while (exp < 0) {
            exp++;
            rest += rest_index * (decimal % 10);
            rest_index *= 10;
            decimal /= 10;
        }

        sprintf(buf, "%d.%d", decimal, rest);

        return buf;
}

static void print_measurement_info(bls_measurement_t *measurement, bool is_sent)
{
        printf("Blood Pressure Measurement\r\n");
        printf("\tUnit: %s\r\n", measurement->unit ? "kPa" : "mm Hg");
        printf("\tSystolic: %s\r\n", ieee11703_to_string(&measurement->pressure_systolic));
        printf("\tDiastolic: %s\r\n", ieee11703_to_string(&measurement->pressure_diastolic));
        printf("\tMAP: %s\r\n", ieee11703_to_string(&measurement->pressure_map));

        if (measurement->pulse_rate_present) {
                printf("\tPulse: %s\r\n", ieee11703_to_string(&measurement->pulse_rate));
        }

        printf("\tStatus: %s\r\n", is_sent ? "sent" : "stored");
        printf("\tStored measurements: %d\r\n", queue_length(&blp_sensor_info.measurements));
}

static bool send_blood_pressure_measurement_to_collector(bool print_measurement_if_not_sent)
{
        measurement_t *measurement;

        if (!queue_length(&blp_sensor_info.measurements)) {
                return false;
        }

        measurement = queue_peek_front(&blp_sensor_info.measurements);

        if (blp_sensor_info.conn_idx == BLE_CONN_IDX_INVALID || !blp_sensor_info.is_bonded) {
                goto measurement_not_sent;
        }

        if (blp_sensor_info.is_sending || !bls_indicate_pressure_measurement(bls,
                                        blp_sensor_info.conn_idx, &measurement->measurement)) {
                goto measurement_not_sent;
        }

        print_measurement_info(&measurement->measurement, true);
        blp_sensor_info.is_sending = true;

        return true;

measurement_not_sent:

        if (print_measurement_if_not_sent) {
                print_measurement_info(&measurement->measurement, false);
        }

        return false;
}

static bool send_intermediate_cuff_pressure_to_collector(bls_measurement_t *measurement)
{
        if (blp_sensor_info.conn_idx == BLE_CONN_IDX_INVALID || !blp_sensor_info.is_bonded) {
                return false;
        }

        if (!bls_notify_intermediate_cuff_pressure(bls, blp_sensor_info.conn_idx, measurement)) {
                return false;
        }

        return true;
}

static void store_measurement(bls_measurement_t *bls_measurement)
{
        measurement_t *measurement;

        measurement = OS_MALLOC(sizeof(*measurement));
        memcpy(&measurement->measurement, bls_measurement, sizeof(*bls_measurement));

        if (queue_length(&blp_sensor_info.measurements) >= CFG_MAX_USER_MEASURMENTS_COUNT) {
                measurement_t *oldest_maesurement;

                oldest_maesurement = queue_pop_front(&blp_sensor_info.measurements);
                OS_FREE(oldest_maesurement);
        }

        queue_push_back(&blp_sensor_info.measurements, measurement);
}

static void remove_measurement(void)
{
        measurement_t *measurement;

        measurement = queue_pop_front(&blp_sensor_info.measurements);

        if (measurement) {
                OS_FREE(measurement);
        }
}

static void handle_sensor_event_intermediate(bls_measurement_t *measurement)
{
        printf("Intermediate Cuff Pressure\r\n");
        printf("\tUnit: %s\r\n", measurement->unit ? "kPa" : "mm Hg");
        printf("\tSystolic: %s\r\n", ieee11703_to_string(&measurement->pressure_systolic));

        if (send_intermediate_cuff_pressure_to_collector(measurement)) {
                printf("\tStatus: sent\r\n");
        } else {
                printf("\tStatus: not sent\r\n");
        }
}

static void handle_sensor_event_measurement(bls_measurement_t *measurement)
{
        store_measurement(measurement);

        send_blood_pressure_measurement_to_collector(true);
}

static void add_time_stamp_to_measurement(bls_measurement_t *measurement)
{
        measurement->time_stamp_present = true;
        measurement->time_stamp = blp_sensor_info.blp_time;
}

static void handle_sensor_notif(void)
{
        sensor_event_t event;

        while (sensor_get_event(&event)) {
                add_time_stamp_to_measurement(&event.value);

                switch (event.type) {
                case SENSOR_EVENT_INTERMEDIATE:
                        handle_sensor_event_intermediate(&event.value);
                        break;
                case SENSOR_EVENT_MEASUREMENT:
                        handle_sensor_event_measurement(&event.value);
                        break;
                default:
                        printf("Measurement failed\r\n");
                        break;
                }
        }
}

static void meas_indication_changed_cb(ble_service_t *service, uint16_t conn_idx, bool enabled)
{
        printf("Blood Pressure Measurement indications are %s\r\n", enabled ? "enabled" :
                                                                                        "disabled");

        if (!enabled) {
                return;
        }

        send_blood_pressure_measurement_to_collector(false);
}

static void meas_indication_sent_cb(uint16_t conn_idx, bool status)
{
        printf("Blood Pressure Measurement Delivered\r\n");
        printf("\tConnection index: %d\r\n", conn_idx);
        printf("\tStatus: %s\r\n", status ? "success": "fail");

        blp_sensor_info.is_sending = false;

        if (status) {
                remove_measurement();
        }

        printf("\tStored measurements: %d\r\n", queue_length(&blp_sensor_info.measurements));

        if (status) {
                send_blood_pressure_measurement_to_collector(false);
        }
}

static void intem_cuff_pressure_notif_changed_cb(ble_service_t *service, uint16_t conn_idx,
                                                                                bool enabled)
{
        printf("Intermediate Cuff Pressure notifications are %s\r\n", enabled ? "enabled" :
                                                                                        "disabled");
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        ble_error_t status;
        gap_device_t gap_device;

        ble_gap_pair(evt->conn_idx, true);

        printf("Device connected\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tAddress: %s\r\n", ble_address_to_string(&evt->peer_address));

        blp_sensor_info.conn_idx = evt->conn_idx;

        status = ble_gap_get_device_by_conn_idx(evt->conn_idx, &gap_device);

        if (status == BLE_STATUS_OK) {
                blp_sensor_info.is_bonded = gap_device.bonded;
        }

}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        printf("Device disconnected\r\n");
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tAddress: %s\r\n", ble_address_to_string(&evt->address));
        printf("\tReason: %d\r\n", evt->reason);

        set_adv_mode(ADV_MODE_FAST_CONNECTION);

        blp_sensor_info.conn_idx = BLE_CONN_IDX_INVALID;
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
        printf("\tConnection index: %d\r\n", evt->conn_idx);
        printf("\tBond: %d\r\n", evt->bond);

        blp_sensor_info.is_bonded = evt->bond;
        ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
}

static void create_timers(void)
{
        /*
         * Create timer for controlling advertising mode. We need to set any non-zero period (i.e. 1)
         * but this will be changed later, when timer is started.
         */
        adv_timer = OS_TIMER_CREATE("adv", /* don't care */ 1, OS_TIMER_FAIL,
                                                        OS_GET_CURRENT_TASK(), adv_timer_cb);
        OS_ASSERT(adv_timer);

        /* Create timer for BLP to update current time every second */
        blp_timer = OS_TIMER_CREATE("blp_timer", OS_MS_2_TICKS(1000), OS_TIMER_SUCCESS, NULL,
                                                                                blp_timer_cb);
        OS_ASSERT(blp_timer);
        OS_TIMER_START(blp_timer, 0);
}

void blp_sensor_task(void *params)
{
        int8_t wdog_id;

        /* Store application task handle for task notifications */
        app_task = OS_GET_CURRENT_TASK();

        /* Register blp_sensor_task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        /* Start BLE device as peripheral */
        ble_peripheral_start();

        /* Register task to BLE framework to receive BLE event notifications */
        ble_register_app();

        /* Set device name */
        ble_gap_device_name_set("Dialog BLP Sensor", ATT_PERM_READ);

        /* Add DIS */
        dis_init(&bls_service_config, &dis_config);

        /* Add BLS */
        bls = bls_init(&bls_service_config, &bls_config, &bls_cb);

        /* Create various timers */
        create_timers();

        /* Set advertising and scan response data */
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, sizeof(scan_rsp), scan_rsp);

        /* Initialize sensor */
        sensor_init(SENSOR_NOTIF);

        /* No collector has connected yet */
        blp_sensor_info.conn_idx = BLE_CONN_IDX_INVALID;

        printf("Blood Pressure Sensor application started\r\n");

        /* Start advertising */
        set_adv_mode(ADV_MODE_FAST_CONNECTION);

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                /* Notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);

                /* Suspend watchdog while blocking on OS_TASK_NOTIFY_WAIT() */
                sys_watchdog_suspend(wdog_id);

                /*
                 * Wait on any of the notification bits, then clear them all
                 */
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif,
                                                                        OS_TASK_NOTIFY_FOREVER);
                /* Blocks forever waiting for the task notification. Therefore, the return value must
                 * always be OS_OK
                 */
                OS_ASSERT(ret == OS_OK);

                /* Resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);

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

                /* Notified from advertising timer? */
                if (notif & ADV_TIMER_NOTIF) {
                        handle_adv_timer_notif();
                }

                /* Notified from BLS measurement timer? */
                if (notif & BLS_MEASUREMENT_NOTIF) {
                        /* Get measurements from blood pressure sensor */
                        sensor_do_measurement();
                }

                /* Notified from sensor? */
                if (notif & SENSOR_NOTIF) {
                        handle_sensor_notif();
                }
        }
}
