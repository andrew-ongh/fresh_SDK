/**
 ****************************************************************************************
 *
 * @file main.c
 *
 * @brief RF Tools CLI application
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
#include <stdlib.h>

#include <stdio.h>

#include "osal.h"
#include "resmgmt.h"
#include "hw_gpio.h"
#include "logging.h"

#include "sys_tcs.h"
#include "hw_rf.h"
#include "hw_watchdog.h"
#include "hw_cpm.h"

#ifdef CONFIG_USE_BLE
#include "rf_tools_ble.h"
#include "ad_ble.h"
#include "ble_mgr.h"
#include "ble_mgr_gtl.h"
#endif
#ifdef CONFIG_USE_FTDF
#include "rf_tools_ftdf.h"
#endif

#include "sys_clock_mgr.h"
#include "sys_power_mgr.h"

#include "mcif.h"
#ifdef CONFIG_USE_FTDF
#include "ad_ftdf_phy_api.h"
#endif

#include "platform_devices.h"

/* BLE Stack includes */
#include "gapm_task.h"

#include "Xtal_TRIM.h"

#define RF_TOOLS_CLI_VERSION_STR "1.3"

#ifndef DEFAULT_VERBOSITY
#define DEFAULT_VERBOSITY 1
#endif

/* Task priorities */
#define mainTEMPLATE_TASK_PRIORITY		(OS_TASK_PRIORITY_NORMAL)

#define mainTASK_STACK_SIZE                      800


#define mainBIT_CLI_RX      (1 << 1)

/* Default values */
#define PINGER_ADDRESS 0x10
#define PONGER_ADDRESS 0xD1A1
#define PANID  0xcafe
#define RADIO_CHANNEL 11
#ifdef AUTO_PING
#define PING_DELAY  2000 /* In ms */
#define PING_MAX_RETRIES 1
#define PING_PACKET_SIZE 1
#else
#define PING_DELAY  2000 /* In ms */
#define PING_MAX_RETRIES 3
#define PING_PACKET_SIZE 5
#endif
#define ACK_ENABLE 1


#define QUEUE_TX_LENGTH 12
#define QUEUE_RX_LENGTH 12

#define RSSI_OFFSET                     ((float)(114.3604))
#define RSSI_FACTOR                     ((float)(0.5239))

#if dg_configFEM == FEM_SKY66112_11
#define RSSI_LNA_OFFSET                 -11
#define RSSI_BYPASS_OFFSET              2.5
#endif


#define FREE_HEAP_STATS_PERIOD 1000

#define LOG_TAG 1

#define MESSAGE_BUSY "Busy. Try later..."
#define FREQ_ERANGE_WPAN "Frequency out of range. Must be in [2405, 2480] MHz..."
#define FREQ_ERANGE_BLE "Frequency out of range. Must be in [2402, 2480] MHz..."
#define POWER_ERANGE "Power out of range. Must be in [-4, 0] dbm ..."
#define WPAN_PAYLOAD_ERANGE "Payload length out of range. Must be in [1, 116] octets..."
#define INTERVAL_ERANGE "TX Interval out of range. Must be in [0, 1048575] uS..."
#define PAYLOAD_ERANGE "Payload length out of range. Must be in [0, 0x25] octets ..."
#define PKTTYPE_EINVAL "Invalid Packet type. Must be one of 0 (PRBS9), 1 (11110000), 2 (10101010)"
#define FEMBIAS_ERANGE "Voltage out of range. Must be in [1200, 1975]"
#define FEMBIAS_NA "FEM Bias Control not available on this board"
#define INVALID_GPIO_PORT "Invalid GPIO peripheral PORT.\r\nValid values: 0..4"
#define INVALID_GPIO_PIN "Invalid GPIO PIN.\r\nValid values: 0..7 (for port 2: 0..4).\r\nQSPI, FEM control, UARTs in use, XTAL32K and JTAG pins are not valid GPIOs"
#define INVALID_GPIO_DIR "Invalid GPIO DIRECTION.\r\nValid values: 0..3 (0: Input, no resistors, 1: Input, Pull-Up, 2: Input, Pull-Down, 3: Output)"
#define INVALID_GPIO_OMODE "Invalid GPIO OUTPUT_MODE.\r\nValid values: 0 (Push-pull) or 1 (Open drain)"
#define INVALID_STATE "Invalid GPIO VALUE.\r\nValid values: 0=Low, 1=High"

#define STATUS_PASS "status = pass\r\n"
#define STATUS_FAIL "status = fail\r\n"

typedef enum  {
        fsm_st_idle,
        fsm_st_wpan_txtone,
        fsm_st_wpan_tx,
        fsm_st_wpan_txstream,
        fsm_st_wpan_rx,
        fsm_st_ble_txtone,
        fsm_st_ble_tx,
        fsm_st_ble_tx_infinite,
        fsm_st_ble_rx,
        fsm_st_ble_txstream
} fsm_states_e;

/*
 * Perform any application specific hardware configuration.  The clocks,
 * memory, etc. are configured before main() is called.
 */
static void prvSetupHardware(void);
/*
 * Task functions .
 */
static void prvCliTask(void *pvParameters);

PRIVILEGED_DATA static OS_QUEUE xQueueTx;
PRIVILEGED_DATA static OS_QUEUE xQueueRx;

PRIVILEGED_DATA static fsm_states_e fsm_state;
PRIVILEGED_DATA static bool ble_waiting_response;
PRIVILEGED_DATA bool verbose;

/* Forward declarations for user commands */

/* General commands */
void cmd_verbose(xQueueHandle txq, void **args);
void cmd_reset(xQueueHandle txq, void **args);
void cmd_version(xQueueHandle txq, void **args);
void cmd_xtal16_trim(xQueueHandle txq, void **args);
#if dg_configFEM == FEM_SKY66112_11
void cmd_fem_mode(xQueueHandle txq, void **args);
void cmd_fem_bias(xQueueHandle txq, void **args);
#if defined(dg_configFEM_SKY66112_11_FEM_BIAS2_V18P) || defined(dg_configFEM_SKY66112_11_FEM_BIAS2_V18)
void cmd_fem_bias2(xQueueHandle txq, void **args);
#endif
#endif

/* WPAN commands */
void cmd_wpan_txtone(xQueueHandle txq, void **args);
void cmd_wpan_txstop(xQueueHandle txq, void **args);
void cmd_wpan_tx(xQueueHandle txq, void **args);
void cmd_wpan_tx_done(void);
void cmd_wpan_txstream(xQueueHandle txq, void **args);
void cmd_wpan_rx(xQueueHandle txq, void **args);
void cmd_wpan_rxstop(xQueueHandle txq, void **args);

/* BLE commands */
void cmd_ble_txtone(xQueueHandle txq, void **args);
void cmd_ble_txstream(xQueueHandle txq, void **args);
void cmd_ble_txstream_response(void);
void cmd_ble_txstop(xQueueHandle txq, void **args);
void cmd_ble_txstop_response(uint8_t status, uint16_t packets);
void cmd_ble_txstream_stop_response(void);
void cmd_ble_tx(xQueueHandle txq, void **args);
void cmd_ble_tx_response(uint8_t status);
void cmd_ble_tx_interval_response(uint8_t completed, uint8_t status);
void cmd_ble_rx(xQueueHandle txq, void **args);
void cmd_ble_rx_response(void);
void cmd_ble_rxstop(xQueueHandle txq, void **args);
void cmd_ble_rxstop_response(uint16_t packets, uint16_t sync_errors, uint16_t crc_errors, uint16_t rssi);

/* GPIO commands */
void cmd_gpio_set(xQueueHandle txq, void **args);
void cmd_gpio_get(xQueueHandle txq, void **args);
void cmd_gpio_volts_set(xQueueHandle txq, void **args);
void cmd_all_gpio_set(xQueueHandle txq, void **args);
int set_gpio(uint8_t port, uint8_t pin, uint8_t dir, uint8_t omode, uint8_t state);

