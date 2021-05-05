/**
 ****************************************************************************************
 *
 * @file demo_breath.c
 *
 * @brief Breath timer demo (hw_breath driver)
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_breath.h>
#include <hw_led.h>
#include "common.h"

static void setup_breath(uint8_t dc_min, uint8_t dc_max, uint8_t dc_step, uint8_t freq_div)
{
        breath_config config = {
                .dc_min = dc_min,
                .dc_max = dc_max,
                .dc_step = dc_step,
                .freq_div = freq_div,
                .polarity = HW_BREATH_PWM_POL_POS
        };

        /*
         * Setup breath timer configuration, so hardware can drive LED automatically.
         */
        hw_breath_init(&config);

        /*
         * Setup LED1 output to be driven by timer.
         */
        hw_led_set_led1_src(HW_LED_SRC1_BREATH);
        hw_led_enable_led1(true);

        /*
         * Then start breath timer.
         */
        hw_breath_enable();
}

void menu_breath_constant_dim_func(const struct menu_item *m, bool checked)
{
        /*
         * With those parameters, breath timer is setup to generate constant
         * PWM with 12% duty cycle (30 / 256 * 100%). It could be used to
         * drive power LED when device is not in standby mode.
         */
        setup_breath(29, 30, 255, 255);
}

void menu_breath_constant_bright_func(const struct menu_item *m, bool checked)
{
        /*
         * With those parameters, breath timer is setup to generate constant
         * PWM with 94% duty cycle (240 / 256 * 100%). It could be used to
         * drive power LED when device is not in standby mode.
         */
        setup_breath(239, 240, 1, 255);
}

void menu_breath_emergency_blink_func(const struct menu_item *m, bool checked)
{
        /*
         * Following configuration allows to setup breath timer settings with maximum PWM
         * changing from 0 to 100% at speed around 4 times per second.
         * Those settings could be used for emergency blinking mode.
         */
        setup_breath(0, 255, 32, 255);
}

void menu_breath_dim_standby_breath_func(const struct menu_item *m, bool checked)
{
        /*
         * Following configuration allows to setup breath timer settings with maximum PWM
         * duty cycle reduce to 31% (80 * 100 / 256).
         * Those settings could be used for standby mode if allowing 100% maximum duty cycle
         * is to bright. With this settings, LED will blink 3 times in 2 seconds.
         */
        setup_breath(0, 80, 255, 255);
}

void menu_breath_standby_breath_func(const struct menu_item *m, bool checked)
{
        /*
         * Following configuration allows to setup slowest possible breath timer settings.
         * PWM duty cycle changes form 0 to 100% and back in around 2 seconds, generating
         * PWM suitable for unobtrusive standby LED.
         */
        setup_breath(0, 255, 255, 255);
}

void menu_breath_stop_func(const struct menu_item *m, bool checked)
{
        /*
         * Stop generating PWM.
         */
        hw_breath_disable();
}
