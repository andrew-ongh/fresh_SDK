/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief Ping-pong test program for the FTDF driver. "Bare metal" version
 *
 * Copyright (C) 2015-2018 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

/* Standard includes. */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "osal.h"
#include "resmgmt.h"
#include "ad_ftdf_phy_api.h"
#include "hw_gpio.h"
#include "hw_rf.h"
#include "sys_clock_mgr.h"
#include "sys_power_mgr.h"
#include "sys_watchdog.h"
#include "platform_devices.h"

/* Set this to 16 for a Simple TX  at 16MHz, to 96 for a Simple TX at 96 MHz,
 * or to 0 for normal operation. Simple TX is TX without CSMA, ACK, Frame Retransmissions
 * or ping-pong operation (ie. RX is not active)
 */
#define SIMPLE_TX 0

/* Set this to 16 for a Simple RX at 16MHz, to 96 for a Simple RX at 96 MHz,
 * or to 0 for normal operation. Simple RX is RX without ACK transmission
 */
#define SIMPLE_RX 0

#if SIMPLE_TX != 0 && SIMPLE_RX != 0
#error Only one of SIMPLE_TX and SIMPLE_RX can be set to non-zero at the same time
#endif

#undef USE_GPIOS_FOR_DEFAULTS

#if SIMPLE_RX != 0
#define NODE_ROLE 0 /* 0: ponger, 1: pinger */
#else
#define NODE_ROLE 1 /* 0: ponger, 1: pinger */
#endif

#define CONTINUOUS_TX 0 /* If set, TX happens continuously (from the TX confirmation ISR callpath) */
#define ALLOW_BLOCK_SLEEP 1 /* If 1, FTDF block will be put to sleep, if possible, after a
                               transaction has finished (Only for pinger/non-continuous mode) */

/* Task priorities */
#define mainTEMPLATE_TASK_PRIORITY		( OS_TASK_PRIORITY_NORMAL )
#define mainTASK_STACK_SIZE                      800

#define TX_POWER                        5; /* The 3-bit bus value that goes to RF_ANT_TRIM */
#define PINGER_ADDRESS 0x10
#define PONGER_ADDRESS 0xD1A1
#define PANID  0xcafe
#define RADIO_CHANNEL 11
#define ACK_ENABLE 1

#if SIMPLE_TX == 0
#define PING_PACKET_SIZE  5
/* Default ftdf values */
#define TX_DELAY                        2000 /* In ms. IF SET TO ZERO, IT DOES CONTINUOUS PING
                                                FROM INSIDE THE ISR CALLPATH */
#else
#define PING_PACKET_SIZE  110
/* Default ftdf values */
#define TX_DELAY                        6 /* In ms. IF SET TO ZERO, IT DOES CONTINUOUS PING
                                                FROM INSIDE THE ISR CALLPATH */
#define SIMPLE_TX_TRIGGER_PORT HW_GPIO_PORT_4
#define SIMPLE_TX_TRIGGER_PIN HW_GPIO_PIN_0
#endif

#define LOG_TAG 1

#define prvSetPibAttribute ftdf_set_value

PRIVILEGED_DATA static OS_TASK xHandle;
PRIVILEGED_DATA static OS_TASK xFtdfTestHandle;

#if dg_configUSE_WDOG
INITIALISED_PRIVILEGED_DATA int8_t idle_task_wdog_id = -1;
#endif

/*
 * Perform any application specific hardware configuration.  The clocks,
 * memory, etc. are configured before main() is called.
 */
static void prvSetupHardware(void);
/*
 * Task functions .
 */
static void prvFtdfTestTask(void *pvParameters);

PRIVILEGED_DATA static struct {
        uint8_t packet_counter;
        int retries;
        OS_TICK_TIME last_tick;

        struct {
                uint32_t transmitted_frames;
                uint32_t txconfirmations;
                uint32_t received_frames;
                uint32_t invalid;
                uint32_t ccafail;
                uint32_t noack;
                uint32_t overflowdata;
                uint32_t otherdata;
        } stats;
        struct {
                ftdf_short_address_t src_address;
                ftdf_pan_id_t panid;
                ftdf_short_address_t dst_address;
                bool ack_enable;
                bool is_pinger;
                int packet_size;
                ftdf_channel_number_t channel;
                bool enable_sleep;
                bool quiet;
                TickType_t ping_delay;
        } config;
} vars ;

