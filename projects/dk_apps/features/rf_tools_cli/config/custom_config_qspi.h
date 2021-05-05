/**
\addtogroup BSP
\{
\addtogroup CONFIG
\{
\addtogroup CUSTOM
\{
*/

/**
 ****************************************************************************************
 *
 * @file custom_config_qspi.h
 *
 * @brief Board Support Package. User Configuration file for execution from QSPI.
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef CUSTOM_CONFIG_QSPI_H_
#define CUSTOM_CONFIG_QSPI_H_

#include "bsp_definitions.h"

#define dg_configUSE_LP_CLK                     LP_CLK_32768
#define dg_configEXEC_MODE                      MODE_IS_CACHED
#define dg_configCODE_LOCATION                  NON_VOLATILE_IS_FLASH
#define dg_configEXT_CRYSTAL_FREQ               EXT_CRYSTAL_IS_16M

#define dg_configIMAGE_SETUP                    DEVELOPMENT_MODE
#define dg_configEMULATE_OTP_COPY               (0)

#define dg_configUSER_CAN_USE_TIMER1            (0)

#define dg_configUSE_WDOG                       (0)


#define dg_configFLASH_CONNECTED_TO             (FLASH_CONNECTED_TO_1V8)
#define dg_configFLASH_POWER_DOWN               (0)

#define dg_configPOWER_1V8_ACTIVE               (1)
#define dg_configPOWER_1V8_SLEEP                (1)

#define dg_configBATTERY_TYPE                   (BATTERY_TYPE_LIMN2O4)
#define dg_configBATTERY_CHARGE_CURRENT         2       // 30mA
#define dg_configBATTERY_PRECHARGE_CURRENT      20      // 2.1mA
#define dg_configBATTERY_CHARGE_NTC             1       // disabled

#define dg_configUSE_USB                        0
#define dg_configUSE_USB_CHARGER                0
#define dg_configALLOW_CHARGING_NOT_ENUM        1

#define dg_configUSE_ProDK                      (1)

#define dg_configUSE_SW_CURSOR                  (1)

#define dg_configCACHEABLE_QSPI_AREA_LEN        (NVMS_PARAM_PART_start - MEMORY_QSPIF_BASE)


/*************************************************************************************************\
 * FreeRTOS specific config
 */
#define OS_FREERTOS                              /* Define this to use FreeRTOS */
#define configTOTAL_HEAP_SIZE                    14000   /* This is the FreeRTOS Total Heap Size */

/*************************************************************************************************\
 * FTDF specific config
 */

#define FTDF_NO_CSL           /* Define this to disable CSL mode */
#define FTDF_NO_TSCH          /* Define this to disable TSCH mode */
#define FTDF_LITE              /* Define this to enable LITE mode (only transparent mode enabled) */
#define FTDF_PHY_API            /* Define this to enable PHY API mode (no FTDF adapter) */

#define BLE_EXTERNAL_HOST
#define BLE_PROD_TEST

/*************************************************************************************************\
 * Logging library specific config
 *
 * See logging/include/logging.h for description of configuration options
 */
#undef LOGGING_MODE_STANDALONE
#undef LOGGING_MODE_QUEUE
#undef LOGGING_MODE_RETARGET
#undef LOGGING_MODE_RTT

#define LOGGING_MIN_COMPILED_SEVERITY           LOG_DEBUG
#define LOGGING_MIN_DEFAULT_SEVERITY            LOG_DEBUG
#define LOGGING_MIN_ALLOWED_FREE_HEAP           600

#define LOGGING_STANDALONE_UART                 HW_UART1
#define LOGGING_STANDALONE_GPIO_PORT_UART_TX    HW_GPIO_PORT_1
#define LOGGING_STANDALONE_GPIO_PIN_UART_TX     HW_GPIO_PIN_7
#define LOGGING_STANDALONE_GPIO_PORT_UART_RX    HW_GPIO_PORT_1
#define LOGGING_STANDALONE_GPIO_PIN_UART_RX     HW_GPIO_PIN_6
#define LOGGING_STANDALONE_UART_BAUDRATE        HW_UART_BAUDRATE_115200
#define LOGGING_STANDALONE_UART_DATABITS        HW_UART_DATABITS_8
#define LOGGING_STANDALONE_UART_STOPBITS        HW_UART_STOPBITS_1
#define LOGGING_STANDALONE_UART_PARITY          HW_UART_PARITY_NONE
#define LOGGING_QUEUE_LENGTH                    12
#define LOGGING_MIN_MSG_SIZE                    16
#define LOGGING_SUPPRESSED_COUNT_ENABLE         1
#define LOGGING_SUPPRESSED_MIN_COUNT            5
#define LOGGING_SUPPRESSED_MSG_TMPL             "%d messages were suppressed\n\r"
#define LOGGING_SUPPRESSED_SEVERITY             LOG_NOTICE
#define LOGGING_SUPPRESSED_TAG                  0

/*************************************************************************************************\
 * MCIF library specific config
 *
 * See mcif/include/mcif.h for description of configuration options
 */
#define MCIF_USE_TASK_NOTIFICATIONS             1
#define MCIF_MAX_ARGS                           6
#define MCIF_UART                               HW_UART2
#define MCIF_GPIO_PORT_UART_RX                  HW_GPIO_PORT_2
#define MCIF_GPIO_PIN_UART_RX                   HW_GPIO_PIN_3
#define MCIF_GPIO_PORT_UART_TX                  HW_GPIO_PORT_1
#define MCIF_GPIO_PIN_UART_TX                   HW_GPIO_PIN_3
#define MCIF_UART_BAUDRATE                      HW_UART_BAUDRATE_115200
#define MCIF_UART_DATABITS                      HW_UART_DATABITS_8
#define MCIF_UART_STOPBITS                      HW_UART_STOPBITS_1
#define MCIF_UART_PARITY                        HW_UART_PARITY_NONE
#define MCIF_LOG_TAG                            30

#define MCIF_UART_DMA_BUFFER                    1024

#define dg_configUSE_HW_COEX                    (0)

#define dg_configUSE_BOD                        (0)

/* Include bsp default values */
#include "bsp_defaults.h"
/* Include memory layout */
#include "bsp_memory_layout.h"

#endif /* CUSTOM_CONFIG_QSPI_H_ */

/**
\}
\}
\}
*/
