/**
 ****************************************************************************************
 *
 * @file wsp_weightscale_task.c
 *
 * @brief Weight Scale Profile / Weight Scale role demo application
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string.h>
#include "ad_nvms.h"
#include "osal.h"
#include "util/queue.h"
#include "sys_watchdog.h"
#include "hw_wkup.h"
#include "ble_common.h"
#include "ble_service.h"
#include "ble_uuid.h"
#include "wss.h"
#include "dis.h"
#include "bcs.h"
#include "bas.h"
#include "cts.h"
#include "uds.h"
#include "dlg_debug.h"
#include "wsp_weightscale_config.h"
#include "cli.h"

enum NOTIF_TYPE {
        WSS_NOTIF_EN    = 0x01,
        BCS_NOTIF_EN    = 0x02,
};

/*
 * Notification bits reservation
 *
 * Bit #0 is always assigned to BLE event queue notification.
 */
#define WSS_MEASUREMENT_NOTIF (1 << 2)
#define BCS_MEASUREMENT_NOTIF (1 << 3)
#define CLI_NOTIF             (1 << 4)

#define CONSENT_MAX_RETRIES_VAL 3

#define CONFIG_WSP_STORAGE_OFFSET (0x00001000)
#define FIRST_NAME_MAX_LEN (32)

PRIVILEGED_DATA static nvms_t nvms;

/*
 * Magic value to identify that partition area contains valid WSP data
 */
static const uint8_t STORAGE_MAGIC[] = { 'W', 'S', 'P', '_', 'K', 'E', 'Y', 0x00 };

static OS_TASK current_task;
static OS_TIMER bcs_meas_tmr;
PRIVILEGED_DATA static ble_service_t *uds;
PRIVILEGED_DATA static ble_service_t *wss;
PRIVILEGED_DATA static ble_service_t *bcs;
PRIVILEGED_DATA static ble_service_t *bas;

/*
 * WSP advertising and scan response data
 *
 * As per WSP specification, Weight Scale should include WSS UUID in advertising data and local name
 * and appearance in either advertising data or scan response.
 */
static const uint8_t adv_data[] = {
        0x03, GAP_DATA_TYPE_UUID16_LIST_INC,
        0x1D, 0x18, // = 0x181D (WSS UUID)
        0x03, GAP_DATA_TYPE_APPEARANCE,
        BLE_GAP_APPEARANCE_GENERIC_WEIGHT_SCALE & 0xFF,
        BLE_GAP_APPEARANCE_GENERIC_WEIGHT_SCALE >> 8,
};

static const uint8_t scan_rsp[] = {
        0x14, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'W', 'e', 'i', 'g', 'h', 't', ' ', 'S', 'c', 'a', 'l', 'e'
};

/*
 * Device Information Service data
 *
 * Manufacturer Name String and Model Number String are mandatory for devices supporting WSP.
 */
static const dis_device_info_t dis_info = {
        .manufacturer = "Dialog Semiconductor",
        .model_number = "Dialog BLE",
};

/*
 * Weight Scale Service data
 */
static void wss_indication_changed(uint16_t conn_idx, bool enabled);
static void wss_indication_sent(uint16_t conn_idx, bool success);

/* Callbacks from WSS implementation */
static const wss_callbacks_t wss_cb = {
        .indication_changed = wss_indication_changed,
        .indication_sent = wss_indication_sent,
};

/*
 * Body Composition Service data
 */
static void bcs_indication_changed(uint16_t conn_idx, bool enabled);
static void bcs_indication_sent(uint16_t conn_idx, bool success);

/* Callbacks from BCS implementation */
static const bcs_callbacks_t bcs_cb = {
        .indication_changed = bcs_indication_changed,
        .indication_sent = bcs_indication_sent,
};

/*
 * Current Time Service data
 */
PRIVILEGED_DATA static OS_TIMER cts_timer;

static void cts_get_time_cb(ble_service_t *svc, uint16_t conn_idx);
static void cts_set_time_cb(ble_service_t *svc, uint16_t conn_idx, const cts_current_time_t *time);
static void cts_set_local_time_info_cb(ble_service_t *svc, uint16_t conn_idx,
                                                                const cts_local_time_info_t *info);
static void cts_get_ref_time_info_cb(ble_service_t *svc, uint16_t conn_idx);

typedef void (* user_data_callback_t) (int argc, const char *argv, uint8_t id);

/* Callbacks from CTS implementation */
static const cts_callbacks_t cts_cb = {
        .get_time = cts_get_time_cb,
        .set_time = cts_set_time_cb,
        .set_local_time_info = cts_set_local_time_info_cb,
        .get_ref_time_info = cts_get_ref_time_info_cb,
};

typedef enum {
        UDS_GENDER_MALE,
        UDS_GENDER_FEMALE,
        UDS_GENDER_UNSPECIFIED,
} uds_gender_t;

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

/*
 * User Data Service data
 */
typedef struct {
        uint8_t day;
        uint8_t month;
        uint16_t year;
} uds_date_t;

typedef struct {
        bool used;
        uint16_t consent;
        uint8_t consent_retries;
        uint32_t db_increment;

        char *first_name;
        uint8_t age;
        uds_date_t date_of_birth;
        uds_gender_t gender;
        uint16_t height;

        queue_t measurements;
} user_data_t;

typedef enum {
        WSP_MEASUREMENT_TYPE_BCS,
        WSP_MEASUREMENT_TYPE_WSS,
} wsp_measurement_type_t;

typedef struct {
        void *next;

        wsp_measurement_type_t type;
        bool sending_in_progress;
        uint16_t conn_idx;

        union {
                bcs_body_measurement_t bcs_measurement;
                wss_weight_measurement_t wss_measurement;
        } measurement;
} wsp_measurement_t;

typedef struct {
        bool used;
        uint16_t consent;
        uint8_t consent_retries;

        char first_name[FIRST_NAME_MAX_LEN];
        uint8_t first_name_len;
        uint8_t age;
        uds_date_t date_of_birth;
        uds_gender_t gender;
        uint16_t height;
} stored_user_data_t;

PRIVILEGED_DATA static user_data_t user_data[CFG_UDS_MAX_USERS];
INITIALISED_PRIVILEGED_DATA static uint8_t selected_user_id = UDS_USER_ID_UNKNOWN;

