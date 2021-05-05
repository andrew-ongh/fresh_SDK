/**
 ****************************************************************************************
 *
 * @file device_handler.c
 *
 * @brief HID Device demo application
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include "osal.h"
#include "hw_gpio.h"
#include "hids.h"
#include "common.h"

#define KEYBOARD_SCANNER_NOTIF          (0x20000000)
#define SIM_KEYBOARD_TIMER_NOTIF        (0x40000000)
#define SIM_MOUSE_TIMER_NOTIF           (0x80000000)

/* Reports for boot mode */
struct boot_report_mouse {
        uint8_t button1 : 1;
        uint8_t button2 : 1;
        uint8_t button3 : 1;
        uint8_t reserved : 5;
        int8_t x;
        int8_t y;
} __attribute__((packed));

struct boot_report_keyboard {
        uint8_t mod;
        uint8_t reserved;
        uint8_t key[6];
} __attribute__((packed));

struct boot_report_keyboard_out {
        uint8_t numlock : 1;
        uint8_t capslock : 1;
        uint8_t scrolllock : 1;
        uint8_t compose : 1;
        uint8_t kana : 1;
        uint8_t reserved : 3;
} __attribute__((packed));

/* Reports for report mode (note they are now the same as for boot mode due to report map set) */
struct report_01_in {
        uint8_t button1 : 1;
        uint8_t button2 : 1;
        uint8_t button3 : 1;
        uint8_t reserved : 5;
        int8_t x;
        int8_t y;
} __attribute__((packed));

struct report_02_in {
        uint8_t mod;
        uint8_t reserved;
        uint8_t key[6];
} __attribute__((packed));

struct report_02_out {
        uint8_t numlock : 1;
        uint8_t capslock : 1;
        uint8_t scrolllock : 1;
        uint8_t compose : 1;
        uint8_t kana : 1;
        uint8_t reserved : 3;
} __attribute__((packed));

__RETAINED static hids_protocol_mode_t hids_mode;

__RETAINED static OS_TIMER sim_keyboard_timer;

__RETAINED static OS_TIMER sim_mouse_timer;

__RETAINED static OS_TASK app_task;

static void timer_cb(OS_TIMER timer)
{
        uint32_t mask = OS_PTR_TO_UINT(OS_TIMER_GET_TIMER_ID(timer));

        OS_TASK_NOTIFY(app_task, mask, OS_NOTIFY_SET_BITS);
}

static void sim_keyboard_in(void)
{
        /*
         * Simulate press and release of Num Lock key. Since LED states are tracked by host, the
         * corresponding LED on any attached keyboard should toggle every time we send this (~1s).
         */

        if (hids_mode == HIDS_PROTOCOL_MODE_REPORT) {
                struct report_02_in kbd = { };

                kbd.key[0] = 0x53; /* Num Lock */
                hids_notify_input_report(hids, 0x02, sizeof(kbd), (uint8_t *) &kbd);
                kbd.key[0] = 0x00;
                hids_notify_input_report(hids, 0x02, sizeof(kbd), (uint8_t *) &kbd);
        } else {
                struct boot_report_keyboard kbd = { };

                kbd.key[0] = 0x53; /* Num Lock */
                hids_notify_boot_keyboard_input_report(hids, sizeof(kbd), (uint8_t *) &kbd);
                kbd.key[0] = 0x00;
                hids_notify_boot_keyboard_input_report(hids, sizeof(kbd), (uint8_t *) &kbd);
        }
}

static void sim_mouse_in(void)
{
        static int counter = 0;
        static int dx = 1;
        static int dy = 1;

        if (++counter > 100) {
                counter = 0;
        }

        if (counter == 25 || counter == 75) {
                dx *= -1;
        }

        if (counter == 0 || counter == 50) {
                dy *= -1;
        }

        if (hids_mode == HIDS_PROTOCOL_MODE_REPORT) {
                struct report_01_in mouse = { };

                mouse.x = 2 * dx;
                mouse.y = 2 * dy;
                hids_notify_input_report(hids, 0x01, sizeof(mouse), (uint8_t *) &mouse);
        } else {
                struct boot_report_mouse mouse = { };

                mouse.x = 2 * dx;
                mouse.y = 2 * dy;
                hids_notify_boot_mouse_input_report(hids, sizeof(mouse), (uint8_t *) &mouse);
        }
}

static void sim_keyboard_out(bool capslock)
{
        /*
         * Check Caps Lock state in received report and enable or disable D2 LED (connected to P1.5
         * on DK accordingly. This report can be triggered by pressing Caps Lock on any keyboard
         * attached to host.
         */

        if (capslock) {
                hw_gpio_set_active(HW_GPIO_PORT_1, HW_GPIO_PIN_5);
        } else {
                hw_gpio_set_inactive(HW_GPIO_PORT_1, HW_GPIO_PIN_5);
        }
}

void device_init(void)
{
        app_task = OS_GET_CURRENT_TASK();

        /* Keyboard timer ticks every 1 second to simulate some keyboard actions. */
        sim_keyboard_timer = OS_TIMER_CREATE("simk", OS_MS_2_TICKS(1000), true,
                                                OS_UINT_TO_PTR(SIM_KEYBOARD_TIMER_NOTIF), timer_cb);

        /* Mouse timer ticks every 20 milliseconds to simulate some mouse actions. */
        sim_mouse_timer = OS_TIMER_CREATE("simm", OS_MS_2_TICKS(20), true,
                                                OS_UINT_TO_PTR(SIM_MOUSE_TIMER_NOTIF), timer_cb);
}

void device_connected(void)
{
        OS_TIMER_START(sim_keyboard_timer, OS_TIMER_FOREVER);
        OS_TIMER_START(sim_mouse_timer, OS_TIMER_FOREVER);
}

void device_disconnected(void)
{
        OS_TIMER_STOP(sim_keyboard_timer, OS_TIMER_FOREVER);
        OS_TIMER_STOP(sim_mouse_timer, OS_TIMER_FOREVER);
}

void device_protocol_mode_set(hids_protocol_mode_t mode)
{
        hids_mode = mode;
}

void device_boot_keyboard_report_written(uint16_t length, const uint8_t *data)
{
        const struct boot_report_keyboard_out *kbd = (void *) data;

        sim_keyboard_out(kbd->capslock);
}

void device_report_written(hids_report_type_t type, uint8_t id,
                                                        uint16_t length, const uint8_t *data)
{
        const struct report_02_out *kbd = (void *) data;

        if (type != HIDS_REPORT_TYPE_OUTPUT || id != 0x02) {
                return;
        }

        sim_keyboard_out(kbd->capslock);
}

void device_suspend(void)
{
        OS_TIMER_STOP(sim_keyboard_timer, OS_TIMER_FOREVER);
        OS_TIMER_STOP(sim_mouse_timer, OS_TIMER_FOREVER);
}

void device_wakeup(void)
{
        OS_TIMER_START(sim_keyboard_timer, OS_TIMER_FOREVER);
        OS_TIMER_START(sim_mouse_timer, OS_TIMER_FOREVER);
}

void device_task_notif(uint32_t notif)
{
        if (notif & SIM_KEYBOARD_TIMER_NOTIF) {
                sim_keyboard_in();
        }

        if (notif & SIM_MOUSE_TIMER_NOTIF) {
                sim_mouse_in();
        }
}
