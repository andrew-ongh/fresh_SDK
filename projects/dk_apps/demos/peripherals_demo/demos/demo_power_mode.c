/**
 ****************************************************************************************
 *
 * @file demo_power_mode.c
 *
 * @brief Power Mode demo
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include "common.h"
#include "sys_power_mgr.h"
#include "hw_wkup.h"

PRIVILEGED_DATA static OS_EVENT event;
PRIVILEGED_DATA static bool wait_event_active   = false;
PRIVILEGED_DATA static bool platform_sleep      = false;

static void power_mode_xtal16m_ready_ind(void)
{
        platform_sleep = true;
}

/*
 * This adapter has been added just to get information whether the platform had gone to sleep or not
 */
static const adapter_call_backs_t power_mode_pm_call_backs = {
        .ad_prepare_for_sleep = NULL,
        .ad_sleep_canceled = NULL,
        .ad_wake_up_ind = NULL,
        .ad_xtal16m_ready_ind = power_mode_xtal16m_ready_ind,
        .ad_sleep_preparation_time = 0
};

static void power_mode_interrupt_cb(void)
{
        hw_wkup_reset_interrupt();

        if (wait_event_active) {
                wait_event_active = false;
                OS_EVENT_SIGNAL(event);
        }
}

void demo_power_mode_init(void)
{
        hw_wkup_init(NULL);

        hw_wkup_configure_pin(HW_GPIO_PORT_1, HW_GPIO_PIN_6, 1, HW_WKUP_PIN_STATE_LOW);

#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        hw_wkup_set_counter_threshold(1);
#endif
        hw_wkup_set_debounce_time(10);

        hw_wkup_register_interrupt(power_mode_interrupt_cb, 1);
}

static void power_mode_set_pm_mode(sleep_mode_t sleep_mode)
{
        pm_id_t adapter_id;

        adapter_id = pm_register_adapter(&power_mode_pm_call_backs);

        if (!event) {
                OS_EVENT_CREATE(event);
        }

        printf("Press the button to return...\r\n");

        pm_set_wakeup_mode(true);
        pm_set_sleep_mode(sleep_mode);
        wait_event_active = true;

        OS_EVENT_WAIT(event, OS_EVENT_FOREVER);

        /**
         * Set active mode to do not let the device go to sleep after wake up.
         */
        pm_set_wakeup_mode(true);
        pm_set_sleep_mode(pm_mode_active);

        if (platform_sleep) {
                platform_sleep = false;
                printf("Platform was awakened.\r\n");
        } else {
                printf("Platform didn't go to sleep.\r\n");
        }

        printf("Active Mode is set.\r\n");

        pm_unregister_adapter(adapter_id);
}

void menu_power_mode_active_func(const struct menu_item *m, bool checked)
{
        pm_set_wakeup_mode(true);
        pm_set_sleep_mode(pm_mode_active);
}

void menu_power_mode_extended_sleep_func(const struct menu_item *m, bool checked)
{
        power_mode_set_pm_mode(pm_mode_extended_sleep);
}

void menu_power_mode_hibernation_func(const struct menu_item *m, bool checked)
{
        power_mode_set_pm_mode(pm_mode_hibernation);
}