static void uds_cp_reg_user_data_cb(ble_service_t *svc, uint16_t conn_idx, uint16_t consent);
static void uds_cp_consent_cb(ble_service_t *svc, uint16_t conn_idx, uint8_t user_id, uint16_t consent);
static void uds_cp_delete_user_cb(ble_service_t *svc, uint16_t conn_idx);
static void uds_db_read_cb(ble_service_t *svc, uint16_t conn_idx, uint32_t field);
static void uds_db_write_cb(ble_service_t *svc, uint16_t conn_idx, uint32_t field, uint16_t offset,
                                                                uint16_t length, const void *value);
static bool uds_db_increment_changed_cb(ble_service_t *svc, uint16_t conn_idx, uint32_t increment);

/* Callbacks from UDS implementation */
static const uds_callbacks_t uds_cb = {
        .cp_register_new_user = uds_cp_reg_user_data_cb,
        .cp_consent = uds_cp_consent_cb,
        .cp_delete_user_data = uds_cp_delete_user_cb,
        .db_read = uds_db_read_cb,
        .db_write = uds_db_write_cb,
        .db_increment_changed = uds_db_increment_changed_cb,
};

static void load_stored_user_data(uint8_t id, const stored_user_data_t *stored_ud)
{
        user_data_t *user;

        if (stored_ud && !stored_ud->used) {
                return;
        }

        user = &user_data[id];

        user->used = true;
        user->consent = stored_ud->consent;
        user->consent_retries = stored_ud->consent_retries;

        if (stored_ud->first_name_len != 0) {
                uint8_t len;

                /* One additional byte for null-terminated character */
                len = stored_ud->first_name_len + 1;
                user->first_name = OS_MALLOC(len);

                memcpy(user->first_name, stored_ud->first_name, stored_ud->first_name_len);
                user->first_name[len - 1] = '\0';
        } else {
                user->first_name = NULL;
        }

        user->age = stored_ud->age;
        user->date_of_birth = stored_ud->date_of_birth;
        user->gender = stored_ud->gender;
        user->height = stored_ud->height;
}

static void convert_to_storage_data(uint8_t id, stored_user_data_t* stored_ud)
{
        user_data_t *user;

        user = &user_data[id];

        stored_ud->used = true;
        stored_ud->consent = user->consent;
        stored_ud->consent_retries = user->consent_retries;

        if (user->first_name) {
                uint8_t len = strlen(user->first_name);

                memcpy(stored_ud->first_name, user->first_name, len);
                stored_ud->first_name_len = len;
        } else {
                stored_ud->first_name_len = 0;
        }

        stored_ud->age = user->age;
        stored_ud->date_of_birth = user->date_of_birth;
        stored_ud->gender = user->gender;
        stored_ud->height = user->height;
}

static uint32_t get_user_storage_offset(uint8_t user_id)
{
        uint32_t ret;

        ret = CONFIG_WSP_STORAGE_OFFSET + sizeof(STORAGE_MAGIC) +
                                                        (user_id * sizeof(stored_user_data_t));

        return ret;
}

static void deactivate_user_in_storage(uint8_t user_id)
{
        uint16_t offset;
        stored_user_data_t storage;

        /* reading user data in storage */
        offset = get_user_storage_offset(user_id);
        ad_nvms_read(nvms, offset, (uint8_t *) &storage, sizeof(stored_user_data_t));

        /* deactivate user in storage by setting used flag to false*/
        storage.used = false;
        ad_nvms_write(nvms, offset, (uint8_t *) &storage, sizeof(stored_user_data_t));
}

static void storage_init()
{
        nvms = ad_nvms_open(NVMS_GENERIC_PART);

        if (nvms) {
                int i;
                uint32_t user_offset;
                stored_user_data_t storage;
                uint8_t key[sizeof(STORAGE_MAGIC)];

                ad_nvms_read(nvms, CONFIG_WSP_STORAGE_OFFSET, key, sizeof(STORAGE_MAGIC));

                /* Clean storage if magic does not match */
                if (memcmp(key, STORAGE_MAGIC, sizeof(STORAGE_MAGIC))) {
                        for (i = 0; i < CFG_UDS_MAX_USERS; i++) {
                                deactivate_user_in_storage(i);
                        }

                        ad_nvms_write(nvms, CONFIG_WSP_STORAGE_OFFSET, STORAGE_MAGIC,
                                                                        sizeof(STORAGE_MAGIC));

                        return;
                }

                for (i = 0; i < CFG_UDS_MAX_USERS; i++) {
                        user_offset = get_user_storage_offset(i);
                        ad_nvms_read(nvms, user_offset, (uint8_t *) &storage,
                                                                        sizeof(stored_user_data_t));

                        load_stored_user_data(i, &storage);
                }
        }
}

static void write_user_to_storage(uint8_t user_id)
{
        uint32_t offset;
        stored_user_data_t storage;

        if (!nvms || user_id >= CFG_UDS_MAX_USERS) {
                return;
        }

        convert_to_storage_data(user_id, &storage);

        offset = get_user_storage_offset(user_id);
        ad_nvms_write(nvms, offset, (uint8_t *) &storage, sizeof(stored_user_data_t));
}

static void delete_user_from_storage(uint8_t user_id)
{
        if (!nvms) {
                return;
        }

        deactivate_user_in_storage(user_id);
}

static void measurement_cb(void)
{
        OS_TASK_NOTIFY_FROM_ISR(current_task, WSS_MEASUREMENT_NOTIF, eSetBits);
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        hw_wkup_reset_counter();
#endif
        hw_wkup_reset_interrupt();
}

/**
 * This function get last bonded connection index for given user.
 */
static uint16_t user_id_get_last_conn_idx(uint8_t user_id)
{
        uint16_t conn_idx = BLE_CONN_IDX_INVALID;
        gap_device_t devices[BLE_GAP_MAX_BONDED];
        size_t i, length = BLE_GAP_MAX_BONDED;
        uint8_t uid;

        ble_gap_get_devices(GAP_DEVICE_FILTER_BONDED, NULL, &length, devices);
        for (i = 0; i < length; i++) {
                if (!devices[i].connected) {
                        continue;
                }

                uid = uds_get_user_id(uds, devices[i].conn_idx);
                if (uid == user_id) {
                        conn_idx = devices[i].conn_idx;
                        break;
                }
        }

        return conn_idx;
}

