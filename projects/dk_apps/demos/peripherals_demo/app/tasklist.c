/**
 ****************************************************************************************
 *
 * @file tasklist.c
 *
 * @brief Implementation of application tasks list
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <string.h>
#include <osal.h>
#include <osal.h>
#include "tasklist.h"

extern struct task_item __TASKLIST[];

static void task_stub(void *p)
{
        const struct task_item *t = p;

        if (t->init_func) {
                t->init_func(t);
        }

        for (;;) {
                if (t->func) {
                        t->func(t);
                } else {
                        OS_TASK_SUSPEND(t->task);
                }
        }
}

void app_tasklist_create(void)
{
        struct task_item *t = __TASKLIST;

        while (t->name) {
                OS_TASK_CREATE(t->name, task_stub, (void *) t, t->stack, t->prio, t->task);
                t++;
        }
}

OS_TASK app_tasklist_get_by_name(const char *name)
{
        struct task_item *t = __TASKLIST;

        while (t->name) {
                if (!strcmp(t->name, name)) {
                        break;
                }
                t++;
        }

        return t->task;
}
