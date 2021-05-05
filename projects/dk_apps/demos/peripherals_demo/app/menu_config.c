/**
 ****************************************************************************************
 *
 * @file menu_config.c
 *
 * @brief Demo application configuration
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <hw_gpadc.h>
#include "config.h"
#include "menu.h"
#include "tasklist.h"
#include "demos/common.h"

MENU_BEGIN(system_clock)
MENU_ITEM_CHKP("Set system clock to RC   16 MHz",       system_clock,                  0)
#if (dg_configEXT_CRYSTAL_FREQ == EXT_CRYSTAL_IS_16M)
MENU_ITEM_CHKP("Set system clock to XTAL 16 MHz",       system_clock,                  1)
#elif (dg_configEXT_CRYSTAL_FREQ == EXT_CRYSTAL_IS_32M)
MENU_ITEM_CHKP("Set system clock to XTAL 32 MHz",       system_clock,                  2)
#endif
MENU_ITEM_CHKP("Set system clock to PLL  48 MHz",       system_clock,                  3)
MENU_ITEM_CHKP("Set system clock to PLL  96 MHz",       system_clock,                  6)
MENU_END()

MENU_BEGIN(system_settings)
MENU_ITEM_SUBM("Clock",                                 system_clock)
MENU_END()

#if CFG_DEMO_HW_IRGEN
MENU_BEGIN(hw_irgen)
MENU_ITEM_PARM("send NEC protocol command (4 shots)",   irgen_send_nec,                 4)
MENU_ITEM_PARM("send NEC protocol command (repeat)",    irgen_send_nec,                 -1)
MENU_ITEM_VOID("stop sending command",                  irgen_stop)
MENU_END()
#endif // CFG_DEMO_HW_IRGEN

#if CFG_DEMO_HW_BREATH
MENU_BEGIN(hw_breath)
MENU_ITEM_VOID("Constant dim led",                      breath_constant_dim)
MENU_ITEM_VOID("Constant bright led",                   breath_constant_bright)
MENU_ITEM_VOID("Emergency led",                         breath_emergency_blink)
MENU_ITEM_VOID("Dim standby breath",                    breath_dim_standby_breath)
MENU_ITEM_VOID("Slow standby breath",                   breath_standby_breath)
MENU_ITEM_VOID("Disable breath",                        breath_stop)
MENU_END()
#endif // CFG_DEMO_HW_BREATH

#if CFG_DEMO_HW_TIMER0
MENU_BEGIN(hw_timer0)
MENU_ITEM_PARM("Blink LED 4 times (error report)",      timer0_blink_led,               4)
MENU_ITEM_PARM("Blink LED 10 times (error report)",     timer0_blink_led,               10)
MENU_ITEM_PARM("Blink LED 7 times lower intensity",     timer0_blink_led_dim,           7)
MENU_ITEM_PARM("Set bright LED with PWM 90%",           timer0_light_led,               90)
MENU_ITEM_PARM("Set dim LED with PWM 20%",              timer0_light_led,               20)
MENU_ITEM_VOID("Low power indicator (short blinks)",    timer0_slow_blink)
MENU_ITEM_VOID("Turn off LED",                          timer0_turn_off)
MENU_END()
#endif // CFG_DEMO_HW_TIMER0

#if CFG_DEMO_HW_QSPI
MENU_BEGIN(hw_qspi)
MENU_ITEM_VOID("Set fastest quad mode",                 qspi_set_quad)
MENU_ITEM_VOID("Set slowest single mode",               qspi_set_single)
MENU_ITEM_VOID("Test performance",                      qspi_test_performance)
MENU_ITEM_CHKP("QSPI divider 1",                        qspi_div,       0)
MENU_ITEM_CHKP("QSPI divider 2",                        qspi_div,       1)
MENU_ITEM_CHKP("QSPI divider 4",                        qspi_div,       2)
MENU_ITEM_CHKP("QSPI divider 8",                        qspi_div,       3)
MENU_END()
#endif // CFG_DEMO_HW_QSPI

#if CFG_DEMO_HW_TIMER1
MENU_BEGIN(hw_timer1_pwm)
MENU_ITEM_PARM("PWM frequency = 64 Hz",                 timer1_set_pwm_freq,            127)
MENU_ITEM_PARM("PWM frequency = 128 Hz",                timer1_set_pwm_freq,            63)
MENU_ITEM_PARM("PWM frequency = 256 Hz",                timer1_set_pwm_freq,            31)
MENU_ITEM_PARM("PWM duty cycle 25 %",                   timer1_set_pwm_dc,              1)
MENU_ITEM_PARM("PWM duty cycle 50 %",                   timer1_set_pwm_dc,              2)
MENU_ITEM_PARM("PWM duty cycle 75 %",                   timer1_set_pwm_dc,              3)
MENU_END()

MENU_BEGIN(hw_timer1)
MENU_ITEM_SUBM("PWM settings",                          hw_timer1_pwm)
MENU_END()
#endif // CFG_DEMO_HW_TIMER1

#if CFG_DEMO_HW_TIMER2
MENU_BEGIN(hw_timer2_rgb_led)
MENU_ITEM_PARM("Grey",                                  timer2_pwm_light_rgb,           0xA9A9A9)
MENU_ITEM_PARM("Orange",                                timer2_pwm_light_rgb,           0xFFA500)
MENU_ITEM_PARM("Violet",                                timer2_pwm_light_rgb,           0x7B68EE)
MENU_ITEM_PARM("Yellow",                                timer2_pwm_light_rgb,           0xFFFF00)
MENU_ITEM_PARM("Magenta",                               timer2_pwm_light_rgb,           0xFF00FF)
MENU_ITEM_PARM("Cyan",                                  timer2_pwm_light_rgb,           0x00FFFF)
MENU_END()

MENU_BEGIN(hw_timer2_pwm_freq)
MENU_ITEM_PARM("PWM frequency = 20 kHz",                timer2_pwm_freq,                200)
MENU_ITEM_PARM("PWM frequency = 4 kHz",                 timer2_pwm_freq,                1000)
MENU_ITEM_PARM("PWM frequency = 250 Hz",                timer2_pwm_freq,                16000)
MENU_END()

MENU_BEGIN(hw_timer2)
MENU_ITEM_SUBM("Set PWMs' frequency",                   hw_timer2_pwm_freq)
MENU_ITEM_VOID("Set start/end PWMs' duty cycles",       timer2_pwm_dc)
MENU_ITEM_SUBM("Light RGB LED",                         hw_timer2_rgb_led)
MENU_ITEM_VOID("PWMs' state",                           timer2_pwm_state)
MENU_ITEM_CHKV("Pause timer",                           timer2_pause)
MENU_END()
#endif // CFG_DEMO_HW_TIMER2

#if CFG_DEMO_HW_QUAD
MENU_BEGIN(hw_quad_echannel)
MENU_ITEM_PARM("Enable X channel",                      quad_channel_ctrl,              QUAD_CHANNEL_X_ON)
MENU_ITEM_PARM("Disable X channel",                     quad_channel_ctrl,              QUAD_CHANNEL_X_OFF)
MENU_ITEM_PARM("Enable Y channel",                      quad_channel_ctrl,              QUAD_CHANNEL_Y_ON)
MENU_ITEM_PARM("Disable Y channel",                     quad_channel_ctrl,              QUAD_CHANNEL_Y_OFF)
MENU_ITEM_PARM("Enable Z channel",                      quad_channel_ctrl,              QUAD_CHANNEL_Z_ON)
MENU_ITEM_PARM("Disable Z channel",                     quad_channel_ctrl,              QUAD_CHANNEL_Z_OFF)
MENU_ITEM_PARM("Enable XYZ channels",                   quad_channel_ctrl,              QUAD_CHANNEL_XYZ_ON)
MENU_ITEM_PARM("Disable XYZ channels",                  quad_channel_ctrl,              QUAD_CHANNEL_XYZ_OFF)
MENU_END()

MENU_BEGIN(hw_quad_threshold)
MENU_ITEM_PARM("Set threshold to 1 event",              quad_set_threshold,             1)
MENU_ITEM_PARM("Set threshold to 64 events",            quad_set_threshold,             64)
MENU_ITEM_PARM("Set threshold to 127 events",           quad_set_threshold,             127)
MENU_END()

MENU_BEGIN(hw_quad)
MENU_ITEM_SUBM("Enable/disable channel",                hw_quad_echannel)
MENU_ITEM_SUBM("Set threshold",                         hw_quad_threshold)
MENU_ITEM_VOID("Get channels state",                    quad_channels_state)
MENU_END()
#endif // CFG_DEMO_HW_QUAD

#if CFG_DEMO_HW_WKUP
MENU_BEGIN(hw_wkup_pin_configure)
MENU_ITEM_CHKP("GPIO#1 enabled",                        wkup_pin_enabled,               1)
MENU_ITEM_CHKP("GPIO#2 enabled",                        wkup_pin_enabled,               2)
MENU_ITEM_CHKP("GPIO#3 enabled",                        wkup_pin_enabled,               3)
MENU_ITEM_CHKP("GPIO#1 trigger on high state (low when unchecked)", wkup_pin_trigger,   1)
MENU_ITEM_CHKP("GPIO#2 trigger on high state (low when unchecked)", wkup_pin_trigger,   2)
MENU_ITEM_CHKP("GPIO#3 trigger on high state (low when unchecked)", wkup_pin_trigger,   3)
MENU_ITEM_VOID("disable all GPIO pins",                 wkup_disable_all)
MENU_END()

MENU_BEGIN(hw_wkup_configure)
#if (dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A)
MENU_ITEM_PARM("set threshold to 1 event",              wkup_threshold,                 1)
MENU_ITEM_PARM("set threshold to 5 events",             wkup_threshold,                 5)
MENU_ITEM_PARM("set threshold to 15 events",            wkup_threshold,                 15)
#endif
MENU_ITEM_PARM("disable debounce",                      wkup_debounce,                  0)
MENU_ITEM_PARM("set debounce to 10ms",                  wkup_debounce,                  10)
MENU_ITEM_PARM("set debounce to 50ms",                  wkup_debounce,                  50)
MENU_END()

MENU_BEGIN(hw_wkup)
MENU_ITEM_SUBM("input configuration",                   hw_wkup_pin_configure)
MENU_ITEM_SUBM("timer configuration",                   hw_wkup_configure)
#if (dg_configBLACK_ORCA_IC_REV == BLACK_ORCA_IC_REV_A)
MENU_ITEM_VOID("reset timer counter",                   wkup_reset)
#endif
MENU_ITEM_VOID("emulate key hit",                       wkup_keyhit)
MENU_ITEM_VOID("get timer state",                       wkup_state)
MENU_END()
#endif // CFG_DEMO_HW_WKUP

#if CFG_DEMO_HW_I2C
MENU_BEGIN(hw_i2c_fm75)
MENU_ITEM_PARM("9-bit resolution",                      i2c_fm75_set_resolution,        0)
MENU_ITEM_PARM("10-bit resolution",                     i2c_fm75_set_resolution,        1)
MENU_ITEM_PARM("11-bit resolution",                     i2c_fm75_set_resolution,        2)
MENU_ITEM_PARM("12-bit resolution",                     i2c_fm75_set_resolution,        3)
MENU_ITEM_PARM("Alarm out of 10.0 - 15.0 C",            i2c_fm75_set_alarm_limits,      ALARM_RANGE(10, 15))
MENU_ITEM_PARM("Alarm out of 20.0 - 25.0 C",            i2c_fm75_set_alarm_limits,      ALARM_RANGE(20, 25))
MENU_ITEM_PARM("Alarm out of 26.0 - 32.0 C",            i2c_fm75_set_alarm_limits,      ALARM_RANGE(26, 32))
MENU_ITEM_PARM("Default alarm (75.0 - 80.0 C)",         i2c_fm75_set_alarm_limits,      ALARM_RANGE(75, 80))
MENU_END()

MENU_BEGIN(hw_i2c)
MENU_ITEM_VOID("Write data to EEPROM",                  i2c_write_to_eeprom)
MENU_ITEM_VOID("Read data from EEPROM",                 i2c_read_from_eeprom)
MENU_ITEM_SUBM("Set FM75 configuration",                hw_i2c_fm75)
MENU_ITEM_CHKV("Enable temperature sensor",             i2c_get_temp_start)
MENU_END()
#endif // CFG_DEMO_HW_I2C

#if CFG_DEMO_HW_I2C_ASYNC
MENU_BEGIN(hw_i2c_async_fm75)
MENU_ITEM_PARM("9-bit resolution",                      i2c_async_fm75_set_resolution,        0)
MENU_ITEM_PARM("10-bit resolution",                     i2c_async_fm75_set_resolution,        1)
MENU_ITEM_PARM("11-bit resolution",                     i2c_async_fm75_set_resolution,        2)
MENU_ITEM_PARM("12-bit resolution",                     i2c_async_fm75_set_resolution,        3)
MENU_ITEM_PARM("Alarm out of 10.0 - 15.0 C",            i2c_async_fm75_set_alarm_limits,      ALARM_RANGE(10, 15))
MENU_ITEM_PARM("Alarm out of 20.0 - 25.0 C",            i2c_async_fm75_set_alarm_limits,      ALARM_RANGE(20, 25))
MENU_ITEM_PARM("Alarm out of 26.0 - 32.0 C",            i2c_async_fm75_set_alarm_limits,      ALARM_RANGE(26, 32))
MENU_ITEM_PARM("Default alarm (75.0 - 80.0 C)",         i2c_async_fm75_set_alarm_limits,      ALARM_RANGE(75, 80))
MENU_END()

MENU_BEGIN(hw_i2c_async)
MENU_ITEM_VOID("Write data to EEPROM",                  i2c_async_write_to_eeprom)
MENU_ITEM_VOID("Read data from EEPROM",                 i2c_async_read_from_eeprom)
MENU_ITEM_SUBM("Set FM75 configuration",                hw_i2c_async_fm75)
MENU_ITEM_CHKV("Enable temperature sensor",             i2c_async_get_temp_start)
MENU_END()
#endif // CFG_DEMO_HW_I2C_ASYNC

#if CFG_DEMO_HW_GPADC
MENU_BEGIN(hw_gpadc_input)
//MENU_ITEM_CHKP("GPIO 0.6",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P06)
MENU_ITEM_CHKP("GPIO 0.7",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P07)
//MENU_ITEM_CHKP("GPIO 1.0",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P10)
MENU_ITEM_CHKP("GPIO 1.2",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P12)
#if dg_configBLACK_ORCA_MB_REV != BLACK_ORCA_MB_REV_D
MENU_ITEM_CHKP("GPIO 1.3",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P13)
#endif
MENU_ITEM_CHKP("GPIO 1.4",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P14)
//MENU_ITEM_CHKP("GPIO 1.5",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P15)
//MENU_ITEM_CHKP("GPIO 2.4",                              gpadc_input_se,                 HW_GPADC_INPUT_SE_P24)
MENU_ITEM_CHKP("GPIO 1.2 vs 1.4 (differential)",        gpadc_input_diff,               HW_GPADC_INPUT_DIFF_P12_P14)
#if dg_configBLACK_ORCA_MB_REV != BLACK_ORCA_MB_REV_D
MENU_ITEM_CHKP("GPIO 1.3 vs 0.7 (differential)",        gpadc_input_diff,               HW_GPADC_INPUT_DIFF_P13_P07)
#endif
MENU_ITEM_CHKP("analog ground level",                   gpadc_input_se,                 HW_GPADC_INPUT_SE_AVS)
MENU_ITEM_CHKP("VDD",                                   gpadc_input_se,                 HW_GPADC_INPUT_SE_VDD)
MENU_ITEM_CHKP("V33",                                   gpadc_input_se,                 HW_GPADC_INPUT_SE_V33)
MENU_ITEM_CHKP("VBAT",                                  gpadc_input_se,                 HW_GPADC_INPUT_SE_VBAT)
MENU_END()

MENU_BEGIN(hw_gpadc_config)
MENU_ITEM_CHKV("use digital clock",                     gpadc_cfg_digiclk)
MENU_ITEM_CHKV("enable input attenuator",               gpadc_cfg_attenuator)
MENU_ITEM_CHKV("enable chopping",                       gpadc_cfg_chopping)
MENU_ITEM_CHKV("enable sign change",                    gpadc_cfg_sign)
MENU_ITEM_CHKP("1 clock cycle sample time",             gpadc_cfg_sample,               0)
MENU_ITEM_CHKP("32 clock cycles sample time",           gpadc_cfg_sample,               1)
MENU_ITEM_CHKP("480 clock cycles sample time",          gpadc_cfg_sample,               15)
MENU_ITEM_CHKP("no oversampling",                       gpadc_cfg_oversampling,         0)
MENU_ITEM_CHKP("oversampling, 2 conversions",           gpadc_cfg_oversampling,         1)
MENU_ITEM_CHKP("oversampling, 128 conversions",         gpadc_cfg_oversampling,         7)
MENU_ITEM_CHKP("0.000ms interval (for cont. mode)",     gpadc_cfg_interval,             0)
MENU_ITEM_CHKP("1.024ms interval (for cont. mode)",     gpadc_cfg_interval,             1)
MENU_ITEM_CHKP("261.120ms interval (for cont. mode)",   gpadc_cfg_interval,             255)
MENU_END()

MENU_BEGIN(hw_gpadc)
MENU_ITEM_SUBM("Configure",                             hw_gpadc_config)
MENU_ITEM_SUBM("Select input",                          hw_gpadc_input)
MENU_ITEM_VOID("Measure",                               gpadc_measure)
MENU_ITEM_CHKV("Continuous mode",                       gpadc_continuous)
MENU_ITEM_VOID("Show state",                            gpadc_state)
MENU_END()
#endif // CFG_DEMO_HW_GPADC

#if CFG_DEMO_AD_GPADC
MENU_BEGIN(ad_gpadc)
MENU_ITEM_PARM("Read battery level",                    gpadc_read,                     GPADC_BATTERY_LEVEL_ID)
MENU_ITEM_PARM("Read temperature sensor",               gpadc_read,                     GPADC_TEMP_SENSOR_ID)
MENU_ITEM_PARM("Read light sensor",                     gpadc_read,                     GPADC_LIGHT_SENSOR_ID)
MENU_ITEM_PARM("Read encoder sensor",                   gpadc_read,                     GPADC_ENCODER_SENSOR_ID)
MENU_ITEM_CHKV("Read light sensor cyclically",          gpadc_read_cyclic)
MENU_END()
#endif // CFG_DEMO_AD_GPADC

#if CFG_DEMO_AD_UART
MENU_BEGIN(ad_uart)
MENU_ITEM_CHKP("UART2 user 1",                          ad_uart_worker_enable,          0)
MENU_ITEM_CHKP("UART2 user 2",                          ad_uart_worker_enable,          1)
MENU_ITEM_CHKP("Lock UART2 for user 1",                 ad_uart_worker_lock,            0)
MENU_ITEM_CHKP("Lock UART2 for user 2",                 ad_uart_worker_lock,            1)
MENU_ITEM_PARM("Prompt from user 1",                    ad_uart_prompt,                 0)
MENU_ITEM_PARM("Prompt from user 2",                    ad_uart_prompt,                 1)
MENU_END()
#endif // CFG_DEMO_AD_UART

#if CFG_DEMO_AD_SPI
MENU_BEGIN(ad_spi)
MENU_ITEM_VOID("Write time to AT45DB011D",              ad_spi_at45_write)
MENU_ITEM_VOID("Erase log area in AT45DB011D",          ad_spi_at45_erase)
MENU_ITEM_VOID("Print log from AT45DB011D",             ad_spi_at45_print_log)
MENU_END()
#endif // CFG_DEMO_AD_SPI

#if CFG_DEMO_AD_SPI_I2C
MENU_BEGIN(ad_i2c_spi)
MENU_ITEM_VOID("Copy from AT45DB011D to 24xx256",       ad_spi_i2c_copy_sync)
MENU_ITEM_VOID("Copy from AT45DB011D to 24xx256 async", ad_spi_i2c_copy_async)
MENU_ITEM_VOID("Copy from 24xx256 to AT45DB011D",       ad_i2c_spi_copy_sync)
MENU_ITEM_VOID("Copy from 24xx256 to AT45DB011D async", ad_i2c_spi_copy_async)
MENU_END()
#endif // CFG_DEMO_AD_SPI

#if CFG_DEMO_SENSOR_BOARD
MENU_BEGIN(sensor_board)
#if CFG_DEMO_SENSOR_BH1750
MENU_ITEM_VOID("Read Light Sensor (BH1750)",            sensor_board_bh1750_read)
#endif
#if CFG_DEMO_SENSOR_BME280
MENU_ITEM_VOID("Read Environ Sensor (BME280)",          sensor_board_bme280_read)
#endif
#if CFG_DEMO_SENSOR_ADXL362
MENU_ITEM_VOID("Read Accelerometer (ADXL362)",          sensor_board_adxl362_read)
#endif
#if CFG_DEMO_SENSOR_BMM150
MENU_ITEM_VOID("Read Geomagnetic Sensor (BMM150)",      sensor_board_bmm150_read)
#endif
#if CFG_DEMO_SENSOR_BMG160
MENU_ITEM_VOID("Read Gyroscope Sensor (BMG160)",        sensor_board_bmg160_read)
#endif
MENU_ITEM_CHKP("Read sensors data every 5 seconds",     sensor_board_cyclic_read,               0)
MENU_END()
#endif

#if CFG_DEMO_AD_TEMPSENS
MENU_BEGIN(temp_sensor)
MENU_ITEM_VOID("Read Temperature Sensor",               temperature_sensor_read)
MENU_ITEM_VOID("Read Temperature Sensor Async",         temperature_sensor_read_async)
MENU_ITEM_CHKP("Read Temperature every 5 seconds",      temperature_sensor_read_cyclic,          0)
MENU_END()
#endif // CFG_DEMO_AD_TEMPSENS

#if CFG_DEMO_POWER_MODE
MENU_BEGIN(power_mode)
MENU_ITEM_VOID("Go to Active Mode",                     power_mode_active)
MENU_ITEM_VOID("Go to Extended Sleep Mode",             power_mode_extended_sleep)
MENU_ITEM_VOID("Go to Hibernation Mode",                power_mode_hibernation)
MENU_END()
#endif // CFG_DEMO_POWER_MODE

MENU_BEGIN(root)
#if CFG_DEMO_HW_IRGEN
MENU_ITEM_SUBM("IR generator",          hw_irgen)
#endif
#if CFG_DEMO_HW_BREATH
MENU_ITEM_SUBM("Breath timer",          hw_breath)
#endif
#if CFG_DEMO_HW_TIMER0
MENU_ITEM_SUBM("Timer 0",               hw_timer0)
#endif
#if CFG_DEMO_HW_TIMER1
MENU_ITEM_SUBM("Timer 1",               hw_timer1)
#endif
#if CFG_DEMO_HW_TIMER2
MENU_ITEM_SUBM("Timer 2",               hw_timer2)
#endif
#if CFG_DEMO_HW_QUAD
MENU_ITEM_SUBM("Quadrature decoder",    hw_quad)
#endif
#if CFG_DEMO_HW_I2C
MENU_ITEM_SUBM("I2C",                   hw_i2c)
#endif
#if CFG_DEMO_HW_I2C_ASYNC
MENU_ITEM_SUBM("I2C_Async",             hw_i2c_async)
#endif
#if CFG_DEMO_HW_QSPI
MENU_ITEM_SUBM("QSPI (for RAM builds only)", hw_qspi)
#endif
#if CFG_DEMO_HW_WKUP
MENU_ITEM_SUBM("Wakeup timer",          hw_wkup)
#endif
#if CFG_DEMO_HW_GPADC
MENU_ITEM_SUBM("ADC",                   hw_gpadc)
#endif
#if CFG_DEMO_AD_GPADC
MENU_ITEM_SUBM("ADC adapter",           ad_gpadc)
#endif
#if CFG_DEMO_AD_UART
MENU_ITEM_SUBM("Sharing UART2",         ad_uart)
#endif
#if CFG_DEMO_AD_SPI
MENU_ITEM_SUBM("Flash on SPI",          ad_spi)
#endif
#if CFG_DEMO_AD_SPI_I2C
MENU_ITEM_SUBM("Async SPI and I2C",     ad_i2c_spi)
#endif
#if CFG_DEMO_SENSOR_BOARD
MENU_ITEM_SUBM("Sensor Board (requires the add-on sensor board)", sensor_board)
#endif
#if CFG_DEMO_AD_TEMPSENS
MENU_ITEM_SUBM("Temperature Sensor",    temp_sensor)
#endif
#if CFG_DEMO_POWER_MODE
MENU_ITEM_SUBM("Power mode",            power_mode)
#endif
MENU_ITEM_SUBM("Settings",              system_settings)
MENU_END()

TASKLIST_BEGIN()
TASKLIST_ENTRY(uart_print, 1, 256, task_uart_printf_init_func, task_uart_printf_func, NULL)
#if CFG_DEMO_HW_I2C
TASKLIST_ENTRY(i2c_get_temp, 1, 800, task_i2c_get_temp_init, task_i2c_get_temp_func, NULL)
#endif
#if CFG_DEMO_HW_I2C_ASYNC
TASKLIST_ENTRY(i2c_async_get_temp, 1, 750, task_i2c_async_get_temp_init, task_i2c_async_get_temp_func, NULL)
#endif
#if CFG_DEMO_AD_UART
TASKLIST_ENTRY(uart_writer1, 1, 300, task_ad_uart_worker_init, task_ad_uart_worker_func, (void *) 0)
TASKLIST_ENTRY(uart_writer2, 1, 300, task_ad_uart_worker_init, task_ad_uart_worker_func, (void *) 1)
#endif
#if CFG_DEMO_AD_SPI
TASKLIST_ENTRY(at45_user, 1, 450, NULL, task_ad_spi_at45_worker_func, NULL)
#endif
#if CFG_DEMO_AD_GPADC
TASKLIST_ENTRY(gpadc_user, 1, 578, NULL, task_ad_gpadc_worker_func, NULL)
#endif
#if CFG_DEMO_AD_TEMPSENS
TASKLIST_ENTRY(tempsens_user, 1, 578, NULL, task_temperature_sensor_worker_func, NULL)
#endif
#if CFG_DEMO_SENSOR_BOARD
TASKLIST_ENTRY(sensor_board_user, 1, 700, NULL, task_sensor_board_worker_func, NULL)
#endif
TASKLIST_END()
