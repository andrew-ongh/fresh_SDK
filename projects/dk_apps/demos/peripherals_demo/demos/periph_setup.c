/**
 ****************************************************************************************
 *
 * @file periph_setup.c
 *
 * @brief Peripherals setup for application
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <hw_gpio.h>
#include <hw_wkup.h>
#include <platform_devices.h>
#include "config.h"
#include "gpio_setup.h"
#include "common.h"

/* GPIO pins configuration array, see periph_setup() for details */
static const gpio_config gpio_cfg[] = {

#if CFG_DEMO_HW_TIMER0
        /* Timer0 timer */
        HW_GPIO_PINCONFIG(CFG_GPIO_TIMER0_PORT,  CFG_GPIO_TIMER0_PIN,      OUTPUT,       PWM0,      true),
#endif // CFG_DEMO_HW_TIMER0

#if CFG_DEMO_HW_GPADC || CFG_DEMO_AD_GPADC
        /* General Purpose ADC */
        //HW_GPIO_PINCONFIG(0,                     6,                         INPUT,        ADC,       true),
        HW_GPIO_PINCONFIG(0,                     7,                         INPUT,        ADC,       true),
        //HW_GPIO_PINCONFIG(1,                     0,                         INPUT,        ADC,       true),
        HW_GPIO_PINCONFIG(1,                     2,                         INPUT,        ADC,       true),
#if dg_configBLACK_ORCA_MB_REV != BLACK_ORCA_MB_REV_D
        HW_GPIO_PINCONFIG(1,                     3,                         INPUT,        ADC,       true),
#endif
        HW_GPIO_PINCONFIG(1,                     4,                         INPUT,        ADC,       true),
        //HW_GPIO_PINCONFIG(1,                     5,                         INPUT,        ADC,       true),
        //HW_GPIO_PINCONFIG(2,                     4,                         INPUT,        ADC,       true),
#endif // CFG_DEMO_HW_GPADC

#if CFG_DEMO_HW_IRGEN
        /* IR generator */
        HW_GPIO_PINCONFIG(CFG_GPIO_IR_PORT,      CFG_GPIO_IR_PIN,          OUTPUT,       IR_OUT,    true),
#endif // CFG_DEMP_HW_IRGEN

#if CFG_DEMO_HW_QUAD
        /* Quadrature decoder */
        HW_GPIO_PINCONFIG(CFG_GPIO_QUAD_XA_PORT, CFG_GPIO_QUAD_XA_PIN,     INPUT,        QUADEC_XA, true),
        HW_GPIO_PINCONFIG(CFG_GPIO_QUAD_XB_PORT, CFG_GPIO_QUAD_XB_PIN,     INPUT,        QUADEC_XB, true),
        HW_GPIO_PINCONFIG(CFG_GPIO_QUAD_YA_PORT, CFG_GPIO_QUAD_YA_PIN,     INPUT,        QUADEC_YA, true),
        HW_GPIO_PINCONFIG(CFG_GPIO_QUAD_YB_PORT, CFG_GPIO_QUAD_YB_PIN,     INPUT,        QUADEC_YB, true),
        HW_GPIO_PINCONFIG(CFG_GPIO_QUAD_ZA_PORT, CFG_GPIO_QUAD_ZA_PIN,     INPUT,        QUADEC_ZA, true),
        HW_GPIO_PINCONFIG(CFG_GPIO_QUAD_ZB_PORT, CFG_GPIO_QUAD_ZB_PIN,     INPUT,        QUADEC_ZB, true),
#endif // CFG_DEMO_HW_QUAD

#if CFG_DEMO_HW_TIMER1
        /* Timer1 */
        HW_GPIO_PINCONFIG(CFG_GPIO_TIMER1_PWM_PORT,
                                                 CFG_GPIO_TIMER1_PWM_PIN,  OUTPUT,       PWM5,      true),
#endif // CFG_DEMO_HW_TIMER1

#if CFG_DEMO_HW_TIMER2
        /* Timer2 */
        HW_GPIO_PINCONFIG(CFG_GPIO_TIMER2_PWM2_PORT,
                                                 CFG_GPIO_TIMER2_PWM2_PIN, OUTPUT,       PWM2,      true),
        HW_GPIO_PINCONFIG(CFG_GPIO_TIMER2_PWM3_PORT,
                                                 CFG_GPIO_TIMER2_PWM3_PIN, OUTPUT,       PWM3,      true),
        HW_GPIO_PINCONFIG(CFG_GPIO_TIMER2_PWM4_PORT,
                                                 CFG_GPIO_TIMER2_PWM4_PIN, OUTPUT,       PWM4,      true),
#endif // CFG_DEMO_HW_WKUP

#if CFG_DEMO_HW_WKUP
        /* Wakeup timer */
        HW_GPIO_PINCONFIG(CFG_GPIO_WKUP_1_PORT,  CFG_GPIO_WKUP_1_PIN,      INPUT_PULLUP, GPIO,      true),
        HW_GPIO_PINCONFIG(CFG_GPIO_WKUP_2_PORT,  CFG_GPIO_WKUP_2_PIN,      INPUT_PULLUP, GPIO,      true),
        HW_GPIO_PINCONFIG(CFG_GPIO_WKUP_3_PORT,  CFG_GPIO_WKUP_3_PIN,      INPUT_PULLUP, GPIO,      true),
#endif // CFG_DEMO_HW_WKUP


#if CFG_DEMO_HW_I2C || CFG_AD_I2C_1
        /* I2C */
        HW_GPIO_PINCONFIG(CFG_GPIO_I2C1_SCL_PORT,
                                                 CFG_GPIO_I2C1_SCL_PIN,    OUTPUT,       I2C_SCL,   true),
        HW_GPIO_PINCONFIG(CFG_GPIO_I2C1_SDA_PORT,
                                                 CFG_GPIO_I2C1_SDA_PIN,    INPUT,        I2C_SDA,   true),
#endif // CFG_DEMO_HW_I2C || CFG_DEMO_HW_I2C_ASYNC || CFG_DEMO_AD_SPI_I2C

#if CFG_AD_SPI_1
        /* SPI1 */
        HW_GPIO_PINCONFIG(CFG_GPIO_SPI1_CLK_PORT,
                                                 CFG_GPIO_SPI1_CLK_PIN,    OUTPUT,       SPI_CLK,   true),
        HW_GPIO_PINCONFIG(CFG_GPIO_SPI1_DO_PORT, CFG_GPIO_SPI1_DO_PIN,     OUTPUT,       SPI_DO,    true),
        HW_GPIO_PINCONFIG(CFG_GPIO_SPI1_DI_PORT, CFG_GPIO_SPI1_DI_PIN,     INPUT,        SPI_DI,    true),
        HW_GPIO_PINCONFIG(CFG_GPIO_SPI1_CS_PORT, CFG_GPIO_SPI1_CS_PIN,     OUTPUT,       GPIO,      true),
#if CFG_DEMO_SENSOR_ADXL362
        /* Chip select for ADXL362 */
        HW_GPIO_PINCONFIG(CFG_GPIO_ADXL362_CS_PORT,
                                                 CFG_GPIO_ADXL362_CS_PIN,  OUTPUT,       GPIO,      true),
#endif // CFG_DEMO_SENSOR_ADXL362
#endif // CFG_AD_SPI_1

        HW_GPIO_PINCONFIG_END // important!!!
};

