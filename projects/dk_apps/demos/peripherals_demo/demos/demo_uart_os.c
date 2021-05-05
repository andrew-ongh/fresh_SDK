/**
 ****************************************************************************************
 *
 * @file demo_uart_os.c
 *
 * @brief UART demo (ad_uart adapter)
 *
 * Copyright (C) 2015-2018 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <platform_devices.h>
#include <hw_timer1.h>
#include "common.h"
#include "config.h"

struct worker_data {
        bool run;
        bool prompt;
        bool lock_uart;
        OS_EVENT event;
        const char *name;
        OS_TICK_TIME delay;
        bool uart_locked;
} worker_data[] = {
        { false, false, false, 0, "Message from worker 1" NEWLINE, 1000 },
        { false, false, false, 0, "Message from worker 2" NEWLINE, 500 },
};

void task_ad_uart_worker_init(const struct task_item *task)
{
      const int my_ix = (int) task->param;

      /*
       * Create event to wait on, when task should print nothing.
       */
      OS_EVENT_CREATE(worker_data[my_ix].event);
}

static const char *prompt = "Type something\r\n";
static char buf[10];

/*
 * Function called from task's main loop.
 */
void task_ad_uart_worker_func(const struct task_item *task)
{
        /*
         * Handle to serial port.
         */
        uart_device dev;

        /*
         * Get task private data.
         */
        struct worker_data *wd = &worker_data[(int) task->param];

        /*
         * Wait in OS friendly way for request to run.
         */
        while (!wd->run) {
              OS_EVENT_WAIT(wd->event, OS_EVENT_FOREVER);
        }

        /*
         * Get access to serial port.
         */
        dev = ad_uart_open(SERIAL2);


        /*
         * Stay in loop printing same string with task specific interval.
         */
        while (wd->run) {
                /*
                 * Just write to serial port.
                 */
                ad_uart_write(dev, wd->name, strlen(wd->name));

                /*
                 * Wait.
                 */
                OS_DELAY(wd->delay);

                /*
                 * If serial should be lock and it was not locked before, acquire bus.
                 */
                if (wd->lock_uart && !wd->uart_locked) {
                        /*
                         * This waits if necessary for access.
                         */
                        ad_uart_bus_acquire(dev);
                        wd->uart_locked = true; /* Now serial is mine */
                } else if (!wd->lock_uart && wd->uart_locked) {
                        /*
                         * If serial was locked and it should not be any more, release it.
                         */
                        ad_uart_bus_release(dev);
                        wd->uart_locked = false;
                }

                /*
                 * Now demonstrate how to lock serial port for task to get some user input.
                 * To make sure that after prompt was displayed no other task use this serial
                 * port, lock it first.
                 */
                if (wd->prompt) {
                        int cnt;

                        wd->prompt = false;

                        /*
                         * Acquire serial.
                         */
                        ad_uart_bus_acquire(dev);

                        /*
                         * Write prompt.
                         */
                        ad_uart_write(dev, prompt, strlen(prompt));

                        /*
                         * Read up to 10 bytes in 5s.
                         * This itself would lock access to serial port, but it was locked before
                         * to have writes undivided.
                         */
                        cnt = ad_uart_read(dev, buf, 10, 5000);

                        if (cnt > 0) {
                                ad_uart_write(dev, "You typed:", 10);
                                ad_uart_write(dev, buf, cnt);
                        } else {
                                ad_uart_write(dev, "Nothing typed", 13);
                        }
                        ad_uart_write(dev, NEWLINE, NEWLINE_SIZE);

                        /*
                         * Let other tasks use serial port again.
                         */
                        ad_uart_bus_release(dev);
                }
        }
        /*
         * This task will not use serial port for time being, close it.
         */
        ad_uart_close(dev);
}

void menu_ad_uart_worker_enable_func(const struct menu_item *m, bool checked)
{
        struct worker_data *wd = &worker_data[(int) m->param];

        /*
         * Toggle run flag.
         */
        wd->run ^= true;
        if (wd->run) {
                OS_EVENT_SIGNAL(wd->event);
        }
}

bool menu_ad_uart_worker_enable_checked_cb_func(const struct menu_item *m)
{
        struct worker_data *wd = &worker_data[(int) m->param];

        return wd->run;
}

void menu_ad_uart_prompt_func(const struct menu_item *m, bool checked)
{
        struct worker_data *wd = &worker_data[(int) m->param];

        /*
         * User interface requested input. Notify task and make sure it is running.
         */
        wd->prompt = true;
        if (!wd->run) {
                wd->run = true;
                OS_EVENT_SIGNAL(wd->event);
        }
}

bool menu_ad_uart_worker_lock_checked_cb_func(const struct menu_item *m)
{
        struct worker_data *wd = &worker_data[(int) m->param];

        return wd->lock_uart;
}

void menu_ad_uart_worker_lock_func(const struct menu_item *m, bool checked)
{
        struct worker_data *wd = &worker_data[(int) m->param];

        wd->lock_uart = !checked;
        if (!wd->run) {
                wd->run = true;
                OS_EVENT_SIGNAL(wd->event);
        }
}