static void delete_user_from_db(uint8_t user_id)
{
        /* case when data for selected user_id has been removed already */
        if (!user_data[user_id].used) {
                return;
        }

        if (user_data[user_id].first_name) {
                OS_FREE(user_data[user_id].first_name);
        }

        queue_remove_all(&user_data[user_id].measurements, OS_FREE_FUNC);

        memset(&user_data[user_id], 0, sizeof(user_data[user_id]));
}

static void deactivate_user(uint8_t user_id)
{
        uint8_t num_conn;
        uint16_t *tmp_conn_idx;
        uint8_t tmp_id;

        ble_gap_get_connected(&num_conn, &tmp_conn_idx);

        while (num_conn--) {
                uint16_t conn_idx = tmp_conn_idx[num_conn];

                tmp_id = uds_get_user_id(uds, conn_idx);
                if (tmp_id == user_id) {
                        uds_set_user_id(uds, conn_idx, UDS_USER_ID_UNKNOWN);
                }
        }

        if (tmp_conn_idx) {
                OS_FREE(tmp_conn_idx);
        }
}

static void delete_user(uint8_t user_id)
{
        if (user_id >= CFG_UDS_MAX_USERS) {
                return;
        }

        delete_user_from_db(user_id);
        deactivate_user(user_id);
        delete_user_from_storage(user_id);
}

static void bcs_meas_tmr_cb(OS_TIMER timer)
{
        OS_TASK_NOTIFY_FROM_ISR(current_task, BCS_MEASUREMENT_NOTIF, eSetBits);
}

static user_data_t *get_user(uint8_t user_id)
{
        user_data_t *user;

        if (user_id == UDS_USER_ID_UNKNOWN) {
                return NULL;
        }

        if (user_id >= CFG_UDS_MAX_USERS) {
                return NULL;
        }

        user = &user_data[user_id];
        if (!user->used) {
                return NULL;
        }

        return user;
}

static void bcs_do_measurement(bcs_body_measurement_t *bcs_meas)
{
        /*
         * Measurement with random value from range 5 - 50 kJ will be sent.
         */
        bcs_meas->basal_metabolism = (rand() % 46 + 5);

        bcs_meas->time_stamp_present = true;
        bcs_meas->time_stamp = cts_time.date_time;

        /*
         * Generate random data of body fat percentage in range of 3.0 % - 40.0 %.
         * Body fat percentage is in resolution of 0.1 %.
         */
        bcs_meas->body_fat_percentage = 30 + rand() % 371;
}

static void wss_do_measurement(wss_weight_measurement_t *wss_meas)
{
        int32_t weight;

        /*
         * Measurement with random value from range 70 - 99 Kg will be sent.
         */
        weight = (rand() % 30 + 70);
        wss_meas->bmi = 10 * weight * 1000000 / (wss_meas->height * wss_meas->height);
        /*
         * Weight is expressed in resolution of 0.005kg (5g)
         */
        wss_meas->weight = weight * 1000 / 5;

        wss_meas->time_stamp_present = true;
        wss_meas->time_stamp = cts_time.date_time;
}

static void user_new_measurement(uint8_t user_id, wsp_measurement_type_t type)
{
        wsp_measurement_t *measurement;
        user_data_t *user;

        user = get_user(user_id);
        if (!user) {
                return;
        }

        if (queue_length(&user->measurements) == CFG_MAX_MEAS_TO_STORE) {
                measurement = queue_pop_front(&user->measurements);
                if (measurement->sending_in_progress) {
                        queue_push_front(&user->measurements, measurement);
                        return;
                }
        } else {
                measurement = OS_MALLOC(sizeof(*measurement));
        }

        memset(measurement, 0, sizeof(*measurement));
        measurement->sending_in_progress = false;
        measurement->conn_idx = BLE_CONN_IDX_INVALID;
        measurement->type = type;

        switch (type) {
        case WSP_MEASUREMENT_TYPE_BCS:
                measurement->measurement.bcs_measurement.user_id = user_id;
                bcs_do_measurement(&measurement->measurement.bcs_measurement);
                break;
        case WSP_MEASUREMENT_TYPE_WSS:
                measurement->measurement.wss_measurement.height = user->height;
                measurement->measurement.wss_measurement.user_id = user_id;
                wss_do_measurement(&measurement->measurement.wss_measurement);
                break;
        }

        queue_push_back(&user->measurements, measurement);
}

static bool match_pending_measurement_with_type(const void *data, const void *match_data)
{
        const wsp_measurement_t *measurement = data;
        const wsp_measurement_type_t *type = match_data;

        if (measurement->type != *type) {
                return false;
        }

        return measurement->sending_in_progress;
}

static bool match_measurement_with_type(const void *data, const void *match_data)
{
        const wsp_measurement_t *measurement = data;
        const wsp_measurement_type_t *type = match_data;

        return measurement->type == *type;
}

static void user_measurement_send(uint8_t user_id, wsp_measurement_type_t type)
{
        wsp_measurement_t *measurement;
        uint16_t conn_idx;
        user_data_t *user;
        ble_error_t status;

        user = get_user(user_id);
        if (!user) {
                return;
        }

        measurement = queue_find(&user->measurements, match_pending_measurement_with_type, &type);
        if (measurement) {
                return;
        }

        measurement = queue_find(&user->measurements, match_measurement_with_type, &type);
        if (!measurement) {
                return;
        }

        conn_idx = user_id_get_last_conn_idx(user_id);
        if (conn_idx == BLE_CONN_IDX_INVALID) {
                return;
        }

        switch (type) {
        case WSP_MEASUREMENT_TYPE_BCS:
                if (!bcs_is_indication_enabled(bcs, conn_idx)) {
                        return;
                }

                measurement->conn_idx = conn_idx;
                measurement->sending_in_progress = true;
                status = bcs_indicate(bcs, conn_idx, &measurement->measurement.bcs_measurement);
                if (status) {
                        measurement->sending_in_progress = false;
                }
                break;
        case WSP_MEASUREMENT_TYPE_WSS:
                if (!wss_is_indication_enabled(wss, conn_idx)) {
                        return;
                }

                measurement->conn_idx = conn_idx;
                measurement->sending_in_progress = true;
                status = wss_indicate_weight(wss, conn_idx, &measurement->measurement.wss_measurement);
                if (status) {
                        measurement->sending_in_progress = false;
                }
                break;
        }
}

