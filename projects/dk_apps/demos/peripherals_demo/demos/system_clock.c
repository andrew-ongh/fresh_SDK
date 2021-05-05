/**
 ****************************************************************************************
 *
 * @file system_clock.c
 *
 * @brief Clock settings menu functions
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <sys_clock_mgr.h>
#include <stdio.h>
#include "common.h"

void menu_system_clock_func(const struct menu_item *m, bool checked)
{
        sys_clk_t clk = (sys_clk_t) m->param;

        if (!cm_sys_clk_set(clk)) {
                printf("Switching to this clock is not allowed\r\n");
        }
}

bool menu_system_clock_checked_cb_func(const struct menu_item *m)
{
        sys_clk_t clk = (sys_clk_t) m->param;

        return cm_sys_clk_get() == clk;
}