typedef uint8_t ftdf_frame_version_t;
typedef struct
{
    ftdf_frame_type_t        frame_type;
    ftdf_frame_version_t     frame_version;
    ftdf_bitmap8_t           options;
    ftdf_sn_t                sn;
    ftdf_address_mode_t      src_addr_mode;
    ftdf_pan_id_t            src_pan_id;
    ftdf_address_t           src_addr;
    ftdf_address_mode_t      dst_addr_mode;
    ftdf_pan_id_t            dst_pan_id;
    ftdf_address_t           dst_addr;
    ftdf_command_frame_id_t  command_frameId;
} ftdf_frame_header_t;
extern  ftdf_octet_t* ftdf_get_frame_header(ftdf_octet_t        *rx_ptr,
                                            ftdf_frame_header_t *frame_header);

/* Helper statics for rx_on_when_idle */
PRIVILEGED_DATA static uint32_t enable_rx;
PRIVILEGED_DATA static uint32_t disable_rx;

PRIVILEGED_DATA static ftdf_octet_t msdu[128];

static void prvEnableTransparentMode(void)
{
        ftdf_bitmap32_t options =  FTDF_TRANSPARENT_ENABLE_FCS_GENERATION;
#if SIMPLE_TX == 0 && SIMPLE_RX == 0
        options |= FTDF_TRANSPARENT_WAIT_FOR_ACK;
        options |= FTDF_TRANSPARENT_AUTO_ACK;
#endif
        ftdf_enable_transparent_mode(FTDF_TRUE, options);

}

static inline void clear_stats(void)
{
        vars.stats.transmitted_frames = 0;
        vars.stats.txconfirmations = 0;
        vars.stats.received_frames = 0;
        vars.stats.invalid = 0;
        vars.stats.ccafail = 0;
        vars.stats.noack = 0;
        vars.stats.otherdata = 0;
        vars.stats.overflowdata = 0;
}


void start_operation()
{
        /* Setup addresses */
        if (vars.config.is_pinger) {
                prvSetPibAttribute(FTDF_PIB_PAN_ID, &vars.config.panid);
                prvSetPibAttribute(FTDF_PIB_CURRENT_CHANNEL,
                        &vars.config.channel);
                prvSetPibAttribute(FTDF_PIB_SHORT_ADDRESS,
                        &vars.config.src_address);
        } else {
                prvSetPibAttribute(FTDF_PIB_PAN_ID, &vars.config.panid);
                prvSetPibAttribute(FTDF_PIB_CURRENT_CHANNEL,
                        &vars.config.channel);
                prvSetPibAttribute(FTDF_PIB_SHORT_ADDRESS,
                        &vars.config.src_address);
                prvSetPibAttribute(FTDF_PIB_RX_ON_WHEN_IDLE, &enable_rx);
        }
        clear_stats();
        vars.packet_counter = 0;
}


