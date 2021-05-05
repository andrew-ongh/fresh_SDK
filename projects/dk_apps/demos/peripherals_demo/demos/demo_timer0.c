/**
 ****************************************************************************************
 *
 * @file demo_timer0.c
 *
 * @brief Timer0 demo (hw_timer0 driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_timer0.h>
#include "common.h"

static int blink_counter = 0;

/*
 * Time0 callback
 *
 * This function is called from interrupt context when T0 clock reloads initial value and NO clock
 * is finished.
 * In this example interrupt is used to disable timer0 after blink_counter timer reaches 0.
 */
static void timer0_interrupt_cb(void)
{
        if (blink_counter == 0) {
                return;
        }

        if (--blink_counter == 0) {
                hw_timer0_disable();
                hw_timer0_unregister_int();
        }
}

/*
 * Blink LED several times using 50% PWM and interrupt.
 */
static void timer0_blink_led(int count, bool bright)
{
        /*
         * Set initial values for timer0
         */
        hw_timer0_init(NULL);

        /*
         * 32kHz clock is enough for lighting LED in PWM mode
         */
        hw_timer0_set_clock_source(HW_TIMER0_CLK_SRC_SLOW);

        /*
         * It's possible to reduce brightness of blinking LED by additionally gating PWM with
         * clock signal.
         * For full brightness PWM mode is used.
         * For half brightness CLOCK mode that will change duty cycle to 50% when LED is on.
         */
        hw_timer0_set_pwm_mode(bright ? HW_TIMER0_MODE_PWM : HW_TIMER0_MODE_CLOCK);

        /*
         * Setup duty cycle for 50%, in a way that positive pulse lights led, negative turns it of.
         * Values passed to hw_timer0_set_t0_reload() function tell how many clock cycles
         * PWM output should be high and how many cycles should be low.
         * For 32kHz clock, both values must be set to 32767 to have 1s pulse width, f = 0.5Hz.
         */
        hw_timer0_set_t0_reload(0x7FFF, 0x7FFF);

        /*
         * Register interrupt callback that will be called whenever ON clock reaches zero
         * and t0 ends its cycle.
         */
        hw_timer0_register_int(timer0_interrupt_cb);

        /*
         * If number of required blinks is lower than 10 it is possible to divide ON clock
         * by 10 and have only two interrupts generated for blink series.
         */
        if (count < 10) {
                /*
                 * Set additional division of ON clock by 10.
                 */
                hw_timer0_set_on_clock_div(true);
                /*
                 * Compute value of ON clock so it will trigger interrupt after several blinks.
                 * While blink time is 2s (1s on 1s off), clock count for this is 65536.
                 * Dividing this by 10 gives 6553 (rounded down since we rather have timer ON
                 * finish sooner than later.
                 */
                hw_timer0_set_on_reload(count * 6553);
                /*
                 * Having set divider, we can wait for just one interrupt.
                 */
                blink_counter = 1;
        } else {
                /*
                 * Can't setup interrupt to be triggered after all sequence.
                 * Do not divide ON clock.
                 */
                hw_timer0_set_on_clock_div(false);
                /*
                 * Setup ON counter to be less then 0x8000 + 0x8000, this will
                 * trigger interrupt after each T0 cycle.
                 * 200 does not matter here as long as it less then 2 * 0x8000
                 */
                hw_timer0_set_on_reload(200);

                /*
                 * In this mode number of interrupts will match requested blink count.
                 */
                blink_counter = count;
        }

        /*
         * Interrupt will be generated as soon as timer is started so add 1 to variable that
         * which will checked for 0 in interrupt handler.
         */
        blink_counter++;


        /*
         * Enable timer.
         */
        hw_timer0_enable();
}

/*
 * Light LED with specified brightness using PWM duty cycle
 */
static void timer0_light_led(uint8_t brightness)
{
        /*
         * Set initial values for timer0.
         */
        hw_timer0_init(NULL);

        /*
         * 32kHz clock is enough for lighting LED in PWM mode.
         */
        hw_timer0_set_clock_source(HW_TIMER0_CLK_SRC_SLOW);
        hw_timer0_set_pwm_mode(HW_TIMER0_MODE_PWM);

        /*
         * brightness is in range 0-100, so we can use it to set
         * duty cycle directly.
         */
        hw_timer0_set_t0_reload(brightness, 100 - brightness);

        /*
         * Enable timer.
         */
        hw_timer0_enable();
}

void menu_timer0_blink_led_func(const struct menu_item *m, bool checked)
{
        timer0_blink_led((int) m->param, true);
}

void menu_timer0_blink_led_dim_func(const struct menu_item *m, bool checked)
{
        timer0_blink_led((int) m->param, false);
}

void menu_timer0_light_led_func(const struct menu_item *m, bool checked)
{
        timer0_light_led((int) m->param);
}

/*
 * Use LED for low power indication, (short blink once in a while)
 */
void menu_timer0_slow_blink_func(const struct menu_item *m, bool checked)
{
        /*
         * Set initial values for timer0.
         */
        hw_timer0_init(NULL);

        /*
         * 32kHz clock is enough for lighting LED in PWM mode.
         */
        hw_timer0_set_clock_source(HW_TIMER0_CLK_SRC_SLOW);
        hw_timer0_set_pwm_mode(HW_TIMER0_MODE_PWM);

        /*
         * Set on time to long enough for end user to see, and off time to maximum value.
         * In this setup low power mode can be signaled without CPU usage.
         */
        hw_timer0_set_t0_reload(500, 0xFFFF);

        /*
         * Enable timer.
         */
        hw_timer0_enable();
}

void menu_timer0_turn_off_func(const struct menu_item *m, bool checked)
{
        /*
         * Disable timer.
         */
        hw_timer0_disable();
}
