/**
 ****************************************************************************************
 *
 * @file cscp_collector_task.h
 *
 * @brief Cycling Speed and Cadence Collector task's header
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef CSCP_COLLECTOR_TASK_H_
#define CSCP_COLLECTOR_TASK_H_

#include "cscs_client.h"

/**
 * CSC Collector demo application's states
 */
typedef enum {
        /* State disconnected */
        CSCP_COLLECTOR_STATE_DISCONNECTED,
        /* State scanning */
        CSCP_COLLECTOR_STATE_SCANNING,
        /* State connecting */
        CSCP_COLLECTOR_STATE_CONNECTING,
        /* State connected */
        CSCP_COLLECTOR_STATE_CONNECTED
} cscp_collector_state_t;

/**
 * CSC Sensor information
 */
typedef struct {
        /** Sensor found flag */
        bool sensor_found;
        /** Bonded flag */
        bool bonded;
        /** CSC Sensor's connection index */
        uint16_t sensor_conn_idx;
        /** CSC Sensor address */
        bd_address_t address;
        /** DIS Client instance */
        ble_client_t *dis_client;
        /** CSCS Client instance */
        ble_client_t *cscs_client;
        /* CSC Sensor's features - static for the lifetime of the CSC Sensor */
        uint16_t features;
        /** Last received RSC Measurement */
        cscs_client_measurement_t last_measurement;
} csc_sensor_info_t;

#endif /* CSCP_COLLECTOR_TASK_H_ */
