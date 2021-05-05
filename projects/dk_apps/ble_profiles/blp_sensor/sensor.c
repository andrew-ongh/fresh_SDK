/**
 ****************************************************************************************
 *
 * @file sensor.c
 *
 * @brief Sensor abstraction
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "osal.h"
#include "blp_sensor_config.h"
#include "sensor.h"

#define NOTIF_DO_MEASUREMENT            (1 << 1)
#define NOTIF_CANCEL_MEASUREMENT        (1 << 2)
#define NOTIF_SEND_MEASUREMENT          (1 << 3)

/*
 * Initial blood pressure value in [Pa]
 *
 * Blood pressure is generated as a random value from 12 to 20 kPa but the device starts measuring
 * from a higher value (arbitrarily selected), decreasing the measured value to achieve a desired
 * blood pressure value.
 */
#define INITIAL_BLOOD_PRESSURE_VALUE    23000

/* "Parent" application task data */
PRIVILEGED_DATA static struct {
        OS_TASK         task;
        uint32_t        notif;
        OS_QUEUE        queue;
} app;

/* Sensor task data */
PRIVILEGED_DATA static struct {
        OS_TASK         task;
        OS_TIMER        timer;
} sensor;

/* Sensor measurement data */
PRIVILEGED_DATA static struct {
        bls_measurement_t       bls_measurement;
        uint32_t                last_systolic_pressure;
        uint32_t                last_intermediate_cuff_pressure;
} sensor_sim;

static void timer_cb(OS_TIMER timer)
{
        OS_TASK_NOTIFY(sensor.task, NOTIF_SEND_MEASUREMENT, OS_NOTIFY_SET_BITS);
}

static void set_default_measurement_values(bls_measurement_t *measurement)
{
        measurement->time_stamp_present = false;
        measurement->user_id_present = false;

        measurement->measurement_status_present = true;
        measurement->measurement_status.body_movement = BLS_BODY_MOVEMENT_DETECTED;
        measurement->measurement_status.cuff_fit = BLS_CUFF_FIT_PROPERLY;
        measurement->measurement_status.irregular_pulse = BLS_IRREGULAR_PULSE_NOT_DETECTED;
        measurement->measurement_status.measurement_pos = BLS_MEASURE_POS_PROPER;
        measurement->measurement_status.pulse_rate_range = BLS_PULSE_RATE_RANGE_WITHIN;
}

static void generate_measurement(bls_measurement_t *measurement)
{
        const int8_t EXPONENT = -1;

        /*
         * Normal pressure of blood:
         * SP (Systolic Pressure) - 16,0 kPa (120 mmHg)
         * DP (Diastolic Pressure) - 10,7 kPa (80 mmHg).
         *
         * MAP pressure can be approximated as: (2/3) * DP + (1/3) * SP
         *
         * MAP = 12.47 kPa (93.33 mmHg).
         *
         * A normal resting heart rate for adults ranges from 60 to 100 beats a minute.
         */

        // Generate measurements data
        measurement->unit = BLS_PRESSURE_UNIT_KPA;
        measurement->pressure_systolic.exp = EXPONENT;
        measurement->pressure_systolic.mantissa = 120 + rand() % 80;

        measurement->pressure_diastolic.exp = EXPONENT;
        measurement->pressure_diastolic.mantissa = 80 + rand() % 40;

        measurement->pressure_map.exp = EXPONENT;
        measurement->pressure_map.mantissa = 100 + rand() % 50;

        measurement->pulse_rate_present = true;
        measurement->pulse_rate.exp = 0;
        measurement->pulse_rate.mantissa = 60 + rand() % 40;

        set_default_measurement_values(measurement);

        // value is in [Pa] so if the EXPONENT = -1, the result need to be multiply by 10^(3 - 1)
        sensor_sim.last_systolic_pressure = measurement->pressure_systolic.mantissa * 100;
}

