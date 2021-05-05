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
 * @brief Board Support Package. User Configuration file for cached QSPI mode.
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

#define CONFIG_USE_BLE
#define CONFIG_USE_FTDF


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
#define SW_CURSOR_PORT                          1
#define SW_CURSOR_PIN                           2

#define dg_configCACHEABLE_QSPI_AREA_LEN        (NVMS_PARAM_PART_start - MEMORY_QSPIF_BASE)


/*************************************************************************************************\
 * FreeRTOS specific config
 */
#define OS_FREERTOS                              /* Define this to use FreeRTOS */
#define configTOTAL_HEAP_SIZE                    13312   /* This is the FreeRTOS Total Heap Size */

/*************************************************************************************************\
 * FTDF specific config
 */

#define FTDF_NO_CSL           /* Define this to disable CSL mode */
#define FTDF_NO_TSCH          /* Define this to disable TSCH mode */
#define FTDF_LITE              /* Define this to enable LITE mode (only transparent mode enabled) */
#define FTDF_PHY_API            /* Define this to enable PHY API mode (no FTDF adapter;
                                 implies FTDF_LITE) */
#define FTDF_COEX_SUPPORT     /* Define to enable arbiter support for FTDF. */

#define FTDF_USE_AUTO_PTI                       (1)
#define FTDF_DBG_BUS_USE_PORT_4                 (1)

/*************************************************************************************************\
 * BLE specific config
 */
#define dg_configFLASH_ADAPTER                  (1)
#define dg_configNVMS_ADAPTER                   (1)

/*************************************************************************************************\
 * COEX specific config
 */
#define dg_configCOEX_ENABLE_STATS              (1)
#define dg_configCOEX_ENABLE_CONFIG             (1)
#define dg_configCOEX_DIAGS_MODE                (HW_COEX_DIAG_MODE_3)
#define dg_configUSE_HW_COEX                    (1)

/*************************************************************************************************\
 * Basic application configuration. Change this to alter application behaviour
 */

/* Enable/disable FTDF diagnostic bus (0: Disable, 1: Enable) */
#define FTDF_DBG_BUS_ENABLE                     (1)

/* Enable/disable COEX diagnostic bus (0: Disable, 1: Enable) */
#define dg_configCOEX_ENABLE_DIAGS              (1)
#define dg_configCOEX_DIAGS_MODE                (HW_COEX_DIAG_MODE_3)

/* Enable/disable BLE diagnostic bus (0: Disable, 5: Enable (bits [0:1], to coexist with COEX Diags) */
#define dg_configBLE_DIAGN_CONFIG               (5)

#define FTDF_RX_HIGHER_THAN_BLE                 (1)

#define defaultBLE_PPCP_INTERVAL_MIN            (BLE_CONN_INTERVAL_FROM_MS(20))
#define defaultBLE_PPCP_INTERVAL_MAX            (BLE_CONN_INTERVAL_FROM_MS(20))

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
