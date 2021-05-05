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
#include "ad_ble.h"
#include "ad_nvms.h"
#include "ble_common.h"
#include "ble_gap.h"
#include "ble_mgr.h"
#include "ble_stack_config.h"
#include "hw_gpio.h"
#include "hw_rf.h"
#include "sys_clock_mgr.h"
#include "sys_power_mgr.h"

#include "gapm_task.h"
#include "gapc_task.h"
#include "gattc_task.h"
#include "gattm_task.h"

#include "platform_devices.h"

#if dg_configCOEX_ENABLE_CONFIG
#include "hw_coex.h"
#endif

/*
 * BLE adv demo advertising data
 */
static const uint8_t adv_data[] = {
        0x11, GAP_DATA_TYPE_LOCAL_NAME,
        'D', 'i', 'a', 'l', 'o', 'g', ' ', 'C', 'O', 'E', 'X', ' ', 'T', 'e', 's', 't'
};

/* Task priorities */
#define mainBLE_ADV_DEMO_TASK_PRIORITY              ( OS_TASK_PRIORITY_NORMAL )


#undef USE_GPIOS_FOR_DEFAULTS

#define NODE_ROLE 1 /* 0: ponger, 1: pinger */
#define CONTINUOUS_TX 0 /* If set, TX happens continuously (from the TX confirmation ISR callpath) */
#define ALLOW_BLOCK_SLEEP 1 /* If 1, FTDF block will be put to sleep, if possible, after a
                               transaction has finished (Only for pinger/non-continuous mode) */

/* Task priorities */
#define mainTEMPLATE_TASK_PRIORITY		( OS_TASK_PRIORITY_NORMAL )
#define mainTASK_STACK_SIZE                      800

/* Default ftdf values */
#define TX_DELAY                        370 /* In ms. IF SET TO ZERO, IT DOES CONTINUOUS PING
                                                FROM INSIDE THE ISR CALLPATH */
#define PINGER_ADDRESS 0x10
#define PONGER_ADDRESS 0xD1A1
#define PANID  0xcafe
#define RADIO_CHANNEL 11
#define ACK_ENABLE 1

/* This is the value of the 3-bit ANT_TRIM bus to set, for BLE and FTDF */
#define FTDF_TX_POWER                        2
#define BLE_TX_POWER                         5

#define PING_PACKET_SIZE 	100

#define LOG_TAG 1

#define COEX_TEST_FTDF_PTI_HI           6
#define COEX_TEST_FTDF_PTI_LO           4
#define COEX_TEST_EXT_PTI               0

#if FTDF_RX_HIGHER_THAN_BLE == 1
#define FTDF_RX_PRIO           6
#else
#define FTDF_RX_PRIO           11
#endif

#define prvSetPibAttribute ftdf_set_value

PRIVILEGED_DATA static OS_TASK xHandle;
PRIVILEGED_DATA static OS_TASK xCoexTestFtdfTaskHandle;
PRIVILEGED_DATA static OS_TASK xCoexTestBleTaskHandle;
PRIVILEGED_DATA static const ble_mgr_interface_t *ble_mgr_if;


/*
 * Perform any application specific hardware configuration.  The clocks,
 * memory, etc. are configured before main() is called.
 */
static void prvSetupHardware(void);
/*
 * Task functions .
 */
static void prvCoexTestFtdfTask(void *pvParameters);

static void ble_adv_demo_task(void *pvParameters);