/* Generic packet transmission function. Used both by pinger and ponger */
static void prvSendPacket(ftdf_octet_t *msduPayload, ftdf_data_length_t len, ftdf_short_address_t addr)
{
        int i = 0;

        /* SN and frame buffer declared as static, so as to avoid memory management issues.
         * FTDF driver does not handle frame buffer in trasnsparent mode. */
        static ftdf_octet_t sn = 0;

        static ftdf_octet_t frame[128];

        /* Build frame. */
        frame[i++] = 0x61;
        frame[i++] = 0x88;

        /* SN */
        frame[i++] = sn++;

        /* Source PAN */
        frame[i++] = (vars.config.panid) & 0xff;
        frame[i++] = (vars.config.panid >> 8) & 0xff;

        /* Dest Address */
        frame[i++] = (addr) & 0xff;
        frame[i++] = (addr >> 8) & 0xff;

        /* Source Address */
        frame[i++] = (vars.config.src_address) & 0xff;
        frame[i++] = (vars.config.src_address >> 8) & 0xff;

        int j;
        for (j = 0; j < len ; j++)
                frame[i++] = msduPayload[j];
#if SIMPLE_TX == 0
        if (ad_ftdf_send_frame_simple(i + 2, frame, vars.config.channel, 0, FTDF_FALSE) == FTDF_TRANSPARENT_OVERFLOW) {
#else
        if (ad_ftdf_send_frame_simple(i + 2, frame, vars.config.channel, 0, FTDF_TRUE) == FTDF_TRANSPARENT_OVERFLOW) {
#endif
                vars.stats.overflowdata++;
        }

}

static bool prvCheckPacket(ftdf_data_length_t frameLength,
        ftdf_octet_t*     frame, uint8_t sn)
{
        int i;

        /* Check packet */
        if (frameLength != vars.config.packet_size + 11) {
                return FTDF_FALSE;
        }

        for (i = 1; i < vars.config.packet_size; i++) {
                if (frame[i + 9] != i)
                        return FTDF_FALSE;
        }

        if (frame[9] != sn) {
                return FTDF_FALSE;
        }

        return FTDF_TRUE;
}





static void prvTransparentEchoPacket(ftdf_data_length_t frameLength, ftdf_octet_t *frame)
{
        ftdf_frame_header_t frameHeader;
        ftdf_octet_t * rxPtr;
        ftdf_data_length_t msduLength, hdrLength;
        rxPtr = frame;
        rxPtr = ftdf_get_frame_header(rxPtr, &frameHeader);
        hdrLength = rxPtr - frame;
        msduLength = frameLength - hdrLength - 2; /* 2 = CRC16 length*/
        vars.stats.transmitted_frames++;
        prvSendPacket(&frame[hdrLength], msduLength, frameHeader.src_addr.short_address);
}

static void set_default_values(void)
{
        /* Set default values. Use jumpers for this */

#ifdef USE_GPIOS_FOR_DEFAULTS
        vars.config.is_pinger = hw_gpio_get_pin_status(GPIO_PORT_CFG_ROLE, GPIO_PIN_CFG_ROLE);
        vars.config.ack_enable = hw_gpio_get_pin_status(GPIO_PORT_CFG_ACK, HW_GPIO_PIN_6);
        if (vars.config.is_pinger) {
                vars.config.src_address_configured = PINGER_ADDRESS;
                vars.config.dst_address_configured = PONGER_ADDRESS;
        } else {
                vars.config.src_address_configured = PONGER_ADDRESS;
                vars.config.dst_address_configured = PINGER_ADDRESS;
        }
#else
        vars.config.is_pinger = NODE_ROLE;
        vars.config.ack_enable = ACK_ENABLE;

        if (vars.config.is_pinger) {
                vars.config.src_address = PINGER_ADDRESS;
                vars.config.dst_address = PONGER_ADDRESS;
        } else {
                vars.config.src_address = PONGER_ADDRESS;
                vars.config.dst_address = PINGER_ADDRESS;
        }

#endif

        vars.config.panid = PANID;
        vars.config.channel = RADIO_CHANNEL;
        vars.config.ping_delay = OS_MS_2_TICKS(TX_DELAY);
        vars.config.packet_size = PING_PACKET_SIZE;
        vars.config.enable_sleep = false;
        vars.config.quiet = false;
}


/**
 * @brief Template main creates a Template task
 */
static void system_init( void *pvParameters )
{
        BaseType_t status;
#if defined CONFIG_RETARGET
        extern void retarget_init(void);
#endif

        /* Prepare clocks. Note: cm_cpu_clk_set() and cm_sys_clk_set() can be called only from a
         * task since they
         * will suspend the task until the XTAL16M has settled and, maybe, the PLL
         * is locked.
         */
        cm_sys_clk_init(sysclk_XTAL16M);
        cm_apb_set_clock_divider(apb_div1);
        cm_ahb_set_clock_divider(ahb_div1);
        cm_lp_clk_init();

        cm_sys_clk_set(sysclk_XTAL16M);

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
        pm_system_init(NULL);
        prvSetupHardware();

        /* init resources */
        resource_init();

        pm_set_wakeup_mode(true);
        pm_set_sleep_mode(pm_mode_extended_sleep);

#if defined CONFIG_RETARGET
        retarget_init();
#endif

#if SIMPLE_TX != 0 || SIMPLE_RX != 0
        /* Switch to DCDC since this will not go to sleep, so that it can switch automatically */
        while (!cm_lp_clk_is_avail());
        hw_cpm_dcdc_on();

#if SIMPLE_TX != 0
        hw_gpio_configure_pin(SIMPLE_TX_TRIGGER_PORT, SIMPLE_TX_TRIGGER_PIN, HW_GPIO_MODE_OUTPUT, HW_GPIO_FUNC_GPIO, false);
#endif

#if SIMPLE_TX == 16 || SIMPLE_RX == 16
        cm_sys_clk_set(sysclk_XTAL16M);
#elif SIMPLE_TX == 96 || SIMPLE_RX == 96
        cm_sys_clk_set(sysclk_PLL96);
#else
        OS_ASSERT(0);
#endif
#endif

        /* Initialize ftdf driver */
        ad_ftdf_init();

        /* start the test app task */
        status = OS_TASK_CREATE("FtdfTest", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                       prvFtdfTestTask, /* The function that implements the task. */
                       NULL, /* The parameter passed to the task. */
                       mainTASK_STACK_SIZE,  /* The size of the stack to allocate to the task. */
                       mainTEMPLATE_TASK_PRIORITY, /* The priority assigned to the task. */
                       xFtdfTestHandle); /* The task handle. */

        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);
        OS_TASK_DELETE( xHandle );

}


