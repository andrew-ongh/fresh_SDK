/**
 ****************************************************************************************
 *
 * @file demo_ad_gpadc.c
 *
 * @brief General Purpose ADC demo (hw_gpadc driver)
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_tempsens.h>
#include <ad_gpadc.h>
#include "common.h"
#include "config.h"
#include "platform_devices.h"

#if CFG_DEMO_AD_GPADC

static bool cyclic_read_enabled;

typedef struct {
        OS_EVENT event;
        uint16_t result_val;
} user_data_t;

void demo_ad_gpadc_init(void)
{
        /*
         * Temperature sensor will be used in demo so there is a necessity to enable it. It will be
         * enabled since demo is running.
         */
        hw_tempsens_enable();
}

static int convert_adc_to_mv(gpadc_source src, uint16_t value)
{
        gpadc_source_config *cfg = (gpadc_source_config *)src;
        const uint16 adc_src_max =  ad_gpadc_get_source_max(src);
        uint32_t mv_src_max = (cfg->hw_init.input_attenuator == HW_GPADC_INPUT_VOLTAGE_UP_TO_1V2) ?
                                                                                        1200 : 3600;

        int ret = 0;

        switch (cfg->hw_init.input_mode) {
        case HW_GPADC_INPUT_MODE_SINGLE_ENDED:
                if (cfg->hw_init.input == HW_GPADC_INPUT_SE_VBAT) {
                        mv_src_max = 5000;
                }
                ret =  (mv_src_max * value) / adc_src_max;
                break;
        case HW_GPADC_INPUT_MODE_DIFFERENTIAL:
                ret = ((int)mv_src_max * (value - (adc_src_max >> 1))) / (adc_src_max >> 1);
                break;
        default:
                /* Invalid input mode */
                OS_ASSERT(0);
        }

        return ret;
}

void task_ad_gpadc_worker_func(const struct task_item *task)
{
        gpadc_source src;
        uint16_t value;

        /*
         * Open source connected to GPADC.
         * This will not start any measurement yet.
         */
        src = ad_gpadc_open(LIGHT_SENSOR);

        for (;;) {
                OS_DELAY(OS_MS_2_TICKS(1000));

                if (cyclic_read_enabled) {
                        ad_gpadc_read(src, &value);

                        printf(NEWLINE "Light: %d[mV]", convert_adc_to_mv(src, value));
                }
        }

        /*
         * This task will not use GPADC for time being, close it.
         */
        ad_gpadc_close(src);
}

static void read_gpadc_cb(void *user_data, int value)
{
        user_data_t *ud = (user_data_t *) user_data;

        ud->result_val = value;

        OS_EVENT_SIGNAL_FROM_ISR(ud->event);
}

void menu_gpadc_read_func(const struct menu_item *m, bool checked)
{
        user_data_t user_data;
        gpadc_source src = NULL;
        int device_id = (int) m->param;

        OS_EVENT_CREATE(user_data.event);

        switch (device_id) {
        case GPADC_BATTERY_LEVEL_ID:
                src = ad_gpadc_open(BATTERY_LEVEL);
                break;

        case GPADC_TEMP_SENSOR_ID:
                src = ad_gpadc_open(TEMP_SENSOR);
                break;

        case GPADC_LIGHT_SENSOR_ID:
                src = ad_gpadc_open(LIGHT_SENSOR);
                break;

        case GPADC_ENCODER_SENSOR_ID:
                src = ad_gpadc_open(ENCODER_SENSOR);
                break;

        default:
                /* Only the above device_id are supported by this program */
                OS_ASSERT(0);
        }

        ad_gpadc_read_async(src, read_gpadc_cb, &user_data);

        printf(NEWLINE "Waiting for the result ...");
        OS_EVENT_WAIT(user_data.event, OS_EVENT_FOREVER);

        OS_EVENT_DELETE(user_data.event);

        switch (device_id) {
        case GPADC_BATTERY_LEVEL_ID:
                printf(NEWLINE "Voltage: %d[mV]", convert_adc_to_mv(src, user_data.result_val));
                break;
        case GPADC_TEMP_SENSOR_ID:
                printf(NEWLINE "Temperature: %d[C]", hw_tempsens_convert_to_temperature(user_data.result_val));
                break;
        case GPADC_ENCODER_SENSOR_ID:
                printf(NEWLINE "Voltage: %d[mV]", convert_adc_to_mv(src, user_data.result_val));
                break;
        default:
                printf(NEWLINE "Voltage: %d[mV]", convert_adc_to_mv(src, user_data.result_val));
        }

        ad_gpadc_close(src);
}

void menu_gpadc_read_cyclic_func(const struct menu_item *m, bool checked)
{
        cyclic_read_enabled ^= true;
}

bool menu_gpadc_read_cyclic_checked_cb_func(const struct menu_item *m)
{
        return cyclic_read_enabled;
}

#endif // CFG_DEMO_AD_GPADC
