/**
 ****************************************************************************************
 *
 * @file gpio_setup.h
 *
 * @brief GPIO pin assignments for demo
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

/*
 * IMPORTANT!
 * this is sample configuration for peripherals demo application GPIO assignments
 * for changing configuration, it's recommended to copy this file to "config/gpio_setup.h" and make
 * changes there - it will be used instead of this file.
 */

/* Breath timer */
#if CFG_DEMO_HW_BREATH
#define CFG_GPIO_BREATH_PORT            (HW_GPIO_PORT_4)
#define CFG_GPIO_BREATH_PIN             (HW_GPIO_PIN_0)
#endif

/* IR generator */
#define CFG_GPIO_IR_PORT                (HW_GPIO_PORT_4)
#define CFG_GPIO_IR_PIN                 (HW_GPIO_PIN_5)

/* Quadrature decoder */
#if CFG_DEMO_HW_QUAD
#define CFG_GPIO_QUAD_XA_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_QUAD_XA_PIN            (HW_GPIO_PIN_4)
#define CFG_GPIO_QUAD_XB_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_QUAD_XB_PIN            (HW_GPIO_PIN_1)
#define CFG_GPIO_QUAD_YA_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_QUAD_YA_PIN            (HW_GPIO_PIN_6)
#define CFG_GPIO_QUAD_YB_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_QUAD_YB_PIN            (HW_GPIO_PIN_7)
#define CFG_GPIO_QUAD_ZA_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_QUAD_ZA_PIN            (HW_GPIO_PIN_2)
#define CFG_GPIO_QUAD_ZB_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_QUAD_ZB_PIN            (HW_GPIO_PIN_3)
#endif

/* Timer 0 */
#if CFG_DEMO_HW_TIMER0
#if   dg_configBLACK_ORCA_MB_REV == BLACK_ORCA_MB_REV_D
#       define CFG_GPIO_TIMER0_PORT     (HW_GPIO_PORT_1)
#       define CFG_GPIO_TIMER0_PIN      (HW_GPIO_PIN_5)
#else
#       error "Unknown board!"
#endif
#endif

/* Timer 1 */
#if   dg_configBLACK_ORCA_MB_REV == BLACK_ORCA_MB_REV_D
#       define CFG_GPIO_TIMER1_PWM_PORT (HW_GPIO_PORT_1)
#       define CFG_GPIO_TIMER1_PWM_PIN  (HW_GPIO_PIN_6)
#else
#       error "Unknown board!"
#endif

/* Timer 2 */
#if CFG_DEMO_HW_TIMER2
#define CFG_GPIO_TIMER2_PWM2_PORT       (HW_GPIO_PORT_3)
#define CFG_GPIO_TIMER2_PWM2_PIN        (HW_GPIO_PIN_5)
#define CFG_GPIO_TIMER2_PWM3_PORT       (HW_GPIO_PORT_3)
#define CFG_GPIO_TIMER2_PWM3_PIN        (HW_GPIO_PIN_6)
#define CFG_GPIO_TIMER2_PWM4_PORT       (HW_GPIO_PORT_3)
#define CFG_GPIO_TIMER2_PWM4_PIN        (HW_GPIO_PIN_7)
#endif

/* UART 2 */
#if CFG_DEMO_AD_UART
#if dg_configBLACK_ORCA_MB_REV == BLACK_ORCA_MB_REV_D
#       define CFG_GPIO_UART2_TX_PORT   (HW_GPIO_PORT_4)
#       define CFG_GPIO_UART2_TX_PIN    (HW_GPIO_PIN_2)
#       define CFG_GPIO_UART2_RX_PORT   (HW_GPIO_PORT_4)
#       define CFG_GPIO_UART2_RX_PIN    (HW_GPIO_PIN_1)
#else
#       define CFG_GPIO_UART2_TX_PORT   (HW_GPIO_PORT_1)
#       define CFG_GPIO_UART2_TX_PIN    (HW_GPIO_PIN_2)
#       define CFG_GPIO_UART2_RX_PORT   (HW_GPIO_PORT_1)
#       define CFG_GPIO_UART2_RX_PIN    (HW_GPIO_PIN_3)
#endif
#endif

/* Wakeup timer */
#define CFG_GPIO_WKUP_1_PORT            (HW_GPIO_PORT_3)
#define CFG_GPIO_WKUP_1_PIN             (HW_GPIO_PIN_0)
#define CFG_GPIO_WKUP_2_PORT            (HW_GPIO_PORT_3)
#define CFG_GPIO_WKUP_2_PIN             (HW_GPIO_PIN_1)
#define CFG_GPIO_WKUP_3_PORT            (HW_GPIO_PORT_3)
#define CFG_GPIO_WKUP_3_PIN             (HW_GPIO_PIN_2)

/* I2C */

#if CFG_DEMO_HW_I2C || CFG_AD_I2C_1
/* I2C1 */
#define CFG_GPIO_I2C1_SCL_PORT          (HW_GPIO_PORT_3)
#define CFG_GPIO_I2C1_SCL_PIN           (HW_GPIO_PIN_5)
#define CFG_GPIO_I2C1_SDA_PORT          (HW_GPIO_PORT_1)
#define CFG_GPIO_I2C1_SDA_PIN           (HW_GPIO_PIN_2)
#endif

#if CFG_AD_SPI_1 && !dg_configUSE_FPGA
/* SPI1 */
#define CFG_GPIO_SPI1_CLK_PORT          (HW_GPIO_PORT_4)
#define CFG_GPIO_SPI1_CLK_PIN           (HW_GPIO_PIN_1)
#define CFG_GPIO_SPI1_DI_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_SPI1_DI_PIN            (HW_GPIO_PIN_0)
#define CFG_GPIO_SPI1_DO_PORT           (HW_GPIO_PORT_3)
#define CFG_GPIO_SPI1_DO_PIN            (HW_GPIO_PIN_7)
#ifdef CONFIG_AT45DB011D
#define CFG_GPIO_SPI1_CS_PORT           (HW_GPIO_PORT_4)
#define CFG_GPIO_SPI1_CS_PIN            (HW_GPIO_PIN_2)
#endif
#endif

#if CFG_DEMO_SENSOR_ADXL362
/* sensors board has only 1 device which is using SPI bus  */
#define CFG_GPIO_ADXL362_CS_PORT        (HW_GPIO_PORT_3)
#define CFG_GPIO_ADXL362_CS_PIN         (HW_GPIO_PIN_6)
#endif

/* SPI1 */