struct match_type_conn_idx {
        wsp_measurement_type_t type;
        uint16_t conn_idx;
};

static bool match_pending_measurement_with_conn_idx(const void *data, const void *match_data)
{
        const struct match_type_conn_idx *match_type_conn_idx_data = match_data;
        const wsp_measurement_t *measurement = data;

        if (!measurement->sending_in_progress) {
                return false;
        }

        if (measurement->type != match_type_conn_idx_data->type) {
                return false;
        }

        return measurement->conn_idx == match_type_conn_idx_data->conn_idx;
}

static void user_measurement_sent(uint8_t user_id, uint16_t conn_idx, wsp_measurement_type_t type,
                                                                                bool success)
{
        struct match_type_conn_idx match_data;
        wsp_measurement_t *measurement;
        user_data_t *user;

        user = get_user(user_id);
        if (!user) {
                return;
        }

        match_data.conn_idx = conn_idx;
        match_data.type = type;
        measurement = queue_remove(&user->measurements, match_pending_measurement_with_conn_idx,
                                                                                &match_data);

        if (!measurement) {
                goto done;
        }

        if (success) {
                OS_FREE(measurement);
        } else {
                measurement->sending_in_progress = false;
                measurement->conn_idx = BLE_CONN_IDX_INVALID;
                queue_push_front(&user->measurements, measurement);
        }
done:
        user_measurement_send(user_id, type);
}

static void bcs_indication_changed(uint16_t conn_idx, bool enabled)
{
        uint8_t user_id;

        if (!enabled) {
                return;
        }

        user_id = uds_get_user_id(uds, conn_idx);
        user_measurement_send(user_id, WSP_MEASUREMENT_TYPE_BCS);
}

static void bcs_indication_sent(uint16_t conn_idx, bool status)
{
        uint8_t user_id;

        user_id = uds_get_user_id(uds, conn_idx);
        user_measurement_sent(user_id, conn_idx, WSP_MEASUREMENT_TYPE_BCS, status);
}

static void wss_indication_changed(uint16_t conn_idx, bool enabled)
{
        uint8_t user_id;

        if (!enabled) {
                return;
        }

        user_id = uds_get_user_id(uds, conn_idx);
        user_measurement_send(user_id, WSP_MEASUREMENT_TYPE_WSS);
}

static void wss_indication_sent(uint16_t conn_idx, bool status)
{
        uint8_t user_id;

        user_id = uds_get_user_id(uds, conn_idx);
        user_measurement_sent(user_id, conn_idx, WSP_MEASUREMENT_TYPE_WSS, status);
}

static void measurement_cancel_send(void *data, void *user_data)
{
        wsp_measurement_t *measurement = data;
        uint16_t *conn_idx = user_data;

        if (measurement->sending_in_progress && (*conn_idx == measurement->conn_idx)) {
                measurement->sending_in_progress = false;
                measurement->conn_idx = BLE_CONN_IDX_INVALID;
        }
}

static void user_cancel_send(uint8_t user_id, uint16_t conn_idx)
{
        user_data_t *user = get_user(user_id);

        if (!user) {
                return;
        }

        queue_foreach(&user->measurements, measurement_cancel_send, &conn_idx);
}

static uint8_t user_reg_helper(uint16_t consent, const char *first_name)
{
        uint8_t user_id;
        size_t size;
        user_data_t *user;
        char *name;

        /* find no used user index */
        for (user_id = 0; user_id < CFG_UDS_MAX_USERS; user_id++) {
                if (!user_data[user_id].used) {
                        break;
                }
        }

        /* all users IDs are occupied */
        if (user_id == CFG_UDS_MAX_USERS) {
                return UDS_USER_ID_UNKNOWN;
        }

        user = &user_data[user_id];

        memset(user, 0, sizeof(user_data_t));
        user->used = true;
        user->consent = consent;

        if (first_name != NULL) {
                size = strlen(first_name) + 1;
                name = OS_MALLOC(size);
                strcpy(name, first_name);
                user->first_name = name;
        }

        queue_init(&user->measurements);
        write_user_to_storage(user_id);
        return user_id;
}

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
        /* Check if current time and adjustment reason have valid value */
        if (!cts_is_current_time_valid(time) || (time->adjust_reason &
                                                           ~CTS_ADJUST_REASON_VALID_VALUES_MASK)) {
                cts_set_time_cfm(svc, conn_idx, CTS_ERROR_DATA_FIELD_IGNORED);
                return;
        }

        cts_time = *time;

        cts_set_time_cfm(svc, conn_idx, ATT_ERROR_OK);

        // notify other clients that time has changed
        cts_time.adjust_reason = CTS_AR_MANUAL_TIME_UPDATE;
        cts_notify_time_all(svc, &cts_time);
}

static void cts_set_local_time_info_cb(ble_service_t *svc, uint16_t conn_idx,
                                                                const cts_local_time_info_t *info)
{
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

        rti.source = CTS_RTS_UNKNOWN;
        rti.accuracy = 255;
        rti.days_since_update = 255;
        rti.hours_since_update = 255;

        cts_get_ref_time_info_cfm(svc, conn_idx, ATT_ERROR_OK, &rti);
}

static void uds_cp_reg_user_data_cb(ble_service_t *svc, uint16_t conn_idx, uint16_t consent)
{
        uint8_t user_id;

        user_id = user_reg_helper(consent, NULL);
        if (user_id == UDS_USER_ID_UNKNOWN) {
                uds_cp_register_new_user_cfm(svc, conn_idx, UDS_CP_RESPONSE_FAILED, 0);
                return;
        }

        uds_cp_register_new_user_cfm(svc, conn_idx, UDS_CP_RESPONSE_SUCCESS, user_id);
}