PRIVILEGED_DATA uint8_t gpio_rail=0; //0=V18P 1=V33


/* MCIF-ASCII command table */
static const struct mcif_ascii_parse_element_s parse_table[] = {
        { "verbose", cmd_verbose,
                "Set verbosity. Usage: verbose [enable | disable]",
                MCIF_ASCII_FLAGS_ARG1_STR_NO_WHITE
        },

        { "da_reset", cmd_reset,
                "Reset the chip. Usage: da_reset",
                MCIF_ASCII_FLAGS_ARG1_NA
        },

        { "da_version", cmd_version,
                "Get the chip and SW version. Usage: da_version",
                MCIF_ASCII_FLAGS_ARG1_NA
        },
#if dg_configFEM == FEM_SKY66112_11
        { "da_femmode", cmd_fem_mode,
                "Set FEM Mode. Usage: da_femmode <TX_MODE> <RX_MODE>",
                MCIF_ASCII_FLAGS_ARG1_STR_NO_WHITE | MCIF_ASCII_FLAGS_ARG2_STR_NO_WHITE
        },

        { "da_fembias", cmd_fem_bias,
                "Set FEM Bias Voltage. Usage: da_fembias <VOLTAGE_MV>",
                MCIF_ASCII_FLAGS_ARG1_INT
        },
#if defined(dg_configFEM_SKY66112_11_FEM_BIAS2_V18P) || defined(dg_configFEM_SKY66112_11_FEM_BIAS2_V18)
        { "da_fembias2", cmd_fem_bias2,
                "Set FEM Bias 2 Voltage. Usage: da_fembias2 <VOLTAGE_MV>",
                MCIF_ASCII_FLAGS_ARG1_INT
        },
#endif
#endif

        { "da_gpio_set", cmd_gpio_set,
                "Set individual GPIO to output high/low (1/0).\r\nUsage: da_gpio_set <PORT> <PIN> <DIRECTION> <OUTPUT_MODE> <VALUE>, where:\r\n" \
                "    PORT: 0..4\r\n    PIN: 0..7 " \
                "(0..4 for Port 2)\r\n    DIRECTION: 0..3 (0: Input, no resistors, 1: Input, Pull-Up, 2: Input, Pull-Down, 3: Output)\r\n" \
                "    OUTPUT_MODE: 0 (Push-pull) or 1 (Open drain)\r\n" \
                "    VALUE: 0 (low) or 1 (high)",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT |
                MCIF_ASCII_FLAGS_ARG3_INT | MCIF_ASCII_FLAGS_ARG4_INT |
                MCIF_ASCII_FLAGS_ARG5_INT
        },
        { "da_gpio_get", cmd_gpio_get,
                "Get GPIO value.\r\nUsage: da_gpio_get <PORT> <PIN>",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT
        },

        { "da_gpio_set_all", cmd_all_gpio_set,
                "Set to output high/low (1/0) all valid GPIOs at once.\r\nUsage: da_gpio_set_all <1/0>\r\n",
                MCIF_ASCII_FLAGS_ARG1_INT
        },

        { "da_gpio_rail", cmd_gpio_volts_set,
                "Select the GPIO power supply to V33 or V18P rails.\r\nUsage: da_gpio_rail <V33/V18P>",
                MCIF_ASCII_FLAGS_ARG1_STR_NO_WHITE
        },

        { "da_xtal16", cmd_xtal16_trim,
                "XTAL16M trimming commands.\r\nUsage: da_xtal16 <CMD> <VALUE>\r\nCommands:\r\nread 1\t\tread the current XTAL16M trimming value\r\nwrite <VALUE>\twrite a 16-bit HEX trim <VALUE> e.g. 0460\r\noutput <EnPoPi>\tdrive the XTAL16M clock to a specific PIN.\r\n\t\tEn=0|1, Po=0|1|2|3|4, Pi=0|1|2|3|4|5|6|7.\r\n\t\te.g. da_xtal16 output 130 means export XTAL16M on P3_0,\r\n\t\tda_xtal16 output 030 means stop XTAL16M clock export on P3_0\r\n+delta <VALUE>\tVALUE=16-bit HEX to add to current XTAL Trimming value\r\n-delta <VALUE>\tVALUE=16-bit HEX to subtract from current XTAL Trimming value\r\nautotrim <PoPi>\tPo=port and Pi=pin where the 500ms ref. pulse is applied\r\n\t\te.g. 42 means port 4 pin 2",
                MCIF_ASCII_FLAGS_ARG1_STR_NO_WHITE | MCIF_ASCII_FLAGS_ARG2_STR_NO_WHITE
        },
#ifdef CONFIG_USE_FTDF
        { "wpan_txtone", cmd_wpan_txtone,
                "Start a WPAN TX tone. Usage: wpan_txtone <FREQUENCY_MHz> <POWER>",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT
        },

        { "wpan_txstream", cmd_wpan_txstream,
                "Start a WPAN continuous TX stream. Usage: wpan_txstream <FREQUENCY_MHz> <POWER>",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT
        },

        { "wpan_txstop", cmd_wpan_txstop,
                "Stop a WPAN TX tone or transmission. Usage: wpan_txstop",
                MCIF_ASCII_FLAGS_ARG1_NA
        },

        { "wpan_tx", cmd_wpan_tx,
                "Transmit 15.4 packets. Usage: wpan_tx <FREQUENCY_MHz> <POWER> <LENGTH_OCTETS> <NUM_PACKETS> <INTERVAL_US>",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT |
                MCIF_ASCII_FLAGS_ARG3_INT | MCIF_ASCII_FLAGS_ARG4_INT |
                MCIF_ASCII_FLAGS_ARG5_INT
        },

        { "wpan_rx", cmd_wpan_rx,
                "Receive 15.4 packets. Usage: wpan_rx <FREQUENCY_MHz>",
                MCIF_ASCII_FLAGS_ARG1_INT
        },

        { "wpan_rxstop", cmd_wpan_rxstop,
                "Stop Receiving 15. packets. Usage: wpan_rxstop",
                MCIF_ASCII_FLAGS_ARG1_NA
        },
#endif
#ifdef CONFIG_USE_BLE
        { "ble_txtone", cmd_ble_txtone,
                "Start a BLE TX tone. Usage: ble_txtone <FREQUENCY_MHz> <POWER>",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT
        },

        { "ble_txstream", cmd_ble_txstream,
                "Start a BLE continuous TX stream. Usage: ble_txstream <FREQUENCY_MHz> <POWER> <PAYLOAD_TYPE>",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT |
                MCIF_ASCII_FLAGS_ARG3_INT
        },

        { "ble_txstop", cmd_ble_txstop,
                "Stop a BLE TX tone or transmission. Usage: ble_txstop",
                MCIF_ASCII_FLAGS_ARG1_NA
        },

        { "ble_tx", cmd_ble_tx,
                "Transmit BLE packets. Usage: ble_tx <FREQUENCY_MHz> <POWER> <LENGTH_OCTETS> <PAYLOAD_TYPE> <NUM_PACKETS> <INTERVAL_US>",
                MCIF_ASCII_FLAGS_ARG1_INT | MCIF_ASCII_FLAGS_ARG2_INT |
                MCIF_ASCII_FLAGS_ARG3_INT | MCIF_ASCII_FLAGS_ARG4_INT |
                MCIF_ASCII_FLAGS_ARG5_INT | MCIF_ASCII_FLAGS_ARG6_INT
        },

        { "ble_rx", cmd_ble_rx,
                "Receive BLE packets. Usage: ble_rx <FREQUENCY_MHz>",
                MCIF_ASCII_FLAGS_ARG1_INT
        },

        { "ble_rxstop", cmd_ble_rxstop,
                "Stop Receiving BLE packets. Usage: ble_rxstop",
                MCIF_ASCII_FLAGS_ARG1_NA
        },
#endif
        NULL
};


