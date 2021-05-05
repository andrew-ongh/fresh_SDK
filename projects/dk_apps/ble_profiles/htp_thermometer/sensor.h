/**
 ****************************************************************************************
 *
 * @file sensor.h
 *
 * @brief Thermometer sensor abstraction API
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef SENSOR_H_
#define SENSOR_H_

#include <stdbool.h>
#include <svc_types.h>

/** Sensor event type */
typedef enum {
        SENSOR_EVENT_INTERMEDIATE,
        SENSOR_EVENT_MEASUREMENT,
        SENSOR_EVENT_ERROR,
} sensor_event_type_t;

/** Sensor event (measurement value) */
typedef struct {
        sensor_event_type_t type;
        svc_ieee11073_float_t value;
} sensor_event_t;

/**
 * \brief Initialize sensor structures
 *
 * Application will receive \p notif notification when there is an event waiting in the queue and
 * shall use sensor_get_event() to retrieve it.
 *
 * \param [in] notif  notification mask
 *
 */
void sensor_init(uint32_t notif);

/**
 * \brief Start a new measurement
 *
 * Has no effect when measurement is already in progress.
 *
 */
void sensor_do_measurement(void);

/**
 * \brief Cancel ongoing measurement
 *
 * Has no effect when no measurement is in progress.
 *
 */
void sensor_cancel_measurement(void);

/**
 * \brief Retrieve event from queue
 *
 * \param [out] event  retrieved event
 *
 * \return true if event was retrieved, false otherwise (no event in queue)
 *
 */
bool sensor_get_event(sensor_event_t *event);

#endif /* SENSOR_H_ */