void uds_cp_consent_cb(ble_service_t *svc, uint16_t conn_idx, uint8_t user_id, uint16_t consent)
{
        user_data_t *user;

        user = &user_data[user_id];
        if (user_id >= CFG_UDS_MAX_USERS || !user->used) {
                uds_cp_consent_cfm(svc, conn_idx, UDS_CP_RESPONSE_INVALID_PARAM);
                return;
        }

        if (user->consent != consent) {
                if (user->consent_retries < CONSENT_MAX_RETRIES_VAL) {
                        uds_cp_consent_cfm(svc, conn_idx, UDS_CP_RESPONSE_NOT_AUTHORIZED);
                } else {
                        /*
                         * user_id will be locked. User id can be unlocked from service
                         * only by using debug menu for WSP or device restart.
                         */
                        uds_cp_consent_cfm(svc, conn_idx, UDS_CP_RESPONSE_FAILED);
                }

                user->consent_retries++;

                return;
        }

        uds_set_user_id(svc, conn_idx, user_id);
        uds_cp_consent_cfm(svc, conn_idx, UDS_CP_RESPONSE_SUCCESS);
        user->consent_retries = 0;
        selected_user_id = user_id;
        user_measurement_send(user_id, WSP_MEASUREMENT_TYPE_WSS);
        user_measurement_send(user_id, WSP_MEASUREMENT_TYPE_BCS);
}

static void uds_cp_delete_user_cb(ble_service_t *svc, uint16_t conn_idx)
{
        uint8_t user_id = uds_get_user_id(svc, conn_idx);

        delete_user(user_id);

        uds_cp_delete_user_cfm(svc, conn_idx, UDS_CP_RESPONSE_SUCCESS);
}

static void uds_db_read_cb(ble_service_t *svc, uint16_t conn_idx, uint32_t field)
{
        uint8_t user_id = uds_get_user_id(svc, conn_idx);
        user_data_t *ud;
        uint8_t date_of_birth[4];
        uint8_t *dob_ptr = date_of_birth;

        if (user_id == UDS_USER_ID_UNKNOWN) {
                uds_db_read_cfm(svc, conn_idx, field, UDS_ERROR_ACCESS_NOT_PERMITTED, 0, NULL);
                return;
        }

        ud = &user_data[user_id];

        switch (field) {
        case UDS_DB_FIELD_FIRST_NAME:
                if (ud->first_name) {
                        uds_db_read_cfm(svc, conn_idx, field, ATT_ERROR_OK, strlen(ud->first_name),
                                                                                ud->first_name);
                } else {
                        uds_db_read_cfm(svc, conn_idx, field, ATT_ERROR_OK, 0, NULL);
                }
                break;
        case UDS_DB_FIELD_AGE:
                uds_db_read_cfm(svc, conn_idx, field, ATT_ERROR_OK, sizeof(ud->age), &ud->age);
                break;
        case UDS_DB_FIELD_DATE_OF_BIRTH:
                put_u16_inc(&dob_ptr, ud->date_of_birth.year);
                put_u8_inc(&dob_ptr, ud->date_of_birth.month);
                put_u8_inc(&dob_ptr, ud->date_of_birth.day);
                uds_db_read_cfm(svc, conn_idx, field, ATT_ERROR_OK, sizeof(date_of_birth),
                                                                                &date_of_birth);
                break;
        case UDS_DB_FIELD_GENDER:
                uds_db_read_cfm(svc, conn_idx, field, ATT_ERROR_OK, sizeof(ud->gender),
                                                                                &ud->gender);
                break;
        case UDS_DB_FIELD_HEIGHT:
                uds_db_read_cfm(svc, conn_idx, field, ATT_ERROR_OK, sizeof(ud->height),
                                                                                &ud->height);
                break;
        default:
                uds_db_read_cfm(svc, conn_idx, field, ATT_ERROR_ATTRIBUTE_NOT_FOUND, 0, NULL);
                break;
        }
}

static void update_field_str(char **field, uint16_t offset, uint16_t length, const char *str)
{
        char *tmp = NULL;
        int tmp_len = offset + length + 1;

        if (tmp_len == 1) {
                goto done;
        }

        tmp = OS_MALLOC(tmp_len);     // One additional byte for null-terminated character
        memset(tmp, 0, tmp_len);

        if (*field && offset) {
                memcpy(tmp, *field, offset);
        }

        memcpy(&tmp[offset], str, length);

done:
        if (*field) {
                OS_FREE(*field);
        }
        *field = tmp;
}

