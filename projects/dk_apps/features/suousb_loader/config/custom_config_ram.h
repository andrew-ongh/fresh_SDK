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
 * @file custom_config_ram.h
 *
 * @brief Board Support Package. User Configuration file for execution from RAM.
 *
 * Copyright (C) 2016-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef CUSTOM_CONFIG_RAM_H_
#define CUSTOM_CONFIG_RAM_H_

#include "bsp_definitions.h"

#undef CONFIG_USE_BLE
#undef CONFIG_USE_FTDF

#define dg_configCACHEABLE_QSPI_AREA_LEN        (NVMS_PARAM_PART_start - MEMORY_QSPIF_BASE)

/* Chose UART that will be used in loader 0 - no UART, 1 or 2 */
#define LOADER_UART                             2

#if LOADER_UART
        #define CONFIG_RETARGET
        #if LOADER_UART == 2
                #define CONFIG_RETARGET_UART HW_UART2
        #elif LOADER_UART == 1
                #define CONFIG_RETARGET_UART HW_UART1
        #else
                #error "Invalid LOADER_UART configuration!"
        #endif
#endif


#define dg_configUSE_LP_CLK                     LP_CLK_32768
#define dg_configEXEC_MODE                      MODE_IS_RAM
#define dg_configCODE_LOCATION                  NON_VOLATILE_IS_NONE
#define dg_configEXT_CRYSTAL_FREQ               EXT_CRYSTAL_IS_16M

#define dg_configIMAGE_SETUP                    DEVELOPMENT_MODE
#define dg_configEMULATE_OTP_COPY               (0)

#define dg_configUSER_CAN_USE_TIMER1            (0)

#define dg_configUSE_DCDC                       (1)

#define dg_configFLASH_CONNECTED_TO             (FLASH_CONNECTED_TO_1V8)
#define dg_configFLASH_POWER_DOWN               (0)

#define dg_configPOWER_1V8_ACTIVE               (1)
#define dg_configPOWER_1V8_SLEEP                (1)

#define dg_configBATTERY_TYPE                   (BATTERY_TYPE_LIMN2O4)
#define dg_configBATTERY_CHARGE_CURRENT         2       // 30mA
#define dg_configBATTERY_PRECHARGE_CURRENT      20      // 2.1mA
#define dg_configBATTERY_CHARGE_NTC             1       // disabled

#define dg_configUSE_HW_USB                     1
#define dg_configUSE_USB                        1
#define dg_configUSE_USB_CHARGER                1
#define dg_configUSE_USB_ENUMERATION            1

#define dg_configALLOW_CHARGING_NOT_ENUM        1
#define dg_configUSE_ProDK                      (1)
#define dg_configUSE_SW_CURSOR                  (1)
#define dg_configRF_ADAPTER                     (0)
#define dg_configUSE_HW_RF                      (0)

/*************************************************************************************************\
 * Memory layout specific config
 */
#define dg_configRAM_RAM_SIZE_AE                (24 * 1024)
#define dg_configRAM_RETRAM_0_SIZE_AE           (120 * 1024 - CODE_SIZE)

#define dg_configRAM_RAM_SIZE_BB                (24 * 1024)
#define dg_configRAM_RETRAM_0_SIZE_BB           (41 * 1024)

/*************************************************************************************************\
 * FreeRTOS specific config
 */
#define OS_FREERTOS                              /* Define this to use FreeRTOS */
#define configTOTAL_HEAP_SIZE                    (14*1024)   /* This is the FreeRTOS Total Heap Size */

/*************************************************************************************************\
 * Peripheral specific config
 */
#define dg_configFLASH_ADAPTER                  1
#define dg_configNVMS_ADAPTER                   1
#define dg_configNVMS_VES                       1

#define dg_configDEBUG_TRACE                    1
#if (dg_configDEBUG_TRACE == 1)
#define LIGHTWEIGHT_DEBUG
#endif

/*
 * When enabled, a special button is detected as having been pressed or not during boot.
 * If the button is pressed, SUOTA service will be started without booting any flashed
 * application. This allows the user to force SUOTA service in certain circumstances.
 */

#define USE_PARTITION_TABLE_1MB_WITH_SUOTA

/*************************************************************************************************\
 * BLE device config
 */
#define dg_configBLE_CENTRAL                    (0)
#define dg_configBLE_GATT_CLIENT                (0)
#define dg_configBLE_OBSERVER                   (0)
#define dg_configBLE_BROADCASTER                (0)
#define dg_configBLE_L2CAP_COC                  (0)

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
