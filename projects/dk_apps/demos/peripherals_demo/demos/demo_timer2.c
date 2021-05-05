/**
 ****************************************************************************************
 *
 * @file demo_timer2.c
 *
 * @brief Timer2 demo (hw_timer2 driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_timer2.h>
#include "common.h"
#include "config.h"

void demo_timer2_init(void)
{
        /*
         * Initialize timer 2. This effectively stops the timer and zeroes PWM duty registers.
         */
        static timer2_config cfg = {
                .frequency = 0,
                .pwm2_end = 0,
                .pwm2_start = 0,
                .pwm3_end = 0,
                .pwm3_start = 0,
                .pwm4_end = 0,
                .pwm4_start = 0,
        };

        hw_timer2_init(&cfg);
        /*
         * Set division factor (divide by 4) for timer2. Clock (timer2_clock) is operating at
         * frequency calculated from system_clock / div_factor = timer2_clock in this case:
         * 16MHz / 4 = 4MHz. System_clock equals always 16MHz in case of timer2.
         */
        hw_timer2_set_division_factor(HW_TIMER2_DIV_4);
        /*
         * Set PWM's frequency to 100 which means that it is operating at 4MHz / 100 = 40kHz.
         */
        hw_timer2_set_frequency(100);
        /*
         * Start timer 2. PWM's duty cycles have default value of 0.
         */
        hw_timer2_enable();
}

void menu_timer2_pwm_freq_func(const struct menu_item *m, bool checked)
{
        /*
         * This value (2 <= pwm_freq <= (2^14 - 1)) divides input clock for generating output
         * frequency for PWMs signals = timer2_clock / pwm_freq [MHz]. For example if pwm_freq = 200
         * then PWM frequency = 4MHz / 200 = 20kHz (timer2_clock = 4MHz in this demo, see
         * demo_timer2_init).
         */
        uint16_t pwm_freq = (int) m->param;

        hw_timer2_set_frequency(pwm_freq);
}

/*
 * Set PWM's duty cycles with using start and end values that determines duty cycle and can be set
 * arbitrary inside PWM cycle.
 */
void menu_timer2_pwm_dc_func(const struct menu_item *m, bool checked)
{
        uint16_t pwm_freq_q;

        /*
         * Timer2 generates 3 PWM signals with the same period but allows to manually specify start
         * and end position of high state inside single cycle for each PWM separately. Each cycle
         * has total length of \p pwm_freq ticks. PWM signal it set to high at the beginning of
         * \p start tick and set to low at the beginning of \p end tick.
         *
         * For example:
         *
         * tick=  0          pwm_freq-1
         *        |<-- period -->|
         *    #1  HHHH............
         *    #2  ......HHHH......
         *    #3  ............HHHH
         *    #4  HH............HH
         *
         * In this case with pwm_freq = 16:
         *    #1 start= 0, end= 4
         *    #2 start= 6, end=10
         *    #3 start=12, end=16 (or end = 0, both have the same result)
         *    #4 start=14, end= 2
         *
         * Note that if \p end is larger than \p pwm_freq, PWM state will never be changed to low.
         */

        /*
         * Following example will set each PWM to have 25% duty cycle and high state will start at
         * the beginning, at 1/4 and in the middle of period, i.e.:
         *
         * PWM2 is HH......
         * PWM3 is ..HH....
         * PWM4 is ....HH..
         */
        pwm_freq_q = hw_timer2_get_frequency() / 4;    // 1/4 of period
        hw_timer2_set_pwm_start_end(HW_TIMER2_PWM_2, 0, pwm_freq_q);
        hw_timer2_set_pwm_start_end(HW_TIMER2_PWM_3, pwm_freq_q, pwm_freq_q * 2);
        hw_timer2_set_pwm_start_end(HW_TIMER2_PWM_4, pwm_freq_q * 2 , pwm_freq_q * 3);
}

/*
 * Light RGB LED with chosen color. Colors are controlled in this way: PWM2 -> Red, PWM3 -> Green
 * and PWM4 -> Blue.
 */