PRIVILEGED_DATA static const ble_mgr_interface_t *ble_mgr_if;

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
        options |= FTDF_TRANSPARENT_WAIT_FOR_ACK;
        options |= FTDF_TRANSPARENT_AUTO_ACK;

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

        if (ad_ftdf_send_frame_simple(i + 2, frame, vars.config.channel,
                sn & 1? COEX_TEST_FTDF_PTI_HI : COEX_TEST_FTDF_PTI_LO,
                FTDF_FALSE) == FTDF_TRANSPARENT_OVERFLOW) {
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

void coex_test_gpio_setup(void)
{
#if FTDF_DBG_BUS_ENABLE
        ad_ftdf_dbg_bus_gpio_config();
#endif

}

/**
 * @brief Template main creates a Template task
 */
static void system_init( void *pvParameters )
{
        OS_BASE_TYPE status;

#if dg_configCOEX_ENABLE_CONFIG
        hw_coex_config_t coex_config = {0};
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

        /* Prepare the hardware to run this demo. */

        pm_system_init(coex_test_gpio_setup);
        prvSetupHardware();

        /* init resources */
        resource_init();

        /* Set the desired sleep mode. */
        pm_set_wakeup_mode(true);
        pm_set_sleep_mode(pm_mode_extended_sleep);
#if dg_configCOEX_ENABLE_CONFIG
        /* If force_cca is set to false, one in two FTDF packets (even SN) will be lost. If set to
         * true then at least some of the even SN packets will succeed in being transmitted.
         */
        coex_config.ctrl &= ~HW_COEX_CTRL_BIT_FTDF_FORCE_CCA;
        coex_config.ctrl |= (FTDF_USE_AUTO_PTI == 1) ? HW_COEX_CTRL_BIT_FTDF_PTI_AUTO : 0;
        coex_config.ctrl |= HW_COEX_CTRL_BIT_BLE_PTI_AUTO;

        coex_config.pri[2].mac = HW_COEX_MAC_TYPE_FTDF;
        coex_config.pri[2].pti = COEX_TEST_FTDF_PTI_HI;
        coex_config.pri[4].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[4].pti = BLE_PTI_CONNECTABLE_ADV_MODE;
        coex_config.pri[FTDF_RX_PRIO].mac = HW_COEX_MAC_TYPE_FTDF;
        coex_config.pri[FTDF_RX_PRIO].pti = 0; // Rx PTI (Default value). Can be changed
                                               // using the FTDF_PIB_PTI_CONFIG PIB parameter.
        coex_config.pri[7].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[7].pti = BLE_PTI_CONNECT_REQ_RESPONSE;
        coex_config.pri[8].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[8].pti = BLE_PTI_LLCP_PACKETS;
        coex_config.pri[9].mac = HW_COEX_MAC_TYPE_BLE;
        coex_config.pri[9].pti = BLE_PTI_DATA_CHANNEL_TX;
        coex_config.pri[10].mac = HW_COEX_MAC_TYPE_FTDF;
        coex_config.pri[10].pti = COEX_TEST_FTDF_PTI_LO;

        hw_coex_config_set(&coex_config);
#endif

        /* Initialize ftdf driver */
        ad_ftdf_init();

        /* Set FEM_CONTROL signals */
#if dg_configFEM == FEM_SKY66112_11
#if (dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A)
        hw_fem_set_tx_bypass(true);
        hw_fem_set_rx_bypass(true);
        hw_fem_set_txpower(false);
#else
#ifdef CONFIG_USE_BLE
        hw_fem_set_tx_bypass_ble(true);
        hw_fem_set_rx_bypass_ble(true);
        hw_fem_set_txpower_ble(false);
#endif
#ifdef CONFIG_USE_FTDF
        hw_fem_set_tx_bypass_ftdf(false);
        hw_fem_set_rx_bypass_ftdf(true);
        hw_fem_set_txpower_ftdf(true);
#endif
#endif /* dg_configBLACK_ORCA_IC_REV */
#endif /* dg_configFEM */

#if FTDF_DBG_BUS_ENABLE
        ftdf_set_dbg_mode(FTDF_DBG_LMAC_PHY_SIGNALS /* FTDF_DBG_LMAC_TSCH_ACK_TIMING *//* FTDF_DBG_LEGACY*/);
#endif

        /* start the test app task */
        status = OS_TASK_CREATE("CoexTestFtdf", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                       prvCoexTestFtdfTask, /* The function that implements the task. */
                       NULL, /* The parameter passed to the task. */
                       mainTASK_STACK_SIZE,  /* The size of the stack to allocate to the task. */
                       mainTEMPLATE_TASK_PRIORITY, /* The priority assigned to the task. */
                       xCoexTestFtdfTaskHandle); /* The task handle. */

        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);

        /* Initialize BLE Manager */
        ble_mgr_init();

        ble_mgr_if = ble_mgr_get_interface();

        /* Start the BLE adv demo application task. */
        status = OS_TASK_CREATE("CoexTestBle",                  /* The text name assigned to the task, for
                                                           debug only; not used by the kernel. */
                       ble_adv_demo_task,               /* The function that implements the task. */
                       NULL,                            /* The parameter passed to the task. */
                       512,                             /* The number of bytes to allocate to the
                                                           stack of the task. */
                       mainBLE_ADV_DEMO_TASK_PRIORITY,  /* The priority assigned to the task. */
                       xCoexTestBleTaskHandle);                         /* The task handle. */

        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);

        /* the work of the SysInit task is done */
        OS_TASK_DELETE(OS_GET_CURRENT_TASK());
}
/*-----------------------------------------------------------*/