static bool uds_date_validation_helper(uint16_t date_year, uint8_t date_month, uint8_t date_day)
{
        uint8_t days_in_month[] = { 31, /* default for month == 0 value as max valid day value */
                         31, ((date_year % 4) ? 28 : 29), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

        /*
         * Check for valid data range.
         * - date_year == 0 is valid and means that "Year is not known",
         * - date_month == 0 is valid and means that "Month is not known",
         */
        if (!((((date_year >= 1582) && (date_year <= 9999)) || (date_year == 0)) &&
                                                                             (date_month <= 12))) {
                return false;
        }

        /* date_day == 0 is valid and means that "Day of Month is not known" */
        return (date_day <= days_in_month[date_month]);
}

static void uds_db_write_cb(ble_service_t *svc, uint16_t conn_idx, uint32_t field, uint16_t offset,
                                                                uint16_t length, const void *value)
{
        uint8_t user_id;
        uint16_t date_year;
        uint8_t month;
        uint8_t day;
        user_data_t *ud;
        att_error_t status = ATT_ERROR_OK;
        const uint8_t *data;

        user_id = uds_get_user_id(svc, conn_idx);
        if (user_id == UDS_USER_ID_UNKNOWN) {
                status = UDS_ERROR_ACCESS_NOT_PERMITTED;
                goto confirm;
        }

        ud = &user_data[user_id];

        switch (field) {
        case UDS_DB_FIELD_FIRST_NAME:
                update_field_str(&ud->first_name, offset, length, value);
                break;
        case UDS_DB_FIELD_AGE:
                ud->age = get_u8(value);
                break;
        case UDS_DB_FIELD_DATE_OF_BIRTH:
                data = (uint8_t *) value;
                date_year = get_u16_inc(&data);
                month = get_u8_inc(&data);
                day = get_u8_inc(&data);

                if (!uds_date_validation_helper(date_year, month, day)) {
                        status = ATT_ERROR_APPLICATION_ERROR;
                        goto confirm;
                }

                ud->date_of_birth.year = date_year;
                ud->date_of_birth.month = month;
                ud->date_of_birth.day = day;
                break;
        case UDS_DB_FIELD_GENDER:
                ud->gender = get_u8(value);
                break;
        case UDS_DB_FIELD_HEIGHT:
                ud->height = get_u16(value);
                break;
        default:
                status = ATT_ERROR_ATTRIBUTE_NOT_FOUND;
                break;
        }

confirm:
        uds_db_write_cfm(svc, conn_idx, field, status);

        write_user_to_storage(user_id);
}

static bool uds_db_increment_changed_cb(ble_service_t *svc, uint16_t conn_idx, uint32_t increment)
{
        uint8_t id;

        id = uds_get_user_id(svc, conn_idx);
        user_data[id].db_increment = increment;

        return true;
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        uint8_t i;

        for (i = 0; i < CFG_UDS_MAX_USERS; i++) {
                if (user_data[i].used) {
                        user_cancel_send(i, evt->conn_idx);
                }
        }

#if !CFG_MULTIPLE_CLIENTS
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
#endif
        selected_user_id = UDS_USER_ID_UNKNOWN;
}

#if CFG_MULTIPLE_CLIENTS
static void handle_evt_gap_adv_completed(ble_evt_gap_adv_completed_t *evt)
{
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
}
#endif

static void handle_connection_req(ble_evt_gap_connected_t *evt)
{
        user_data_t *user;
        uint8_t user_id;

        for (user_id = 0; user_id < CFG_UDS_MAX_USERS; user_id++) {
                user = &user_data[user_id];
                if (user->used) {
                        uds_set_user_id(uds, evt->conn_idx, user_id);

                        user_measurement_send(user_id, WSP_MEASUREMENT_TYPE_BCS);
                        user_measurement_send(user_id, WSP_MEASUREMENT_TYPE_WSS);

                        break;
                }
        }

        selected_user_id = UDS_USER_ID_UNKNOWN;
}

static void cmd_user_register_handler(int argc, const char *argv[])
{
        uint8_t user_id;

        if (argc < 3) {
                printf("Wrong parameter, type: user register <consent> [first_name]\r\n");
                return;
        }

        user_id = user_reg_helper(atoi(argv[2]), (argc > 3) ? argv[3] : NULL);
        if (user_id == UDS_USER_ID_UNKNOWN) {
                printf("All users IDs are occupied\r\n");
                return;
        }

        printf("User registered with id %d\r\n", user_id);
}

static void cmd_user_remove_handler(int argc, const char *argv[])
{
        uint8_t id;
        int i;

        if (argc < 3) {
                printf("Wrong parameter, type: user remove <user_id | all>\r\n");
                return;
        }

        if (!strcmp(argv[2], "all")) {
                for (i = 0; i < CFG_UDS_MAX_USERS; i++) {
                        if (user_data[i].used) {
                                delete_user(i);
                        }
                }
                printf("All users removed\r\n");
                return;
        }

        id = atoi(argv[2]);

        if (get_user(id) == NULL) {
                printf("User with id %d doesn't exist\r\n", id);
                return;
        }

        delete_user(id);
        printf("User with id %d removed\r\n", id);
}

static void cmd_user_list_handler(int argc, const char *argv[])
{
        int i;
        bool list_empty = true;

        printf("Idx    Cons    First name\r\n");
        printf("-------------------------------\r\n");

        for (i = 0; i < CFG_UDS_MAX_USERS; i++) {
                if (!user_data[i].used) {
                    continue;
                }

                list_empty = false;
                printf("%3d   %5d    %s\r\n", i, user_data[i].consent,
                                        user_data[i].first_name ? user_data[i].first_name : "");
        }

        if (list_empty) {
                printf("List of users is empty\r\n");
        }
}

static void cmd_user_info_handler(int argc, const char *argv[])
{
        uint8_t id;

        if (argc < 3) {
                printf("Wrong parameter, type: user info <index>\r\n");
                return;
        }

        id = atoi(argv[2]);

        if (get_user(id) == NULL) {
                printf("User with id %d doesn't exist\r\n", id);
                return;
        }

        printf("User id: %d\r\n", id);
        printf("Consent: %d\r\n", user_data[id].consent);

        if (user_data[id].first_name != NULL) {
                printf("First name: %s\r\n", user_data[id].first_name);
        } else {
                printf("First name: not set\r\n");
        }

        if (user_data[id].age > 0) {
                printf("Age: %d\r\n", user_data[id].age);
        } else {
                printf("Age: not set\r\n");
        }

        if (user_data[id].height > 0) {
                printf("Height: %d\r\n", user_data[id].height);
        } else {
                printf("Height: not set\r\n");
        }

        switch (user_data[id].gender) {
        case UDS_GENDER_MALE:
                printf("Gender: male\r\n");
                break;
        case UDS_GENDER_FEMALE:
                printf("Gender: female\r\n");
                break;
        case UDS_GENDER_UNSPECIFIED:
                printf("Gender: unspecified\r\n");
                break;
        }

         printf("Date of birth: %02d-%02d-%04d\r\n", user_data[id].date_of_birth.day,
                               user_data[id].date_of_birth.month, user_data[id].date_of_birth.year);
}

static void check_id_cb(int argc, const char *argv[], void *user_data)
{
        user_data_callback_t callback = user_data;
        uint8_t id;

        id = strtoul(argv[1], NULL, 0);

        if (!get_user(id)) {
                printf("User with id %d doesn't exist\r\n", id);
                return;
        }

        callback(argc, argv[2], id);
}

static void cmd_user_change_name_handler(int argc, const char *argv, uint8_t id)
{

        size_t size;
        char *name;

        if (argc < 3) {
                printf("Wrong parameter, type: user change_name <index> <name>\r\n");
                return;
        }

        /* Free previous name */
        OS_FREE(user_data[id].first_name);

        size = strlen(argv) + 1;
        name = OS_MALLOC(size);
        strcpy(name, argv);
        user_data[id].first_name = name;

        write_user_to_storage(id);
}

static void cmd_user_change_age_handler(int argc, const char *argv, uint8_t id)
{
        if (argc < 3) {
                printf("Wrong parameter, type: user change_age <index> <age>\r\n");
                return;
        }

        user_data[id].age = atoi(argv);

        write_user_to_storage(id);
}

static void cmd_user_change_height_handler(int argc, const char *argv, uint8_t id)
{
        if (argc < 3) {
                printf("Wrong parameter, type: user change_height <index> <height>\r\n");
                return;
        }

        user_data[id].height = atoi(argv);

        write_user_to_storage(id);
}

static void cmd_user_change_gender_handler(int argc, const char *argv, uint8_t id)
{
        if (argc < 3) {
                printf("Wrong parameter, type: user change_gender <index> <gender>\r\n");
                return;
        }

        if (!strcmp(argv, "male")) {
                user_data[id].gender = UDS_GENDER_MALE;
        } else if (!strcmp(argv, "female")) {
                user_data[id].gender = UDS_GENDER_FEMALE;
        } else {
                user_data[id].gender = UDS_GENDER_UNSPECIFIED;
        }

        write_user_to_storage(id);
}

static void cmd_user_change_date_of_birth_handler(int argc, const char *argv, uint8_t id)
{
        int day, month, year;
        int res;

        if (argc < 3) {
                printf("Wrong parameter, type: user change_date_of_birth <index> <date>\r\n");
                return;
        }

        res = sscanf(argv, "%d-%d-%d", &day, &month, &year);
        if (res != 3) {
                printf("Incorrect date of birth format, please type e.g 30-05-1990\r\n");
                return;
                }

        user_data[id].date_of_birth.day = day;
        user_data[id].date_of_birth.month = month;
        user_data[id].date_of_birth.year = year;

        write_user_to_storage(id);
}

static void cmd_user_select_handler(int argc, const char *argv[])
{
        uint8_t id;

        if (argc < 3) {
                printf("Wrong parameter, type: user select <index>\r\n");
                return;
        }

        id = atoi(argv[2]);

        if (get_user(id) == NULL) {
                printf("User with id %d doesn't exist\r\n", id);
                return;
        }

        if (!user_data[id].used) {
                return;
        }

        selected_user_id = id;
        printf("User with id %d selected\r\n", id);
}

static void cmd_user_usage(void)
{
        printf("Usage: user register <consent> [first_name]\r\n");
        printf("       user remove <index | all>\r\n");
        printf("       user list\r\n");
        printf("       user info <index>\r\n");
        printf("       user select <index>\r\n");
}

static void cmd_user_handler(int argc, const char *argv[], void *data)
{
        if (argc < 2) {
                cmd_user_usage();
        }

        if (!strcmp(argv[1], "register")) {
                cmd_user_register_handler(argc, argv);
        } else if (!strcmp(argv[1], "remove")) {
                cmd_user_remove_handler(argc, argv);
        } else if (!strcmp(argv[1], "list")) {
                cmd_user_list_handler(argc, argv);
        } else if (!strcmp(argv[1], "info")) {
                cmd_user_info_handler(argc, argv);
        } else if (!strcmp(argv[1], "select")) {
                cmd_user_select_handler(argc, argv);
        }
}

static void cmd_time_get_handler(int argc, const char *argv[])
{
        static const char *day[8] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
                                                                   "Friday", "Saturday", "Unknown" };

        printf("%s, ", (cts_time.day_of_week < 8) ? day[cts_time.day_of_week] : day[7]);

        printf("%02d-%02d-%4d ", cts_time.date_time.day, cts_time.date_time.month,
                                                                           cts_time.date_time.year);
        printf("%02d:%02d:%02d\r\n", cts_time.date_time.hours, cts_time.date_time.minutes,
                                                                        cts_time.date_time.seconds);
}