void menu_timer2_pwm_light_rgb_func(const struct menu_item *m, bool checked)
{
        int color;
        uint8_t color_r, color_g, color_b;
        uint16_t pwm_freq;

        pwm_freq = hw_timer2_get_frequency();

        /*
         * Get m->param as RGB color in form 0xRRGGBB where RR, GG and BB are values of primary
         * colors. As a results there is a possibility to set any color from RGB palette.
         */
        color = (int) m->param;         // 0xRRGGBB
        color_r = color >> 16;          // 0xRR, red
        color_g = color >> 8;           // 0xGG, green
        color_b = color;                // 0xBB, blue

        /*
         * Set PWM duty cycle that color is scaled to actual pwm_freq. When color value = 0x00 (min)
         * then duty cycle = 0% and when color value = 0xFF (max) then duty cycle = 100%.
         */
        hw_timer2_set_pwm_duty_cycle(HW_TIMER2_PWM_2, pwm_freq * color_r / 255);
        hw_timer2_set_pwm_duty_cycle(HW_TIMER2_PWM_3, pwm_freq * color_g / 255);
        hw_timer2_set_pwm_duty_cycle(HW_TIMER2_PWM_4, pwm_freq * color_b / 255);
}

static inline int get_clock_divider(HW_TIMER2_DIV div)
{
        /*
         * We need to convert clock division factor (\p div) returned from driver to actual number
         * by which clock is divided. This is a simple translation between enum symbol and scalar
         * value.
         *
         * \note Due to values assigned to enum symbols, this is actually equivalent to '1 << div'
         * and can be used as replacement, however using actual symbols is just to make sure
         * calculation is up to date with current driver code.
         */
        switch (div) {
        case HW_TIMER2_DIV_1:
                return 1;

        case HW_TIMER2_DIV_2:
                return 2;

        case HW_TIMER2_DIV_4:
                return 4;

        case HW_TIMER2_DIV_8:
                return 8;
        }

        /* should not happen, we can return 0 */
        return 0;
}

void menu_timer2_pwm_state_func(const struct menu_item *m, bool checked)
{
        uint32_t timer2_clock;
        uint16_t pwm_freq;

        printf(NEWLINE "==== hw_timer2 state ====" NEWLINE);

        /*
         * Actual PWM frequency expressed in Hz can be calculated using clock division factor (i.e.
         * 16MHz clock divider) and internal PWM frequency which is basically divider for input
         * clock. So this is basically: 16MHz / timer2_clock_div / pwm_freq.
         */
        timer2_clock = 16000000 / get_clock_divider(hw_timer2_get_division_factor());
        pwm_freq = hw_timer2_get_frequency();
        printf("PWM frequency: %ld Hz" NEWLINE, timer2_clock / pwm_freq);

        /*
         * Print current duty cycles of all PWMs.
         * Duty cycle is returned as value in range 0..pwm_freq, so it should be scaled accordingly
         * to have percentage value.
         */
        printf("PWM duty cycles:" NEWLINE);
        printf("    PWM2 = %d %%" NEWLINE,
                                (hw_timer2_get_pwm_duty_cycle(HW_TIMER2_PWM_2) * 100) / pwm_freq);
        printf("    PWM3 = %d %%" NEWLINE,
                                (hw_timer2_get_pwm_duty_cycle(HW_TIMER2_PWM_3) * 100) / pwm_freq);
        printf("    PWM4 = %d %%" NEWLINE,
                                (hw_timer2_get_pwm_duty_cycle(HW_TIMER2_PWM_4) * 100) / pwm_freq);

        printf("========= /// =========" NEWLINE);
}

/*
 * Software pause or resume for PWM generation.
 */
void menu_timer2_pause_func(const struct menu_item *m, bool checked)
{
        /*
         * Timer should be resumed when 'checked' is true (timer is paused) and paused when
         * 'checked' is false (timer is running).
         */
        if (checked) {
                hw_timer2_set_sw_pause(false);
        } else {
                hw_timer2_set_sw_pause(true);
        }
}

bool menu_timer2_pause_checked_cb_func(const struct menu_item *m)
{
        return hw_timer2_get_sw_pause();
}