void send_simple_message(xQueueHandle txq, const char *simple_msg, bool show_prompt);


PRIVILEGED_DATA static OS_TASK xHandle = NULL;
PRIVILEGED_DATA static OS_TASK xCliTaskHandle = NULL;

#ifdef CONFIG_USE_FTDF
PRIVILEGED_DATA uint32_t ftdf_rssi_raw;
PRIVILEGED_DATA uint32_t ftdf_frames;
#endif

/************************************************************************
 * Helper functions
 */

static inline int8_t get_power_setting(void *power)
{
        int8_t p = *(int8_t *)power;
        if (p < -4 || p > 0)
                return -1;

        /* Power LUTs are 1: -1 dbm, 2: -2 dbm, 3:, -3 dbm, 4: -4 dbm
         * Value 0 disables the LUTs (0dbm)
         */
        return -p;
}

/* End of Helper functions
 ************************************************************************/

#ifdef CONFIG_USE_FTDF
/************************************************************************
 * FTDF PHY API Callbacks
 */
void ftdf_send_frame_transparent_confirm(void            *handle,
                                         ftdf_bitmap32_t status)
{
        rf_tools_ftdf_send_frame_confirm(handle, status);

}

/* Receive packet from n - 1, switch on led, and start timer to send to n + 1 */
void ftdf_rcv_frame_transparent(ftdf_data_length_t frameLength,
                                ftdf_octet_t       *frame,
                                ftdf_bitmap32_t    status,
                                ftdf_link_quality_t link_quality)
{

        ftdf_rssi_raw += link_quality;
        ftdf_frames++;

        rf_tools_ftdf_recv_frame(frameLength, frame, status);
}

/* End of FTDF PHY API Callbacks
 ************************************************************************/
#endif

/**
 * @brief Template main creates a Template task
 */
static void system_init(void *pvParameters)
{
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

        cm_sys_clk_set(sysclk_XTAL16M);

        /* Prepare the hardware to run this demo. */
        pm_system_init(NULL);
        prvSetupHardware();

        /* init resources */
        resource_init();

        /* Turn on the DCDC. It's needed for fembias control and will not
           be turned on automatically, since this app doesn't sleep/wakeup */
        hw_cpm_dcdc_on();

        pm_set_wakeup_mode(true);
        pm_set_sleep_mode(pm_mode_active);

        OS_QUEUE_CREATE(xQueueTx, sizeof(struct mcif_message_s *), QUEUE_TX_LENGTH);
        OS_QUEUE_CREATE(xQueueRx, sizeof(struct mcif_message_s *), QUEUE_RX_LENGTH);

        OS_ASSERT(xQueueTx);
        OS_ASSERT(xQueueRx);

        /* Initialize logging library */
        log_init();

#ifdef CONFIG_USE_FTDF
        /* Initialize ftdf driver */
        ad_ftdf_init();
#endif
        /* start the test app task */
        OS_TASK_CREATE("CliTask", /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                prvCliTask, /* The function that implements the task. */
                NULL, /* The parameter passed to the task. */
                mainTASK_STACK_SIZE * OS_STACK_WORD_SIZE,  /* The size of the stack to allocate to the task. */
                mainTEMPLATE_TASK_PRIORITY, /* The priority assigned to the task. */
                xCliTaskHandle); /* The task handle. */
        OS_ASSERT(xCliTaskHandle);

        /* Initialize MCIF. mcif_setup_queues() MUST be done before mcif_init() */
        mcif_setup_queues(0, xQueueTx, xQueueRx);
        mcif_setup_client_notifications(0, xCliTaskHandle, 1);
        mcif_init();

#ifdef CONFIG_USE_BLE
        /* Initialize BLE Manager */
        ble_mgr_init();
#endif

        OS_TASK_DELETE(xHandle);

}

/**
 * @brief Template main creates a Template task
 */
int main(void)
{
        OS_BASE_TYPE status;

        /* Uncomment this to allow the application to really go to sleep mode */
        //pm_wait_debugger_detach(pm_mode_extended_sleep);
        cm_clk_init_low_level();                            /* Basic clock initializations. */

        /* Start the two tasks as described in the comments at the top of this
        file. */
        status = OS_TASK_CREATE("SysInit",              /* The text name assigned to the task - for debug only as it is not used by the kernel. */
                        system_init,                    /* The System Initialization task. */
                        (void *) 0,                     /* The parameter passed to the task. */
                        1024,
                        //configMINIMAL_STACK_SIZE,       /* The size of the stack to allocate to the task. */
                        OS_TASK_PRIORITY_HIGHEST,       /* The priority assigned to the task. */
                        xHandle);                       /* The task handle is not required, so NULL is passed. */
        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);

        /* Start the tasks and timer running. */
        vTaskStartScheduler();

        /* If all is well, the scheduler will now be running, and the following
        line will never be reached.  If the following line does execute, then
        there was insufficient FreeRTOS heap memory available for the idle and/or
        timer tasks     to be created.  See the memory management section on the
        FreeRTOS web site for more details. */
        for(;;);
}