static void generate_intermediate_measurement(uint8_t num, bls_measurement_t *measurement)
{
        const int8_t EXPONENT = -1;

        /*
         * Normal pressure of blood:
         * SP (Systolic Pressure) - 16,0 kPa (120 mm Hg)
         * DP (Diastolic Pressure) - 10,7 kPa (80 mm Hg).
         *
         * MAP pressure can be approximated as: (2/3) * DP + (1/3) * SP, where
         *
         * MAP = 12.47 kPa (93.33 mm Hg).
         *
         * A normal resting heart rate for adults ranges from 60 to 100 beats a minute.
         */

        /*
         * INITIAL_BLOOD_PRESSURE_VALUE is in [Pa] so if the EXPONENT = -1, the mantissa should
         * be equal INITIAL_BLOOD_PRESSURE_VALUE divided by 10^(3 - 1) to get value in [kPa]
         */
        int32_t initial_systolic_mantissa = INITIAL_BLOOD_PRESSURE_VALUE / 100;

        // Generate measurements data
        measurement->unit = BLS_PRESSURE_UNIT_KPA;
        measurement->pressure_systolic.exp = EXPONENT;
        measurement->pressure_systolic.mantissa = initial_systolic_mantissa - num * 10;

        measurement->pulse_rate_present = true;
        measurement->pulse_rate.exp = EXPONENT;
        measurement->pulse_rate.mantissa = 600 + rand() % 400;

        set_default_measurement_values(measurement);

        // value is in [Pa] so if the EXPONENT = -1, the result need to be multiply by 10^(3 - 1)
        sensor_sim.last_intermediate_cuff_pressure = measurement->pressure_systolic.mantissa * 100;
}

static void sim_start(void)
{
        generate_measurement(&sensor_sim.bls_measurement);
        sensor_sim.last_intermediate_cuff_pressure = INITIAL_BLOOD_PRESSURE_VALUE;
}

static void send_intermediate_measurement(uint8_t counter)
{
        sensor_event_t event;

        event.type = SENSOR_EVENT_INTERMEDIATE;
        generate_intermediate_measurement(counter, &event.value);

        OS_QUEUE_PUT(app.queue, &event, OS_QUEUE_FOREVER);
        OS_TASK_NOTIFY(app.task, app.notif, OS_NOTIFY_SET_BITS);
}

static void send_measurement(void)
{
        sensor_event_t event;

        event.type = SENSOR_EVENT_MEASUREMENT;
        event.value = sensor_sim.bls_measurement;

        OS_QUEUE_PUT(app.queue, &event, OS_QUEUE_FOREVER);
        OS_TASK_NOTIFY(app.task, app.notif, OS_NOTIFY_SET_BITS);
}

static inline bool measurement_is_done(void)
{
        return (sensor_sim.last_intermediate_cuff_pressure < sensor_sim.last_systolic_pressure);
}

static void sensor_task(void *param)
{
        uint8_t counter = 0;

        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                ret = OS_TASK_NOTIFY_WAIT(0, OS_TASK_NOTIFY_ALL_BITS, &notif,
                                                                        OS_TASK_NOTIFY_FOREVER);
                OS_ASSERT(ret == OS_OK);

                if ((notif & NOTIF_DO_MEASUREMENT) && !OS_TIMER_IS_ACTIVE(sensor.timer)) {
                        /* Initialize measurement and start timer for reading intermediate results */
                        sim_start();
                        counter = 0;
                        OS_TIMER_START(sensor.timer, OS_TIMER_FOREVER);
                } else if (notif & NOTIF_CANCEL_MEASUREMENT) {
                        /* Cancel measurement */
                        OS_TIMER_STOP(sensor.timer, OS_TIMER_FOREVER);
                } else if (notif & NOTIF_SEND_MEASUREMENT) {
                        /* Send current intermediate result */
                        send_intermediate_measurement(counter++);

                        if (measurement_is_done()) {
                                send_measurement();
                                OS_TIMER_STOP(sensor.timer, OS_TIMER_FOREVER);
                        }
                }
        }
}

void sensor_init(uint32_t notif)
{
        app.task = OS_GET_CURRENT_TASK();
        app.notif = notif;
        OS_QUEUE_CREATE(app.queue, sizeof(sensor_event_t), 10);

        sensor.timer = OS_TIMER_CREATE("sensor_timer",
                                                OS_MS_2_TICKS(CFG_INTER_CUFF_TIME_INTERVAL_MS),
                                                OS_TIMER_SUCCESS, NULL, timer_cb);
        OS_TASK_CREATE("sensor", sensor_task, NULL, 400, OS_TASK_PRIORITY_NORMAL, sensor.task);
}

void sensor_do_measurement(void)
{
        OS_TASK_NOTIFY(sensor.task, NOTIF_DO_MEASUREMENT, OS_NOTIFY_SET_BITS);
}

void sensor_cancel_measurement(void)
{
        OS_TASK_NOTIFY(sensor.task, NOTIF_CANCEL_MEASUREMENT, OS_NOTIFY_SET_BITS);
}

bool sensor_get_event(sensor_event_t *event)
{
        return OS_QUEUE_GET(app.queue, event, OS_QUEUE_NO_WAIT) == OS_QUEUE_OK;
}