static uint8_t get_day_of_week(int year, int month, int day)
{
        if (!uds_date_validation_helper(year, month, day)) {
                printf("Date range is not valid\r\n");
                return 0;
        }

        /* It is implementation of Mike Keith algorithm */
        day += (month < 3) ? (year--) : (year - 2);

        return (((23 * month) / 9) + day + 4 + (year / 4) - (year / 100) +
                (year / 400)) % 7;
}

static void cmd_time_set_handler(int argc, const char *argv[])
{
        int year, month, day;
        int hour, min, sec;
        int res;

        if (argc < 4) {
                printf("Wrong parameter, type: time set <date> <time>\r\n");
                return;
        }

        res = sscanf(argv[2], "%d-%d-%d", &day, &month, &year);
        if (res != 3) {
                printf("Incorrect format, please type e.g. time set 09-12-2016 13:45:00\r\n");
                return;
        }

        res = sscanf(argv[3], "%d:%d:%d", &hour, &min, &sec);
        if (res != 3) {
                printf("Incorrect format, please type e.g. time set 09-12-2016 13:45:00\r\n");
                return;
        }

        cts_time.date_time.year = year;
        cts_time.date_time.month = month;
        cts_time.date_time.day = day;
        cts_time.date_time.hours = hour;
        cts_time.date_time.minutes = min;
        cts_time.date_time.seconds = sec;
        cts_time.day_of_week = get_day_of_week(year, month, day);
}

static void cmd_time_usage(void)
{
        printf("Usage: time get\r\n");
        printf("       time set <date> <time>\r\n");
}

static void cmd_time_handler(int argc, const char *argv[], void *data)
{
        if (argc < 2) {
                cmd_time_usage();
        }

        if (!strcmp(argv[1], "get")) {
                cmd_time_get_handler(argc, argv);
        } else if (!strcmp(argv[1], "set")) {
                cmd_time_set_handler(argc, argv);
        }
}

static const cli_command_t debug_handlers[] = {
        { "user",               cmd_user_handler,           NULL },
        { "user_change_name",   check_id_cb,                cmd_user_change_name_handler},
        { "user_change_age",    check_id_cb,                cmd_user_change_age_handler},
        { "user_change_height", check_id_cb,                cmd_user_change_height_handler},
        { "user_change_gender", check_id_cb,                cmd_user_change_gender_handler},
        { "user_change_date",   check_id_cb,                cmd_user_change_date_of_birth_handler},
        { "time",               cmd_time_handler,           NULL },
        { NULL },
};