/**
 * @brief Template main creates a Template task
 */
int main( void )
{
        OS_BASE_TYPE status;

        /* Uncomment this to allow the application to really go to sleep mode */
#if SIMPLE_TX == 0 && SIMPLE_RX == 0
        pm_wait_debugger_detach(pm_mode_extended_sleep);
#endif
        cm_clk_init_low_level();                            /* Basic clock initializations. */

        /* Start the two tasks as described in the comments at the top of this
        file. */
        status = OS_TASK_CREATE("SysInit",              /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                        system_init,                    /* The System Initialization task. */
                        ( void * ) 0,                   /* The parameter passed to the task. */
                        800,
                        //configMINIMAL_STACK_SIZE,       /* The size of the stack to allocate to the task. */
                        OS_TASK_PRIORITY_HIGHEST,       /* The priority assigned to the task. */
                        xHandle );                      /* The task handle is not required, so NULL is passed. */
        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);

        /* Start the tasks and timer running. */
        vTaskStartScheduler();

        /* If all is well, the scheduler will now be running, and the following
        line will never be reached.  If the following line does execute, then
        there was insufficient FreeRTOS heap memory available for the idle and/or
        timer tasks     to be created.  See the memory management section on the
        FreeRTOS web site for more details. */
        for( ;; );
}

static void prvFtdfTestTask(void *pvParameters)
{


        ftdf_msg_buffer_t *msgBuf;
        uint32_t pibValue;
        uint64_t extadr;
        vars.packet_counter = 0;
        QueueSetMemberHandle_t xActivatedMember;
        OS_TICK_TIME sleepTime;
        OS_TICK_TIME elapsedTime;
        int i;

        set_default_values();

        vars.last_tick = OS_GET_TICK_COUNT();
        start_operation();

        prvEnableTransparentMode();

        if (!vars.config.is_pinger) {
                enable_rx = 1;
                prvSetPibAttribute(FTDF_PIB_RX_ON_WHEN_IDLE, &enable_rx);
        } else if (CONTINUOUS_TX) {
                /* Do continuous ping through the handler */
                msdu[0] = vars.packet_counter++;
                prvSendPacket(msdu, vars.config.packet_size, vars.config.dst_address);
        }


        for (i = 0; i < 128; i++) {
                msdu[i] = i;

        }

        int txpower = TX_POWER;
        prvSetPibAttribute(FTDF_PIB_TX_POWER, &txpower);

#if SIMPLE_TX != 0
        i = 0;
        prvSetPibAttribute(FTDF_PIB_MAX_FRAME_RETRIES, &i);
#endif


        for (;;) {
                /* Compute sleep time when on started state */
                if (vars.config.is_pinger) {
                        elapsedTime = OS_GET_TICK_COUNT() - vars.last_tick;
                        sleepTime = vars.config.ping_delay - elapsedTime;

                        /* Check if elapsedTime passed vars.config.ping_delay, in which case, it will
                         * become negative -> i.e. a huge unsigned positive
                         */
                        if (sleepTime > vars.config.ping_delay)
                        {
                                sleepTime = 0;
                        }
                } else {
                        sleepTime = 2000;
                }

                vTaskDelay(sleepTime);
                vars.last_tick = xTaskGetTickCount();
                if (vars.config.is_pinger && !CONTINUOUS_TX) {
                        ad_ftdf_wake_up();
#if SIMPLE_TX == 0
                        enable_rx = 1;
                        prvSetPibAttribute(FTDF_PIB_RX_ON_WHEN_IDLE, &enable_rx);
#endif
                        msdu[0] = vars.packet_counter++;
                        prvSendPacket(msdu, vars.config.packet_size, vars.config.dst_address);
#if SIMPLE_TX != 0
                        hw_gpio_set_active(SIMPLE_TX_TRIGGER_PORT, SIMPLE_TX_TRIGGER_PIN);
                        hw_cpm_delay_usec(200);
                        hw_gpio_set_inactive(SIMPLE_TX_TRIGGER_PORT, SIMPLE_TX_TRIGGER_PIN);
#endif
                }
        }
}



