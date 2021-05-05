/**
 ****************************************************************************************
 *
 * @file tasklist.h
 *
 * @brief Declarations for application tasks list
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef TASKLISTDEF_H_
#define TASKLISTDEF_H_

#include <osal.h>
#include <osal.h>

/** begin of task list definition */
#define TASKLIST_BEGIN() struct task_item __TASKLIST[] = {
/** definition of task list entry */
#define TASKLIST_ENTRY(t_name, t_prio, t_stack, t_init_func, t_func, t_param) { \
                                                .name = #t_name, .prio = t_prio, .stack = t_stack, \
                                                .init_func = t_init_func, .func = t_func, \
                                                .param = t_param },
/** end of task list definition */
#define TASKLIST_END() { /* end */ } };

struct task_item;

/**
 * \brief Task handler function
 *
 */
typedef void (* task_func) (const struct task_item *task);

/**
 * \brief Task definition
 *
 */
struct task_item {
        const char *name;
        const int prio;
        const int stack;
        const task_func init_func;
        const task_func func;
        const void *param;

        OS_TASK task;
};

/**
 * \brief Create all defined tasks
 *
 */
void app_tasklist_create(void);

/**
 * \brief Get task handle by name
 *
 * This only works for tasks defined in task list.
 *
 * \param [in] name task name
 *
 * \return task handle
 *
 */
OS_TASK app_tasklist_get_by_name(const char *name);

#endif /* TASKLISTDEF_H_ */
