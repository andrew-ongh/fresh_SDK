/**
 ****************************************************************************************
 *
 * @file power_demo_settings.h
 *
 * @brief Power Demo Settings configuration
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef POWER_DEMO_SETTINGS_H_
#define POWER_DEMO_SETTINGS_H_

#include "ble_gap.h"

/**
 * BUTTON configuration. It will be used with GPIO configuration enabled
 */
#define CFG_BUTTON_TRIGGER_GPIO_PORT    (HW_GPIO_PORT_1)
#define CFG_BUTTON_TRIGGER_GPIO_PIN     (HW_GPIO_PIN_0)

/**
 * Advertising interval configuration
 */
typedef struct {
        uint16_t interval_min;
        uint16_t interval_max;
} adv_interval_config_t;

/**
 * Configuration type
 */
typedef enum {
        CONFIG_TYPE_ADV_INTERVAL = 0x00,
        CONFIG_TYPE_CHANNEL_MAP = 0x01,
        CONFIG_TYPE_RECHARGE_PERIOD = 0x02,
        CONFIG_TYPE_CONN_PARAM_UPDATE = 0x03,
} config_type_t;

/**
 * \brief Get advertising interval configuration
 *
 * Function returns advertising interval configuration with given index.
 *
 * \param [in]  idx             configuration index
 *
 * \return Advertising interval configuration or NULL if idx is not valid
 */
const adv_interval_config_t * get_adv_interval_config(int idx);

/**
 * \brief Get advertising channel map configuration
 *
 * Function gets advertising channel map configuration with given index.
 *
 * \param [in]  idx             configuration index
 * \param [out] chnl_map        advertising channel map
 *
 * \return True if configuration found, otherwise false
 */
bool get_channel_map_config(int idx, uint8_t *chnl_map);

/**
 * \brief Get recharge period configration
 *
 * Function gets recharge period configuration with given index.
 *
 * \param [in]  idx             configuration index
 * \param [out] recharge_period recharge period
 *
 * \return True if configuration found, otherwise false
 */
bool get_recharge_period_config(int idx, uint16_t *recharge_period);

/**
 * \brief Get connection parameters configuration
 *
 * Function returns connection params configuration with given index.
 *
 * \param [in]  idx             configuration index
 *
 * \return Connection parameters configuration or NULL if idx is not valid
 */
const gap_conn_params_t * get_conn_params_config(int idx);

/**
 * \brief Get configuration type
 *
 * Function gets configuration type selected by user
 *
 * \return Configuration type
 */
config_type_t get_config_type(void);

/**
 * \brief Get configuration index
 *
 * Function gets configuration index selected by user
 *
 * \return Configuration type
 */
int get_config_index(void);

/**
 * \brief Initialize GPIOs
 *
 * Function initialize GPIOs responsible for selecting configuration type and configuration index.
 */
void init_gpios(void);

#endif /* POWER_DEMO_SETTINGS_H_ */