static void default_handler(int argc, const char **argv, void *user_data)
{
        printf("Unknown command\r\n");
}

cli_t register_debug(uint32_t notif_mask)
{
        return cli_register(notif_mask, debug_handlers, default_handler);
}

void wsp_weightscale_task(void *params)
{
        ble_service_config_t service_config;
        cts_local_time_info_t cts_lti = {
                /* Example time zone, should be taken from permanent storage or RTC */
                .dst = CTS_DST_DAYLIGHT_TIME,
                .time_zone = cts_get_time_zone(+3, 0), // UTC + 3 Athens
        };
        int8_t wdog_id;
        cli_t cli;

        /* Register wsp_weightscale task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);

        /* Initialize storage WSP module */
        storage_init();

        current_task = OS_GET_CURRENT_TASK();
        /*
         * Register wake-up interrupt for handling button action for generating sample measurements
         */
        hw_wkup_register_interrupt(measurement_cb, 1);

        /* Start BLE device as peripheral */
        ble_peripheral_start();

        /* Register current task to BLE framework to receive BLE event notifications */
        ble_register_app();

        /* Set device name and appearance */
        ble_gap_device_name_set("Dialog Weight Scale", ATT_PERM_READ);
        ble_gap_appearance_set(BLE_GAP_APPEARANCE_GENERIC_WEIGHT_SCALE, ATT_PERM_READ);

        /*
         * Add Body Composition Service
         *
         * BCS is an optional secondary service which is included into WSS.
         */
        service_config.service_type = GATT_SERVICE_SECONDARY;
        service_config.sec_level = GAP_SEC_LEVEL_2;
        service_config.num_includes = 0;
        bcs = bcs_init(&service_config, BCS_FEAT_MULTIPLE_USERS | BCS_FEAT_TIME_STAMP |
                                        BCS_FEAT_BASAL_METABOLISM, &bcs_cb);

        bcs_meas_tmr = OS_TIMER_CREATE("bcs_meas", portCONVERT_MS_2_TICKS(5000), pdTRUE,
                                                                     current_task, bcs_meas_tmr_cb);
        OS_ASSERT(bcs_meas_tmr);
        OS_TIMER_START(bcs_meas_tmr, OS_TIMER_FOREVER);

        /*
         * Register WSS
         *
         * WSS instance should be registered in ble_service framework in order for events inside
         * service to be processed properly.
         */
        service_config.service_type = GATT_SERVICE_PRIMARY;
        service_config.sec_level = GAP_SEC_LEVEL_2;
        service_config.num_includes = 1;
        service_config.includes = &bcs;
        wss = wss_init(&service_config, WSS_FEAT_MULTI_USER_SUPPORTED | WSS_FEAT_TIME_STAMP_SUPPORTED |
                                        WSS_FEAT_BMI_SUPPORTED | WSS_FEAT_WT_DISPLAY_500G_ACC |
                                        WSS_FEAT_HT_DISPLAY_10MM_ACC, &wss_cb);

        /*
         * Register DIS
         *
         * DIS doesn't contain any dynamic data thus it doesn't need to be registered in ble_service
         * framework (but it's not an error to do so).
         */
        dis_init(NULL, &dis_info);

        /* Add User Data Service */
        service_config.service_type = GATT_SERVICE_PRIMARY;
        service_config.sec_level = GAP_SEC_LEVEL_2;
        service_config.num_includes = 0;
        uds = uds_init(&service_config, UDS_DB_FIELD_AGE | UDS_DB_FIELD_DATE_OF_BIRTH |
                                        UDS_DB_FIELD_FIRST_NAME | UDS_DB_FIELD_HEIGHT |
                                        UDS_DB_FIELD_GENDER, &uds_cb);

        /* Add Battery Service */
        bas = bas_init(NULL, NULL);
        bas_set_level(bas, 90, false);

        /* Add Current Time Service */
        cts_init(&cts_lti, &cts_cb);

        /*
         * Create timer for CTS, this will be used to update current time every second
         *
         * Used only for demo purpose. Instead of it RTC should be used,
         * e.g. some RTC connected to I2C.
         */
        cts_timer = OS_TIMER_CREATE("cts", portCONVERT_MS_2_TICKS(1000), pdTRUE,
                                                                        current_task, cts_timer_cb);
        OS_ASSERT(cts_timer);
        OS_TIMER_START(cts_timer, OS_TIMER_FOREVER);


        /*
         * Set advertising data and scan response, then start advertising.
         */
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, sizeof(scan_rsp), scan_rsp);
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

        cli = register_debug(CLI_NOTIF);

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
                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif, OS_TASK_NOTIFY_FOREVER);
                /* Guaranteed to succeed since we're waiting forever for the notification */
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
                                case BLE_EVT_GAP_DISCONNECTED:
                                        handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *)
                                                                                        hdr);
                                        break;
#if CFG_MULTIPLE_CLIENTS
                                case BLE_EVT_GAP_ADV_COMPLETED:
                                        handle_evt_gap_adv_completed((ble_evt_gap_adv_completed_t *)
                                                                                        hdr);
                                        break;
#endif
                                case BLE_EVT_GAP_PAIR_REQ:
                                {
                                        ble_evt_gap_pair_req_t *evt = (ble_evt_gap_pair_req_t *)
                                                                                        hdr;
                                        ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
                                        break;
                                }
                                case BLE_EVT_GAP_CONNECTED:
                                        handle_connection_req((ble_evt_gap_connected_t *) hdr);
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
                                OS_TASK_NOTIFY(OS_GET_CURRENT_TASK(), BLE_APP_NOTIFY_MASK, eSetBits);
                        }
                }


                if (notif & CLI_NOTIF) {
                        cli_handle_notified(cli);
                }

                if ((notif & WSS_MEASUREMENT_NOTIF) || (notif & BCS_MEASUREMENT_NOTIF)) {
                        wsp_measurement_type_t type;

                        if (notif & WSS_MEASUREMENT_NOTIF) {
                                type = WSP_MEASUREMENT_TYPE_WSS;
                        } else {
                                type = WSP_MEASUREMENT_TYPE_BCS;
                        }

                        user_new_measurement(selected_user_id, type);
                        user_measurement_send(selected_user_id, type);
                }
        }
}
