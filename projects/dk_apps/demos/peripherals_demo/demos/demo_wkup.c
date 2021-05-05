/**
 ****************************************************************************************
 *
 * @file demo_wkup.c
 *
 * @brief Wakeup timer demo (hw_wkup driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <hw_gpio.h>
#include <hw_wkup.h>
#include "common.h"
#include "config.h"
#include "gpio_setup.h"

static void wkup_intr_cb(void)
{
        /*
         * Interrupt handler should always reset interrupt state, otherwise it will be called again.
         */
        hw_wkup_reset_interrupt();
        printf("Wake up interrupt triggered\r\n");
}

void demo_wkup_init(void)
{
        hw_wkup_init(NULL);

#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        /*
         * Default value for counter threshold is 0. It's important to change this value before
         * interrupt is registered as otherwise interrupt will be triggered indefinitely due to
         * counter value being equal to threshold.
         */
        hw_wkup_set_counter_threshold(1);
#endif
        hw_wkup_register_interrupt(wkup_intr_cb, 1);
}

/* helper to translate between predefined wkup pin and actual GPIO port/pin assigned */
static void wkup_pin_to_gpio(int wkup_pin, HW_GPIO_PORT *gpio_port, HW_GPIO_PIN *gpio_pin)
{
        switch (wkup_pin) {
        case 1:
                *gpio_port = CFG_GPIO_WKUP_1_PORT;
                *gpio_pin = CFG_GPIO_WKUP_1_PIN;
                break;
        case 2:
                *gpio_port = CFG_GPIO_WKUP_2_PORT;
                *gpio_pin = CFG_GPIO_WKUP_2_PIN;
                break;
        case 3:
                *gpio_port = CFG_GPIO_WKUP_3_PORT;
                *gpio_pin = CFG_GPIO_WKUP_3_PIN;
                break;
        }
}

void menu_wkup_pin_enabled_func(const struct menu_item *m, bool checked)
{
        int wkup_pin = (int) m->param;
        HW_GPIO_PORT port;
        HW_GPIO_PIN pin;
        bool enabled;

        /* new 'enabled' state is opposite of current state */
        enabled = !checked;

        wkup_pin_to_gpio(wkup_pin, &port, &pin);

        /*
         * It's also possible to configure pin state and trigger at once in a single call, see
         * hw_wkup_configure_pin() API for this.
         */
        hw_wkup_set_pin_state(port, pin, enabled);
}

bool menu_wkup_pin_enabled_checked_cb_func(const struct menu_item *m)
{
        int wkup_pin = (int) m->param;
        HW_GPIO_PORT port;
        HW_GPIO_PIN pin;

        wkup_pin_to_gpio(wkup_pin, &port, &pin);

        /*
         * There are also APIs which can retrieve state of all pins in port at once, see
         * menu_wkup_state_func() for an example.
         */
        return hw_wkup_get_pin_state(port, pin);
}

void menu_wkup_pin_trigger_func(const struct menu_item *m, bool checked)
{
        int wkup_pin = (int) m->param;
        HW_GPIO_PORT port;
        HW_GPIO_PIN pin;
        HW_WKUP_PIN_STATE state;

        /* new state is 'low' when menu item is currently selected, 'high' otherwise */
        state = checked ? HW_WKUP_PIN_STATE_LOW : HW_WKUP_PIN_STATE_HIGH;

        wkup_pin_to_gpio(wkup_pin, &port, &pin);

        /*
         * Note that counter is edge-sensitive. After triggering edge (i.e. pin goes to state which
         * is set as trigger) is detected, a reverse edge must be detected before timer goes back
         * to idle state and can detect and count another event.
         */
        hw_wkup_set_pin_trigger(port, pin, state);
}

bool menu_wkup_pin_trigger_checked_cb_func(const struct menu_item *m)
{
        int wkup_pin = (int) m->param;
        HW_GPIO_PORT port;
        HW_GPIO_PIN pin;

        wkup_pin_to_gpio(wkup_pin, &port, &pin);

        /*
         * There are also APIs which can retrieve state of all pins in port at once, see
         * menu_wkup_state_func() for an example.
         */
        return hw_wkup_get_pin_trigger(port, pin) == HW_WKUP_PIN_STATE_HIGH;
}

