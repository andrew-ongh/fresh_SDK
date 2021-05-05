/**
 ****************************************************************************************
 *
 * @file demo_quad.c
 *
 * @brief Quadrature decoder demo (hw_quad driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_quad.h>
#include "common.h"
#include "config.h"
#include "gpio_setup.h"

static int16_t px, py, pz;

void demo_quad_init(void)
{
        /*
         * Initialize quadrature decoder with predefined clock divider
         */
        hw_quad_init(128);
        /*
         * Then enable the quadrature decoder.
         */
        hw_quad_enable();
}

/*
 * Quadrature decoder interrupt callback.
 * Activate when appropriate number of steps will be counted on X or Y axis.
 */
void quad_interrupt_cb(void)
{
        /*
         * Get current number of steps from all channels.
         */
        int16_t x = hw_quad_get_x();
        int16_t y = hw_quad_get_y();
        int16_t z = hw_quad_get_z();

        /*
         * Print current steps and delta values for all channels.
         */
        printf("X=%d (%d) Y=%d (%d) Z=%d (%d)" NEWLINE, x, (x - px), y, (y - py), z, (z - pz));

        /*
         * Set the current steps values as the previous ones.
         */
        px = x;
        py = y;
        pz = z;
}

void menu_quad_channel_ctrl_func(const struct menu_item *m, bool checked)
{
        /*
         * Enable or disable X, Y, Z channels in quadrature decoder.
         */
        switch((int) m->param) {
        case QUAD_CHANNEL_X_ON:
                hw_quad_enable_channels(HW_QUAD_CHANNEL_X);
                break;
        case QUAD_CHANNEL_X_OFF:
                hw_quad_disable_channels(HW_QUAD_CHANNEL_X);
                break;
        case QUAD_CHANNEL_Y_ON:
                hw_quad_enable_channels(HW_QUAD_CHANNEL_Y);
                break;
        case QUAD_CHANNEL_Y_OFF:
                hw_quad_disable_channels(HW_QUAD_CHANNEL_Y);
                break;
        case QUAD_CHANNEL_Z_ON:
                hw_quad_enable_channels(HW_QUAD_CHANNEL_Z);
                break;
        case QUAD_CHANNEL_Z_OFF:
                hw_quad_disable_channels(HW_QUAD_CHANNEL_Z);
                break;
        case QUAD_CHANNEL_XYZ_ON:
                hw_quad_enable_channels(HW_QUAD_CHANNEL_XYZ);
                break;
        case QUAD_CHANNEL_XYZ_OFF:
                hw_quad_disable_channels(HW_QUAD_CHANNEL_XYZ);
                break;
        }
}

void menu_quad_set_threshold_func(const struct menu_item *m, bool checked)
{
        int threshold = (int) m-> param;
        /*
         * Register an interrupt and set predefined threshold for it.
         */
        hw_quad_register_interrupt(quad_interrupt_cb, threshold);
}

void menu_quad_channels_state_func(const struct menu_item *m, bool checked)
{
        /*
         * Get state of all channels.
         */
        HW_QUAD_CHANNEL ch_state = hw_quad_get_channels();

        printf(NEWLINE "==== hw_quad state ====" NEWLINE);

        /*
         * Print current state of each channel.
         */
        printf("channels state: " NEWLINE);
        printf("channel X: %s" NEWLINE, ch_state & HW_QUAD_CHANNEL_X ? "active" : "inactive");
        printf("channel Y: %s" NEWLINE, ch_state & HW_QUAD_CHANNEL_Y ? "active" : "inactive");
        printf("channel Z: %s" NEWLINE, ch_state & HW_QUAD_CHANNEL_Z ? "active" : "inactive");

        /*
         * Print counted number of steps on each channel.
         */
        printf("current channels steps: " NEWLINE);
        printf("X steps: %d" NEWLINE, hw_quad_get_x());
        printf("Y steps: %d" NEWLINE, hw_quad_get_y());
        printf("Z steps: %d" NEWLINE, hw_quad_get_z());

        printf("========= /// =========" NEWLINE);
}
