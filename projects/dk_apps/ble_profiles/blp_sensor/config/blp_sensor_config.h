/**
 ****************************************************************************************
 *
 * @file blp_sensor_config.h
 *
 * @brief Application configuration
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef BLP_SENSOR_CONFIG_H_
#define BLP_SENSOR_CONFIG_H_

/* Set interrupt button port */
#define CFG_TRIGGER_MEASUREMENT_BUTTON_GPIO_PORT        (HW_GPIO_PORT_1)
/* Set interrupt button pin */
#define CFG_TRIGGER_MEASUREMENT_BUTTON_GPIO_PIN         (HW_GPIO_PIN_6)

/* Interval between next intermediate blood cuff pressure measurement in [ms] */
#define CFG_INTER_CUFF_TIME_INTERVAL_MS                 (1000)

/* Maximum number of user measurements stored in queue */
#define CFG_MAX_USER_MEASURMENTS_COUNT                  (20)

#endif /* BLP_SENSOR_CONFIG_H_ */