void periph_setup(void)
{
        /* configure GPIOs for input/output UART */
#       if dg_configBLACK_ORCA_MB_REV == BLACK_ORCA_MB_REV_D
#               define UART_TX_PORT    HW_GPIO_PORT_1
#               define UART_TX_PIN     HW_GPIO_PIN_3
#               define UART_RX_PORT    HW_GPIO_PORT_2
#               define UART_RX_PIN     HW_GPIO_PIN_3
#       else
#               error "Unknown value for dg_configBLACK_ORCA_MB_REV!"
#       endif
        hw_gpio_set_pin_function(UART_TX_PORT, UART_TX_PIN, HW_GPIO_MODE_OUTPUT,
                        HW_GPIO_FUNC_UART_TX);
        hw_gpio_set_pin_function(UART_RX_PORT, UART_RX_PIN, HW_GPIO_MODE_INPUT,
                        HW_GPIO_FUNC_UART_RX);

#if CFG_DEMO_AD_UART
        /*
         * Before using any peripheral its IO signals should be mapped to IO pins. In general, any
         * peripheral IO signal can be assigned to any IO pin (with exception of GPADC and QSPI
         * which have fixed pins assigned).
         *
         * Each mapping can be done by assigning mode (input/output with optional pull-up/pull-down
         * and open drain) and function to given IO pin. In case of UART2 we need two pins configured
         * as outputs for TX and RX function.
         *
         * \note In case of general purpose IO pins (function set to GPIO) it's possible to select
         * initial state for such pin by using hw_gpio_configure_pin() function.
         */
        hw_gpio_set_pin_function(CFG_GPIO_UART2_TX_PORT, CFG_GPIO_UART2_TX_PIN,
                        HW_GPIO_MODE_OUTPUT, HW_GPIO_FUNC_UART2_TX);
        hw_gpio_set_pin_function(CFG_GPIO_UART2_RX_PORT, CFG_GPIO_UART2_RX_PIN,
                        HW_GPIO_MODE_INPUT, HW_GPIO_FUNC_UART2_RX);
#endif

        /*
         * In most cases application will configure a lot of pins, e.g. during initialization. In
         * such case it's usually convenient to use some predefined configuration of pins. This can
         * be described using array of \p gpio_config structures where each element describes single
         * pin configuration and it can be easily created using \p HW_GPIO_PINCONFIG macro. Since
         * array can have variable number of elements it should be terminated by special entry
         * which can be inserted using \p HW_GPIO_PINCONFIG_END macro. See \p gpio_cfg definition
         * above for an example.
         */
        hw_gpio_configure(gpio_cfg);

        /* do some peripherals initialization necessary to run demos */
#if CFG_DEMO_HW_GPADC
        demo_gpadc_init();
#endif
#if CFG_DEMO_HW_I2C
        demo_i2c_init();
#endif

#if CFG_DEMO_HW_QUAD
        demo_quad_init();
#endif
#if CFG_DEMO_HW_TIMER2
        demo_timer2_init();
#endif

#if CFG_DEMO_POWER_MODE
        hw_gpio_set_pin_function(HW_GPIO_PORT_1, HW_GPIO_PIN_6, HW_GPIO_MODE_INPUT_PULLUP,
                HW_GPIO_FUNC_GPIO);

#endif
}
