/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief Peripherals demo application
 *
 * Copyright (C) 2015-2018 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
/* Standard includes. */
#include <string.h>
#include <stdio.h>

#include <platform_devices.h>
#include "osal.h"
#include "hw_gpio.h"
#include "sys_clock_mgr.h"
#include "sys_power_mgr.h"
#include "sys_watchdog.h"
#include "demos/common.h"
#include "ad_uart.h"

/*
 * FreeRTOS includes
 */


/* Task priorities */
#define mainTEMPLATE_TASK_PRIORITY		( OS_TASK_PRIORITY_NORMAL )

/* The rate at which data is template task counter is incremented. */
#define mainCOUNTER_FREQUENCY_MS                ( 200 / portTICK_PERIOD_MS )

#if (dg_configTRACK_OS_HEAP == 1)
/*
 * ConstantsVariables used for Tasks Stack and OS Heap tracking
 * Declared global to avoid IDLE stack Overflows
 */
#define mainMAX_NB_OF_TASKS           10
#define mainMIN_STACK_GUARD_SIZE      8 /* words */
#define mainTOTAL_HEAP_SIZE_GUARD     64 /*bytes */

TaskStatus_t pxTaskStatusArray[mainMAX_NB_OF_TASKS];
uint32_t ulTotalRunTime;
#endif /* (dg_configTRACK_OS_HEAP == 1) */

#if dg_configUSE_WDOG
INITIALISED_PRIVILEGED_DATA int8_t idle_task_wdog_id = -1;
#endif

/*
 * Perform any application specific hardware configuration.  The clocks,
 * memory, etc. are configured before main() is called.
 */
static void prvSetupHardware( void );
/*
 * Task functions .
 */
void main_task_func(void *param);

static OS_TASK xHandle;

/*
 * Initialize demos.
 *
 */
static void init_demos(void)
{
#if CFG_DEMO_AD_GPADC
        demo_ad_gpadc_init();
#endif // CFG_DEMO_AD_GPADC
}

static void system_init( void *pvParameters )
{
        OS_TASK task_h;

#if defined CONFIG_RETARGET
        extern void retarget_init(void);
#endif

        /* Prepare clocks. Note: cm_cpu_clk_set() and cm_sys_clk_set() can be called only from a
         * task since they will suspend the task until the XTAL16M has settled and, maybe, the PLL
         * is locked.
         */
        cm_sys_clk_init(sysclk_XTAL16M);
        cm_apb_set_clock_divider(apb_div1);
        cm_ahb_set_clock_divider(ahb_div1);
        cm_lp_clk_init();

        /*
         * Initialize platform watchdog
         */
        sys_watchdog_init();

#if dg_configUSE_WDOG
        // Register the Idle task first.
        idle_task_wdog_id = sys_watchdog_register(false);
        ASSERT_WARNING(idle_task_wdog_id != -1);
        sys_watchdog_configure_idle_id(idle_task_wdog_id);
#endif

        /* Prepare the hardware to run this demo. */
        prvSetupHardware();

#if defined CONFIG_RETARGET
        retarget_init();
#endif

        init_demos();

        /* Start main task here (text menu available via UART1 to control application) */
        OS_TASK_CREATE("main", main_task_func, NULL, 768, mainTEMPLATE_TASK_PRIORITY, task_h);

        OS_TASK_DELETE( xHandle );
}

/**
 * @brief Template main creates a Template task
 */
int main( void )
{
        OS_BASE_TYPE status;

        cm_clk_init_low_level();                            /* Basic clock initializations. */

        /* Start the two tasks as described in the comments at the top of this
        file. */
        status = OS_TASK_CREATE("SysInit",              /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                        system_init,                    /* The System Initialization task. */
                        ( void * ) 0,                   /* The parameter passed to the task. */
                        2 * configMINIMAL_STACK_SIZE * OS_STACK_WORD_SIZE,
                                                        /* The size of the stack to allocate to the task. */
                        OS_TASK_PRIORITY_HIGHEST,       /* The priority assigned to the task. */
                        xHandle );                      /* The task handle is not required, so NULL is passed. */
        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);

        /* Start the tasks and timer running. */
        vTaskStartScheduler();

        /* If all is well, the scheduler will now be running, and the following
        line will never be reached.  If the following line does execute, then
        there was insufficient FreeRTOS heap memory available for the idle and/or
        timer tasks	to be created.  See the memory management section on the
        FreeRTOS web site for more details. */
        for( ;; );
}

/**
 * @brief Hardware Initialization
 */
static void prvSetupHardware( void )
{
        /* Init hardware */
        pm_system_init(periph_setup);
        /* The WAKEUP block must be configured only once */
#if CFG_DEMO_HW_WKUP
        demo_wkup_init();
#endif
#if CFG_DEMO_POWER_MODE
        demo_power_mode_init();
#endif // CFG_DEMO_HW_POWER_MODE
}

/**
 * @brief Malloc fail hook
 */
void vApplicationMallocFailedHook( void )
{
	/* vApplicationMallocFailedHook() will only be called if
	configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
	function that will get called if a call to OS_MALLOC() fails.
	OS_MALLOC() is called internally by the kernel whenever a task, queue,
	timer or semaphore is created.  It is also called by various parts of the
	demo application.  If heap_1.c or heap_2.c are used, then the size of the
	heap available to OS_MALLOC() is defined by configTOTAL_HEAP_SIZE in
	FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
	to query the size of free heap space that remains (although it does not
	provide information on how the remaining heap might be fragmented). */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}

/**
 * @brief Application idle task hook
 */
void vApplicationIdleHook( void )
{
	/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
	to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
	task.  It is essential that code added to this hook function never attempts
	to block in any way (for example, call OS_QUEUE_GET() with a block time
	specified, or call OS_DELAY()).  If the application makes use of the
	OS_TASK_DELETE() API function (as this demo application does) then it is also
	important that vApplicationIdleHook() is permitted to return to its calling
	function, because it is the responsibility of the idle task to clean up
	memory allocated by the kernel to any task that has since been deleted. */

#if (dg_configTRACK_OS_HEAP == 1)
        OS_BASE_TYPE i = 0;
        OS_BASE_TYPE uxMinimumEverFreeHeapSize;

        // Generate raw status information about each task.
        UBaseType_t uxNbOfTaskEntries = uxTaskGetSystemState(pxTaskStatusArray,
                                                        mainMAX_NB_OF_TASKS, &ulTotalRunTime);

        for (i = 0; i < uxNbOfTaskEntries; i++) {
                /* Check Free Stack*/
                OS_BASE_TYPE uxStackHighWaterMark;

                uxStackHighWaterMark = uxTaskGetStackHighWaterMark(pxTaskStatusArray[i].xHandle);
                OS_ASSERT(uxStackHighWaterMark >= mainMIN_STACK_GUARD_SIZE);
        }

        /* Check Minimum Ever Free Heap against defined guard. */
        uxMinimumEverFreeHeapSize = xPortGetMinimumEverFreeHeapSize();
        OS_ASSERT(uxMinimumEverFreeHeapSize >= mainTOTAL_HEAP_SIZE_GUARD);
#endif /* (dg_configTRACK_OS_HEAP == 1) */

#if dg_configUSE_WDOG
        sys_watchdog_notify(idle_task_wdog_id);
#endif
}

/**
 * @brief Application stack overflow hook
 */
void vApplicationStackOverflowHook( OS_TASK pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}

/**
 * @brief Application tick hook
 */
void vApplicationTickHook( void )
{

        OS_POISON_AREA_CHECK( OS_POISON_ON_ERROR_HALT, result );

}