/**
 * @brief Hardware Initialization
 */
static void prvSetupHardware(void)
{
}

/**
 * @brief Malloc fail hook
 */
void vApplicationMallocFailedHook(void)
{
        volatile unsigned free_heap = xPortGetFreeHeapSize();
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
        for (;;)
                ;
}

/**
 * @brief Application idle task hook
 */
void vApplicationIdleHook(void)
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

#if dg_configUSE_WDOG
        sys_watchdog_notify(idle_task_wdog_id);
#endif
}

/**
 * @brief Application stack overflow hook
 */
void vApplicationStackOverflowHook(OS_TASK pxTask, char *pcTaskName)
{
        (void)pcTaskName;
        (void)pxTask;

        /* Run time stack overflow checking is performed if
         configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
         function is called if a stack overflow is detected. */
        taskDISABLE_INTERRUPTS();
        for (;;)
                ;
}

/**
 * @brief Application tick hook
 */
void vApplicationTickHook(void)
{

        OS_POISON_AREA_CHECK( OS_POISON_ON_ERROR_HALT, result );

}

void ftdf_send_frame_transparent_confirm( void*         handle,
                                       ftdf_bitmap32_t status )
{
        switch(status) {
        case FTDF_TRANSPARENT_SEND_SUCCESSFUL:
                vars.stats.txconfirmations++;
                break;
        case FTDF_TRANSPARENT_CSMACA_FAILURE:
                vars.stats.ccafail++;
                break;
        case FTDF_TRANSPARENT_NO_ACK:
                vars.stats.noack++;
                break;
        case FTDF_TRANSPARENT_OVERFLOW:
                vars.stats.overflowdata++;
                break;
        default:
                vars.stats.otherdata++;
        }

        if (vars.config.is_pinger && CONTINUOUS_TX) {
                /* Send next packet immediately */
                msdu[0] = vars.packet_counter++;
                prvSendPacket(msdu, vars.config.packet_size, vars.config.dst_address);
        }
}

void ftdf_rcv_frame_transparent( ftdf_data_length_t frameLength,
                               ftdf_octet_t*     frame,
                               ftdf_bitmap32_t   status,
                               ftdf_link_quality_t link_quality)
{
        vars.stats.received_frames++;

        if (vars.config.is_pinger && !CONTINUOUS_TX) {
                if (prvCheckPacket(frameLength, frame, vars.packet_counter - 1) == FTDF_TRUE) {
                        disable_rx = 0;
                        prvSetPibAttribute(FTDF_PIB_RX_ON_WHEN_IDLE, &disable_rx);

                        if (ALLOW_BLOCK_SLEEP == 1) {
                                ad_ftdf_sleep_when_possible(FTDF_TRUE);
                        }
                }
        } else {
                prvTransparentEchoPacket(frameLength, frame);
        }

}
