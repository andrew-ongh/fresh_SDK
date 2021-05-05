/**
 ****************************************************************************************
 *
 * @file kbscn_task.c
 *
 * @brief Keyboard Scanner demo task
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include "osal.h"
#include "sys_power_mgr.h"
#include "hw_gpio.h"
#include "ad_keyboard_scanner.h"

static const ad_kbscn_pin_setup kbscn_row[] = {
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_0),
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_1),
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_2),
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_3),
};

static const ad_kbscn_pin_setup kbscn_col[] = {
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_4),
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_5),
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_6),
        AD_KBSCN_PIN_SETUP(HW_GPIO_PORT_4, HW_GPIO_PIN_7),
};

static const char kbscn_matrix[] = {
        '1', '2', '3', 'A',
        '4', '5', '6', 'B',
        '7', '8', '9', 'C',
        '*', '0', '#', 'D',
};

static void kbscn_cb(AD_KBSCN_EVENT event, char c);
/*
static const ad_kbscn_config kbscn_config = AD_KBSCN_CONFIG_WITH_INACTIVE_TIME(kbscn_row, kbscn_col, kbscn_matrix,
                                                                AD_KBSCN_CLOCK_DIV_16, 150, 10, 10, 127,
                                                                kbscn_cb);
*/
static const ad_kbscn_config kbscn_config = AD_KBSCN_CONFIG(kbscn_row, kbscn_col, kbscn_matrix,
                                                                AD_KBSCN_CLOCK_DIV_16, 150, 10, 10,
                                                                kbscn_cb);

PRIVILEGED_DATA OS_TASK app_task;

/*
 * When cancel_sleep is true fake adapter always prevents platform from going to sleep.
 * This allows to check whether power management aspect of keyboard scanner adapter
 * has correct behavior when sleep was initiated and later canceled.
 */
PRIVILEGED_DATA bool cancel_sleep/* = false*/;

static bool prepare_for_sleep_cb(void)
{
        return !cancel_sleep;
}

static const adapter_call_backs_t pm_callbacks = {
        .ad_prepare_for_sleep = prepare_for_sleep_cb,
};

/*
 * KEY_FIFO_SIZE must be a power of 2, for best performance
 */
#define KEY_FIFO_SIZE 32
#if KEY_FIFO_SIZE & (KEY_FIFO_SIZE - 1)
#       error "KEY_FIFO_SIZE must be a power of 2"
#endif

PRIVILEGED_DATA static char key_fifo[KEY_FIFO_SIZE];
PRIVILEGED_DATA static size_t key_fifo_h;
PRIVILEGED_DATA static size_t key_fifo_t;

static void kbscn_cb(AD_KBSCN_EVENT event, char c)
{
        if (event != AD_KBSCN_EVENT_PRESSED) {
                return;
        }

        key_fifo[key_fifo_t] = c;
        key_fifo_t++;
        key_fifo_t %= KEY_FIFO_SIZE;

        OS_TASK_NOTIFY_FROM_ISR(app_task, 1, OS_NOTIFY_SET_BITS);
}

void kbscn_task(void *params)
{
        bool stay_alive = false;
        bool ret;

        app_task = OS_GET_CURRENT_TASK();

        pm_register_adapter(&pm_callbacks);
        ret = ad_kbscn_init(&kbscn_config);
        ASSERT_WARNING(ret);

        for (;;) {
                uint32_t notif;

                OS_TASK_NOTIFY_WAIT(0, (uint32_t) -1, &notif, OS_TASK_NOTIFY_FOREVER);

                while (true) {
                        OS_ENTER_CRITICAL_SECTION();
                        if (key_fifo_h == key_fifo_t) {
                                OS_LEAVE_CRITICAL_SECTION();
                                break;
                        }
                        OS_LEAVE_CRITICAL_SECTION();

                        char c = key_fifo[key_fifo_h];
                        key_fifo_h++;
                        key_fifo_h %= KEY_FIFO_SIZE;

                        printf("%c", c);
                        fflush(stdout);
                        if (c == '*') {
                                if (stay_alive) {
                                        printf ("Resume sleep\n");
                                        pm_resume_sleep();
                                } else {
                                        printf ("Stay alive\n");
                                        pm_stay_alive();
                                }
                                stay_alive = !stay_alive;
                        } else if (c == '#') {
                                cancel_sleep = !cancel_sleep;
                                if (cancel_sleep) {
                                        printf ("Cancel sleep enabled\n");
                                } else {
                                        printf ("Cancel sleep disabled\n");
                                }
                        }
                }
        }
}
