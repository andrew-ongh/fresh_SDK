/**
 ****************************************************************************************
 *
 * @file demo_timer1.c
 *
 * @brief Timer1 demo (hw_timer1 driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_timer1.h>
#include <hw_gpio.h>
#include "common.h"

void menu_timer1_set_pwm_freq_func(const struct menu_item *m, bool checked)
{
        hw_timer1_set_pwm_freq((int) m->param);
}

void menu_timer1_set_pwm_dc_func(const struct menu_item *m, bool checked)
{
        uint16_t pwm_freq_val = 0;
        uint16_t pwm_dc_val = 0;

        /*
         * Need to add 1 to the hw_timer1_get_pwm_freq result to have an appropriate value for
         * calculating PWM duty cycle.
         *
         * Actual PWM duty cycle is pwm_dc = pwm_dc_val / (pwm_freq_val + 1)
         */
        pwm_freq_val = (hw_timer1_get_pwm_freq() + 1);

        switch ((int) m->param) {
        case 1:
                pwm_dc_val = pwm_freq_val / 4;          // divide by 4 to get 25 % duty cycle
                break;
        case 2:
                pwm_dc_val = pwm_freq_val / 2;          // divide by 2 to get 50 % duty cycle
                break;
        case 3:
                pwm_dc_val = pwm_freq_val * 3 / 4;      // multiply by 3/4 to get 75 % duty cycle
                break;
        }

        hw_timer1_set_pwm_duty_cycle(pwm_dc_val);
}
