/**
 ****************************************************************************************
 *
 * @file htp_thermometer_config.h
 *
 * @brief Application configuration
 *
 * Copyright (C) 2017-2018 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef HTP_THERMOMETER_CONFIG_H_
#define HTP_THERMOMETER_CONFIG_H_

/*
 * Pin used for generating sample measurements
 */
#define CFG_MAKE_MEASUREMENTS_BUTTON_GPIO_PORT  (HW_GPIO_PORT_1)
#define CFG_MAKE_MEASUREMENTS_BUTTON_GPIO_PIN   (HW_GPIO_PIN_2)

/*
 * \brief Interval between consecutive temperature measurements
 *
 * User can enable periodic measurements and define a specific interval of periodicity. Valid values
 * range from 1 to 65535 seconds. If the value equals 0 then no periodic measurements will be done.
 */
#define CFG_HTS_MEAS_INTERVAL                   (0)      // 0 seconds - no periodic measurements

/*
 * \brief Lower inclusive value (lower bound)
 *
 * This is the lower bound of the interval range which limits the measurement interval value that
 * can be set by the connected client. This has to be equal to or lower than
 * CFG_HTS_MEAS_INTERVAL_HIGH_BOUND, otherwise Health Thermometer Service will not be properly
 * initialized.
 */
#define CFG_HTS_MEAS_INTERVAL_LOW_BOUND         (10)    // 10 seconds

/*
 * \brief Higher inclusive value (upper bound)
 *
 * This is the higher bound of the interval range which limits the measurement interval value that
 * can be set by the connected client. This has to be equal to or higher than
 * CFG_HTS_MEAS_INTERVAL_LOW_BOUND, otherwise Health Thermometer Service will not be properly
 * initialized.
 */
#define CFG_HTS_MEAS_INTERVAL_HIGH_BOUND        (600)   // 600 seconds (=10 minutes)

/*
 * Time in seconds after which, if there are no more measurements, the connection(s) with connected
 * user(s) will be terminated.
 *
 * \note CFG_HTS_CONN_IDLE_TIME is used only for autotests
 */
#ifndef CFG_HTS_CONN_IDLE_TIME
#define CFG_HTS_CONN_IDLE_TIME                  (30)    // 30 seconds
#elif (CFG_HTS_CONN_IDLE_TIME < 5)
#pragma message "HTP specification allows an idle connection timeout of at least 5 seconds."
#endif

/*
 * \brief Set maximum number of measurements to be stored
 *
 * In Health Thermometer Service there is a need to store data. For servers that support sending the
 * Temperature Measurement characteristic value indication at specified intervals using the
 * Measurement Interval characteristic, it is recommended that at least one day of time-stamped data
 * can be stored. If the device is configured to send data at intervals of 30 minutes over a 24-hour
 * period, this is equal to a recommendation of at least 48 stored data measurements. For more basic
 * scenarios, the storage of at least 30 data measurements is recommended.
 */
#define CFG_HTS_MAX_MEAS_TO_STORE               (30)    // 30 measurements

#endif /* HTP_THERMOMETER_CONFIG_H_ */