/**
 * @brief Template main creates a Template task
 */
int main( void )
{
        OS_BASE_TYPE status;
        if (!dg_configENABLE_DEBUGGER) {
                DISABLE_DEBUGGER;
                /* Uncomment this to allow the application to really go to sleep mode */
                pm_wait_debugger_detach(pm_mode_extended_sleep);
        }
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

static void prvCoexTestFtdfTask(void *pvParameters)
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

        int txpower = FTDF_TX_POWER;

        prvSetPibAttribute(FTDF_PIB_TX_POWER, &txpower);

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
                        enable_rx = 1;
                        prvSetPibAttribute(FTDF_PIB_RX_ON_WHEN_IDLE, &enable_rx);
                        msdu[0] = vars.packet_counter++;
                        prvSendPacket(msdu, vars.config.packet_size, vars.config.dst_address);
                }
        }
}

static void handle_evt_gap_connected(ble_evt_gap_connected_t *evt)
{
        /**
         * Manage behavior upon connection
         */

        gap_conn_params_t cp;

        cp.interval_min = defaultBLE_PPCP_INTERVAL_MIN;
        cp.interval_max = defaultBLE_PPCP_INTERVAL_MAX;
        cp.slave_latency = defaultBLE_PPCP_SLAVE_LATENCY;
        cp.sup_timeout = defaultBLE_PPCP_SUP_TIMEOUT;

        ble_gap_conn_param_update(evt->conn_idx, &cp);
}

static void handle_evt_gap_disconnected(ble_evt_gap_disconnected_t *evt)
{
        /**
         * Manage behavior upon disconnection
         */

        // Restart advertising
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);
}

static void ble_adv_demo_task(void *pvParameters)
{
        // Just remove compiler warnings about the unused parameter
        ( void ) pvParameters;

        // Start BLE module as a peripheral device
        ble_peripheral_start();

        // Set device name
        ble_gap_device_name_set("Dialog COEX Test", ATT_PERM_READ);

        // Set advertising data
        ble_gap_adv_data_set(sizeof(adv_data), adv_data, 0, NULL);

        // Start advertising
        ble_gap_adv_start(GAP_CONN_MODE_UNDIRECTED);

        for (int i = 0; i < 39; i++) {
                ble_set_fem_voltage_trim(i, BLE_TX_POWER);
        }

        for (;;) {
                ble_evt_hdr_t *hdr;

                /* Wait for a BLE event - this task will block
                   indefinitely until something is received. */
                hdr = ble_get_event(true);
                if (!hdr) {
                        continue;
                }

                switch (hdr->evt_code) {
                case BLE_EVT_GAP_CONNECTED:
                        handle_evt_gap_connected((ble_evt_gap_connected_t *) hdr);
                        break;
                case BLE_EVT_GAP_DISCONNECTED:
                        handle_evt_gap_disconnected((ble_evt_gap_disconnected_t *) hdr);
                        break;
                case BLE_EVT_GAP_PAIR_REQ:
                {
                        ble_evt_gap_pair_req_t *evt = (ble_evt_gap_pair_req_t *) hdr;
                        ble_gap_pair_reply(evt->conn_idx, true, evt->bond);
                        break;
                }
                default:
                        ble_handle_event_default(hdr);
                        break;
                }

                // Free event buffer
                OS_FREE(hdr);
        }
}


/**
 * @brief Hardware Initialization
 */
static void prvSetupHardware(void)
{
        coex_test_gpio_setup();
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