static void prvCliTask(void *pvParameters)
{
        uint32_t ulNotifiedValue;

#ifdef CONFIG_USE_BLE
        rf_tools_ble_evt_cbs_t ble_cbs = {
                .rx_stats = cmd_ble_rx_response,
                .tx = cmd_ble_tx_response,
                .tx_intv = cmd_ble_tx_interval_response,
                .stop = cmd_ble_txstop_response,
                .stop_rx_stats = cmd_ble_rxstop_response,
                .start_cont_tx = cmd_ble_txstream_response,
                .stop_cont_tx = cmd_ble_txstream_stop_response
        };
#endif

#ifdef CONFIG_USE_FTDF
        rf_tools_ftdf_cbs_t ftdf_cbs = {
                .tx_done = cmd_wpan_tx_done
        };
#endif
        struct mcif_message_s *userMsg;

        fsm_state = fsm_st_idle;
        ble_waiting_response  = false;
        verbose = DEFAULT_VERBOSITY;

#ifdef CONFIG_USE_BLE
        rf_tools_ble_init(xCliTaskHandle, ble_cbs);
#endif

#ifdef CONFIG_USE_FTDF
        rf_tools_ftdf_init(ftdf_cbs);
#endif
        log_printf(LOG_NOTICE, LOG_TAG, "Started\n\r");

        send_simple_message(xQueueTx, "\x1b[2J\x1b[HWelcome to RF Tools CLI\r\n", true);

#ifdef HIGH_POWER_STARTUP
        char* hp_vals[2] = { "txhp", "lna" };
        void* high_power_args[2] = { (void *)hp_vals[0], (void *)hp_vals[1] };
        cmd_fem_mode(xQueueTx, high_power_args);
#endif

#ifdef PERSISTENT_TX
        uint16_t vals[2] = { 2440, 0};
        void *txtone_args[2] = { (void *)&vals[0], (void *)&vals[1]};
        cmd_wpan_txtone(xQueueTx, txtone_args);
#endif

        for (;;) {
                /*
                 * Wait on any of the event group bits, then clear them all
                 */
                OS_TASK_NOTIFY_WAIT(0x0, 0xFFFFFFFF, &ulNotifiedValue, portMAX_DELAY);

#ifdef CONFIG_USE_BLE
                // Check if notification is coming from BLE manager's event queue
                rf_tools_ble_handle_evt(ulNotifiedValue);
#endif

                /* Handle CLI incoming message */
                if (ulNotifiedValue & mainBIT_CLI_RX) {
                        OS_QUEUE_GET(xQueueRx, &userMsg, 0);
                        log_printf(LOG_DEBUG, LOG_TAG, "Got message [%s]\r\n", userMsg->buffer);
                        mcif_ascii_parse_message(parse_table, xQueueTx, userMsg);
                        OS_FREE(userMsg);

                        if (uxQueueMessagesWaiting(xQueueRx)) {
                                OS_TASK_NOTIFY(xCliTaskHandle, mainBIT_CLI_RX,
                                        OS_NOTIFY_SET_BITS);
                        }
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

void send_simple_message(xQueueHandle txq, const char *simple_msg, bool show_prompt)
{
        struct mcif_message_s *tx_msg;
        int len = strlen(simple_msg) + 1;

        tx_msg = OS_MALLOC(sizeof(struct message_s *) + len);
        strcpy((char *)tx_msg->buffer, simple_msg);
        tx_msg->len = len;

        mcif_ascii_send_response(parse_table, txq, tx_msg, show_prompt);
}

static inline void send_status_pass(xQueueHandle txq)
{
        send_simple_message(xQueueTx, STATUS_PASS, true);
}

static inline void send_status_fail(xQueueHandle txq)
{
        send_simple_message(xQueueTx, STATUS_FAIL, true);
}

static int convert_rssi_raw_to_dbm(bool is_ble, uint16_t rssi_raw)
{
        int8_t offset = 0;

#if dg_configFEM == FEM_SKY66112_11
        bool bypass;

#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
        bypass = hw_fem_get_rx_bypass();
#else
#ifdef CONFIG_USE_FTDF
        if (is_ble) {
#endif
#ifdef CONFIG_USE_BLE
                bypass = hw_fem_get_rx_bypass_ble();
#endif
#ifdef CONFIG_USE_FTDF
        } else {
                bypass = hw_fem_get_rx_bypass_ftdf();
        }
#endif
#endif

        if (bypass) {
                offset = RSSI_BYPASS_OFFSET;
        } else {
                offset = RSSI_LNA_OFFSET;
        }
#endif

        return (int)((RSSI_FACTOR * (float)rssi_raw) - RSSI_OFFSET + offset);
}

/* User commands */
/* General commands */
const char *verbose_modes[] = { "disable", "enable" };

void cmd_verbose(xQueueHandle txq, void **args)
{
        char *verbosity = (char *)args[0];
        int i;

        for (i = 0; i < sizeof(verbose_modes) / sizeof(char *); i++) {
                if (!strncmp(verbosity, verbose_modes[i], strlen(verbosity))) {
                        verbose = i;
                        if (verbose)
                                send_simple_message(txq, "Verbosity set to enabled", true);
                        else
                                send_status_pass(txq);
                        return;
                }
        }

        send_simple_message(txq, "Illegal verbose mode. Must be one of [enable, disable]", true);
}


#if dg_configFEM == FEM_SKY66112_11
void cmd_fem_bias(xQueueHandle txq, void **args)
{
        uint16_t voltage = *(uint16_t *)args[0];
        int r = hw_fem_set_bias(voltage);

        switch(r) {
        case -1:
                send_simple_message(txq, FEMBIAS_ERANGE, true);
                break;
        case -2:
                send_simple_message(txq, FEMBIAS_NA, true);
                break;
        default:
                /* Just wait some time for Voltage to settle. */
                vTaskDelay(5);
                send_simple_message(txq, "DONE", true);
        }
}

#if defined(dg_configFEM_SKY66112_11_FEM_BIAS2_V18P) || defined(dg_configFEM_SKY66112_11_FEM_BIAS2_V18)
void cmd_fem_bias2(xQueueHandle txq, void **args)
{
        uint16_t voltage = *(uint16_t *)args[0];
        int r = hw_fem_set_bias2(voltage);

        switch(r) {
        case -1:
                send_simple_message(txq, FEMBIAS_ERANGE, true);
                break;
        case -2:
                send_simple_message(txq, FEMBIAS_NA, true);
                break;
        default:
                /* Just wait some time for Voltage to settle. */
                vTaskDelay(5);
                send_simple_message(txq, "DONE", true);
        }
}
#endif

#endif

void cmd_reset(xQueueHandle txq, void **args)
{
        hw_watchdog_gen_RST();               // Reset when reach 0
        hw_watchdog_set_pos_val(0x64);       // Reset after ~100ms
        hw_watchdog_unfreeze();              // Start watchdog
        while(1){}                           // Loop forever
}

#define VERSION_STR \
        "HW Version: %c%c, SW Version: " RF_TOOLS_CLI_VERSION_STR

void cmd_version(xQueueHandle txq, void **args)
{
        /* No more bytes needed since chip version is 2 bytes, as the length of %s */
        char buf[sizeof(VERSION_STR)];

        snprintf(buf, sizeof(buf), VERSION_STR, CHIP_VERSION->CHIP_REVISION_REG,
                CHIP_VERSION->CHIP_TEST1_REG + 'A');


        if (verbose) {
                send_simple_message(xQueueTx, buf, true);
        } else {
                send_simple_message(xQueueTx, buf, false);
                send_simple_message(xQueueTx, "\r\n", false);
                send_status_pass(xQueueTx);
        }

}

#if dg_configFEM == FEM_SKY66112_11
const char *tx_modes[] = { "txhp", "txlp", "txbp" };
const char *rx_modes[] = { "lna", "rxbp"};


void cmd_fem_mode(xQueueHandle txq, void **args)
{
        char *tx_mode = (char *)args[0];
        char *rx_mode = (char *)args[1];
        uint8_t txm = 0xff;
        uint8_t rxm = 0xff;

        int i;

        for (i = 0; i < sizeof(tx_modes) / sizeof(char *); i++) {
                if (!strncmp(tx_mode, tx_modes[i], strlen(tx_mode))) {
                        txm = i;
                }
        }

        for (i = 0; i < sizeof(rx_modes) / sizeof(char *); i++) {
                if (!strncmp(rx_mode, rx_modes[i], strlen(rx_mode))) {
                        rxm = i;
                }
        }

        if (txm == 0xff) {
                if (verbose)
                        send_simple_message(txq, "ERROR: TX MODE valid values: [txhp (TX High Power), txlp (TX Low Power), txbp (TX Bypass)]", true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (rxm == 0xff) {
                if (verbose)
                        send_simple_message(txq, "ERROR: RX MODE values: [lna (RX LNA), rxbp (RX Bypass)]", true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        switch (txm) {
        case 0: /* Txhp */
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
                hw_fem_set_txpower(true);
                hw_fem_set_tx_bypass(false);
#else
#ifdef CONFIG_USE_BLE
                hw_fem_set_txpower_ble(true);
                hw_fem_set_tx_bypass_ble(false);
#endif
#ifdef CONFIG_USE_FTDF
                hw_fem_set_txpower_ftdf(true);
                hw_fem_set_tx_bypass_ftdf(false);
#endif
#endif /* dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A */
                break;
        case 1: /* Txhp */
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
                hw_fem_set_txpower(false);
                hw_fem_set_tx_bypass(false);

#else
#ifdef CONFIG_USE_BLE
                hw_fem_set_txpower_ble(false);
                hw_fem_set_tx_bypass_ble(false);
#endif
#ifdef CONFIG_USE_FTDF
                hw_fem_set_txpower_ftdf(false);
                hw_fem_set_tx_bypass_ftdf(false);
#endif
#endif /* dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A */
                break;
        case 2: /* Txbp */
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
                hw_fem_set_txpower(false);
                hw_fem_set_tx_bypass(true);
#else
#ifdef CONFIG_USE_BLE
                hw_fem_set_txpower_ble(false);
                hw_fem_set_tx_bypass_ble(true);
#endif
#ifdef CONFIG_USE_FTDF
                hw_fem_set_txpower_ftdf(false);
                hw_fem_set_tx_bypass_ftdf(true);
#endif
#endif /* dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A */
                break;

        }

        switch (rxm) {
        case 0: /* lna */
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
                hw_fem_set_rx_bypass(false);
#else
#ifdef CONFIG_USE_BLE
                hw_fem_set_rx_bypass_ble(false);
#endif
#ifdef CONFIG_USE_FTDF
                hw_fem_set_rx_bypass_ftdf(false);
#endif
#endif /* dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A */

                break;
        case 1: /* rxbp */
#if dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A
                hw_fem_set_rx_bypass(true);
#else
#ifdef CONFIG_USE_BLE
                hw_fem_set_rx_bypass_ble(true);
#endif
#ifdef CONFIG_USE_FTDF
                hw_fem_set_rx_bypass_ftdf(true);
#endif
#endif /* dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A */
                break;
        }

        if(verbose)
                send_simple_message(txq, "Done", true);
        else
                send_status_pass(xQueueTx);
}
#endif

#ifdef CONFIG_USE_FTDF
/* WPAN commands */
void cmd_wpan_txtone(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ftdf_get_channel_rf(args[0]);
        int8_t power = get_power_setting(args[1]);

        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_WPAN, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (power < 0) {
                if (verbose)
                        send_simple_message(txq, POWER_ERANGE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        fsm_state = fsm_st_wpan_txtone;

        hw_rf_set_tx_power(power);
        hw_rf_start_continuous_wave(0x2, ch);
        if (verbose)
                send_simple_message(txq, "Started WPAN TX Tone...", true);
        else
                send_status_pass(xQueueTx);
}

void cmd_wpan_txstop(xQueueHandle txq, void **args)
{

        switch (fsm_state) {
        case fsm_st_wpan_txtone:
                hw_rf_stop_continuous_wave();
                if (verbose)
                        send_simple_message(txq, "Stopped WPAN TX Tone...", true);
                else
                        send_status_pass(xQueueTx);
                fsm_state = fsm_st_idle;
                break;
        case fsm_st_wpan_tx:
                rf_tools_ftdf_stop_tx();
                if (verbose)
                        send_simple_message(txq, "\r\nStopped WPAN TX...", true);
                else
                        send_status_pass(xQueueTx);

                fsm_state = fsm_st_idle;
                break;
        case fsm_st_wpan_txstream:
                rf_tools_ftdf_stop_txstream();
                if (verbose)
                        send_simple_message(txq, "\r\nStopped WPAN TX Stream...", true);
                else
                        send_status_pass(xQueueTx);

                fsm_state = fsm_st_idle;
                break;
        default:
                if (verbose)
                        send_simple_message(txq, "Nothing to stop", true);
                else
                        send_status_fail(xQueueTx);

        }
}

void cmd_wpan_tx(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ftdf_get_channel_mac(args[0]);
        int8_t power = get_power_setting(args[1]);
        uint8_t len_octets = *(uint8_t *)args[2];
        uint16_t num_packets = *(uint16_t *)args[3];
        uint32_t interval = *(uint32_t *)args[4];

        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);

                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_WPAN, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (power < 0) {
                if (verbose)
                        send_simple_message(txq, POWER_ERANGE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (len_octets < 1 || len_octets > 116) {
                if (verbose)
                        send_simple_message(txq, WPAN_PAYLOAD_ERANGE, true);
                else
                        send_status_fail(xQueueTx);

                return;

        }

        if (interval > 1048575) {
                if (verbose)
                        send_simple_message(txq, INTERVAL_ERANGE, true);
                else
                        send_status_fail(xQueueTx);

                return;
        }
        fsm_state = fsm_st_wpan_tx;

        if (num_packets) {
                if (verbose)
                        send_simple_message(xQueueTx, "Started WPAN TX. Please wait...", false);
        } else {
                if (verbose)
                        send_simple_message(xQueueTx, "Started WPAN TX. Enter wpan_txstop to stop...", true);
                else
                        send_status_pass(xQueueTx);

        }

        hw_rf_set_tx_power(power);
        rf_tools_ftdf_start_tx(ch, len_octets, num_packets, interval);
}

void cmd_wpan_tx_done(void)
{
        fsm_state = fsm_st_idle;
        if (verbose)
                send_simple_message(xQueueTx, "Done", true);
        else
                send_status_pass(xQueueTx);
}

/* Weak-override cb, executed from SysTick handler for txstream command.
 * Since timer1 is disabled, the timer that monitors the uart dma is not running,
 * therefore there is no way to stop txstream.
 * This function simply checks for incoming data in the uart DMA, and if so,
 * enables timer1 irq.
 */
void rf_tools_ftdf_check_stop_txstream(void)
{
        if (MCIF_UART == HW_UART1) {
                if (DMA->DMA0_IDX_REG) {
                        NVIC_EnableIRQ(SWTIM1_IRQn);
                }
        } else {
                if (DMA->DMA2_IDX_REG) {
                        NVIC_EnableIRQ(SWTIM1_IRQn);
                }
        }
}

void cmd_wpan_txstream(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ftdf_get_channel_rf(args[0]);
        int8_t power = get_power_setting(args[1]);

        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_WPAN, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (power < 0) {
                if (verbose)
                        send_simple_message(txq, POWER_ERANGE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        fsm_state = fsm_st_wpan_txstream;

        if (verbose)
                send_simple_message(xQueueTx, "Started WPAN TX Stream. Enter wpan_txstop to stop...", true);
        else
                send_status_pass(xQueueTx);

        hw_rf_set_tx_power(power);

        rf_tools_ftdf_start_txstream(ch);
}


void cmd_wpan_rx(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ftdf_get_channel_mac(args[0]);

        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_WPAN, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }
        fsm_state = fsm_st_wpan_rx;
        ftdf_frames = 0;
        ftdf_rssi_raw = 0;

        rf_tools_ftdf_start_rx(ch);
        if (verbose)
                send_simple_message(txq, "Started WPAN RX. Enter wpan_rxstop to stop...", true);
        else
                send_status_pass(xQueueTx);

}

#define WPAN_RXSTOP_RESPONSE_STRING_VERBOSE \
        "\r\n" \
        "Stopped WPAN RX. Results: \r\n" \
        "  Packets received: %d\r\n"     \
        "  CRC errors:       %d\r\n"     \
        "  RSSI:             %d\r\n\r\n"

#define WPAN_RXSTOP_RESPONSE_STRING \
        "valid packets = %d\r\n" \
        "CRC errors = %d\r\n"    \
        "RSSI = %d\r\n"

void cmd_wpan_rxstop(xQueueHandle txq, void **args)
{
        ftdf_count_t success_count;
        ftdf_count_t fcs_error_count;

        if (fsm_state != fsm_st_wpan_rx) {
                if (verbose)
                        send_simple_message(txq, "Nothing to stop", true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        fsm_state = fsm_st_idle;
        rf_tools_ftdf_stop_rx(&success_count, &fcs_error_count);


        uint16_t rssi_avg_raw = ftdf_frames ? (float)ftdf_rssi_raw / ftdf_frames : 0;
        int rssi_dbm = convert_rssi_raw_to_dbm(false, rssi_avg_raw);

        /* Print stats */
        /* Max Size of the buffer is the size of the template string, plus 1
         * for terminating null,
         * plus the max size for printing a uint32_t
         * (10 bytes) minus 2 (the %d is already there), times the number of args,
         * for uint32_t args
         */
        if (verbose) {
                char buf[sizeof(WPAN_RXSTOP_RESPONSE_STRING_VERBOSE) + 3 * 10 + 1];

                snprintf(buf, sizeof(buf), WPAN_RXSTOP_RESPONSE_STRING_VERBOSE,
                        success_count, fcs_error_count, rssi_dbm);

                send_simple_message(xQueueTx, buf, true);
        } else {
                char buf[sizeof(WPAN_RXSTOP_RESPONSE_STRING) + 3 * 10 + 1];

                snprintf(buf, sizeof(buf), WPAN_RXSTOP_RESPONSE_STRING,
                        success_count, fcs_error_count, rssi_dbm);
                send_simple_message(xQueueTx, buf, false);

                send_status_pass(xQueueTx);
        }

}
#endif

#ifdef CONFIG_USE_BLE
/* BLE commands */
void cmd_ble_txtone(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ble_get_channel_rf(args[0]);
        int8_t power = get_power_setting(args[1]);

        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_BLE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (power < 0) {
                if (verbose)
                        send_simple_message(txq, POWER_ERANGE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }


        fsm_state = fsm_st_ble_txtone;

        hw_rf_set_tx_power(power);
        hw_rf_start_continuous_wave(0x1, ch);
        if (verbose)
                send_simple_message(txq, "Started BLE TX Tone...", true);
        else
                send_status_pass(xQueueTx);

}

void cmd_ble_txstop(xQueueHandle txq, void **args)
{

        switch (fsm_state) {
        case fsm_st_ble_txtone:
                hw_rf_stop_continuous_wave();
                if (verbose)
                        send_simple_message(txq, "Stopped BLE TX Tone...", true);
                else
                        send_status_pass(xQueueTx);
                fsm_state = fsm_st_idle;
                break;
        case fsm_st_ble_tx_infinite:
                /* Handle case where tx has started but stack hasn't responded yet */
                if (ble_waiting_response) {
                        if (verbose)
                                send_simple_message(txq, "Busy waiting for TX to start...", true);
                        else
                                send_status_fail(xQueueTx);

                        return;
                }
                rf_tools_ble_stop_cont_tx();
                ble_waiting_response = true;
                break;
        case fsm_st_ble_tx:
                if (verbose)
                        send_simple_message(txq, "Busy waiting for TX to complete...", true);
                else
                        send_status_fail(xQueueTx);
                break;
        case fsm_st_ble_txstream:
                /* Handle case where tx has started but stack hasn't responded yet */
                if (ble_waiting_response) {
                        if (verbose)
                                send_simple_message(txq, "Busy waiting for Continuous TX to start...", true);
                        else
                                send_status_fail(xQueueTx);
                        return;
                }
                rf_tools_ble_stop_cont_tx();
                ble_waiting_response = true;
                break;

        default:
                if (verbose)
                        send_simple_message(txq, "Nothing to stop", true);
                else
                        send_status_fail(xQueueTx);
        }
}

void cmd_ble_txstop_response(uint8_t status, uint16_t packets)
{
        ble_waiting_response = false;
        fsm_state = fsm_st_idle;
        if (status == 0)
                if (verbose)
                        send_simple_message(xQueueTx, "Stopped BLE TX...", true);
                else
                        send_status_pass(xQueueTx);
        else {
                if (verbose)
                        send_simple_message(xQueueTx, "Error stopping BLE TX...", true);
                else
                        send_status_fail(xQueueTx);
        }
}

void cmd_ble_txstream_stop_response(void)
{
        ble_waiting_response = false;
        fsm_state = fsm_st_idle;
        if (verbose)
                send_simple_message(xQueueTx, "Stopped Continuous BLE TX...", true);
        else
                send_status_pass(xQueueTx);
}

void cmd_ble_tx(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ble_get_channel_rf(args[0]);
        int8_t power = get_power_setting(args[1]);
        uint8_t payload_len = *(uint8_t *)args[2];
        uint8_t payload_type = *(uint8_t *)args[3];
        uint16_t numpkts = *(uint16_t *)args[4];
        uint32_t intv = *(uint32_t *)args[5];


        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_BLE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (power < 0) {
                if (verbose)
                        send_simple_message(txq, POWER_ERANGE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (payload_len > 0x25) {
                if (verbose)
                        send_simple_message(txq, PAYLOAD_ERANGE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (payload_type > 2) {
                if (verbose)
                        send_simple_message(txq, PKTTYPE_EINVAL, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (numpkts) {
                fsm_state = fsm_st_ble_tx;
                if (verbose)
                        send_simple_message(xQueueTx, "Started BLE TX. Please wait...", false);
        } else {
                fsm_state = fsm_st_ble_tx_infinite;
                if (verbose)
                        send_simple_message(xQueueTx, "Started BLE TX. Enter ble_txstop to stop...", true);
                else
                        send_status_pass(xQueueTx);
        }

        hw_rf_set_tx_power(power);
        rf_tools_ble_start_pkt_tx_interval(ch, payload_len, payload_type, numpkts, intv);
        ble_waiting_response = true;
}

void cmd_ble_tx_response(uint8_t status)
{
        /* The standard ble TX command (0x201E) is not used by this tool.
         * Instead the custom TX with interval command is used.
         * So, this callback function should never be called by the rf_tools_ble frameworks,
         * in this app.
         */
        OS_ASSERT(0);
}

void cmd_ble_tx_interval_response(uint8_t completed, uint8_t status)
{
        ble_waiting_response = false;
        if (completed == 1) {
                if (status == 0) {
                        if (verbose)
                                send_simple_message(xQueueTx, "Done", true);
                        else
                                send_status_pass(xQueueTx);
                } else {
                        if (verbose)
                                send_simple_message(xQueueTx, "Error in arguments", true);
                        else
                                send_status_fail(xQueueTx);
                }

                fsm_state = fsm_st_idle;
        }
}

void cmd_ble_txstream(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ble_get_channel_rf(args[0]);
        int8_t power = get_power_setting(args[1]);
        uint8_t payload_type = *(uint8_t *)args[2];

        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_BLE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (power < 0) {
                if (verbose)
                        send_simple_message(txq, POWER_ERANGE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (payload_type > 2) {
                if (verbose)
                        send_simple_message(txq, PKTTYPE_EINVAL, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        fsm_state = fsm_st_ble_txstream;

        hw_rf_set_tx_power(power);
        rf_tools_ble_start_cont_tx(ch, payload_type);
        ble_waiting_response = true;
}

void cmd_ble_txstream_response(void)
{
        ble_waiting_response = false;
        if (verbose)
                send_simple_message(xQueueTx, "Started BLE Continuous TX...", true);
        else
                send_status_pass(xQueueTx);
}

void cmd_ble_rx(xQueueHandle txq, void **args)
{
        int8_t ch = rf_tools_ble_get_channel_rf(args[0]);

        if (fsm_state != fsm_st_idle) {
                if (verbose)
                        send_simple_message(txq, MESSAGE_BUSY, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        if (ch < 0) {
                if (verbose)
                        send_simple_message(txq, FREQ_ERANGE_BLE, true);
                else
                        send_status_fail(xQueueTx);
                return;
        }
        fsm_state = fsm_st_ble_rx;
        ble_waiting_response = true;
        rf_tools_ble_start_pkt_rx_stats(ch);
}

void cmd_ble_rx_response(void)
{
        ble_waiting_response = false;
        if (verbose)
                send_simple_message(xQueueTx, "Started BLE RX...", true);
        else
                send_status_pass(xQueueTx);
}

void cmd_ble_rxstop(xQueueHandle txq, void **args)
{
        if (fsm_state != fsm_st_ble_rx) {
                if (verbose)
                        send_simple_message(txq, "Nothing to stop", true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        /* Handle case where rx has started but stack hasn't responded yet */
        if (ble_waiting_response) {
                if (verbose)
                        send_simple_message(txq, "Busy waiting for TX to start...", true);
                else
                        send_status_fail(xQueueTx);
                return;
        }

        rf_tools_ble_stop_pkt_rx_stats();
        ble_waiting_response = true;
}

#define BLE_RXSTOP_RESPONSE_STRING_VERBOSE \
        "\r\n" \
        "Stopped BLE RX. Results: \r\n" \
        "  Packets received: %d\r\n" \
        "  Sync errors:      %d\r\n" \
        "  CRC errors:       %d\r\n" \
        "  RSSI:             %d\r\n\r\n"

#define BLE_RXSTOP_RESPONSE_STRING \
        "valid packets = %d\r\n" \
        "sync errors = %d\r\n" \
        "CRC errors = %d\r\n" \
        "RSSI = %d\r\n"

void cmd_ble_rxstop_response(uint16_t packets,
        uint16_t sync_errors, uint16_t crc_errors, uint16_t rssi)
{
        ble_waiting_response = false;
        fsm_state = fsm_st_idle;

        int rssi_dbm = convert_rssi_raw_to_dbm(true, rssi);

        /* Max Size of the buffer is the size of the template string, plus 1
         * for terminating null,
         * plus the max size for printing a uint16_t
         * (5 bytes) minus 2 (the %d is already there), times the number of args,
         * for uint16_t args
         */
        if (verbose) {
                char buf[sizeof(BLE_RXSTOP_RESPONSE_STRING_VERBOSE) + 5 * 5 + 1];

                snprintf(buf, sizeof(buf), BLE_RXSTOP_RESPONSE_STRING_VERBOSE,
                        packets, sync_errors, crc_errors, rssi_dbm);

                send_simple_message(xQueueTx, buf, true);
        } else {
                char buf[sizeof(BLE_RXSTOP_RESPONSE_STRING) + 5 * 5 + 1];

                snprintf(buf, sizeof(buf), BLE_RXSTOP_RESPONSE_STRING,
                        packets, sync_errors, crc_errors, rssi_dbm);

                send_simple_message(xQueueTx, buf, false);
                send_status_pass(xQueueTx);
        }
}
#endif /* CONFIG_USE_BLE */

static bool check_gpios(xQueueHandle txq, HW_GPIO_PORT port, HW_GPIO_PIN pin)
{
        if (!(port >= HW_GPIO_PORT_0 && port <= HW_GPIO_PORT_4)) {
                if (verbose) {
                        send_simple_message(txq, INVALID_GPIO_PORT, true);
                } else {
                        send_status_fail(xQueueTx);
                }
                return false;
        }

        if (!(pin >= HW_GPIO_PIN_0 && pin <= HW_GPIO_PIN_7) || (port == HW_GPIO_PORT_2 && pin >= HW_GPIO_PIN_4)) {
                if (verbose) {
                        send_simple_message(txq, INVALID_GPIO_PIN, true);
                } else {
                        send_status_fail(xQueueTx);
                }
                return false;
        }
        return true;
}

void cmd_gpio_set(xQueueHandle txq, void **args)
{
        HW_GPIO_PORT port = *(uint8_t *)args[0];
        HW_GPIO_PIN pin = *(uint8_t *)args[1];
        uint8_t dir = *(uint8_t *)args[2];
        uint8_t omode = *(uint8_t *)args[3];
        uint8_t state = *(uint8_t *)args[4];
        char msg[20];

        if (!check_gpios(txq, port, pin))
                return;

        if (dir > 3) {
                if (verbose) {
                        send_simple_message(txq, INVALID_GPIO_DIR, true);
                } else {
                        send_status_fail(xQueueTx);
                }
                return;
        }

        if (omode > 1) {
                if (verbose) {
                        send_simple_message(txq, INVALID_GPIO_OMODE, true);
                } else {
                        send_status_fail(xQueueTx);
                }
                return;
        }

        if (state!=0 && state!=1) {
                if (verbose) {
                        send_simple_message(txq, INVALID_STATE, true);
                } else {
                        send_status_fail(xQueueTx);
                }
                return;
        }

        if (set_gpio(port, pin, dir, omode, state)) {
                if (verbose) {
                        sprintf(msg, "GPIO P%u_%u set to %u", port, pin, state);
                        send_simple_message(txq, msg, true);
                } else {
                        send_status_pass(xQueueTx);
                }
        } else {
                if (verbose) {
                        send_simple_message(txq, INVALID_GPIO_PIN, true);
                } else {
                        send_status_fail(xQueueTx);
                }
        }
}

void cmd_gpio_get(xQueueHandle txq, void **args)
{
        HW_GPIO_PORT port = *(uint8_t *)args[0];
        HW_GPIO_PIN pin = *(uint8_t *)args[1];
        uint8_t value;

        char msg[100];

        if (!check_gpios(txq, port, pin))
                return;

        value = hw_gpio_get_pin_status(port, pin);

        if (verbose) {
                sprintf(msg, "GPIO P%u%u value=%u", port, pin, value);
        } else{
                sprintf(msg, "%u", value);
        }
        send_simple_message(txq, msg, true);
}



void cmd_all_gpio_set(xQueueHandle txq, void **args)
{
        uint8_t port, pin, state = *(uint8_t *)args[0];
        char msg[20];

        if (state!=0 && state!=1) {
                if (verbose) {
                        send_simple_message(txq, INVALID_STATE, true);
                } else {
                        send_status_fail(xQueueTx);
                }
                return;
        }

        for (port = HW_GPIO_PORT_0; port<=HW_GPIO_PORT_4; port++) {
                for (pin = HW_GPIO_PIN_0; pin<=HW_GPIO_PIN_7; pin++) {
                        set_gpio(port, pin, 3, 0, state);
                }
        }

        if (verbose) {
                sprintf(msg, "All valid GPIOs set to %u", state);
                send_simple_message(txq, msg, true);
        } else {
                send_status_pass(xQueueTx);
        }
}


int set_gpio(HW_GPIO_PORT port, HW_GPIO_PIN pin, uint8_t dir, uint8_t omode, uint8_t state)
{
        uint8_t val =0;
        HW_GPIO_MODE mode;

        //P0_0 to P0_5 are the QSPI pins.
        //P0_6 is for the SWDIO
        if (port == HW_GPIO_PORT_0 && pin <= HW_GPIO_PIN_6) {
                return 0;
        }

        //P2 has 5 pins and P2_4 is the SWCLK
        if (port == HW_GPIO_PORT_2 && pin >= HW_GPIO_PIN_4) {
                return 0;
        }

        //P2_0 and P2_1 are the XTAL32 pads. DO NOT TOUCH THEM
        if ((port == HW_GPIO_PORT_2 && pin >= HW_GPIO_PIN_0) ||
                (port == HW_GPIO_PORT_2 && pin >= HW_GPIO_PIN_1)) {
                return 0;
        }

        //P1_1 and P2_2 are the USB port pins.
        //They need special handling to power them and use them as GPIOs.
        if ((port == HW_GPIO_PORT_1 && pin == HW_GPIO_PIN_1) ||
                (port == HW_GPIO_PORT_2 && pin == HW_GPIO_PIN_2)) {
                REG_CLR_BIT(USB, USB_MCTRL_REG, USBEN);
                REG_SET_BIT(CRG_PER, USBPAD_REG, USBPAD_EN);
        }

#if defined(LOGGING_MODE_STANDALONE) || defined(LOGGING_MODE_QUEUE) || defined(LOGGING_MODE_RETARGET) || defined(LOGGING_MODE_RTT) //IS LOGGING is enabled skip LOG UART
        if (port == LOGGING_STANDALONE_GPIO_PORT_UART_TX && pin == LOGGING_STANDALONE_GPIO_PIN_UART_TX) return 0;
        if (port == LOGGING_STANDALONE_GPIO_PORT_UART_RX && pin == LOGGING_STANDALONE_GPIO_PIN_UART_RX) return 0;
#endif

        //SKIP the communication UART
        if (port == MCIF_GPIO_PORT_UART_TX && pin == MCIF_GPIO_PIN_UART_TX) {
                return 0;
        }

        if (port == MCIF_GPIO_PORT_UART_RX && pin == MCIF_GPIO_PIN_UART_RX) {
                return 0;
        }

        //If FEM is enabled skip the FEM control pins
#if dg_configFEM == FEM_SKY66112_11
        if (port == dg_configFEM_SKY66112_11_CSD_PORT && pin == dg_configFEM_SKY66112_11_CSD_PIN) {
                return 0;
        }
        if (port == dg_configFEM_SKY66112_11_CPS_PORT && pin == dg_configFEM_SKY66112_11_CPS_PIN) {
                return 0;
        }
        if (port == dg_configFEM_SKY66112_11_CRX_PORT && pin == dg_configFEM_SKY66112_11_CRX_PIN) {
                return 0;
        }
        if (port == dg_configFEM_SKY66112_11_CTX_PORT && pin == dg_configFEM_SKY66112_11_CTX_PIN) {
                return 0;
        }
        if (port == dg_configFEM_SKY66112_11_CHL_PORT && pin == dg_configFEM_SKY66112_11_CHL_PIN) {
                return 0;
        }
#endif

        //Set the selected pin voltage
        hw_gpio_configure_pin_power(port, pin, gpio_rail ? HW_GPIO_POWER_V33 : HW_GPIO_POWER_VDD1V8P);

        switch (dir) {
        case 0:
                mode = HW_GPIO_MODE_INPUT;
                break;
        case 1:
                mode = HW_GPIO_MODE_INPUT_PULLUP;
                break;
        case 2:
                mode = HW_GPIO_MODE_INPUT_PULLDOWN;
                break;
        case 3:
                if (omode == 0) {
                        mode = HW_GPIO_MODE_OUTPUT;
                } else {
                        mode = HW_GPIO_MODE_OUTPUT_OPEN_DRAIN;
                }
                break;
        default:
                return 0;
        }

        //Configure the pin
        hw_gpio_configure_pin(port, pin, mode, HW_GPIO_FUNC_GPIO, state==1?true:false);

        return 1;
}


void cmd_gpio_volts_set(xQueueHandle txq, void **args)
{//we set the flag for the voltage to be applied since the NEXT GPIO(s) change. We DO NOT change the voltage here. This way the user can set individual pins to V33 or V18P
        char *rail = (char *)args[0];

        if (strncmp(rail,"V33", 3)==0) {
                gpio_rail=1;
                if (verbose) {
                        send_simple_message(txq, "Next GPIOs set/cleared using 'da_gpio_set' or 'da_gpio_set_all' command will be powered by V33 rail", true);
                } else {
                        send_status_pass(xQueueTx);
                }
                return;
        }

        if (strncmp(rail,"V18P", 4)==0) {
                gpio_rail=0;
                if (verbose) {
                        send_simple_message(txq, "Next GPIOs set/cleared using 'da_gpio_set' or 'da_gpio_set_all' command will be powered by V18P rail", true);
                } else {
                        send_status_pass(xQueueTx);
                }
                return;
        }

        if (verbose) {
                send_simple_message(txq, "Possible parameters are 'V33' and 'V18P' case sensitive.", true);
        } else {
                send_status_fail(xQueueTx);
        }
}

static inline uint16_t auto_xtal_trim(uint16_t gpio_input)
{

        int r;
        HW_GPIO_PORT port;
        HW_GPIO_PIN pin;
        HW_GPIO_MODE mode;
        HW_GPIO_FUNC function;

        int gpio = gpio_input & 0xFF;
        port = gpio / 10;
        pin = gpio % 10;

        /* Store pulse input gpio previous mode and function */
        hw_gpio_get_pin_function(port, pin, &mode, &function);

        r = auto_trim(gpio);

        /* Restore pulse input gpio previous mode and functions.
         * This is needed because they use the UART RX pin for
         * pulse input. It must be restored to resume UART operation
         */
        hw_gpio_set_pin_function(port, pin, mode, function);


        if (r < 0) {
                return -r;
        } else {
                return 0;
        }
}


static inline void enable_output_xtal(uint8_t port, uint8_t pin)
{
        GPIO->GPIO_CLK_SEL = 0x3; /* Select XTAL16 clock */
        hw_gpio_set_pin_function(port, pin, HW_GPIO_MODE_OUTPUT, HW_GPIO_FUNC_CLOCK);
}

static inline void disable_output_xtal(uint8_t port, uint8_t pin)
{
        hw_gpio_set_pin_function(port, pin, HW_GPIO_MODE_INPUT, HW_GPIO_FUNC_GPIO);
}


void cmd_xtal16_trim(xQueueHandle txq, void **args)
{
        uint8_t *b;
        char *operation = (char *)args[0];
        uint32_t value = strtol(args[1], NULL, 16);
        uint16_t outval = 0;
        uint8_t onoff, pin, port;
        char msg[100];

        if (strncmp(operation, "read", 4)==0) {
                /* Read trim value */
                outval = CRG_TOP->CLK_FREQ_TRIM_REG;
                sprintf(msg, "CLK_FREQ_TRIM_REG=0x%04x", outval);
        } else {
                if (strncmp(operation, "write", 5)==0) {
                        if (value == 0) {
                                sprintf(msg, "Invalid XTAL trimming value 0x%04x", value);
                                send_simple_message(txq, msg, true);
                                return;
                        }
                        /* Write trim value */
                        CRG_TOP->CLK_FREQ_TRIM_REG = value;
                        outval = CRG_TOP->CLK_FREQ_TRIM_REG;
                        sprintf(msg, "CLK_FREQ_TRIM_REG=0x%04x", outval);
                } else {
                        if (strncmp(operation, "output", 6)==0) {
                                onoff = (value&0xf00)>>8;
                                port = (value&0x0f0)>>4;
                                pin = (value&0x00f);
                                switch (onoff)
                                {
                                case 0: /* Disable output xtal on Pxx */
                                        sprintf(msg, "XTAL16M on P%u%u disabled", port, pin);
                                        disable_output_xtal(port, pin);
                                        break;
                                case 1: /* Enable output xtal on Pxx */
                                        enable_output_xtal(port, pin);
                                        sprintf(msg, "XTAL16M on P%u%u enabled", port, pin);
                                        break;
                                default:
                                        sprintf(msg, "Invalid 'out' sub-command parameter");
                                }
                        } else {
                                if (strncmp(operation, "+delta",6)==0) {
                                        if (value == 0) {
                                                sprintf(msg, "Invalid XTAL delta trimming value 0x%04x", value);
                                                send_simple_message(txq, msg, true);
                                                return;
                                        }
                                        /* Increase trim value by delta */
                                        CRG_TOP->CLK_FREQ_TRIM_REG = CRG_TOP->CLK_FREQ_TRIM_REG + value;
                                        outval = CRG_TOP->CLK_FREQ_TRIM_REG;
                                        sprintf(msg, "CLK_FREQ_TRIM_REG=0x%04X", outval);
                                } else {
                                        if (strncmp(operation, "-delta",6)==0) {
                                                if (value == 0) {
                                                        sprintf(msg, "Invalid XTAL delta trimming value 0x%04x", value);
                                                        send_simple_message(txq, msg, true);
                                                        return;
                                                }
                                                /* Decrease trim value by delta */
                                                CRG_TOP->CLK_FREQ_TRIM_REG = CRG_TOP->CLK_FREQ_TRIM_REG - value;
                                                outval = CRG_TOP->CLK_FREQ_TRIM_REG;
                                                sprintf(msg, "CLK_FREQ_TRIM_REG=0x%04x", outval);
                                        } else {
                                                if (strncmp(operation, "autotrim", 8)==0) {
                                                        value = strtol(args[1], NULL, 10);
                                                        if (value == 0) {
                                                                sprintf(msg, "Invalid port/pin value %u", value);
                                                                send_simple_message(txq, msg, true);
                                                                return;
                                                        }
                                                        /* Auto calibration test */
                                                        outval = auto_xtal_trim(value);
                                                        if (outval == 0){
                                                                sprintf(msg, "CLK_FREQ_TRIM_REG=0x%04x", CRG_TOP->CLK_FREQ_TRIM_REG);
                                                        }

                                                        if (outval == 1)
                                                        {
                                                                sprintf(msg, "FAIL: trimming out of limits");
                                                        }

                                                        if (outval == 2)
                                                        {
                                                                sprintf(msg, "FAIL: Reference 500ms pulse not detected on P%u", value);
                                                        }
                                                } else {
                                                        /* not recognized command */
                                                        sprintf(msg, "Command not recognized"); // change the message, use definitions
                                                }
                                        }
                                }
                        }
                }
        }

        send_simple_message(txq, msg, true);

        return;
}