void menu_wkup_disable_all_func(const struct menu_item *m, bool checked)
{
        int i;

        for (i = 0; i < HW_GPIO_NUM_PORTS; i++) {
                /*
                 * This is a convenient shortcut to quickly configure all pins in GPIO port.
                 * Each bit in state and trigger mask configures enabled state and trigger state
                 * of corresponding pin.
                 */
                hw_wkup_configure_port(i, 0x00, 0x00);
        }
}

#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
void menu_wkup_threshold_func(const struct menu_item *m, bool checked)
{
        int threshold = (int) m->param;

        /*
         * Counter threshold defined number of events to be counted before interrupt is fired.
         */
        hw_wkup_set_counter_threshold(threshold);
}
#endif

void menu_wkup_debounce_func(const struct menu_item *m, bool checked)
{
        int debounce = (int) m->param;

        /*
         * Wakeup timer block offers hardware debouncer with configurable debounce time up to 63ms.
         * To disable, simply set debounce time to 0.
         */
        hw_wkup_set_debounce_time(debounce);
}

#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
void menu_wkup_reset_func(const struct menu_item *m, bool checked)
{
        /*
         * Counter always resets automatically once threshold is reached, but it can be also reset
         * manually if needed.
         */
        hw_wkup_reset_counter();
}
#endif

void menu_wkup_keyhit_func(const struct menu_item *m, bool selected)
{
        /*
         * It's possible to simulate single event which will increase events counter. This is useful
         * e.g. for testing since there's no need to configure and connect any GPIOs. Simulated
         * event does however take debounce time into account (i.e. event is counted after debounce
         * timer expires).
         */
        hw_wkup_emulate_key_hit();
}

void menu_wkup_state_func(const struct menu_item *m, bool selected)
{
        int port, pin, count = 0;

        printf(NEWLINE "==== hw_wkup state ====");
        printf(NEWLINE "configurable pins:");
        printf(NEWLINE "    GPIO #1 = P%d.%d", CFG_GPIO_WKUP_1_PORT, CFG_GPIO_WKUP_1_PIN);
        printf(NEWLINE "    GPIO #2 = P%d.%d", CFG_GPIO_WKUP_2_PORT, CFG_GPIO_WKUP_2_PIN);
        printf(NEWLINE "    GPIO #3 = P%d.%d", CFG_GPIO_WKUP_3_PORT, CFG_GPIO_WKUP_3_PIN);
        printf(NEWLINE "active pins:");
        for (port = 0; port < HW_GPIO_NUM_PORTS; port++) {
                uint8_t state, trigger;

                /*
                 * There are "shortcut" APIs available to manage configuration for whole GPIO port
                 * at once without need to configure every single pin separately. They accept or
                 * return a bitmask of states where each bit corresponds to appropriate pin in GPIO
                 * port.
                 *
                 * Note that even though these APIs have HW_GPIO_PORT enum used as a parameter, it's
                 * safe to call them with numeric value since they map directly to port number as
                 * expected.
                 */
                state = hw_wkup_get_port_state(port);
                trigger = hw_wkup_get_port_trigger(port);

                for (pin = 0 ; pin < hw_gpio_port_num_pins[port]; pin++) {
                        /* n-th pin state in port is simply value of n-th bit in bitmask */
                        if (!(state & (1 << pin))) {
                                continue;
                        }

                        printf(NEWLINE "    P%d.%d trigger on %s state", port, pin,
                                                        (trigger & (1 << pin)) ? "high" : "low");
                        count++;
                }
        }
        if (count == 0) {
                printf(NEWLINE "    (none)");
        }

#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        printf(NEWLINE "threshold: %d", hw_wkup_get_counter_threshold());
#endif
        printf(NEWLINE "debounce: %dms", hw_wkup_get_debounce_time());
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        printf(NEWLINE "counter: %d", hw_wkup_get_counter());
#endif
        printf(NEWLINE "========= /// =========");
}
