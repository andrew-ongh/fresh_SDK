/**
 ****************************************************************************************
 *
 * @file sensor.c
 *
 * @brief Thermometer sensor abstraction
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "osal.h"
#include "sensor.h"

#define NOTIF_DO_MEASUREMENT            (1 << 1)
#define NOTIF_CANCEL_MEASUREMENT        (1 << 2)
#define NOTIF_SEND_MEASUREMENT          (1 << 3)

/* Constant exponent used for all measurements */
#define EXPONENT  -1

/* Interval for intermediate measurements */
#define INTERMEDIATE_INTERVAL_MS  250

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

/* Sensor simulation data */
PRIVILEGED_DATA static struct {
        int             temperature;
        int             last_read;
        unsigned        step;
        unsigned        stable;
} sensor_sim;

static inline bool sim_is_error(void)
{
        return sensor_sim.last_read == INT_MIN;
}

static inline bool sim_is_stable(void)
{
        return sensor_sim.stable >= 2;
}

/*
 * This simulates the initialization of temperature sensor before measurement. Since we don't do an
 * actual measurement, a target value is randomized at startup and subsequent "reads" from the
 * sensor will return more accurate approximations of this target value until the final value is
 * reached. The target value is something between 33.0 and 42.9 degrees (i.e. 330-429 and -1 exp).
 */
static void sim_start(void)
{
        sensor_sim.temperature = rand() % 100 + 330;
        sensor_sim.last_read = 0;
        sensor_sim.step = 0;
        sensor_sim.stable = 0;
}

/*
 * Each call to sim_read() simulates reading from a temperature sensor and will return more accurate
 * approximation of the target value set on initialization - this is calculated using a
 * pre-calculated table with 50 approximation steps. If the approximated value does not change for 3
 * consecutive reads it is considered stable and can be used as the final measurement. If it is not
 * possible to obtain a stable value within 50 reads, an error is returned. The returned value
 * quickly rises from 0 to something close to the target value and then slowly stabilizes, something
 * similar to what an actual sensor would do.
 */
static int sim_read(void)
{
        static const unsigned approx_table[50] = {
                   0, 4097, 6257, 7338, 7952, 8341, 8608, 8802, 8949, 9064,
                9156, 9232, 9296, 9349, 9396, 9436, 9471, 9502, 9529, 9554,
                9576, 9596, 9615, 9631, 9647, 9661, 9674, 9686, 9697, 9708,
                9717, 9726, 9735, 9743, 9750, 9758, 9764, 9771, 9777, 9782,
                9788, 9793, 9798, 9803, 9807, 9811, 9816, 9819, 9823, 9827,
        };

        int t;

        if (sim_is_stable()) {
                return sensor_sim.last_read;
        }

        if (sensor_sim.step < 50) {
                t = sensor_sim.temperature * approx_table[sensor_sim.step++] / 10000;
        } else {
                t = INT_MIN;
        }

        if (t == sensor_sim.last_read) {
                sensor_sim.stable++;
        } else {
                sensor_sim.stable = 0;
        }

        sensor_sim.last_read = t;

        return t;
}

static void send_measurement(void)
{
        sensor_event_t event;

        if (!sim_is_error()) {
                event.type = SENSOR_EVENT_MEASUREMENT;
                event.value.mantissa = sim_read();
                event.value.exp = EXPONENT;
        } else {
                event.type = SENSOR_EVENT_ERROR;
                event.value.mantissa = SVC_IEEE11073_FLOAT_NAN;
                event.value.exp = 0;
        }

        OS_QUEUE_PUT(app.queue, &event, OS_QUEUE_FOREVER);
        OS_TASK_NOTIFY(app.task, app.notif, OS_NOTIFY_SET_BITS);

}

static void send_intermediate_measurement(void)
{
        sensor_event_t event;

        event.type = SENSOR_EVENT_INTERMEDIATE;
        event.value.mantissa = sim_read();
        event.value.exp = EXPONENT;

        if (sim_is_error()) {
                return;
        }

        OS_QUEUE_PUT(app.queue, &event, OS_QUEUE_FOREVER);
        OS_TASK_NOTIFY(app.task, app.notif, OS_NOTIFY_SET_BITS);
}

static void timer_cb(OS_TIMER timer)
{
        OS_TASK_NOTIFY(sensor.task, NOTIF_SEND_MEASUREMENT, OS_NOTIFY_SET_BITS);
}

static void sensor_task(void *param)
{
        for (;;) {
                OS_BASE_TYPE ret;
                uint32_t notif;

                ret = OS_TASK_NOTIFY_WAIT(0, (uint32_t) -1, &notif, OS_TASK_NOTIFY_FOREVER);
                OS_ASSERT(ret == OS_OK);

                if ((notif & NOTIF_DO_MEASUREMENT) && !OS_TIMER_IS_ACTIVE(sensor.timer)) {
                        /* Initialize measurement and start timer for reading intermediate results */
                        sim_start();
                        OS_TIMER_START(sensor.timer, OS_TIMER_FOREVER);
                }

                if (notif & NOTIF_CANCEL_MEASUREMENT) {
                        /* Cancel measurement */
                        OS_TIMER_STOP(sensor.timer, OS_TIMER_FOREVER);
                }

                if (notif & NOTIF_SEND_MEASUREMENT) {
                        /* Send current intermediate result */
                        send_intermediate_measurement();

                        /*
                         * In case measurement value is stable or error occured, send final
                         * measurement value and stop timer
                         */
                        if (sim_is_stable() || sim_is_error()) {
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

        sensor.timer = OS_TIMER_CREATE("sensor_timer", OS_MS_2_TICKS(INTERMEDIATE_INTERVAL_MS),
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
