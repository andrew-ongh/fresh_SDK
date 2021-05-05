/**
 ****************************************************************************************
 *
 * @file power_demo_settings.c
 *
 * @brief Power Demo Settings
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
#include "hw_gpio.h"
#include "power_demo_settings.h"

#define NELEM(x) (sizeof(x) / sizeof(x[0]))

typedef struct {
        uint8_t port;
        uint8_t pin;
} pin_config_t;

/*
 * Advertising interval configurations
 */
const adv_interval_config_t adv_interval_config[] = {
        { BLE_ADV_INTERVAL_FROM_MS(400), BLE_ADV_INTERVAL_FROM_MS(600) },
        { BLE_ADV_INTERVAL_FROM_MS(30), BLE_ADV_INTERVAL_FROM_MS(60) },
        { BLE_ADV_INTERVAL_FROM_MS(1000), BLE_ADV_INTERVAL_FROM_MS(1200) },
};

/*
 * Channel map configurations
 */
const uint8_t channel_map_config[] = {
        ( GAP_ADV_CHANNEL_37 | GAP_ADV_CHANNEL_38 | GAP_ADV_CHANNEL_39 ),
        ( GAP_ADV_CHANNEL_38 | GAP_ADV_CHANNEL_39 ),
};

/*
 * Recharge period values
 */
const uint16_t recharge_period_config[] = {
        ( 3000 ),
        ( 100 ),
        ( 900 ),
};

/*
 * Connection parameters configurations
 */
const gap_conn_params_t conn_params_config[] = {
        { BLE_CONN_INTERVAL_FROM_MS(400), BLE_CONN_INTERVAL_FROM_MS(600), 0, BLE_SUPERVISION_TMO_FROM_MS(1500) },
        { BLE_CONN_INTERVAL_FROM_MS(10), BLE_CONN_INTERVAL_FROM_MS(15), 0, BLE_SUPERVISION_TMO_FROM_MS(100) },
        { BLE_CONN_INTERVAL_FROM_MS(1000), BLE_CONN_INTERVAL_FROM_MS(1200), 0, BLE_SUPERVISION_TMO_FROM_MS(3000) },
};

/*
 * Index pin configuration. User can select configuration index using pins below.
 * Index returned from get_config_index() is a bit mask of selected pins.
 */
const pin_config_t index_pin_config[] = {
        {HW_GPIO_PORT_1, HW_GPIO_PIN_5},
        {HW_GPIO_PORT_1, HW_GPIO_PIN_7},
};

/*
 * Type of configuration. User can select configuration type using pins below.
 * I.e. if no pin is selected, function get_config_type() returns CONFIG_TYPE_ADV_INTERVAL.
 * If both pins are selected, function returns CONFIG_TYPE_CONN_PARAM_UPDATE.
 */
const pin_config_t type_pin_config[] = {
        {HW_GPIO_PORT_1, HW_GPIO_PIN_2},
        {HW_GPIO_PORT_1, HW_GPIO_PIN_4},
};

const adv_interval_config_t * get_adv_interval_config(int idx)
{
        if ((idx < 0) || (NELEM(adv_interval_config) <= idx)) {
                return NULL;
        }

        return &adv_interval_config[idx];
}

bool get_channel_map_config(int idx, uint8_t *chnl_map)
{
        if ((idx < 0) || (NELEM(channel_map_config) <= idx)) {
                return false;
        }

        *chnl_map = channel_map_config[idx];

        return true;
}

bool get_recharge_period_config(int idx, uint16_t *recharge_period)
{
        if ((idx < 0) || (NELEM(recharge_period_config) <= idx)) {
                return false;
        }

        *recharge_period = recharge_period_config[idx];

        return true;
}

const gap_conn_params_t * get_conn_params_config(int idx)
{
        if ((idx < 0) || (NELEM(conn_params_config) <= idx)) {
                return NULL;
        }

        return &conn_params_config[idx];
}

int get_config_index(void)
{
        int i, index = 0;

        for (i = 0; i < NELEM(index_pin_config); i++) {
                if (hw_gpio_get_pin_status(index_pin_config[i].port, index_pin_config[i].pin)) {
                        index |= (1 << i);
                }
        }

        return index;
}

config_type_t get_config_type(void)
{
        config_type_t type = CONFIG_TYPE_ADV_INTERVAL;
        int i;

        for (i = 0; i < NELEM(type_pin_config); i++) {
                if (hw_gpio_get_pin_status(type_pin_config[i].port, type_pin_config[i].pin)) {
                        type |= (1 << i);
                }
        }

        return type;
}

void init_gpios(void)
{
        int i;

        for (i = 0; i < NELEM(index_pin_config); i++) {
                hw_gpio_configure_pin(index_pin_config[i].port, index_pin_config[i].pin,
                                        HW_GPIO_MODE_INPUT_PULLDOWN, HW_GPIO_FUNC_GPIO, 0);
        }

        for (i = 0; i < NELEM(type_pin_config); i++) {
                hw_gpio_configure_pin(type_pin_config[i].port, type_pin_config[i].pin,
                                        HW_GPIO_MODE_INPUT_PULLDOWN, HW_GPIO_FUNC_GPIO, 0);
        }
}
