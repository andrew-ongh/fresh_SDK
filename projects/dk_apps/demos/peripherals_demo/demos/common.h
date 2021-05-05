/**
 ****************************************************************************************
 *
 * @file common.h
 *
 * @brief Common definitions for demo application
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <stdbool.h>
#include "app/menu.h"
#include "app/tasklist.h"

/**
 * \brief Setup peripherals used in demo application
 *
 */
void periph_setup(void);

#define uart_printfln_s(fmt, ...) uart_printfln(false, fmt, __VA_ARGS__)
#define uart_printfln_a(fmt, ...) uart_printfln(true, fmt, __VA_ARGS__)
void uart_printfln(bool async, const char *fmt, ...);
void task_uart_printf_init_func(const struct task_item *t);
void task_uart_printf_func(const struct task_item *t);

void menu_system_clock_func(const struct menu_item *m, bool checked);
bool menu_system_clock_checked_cb_func(const struct menu_item *m);

void menu_irgen_send_nec_func(const struct menu_item *m, bool checked);
void menu_irgen_stop_func(const struct menu_item *m, bool checked);

void menu_breath_constant_dim_func(const struct menu_item *m, bool checked);
void menu_breath_constant_bright_func(const struct menu_item *m, bool checked);
void menu_breath_emergency_blink_func(const struct menu_item *m, bool checked);
void menu_breath_dim_standby_breath_func(const struct menu_item *m, bool checked);
void menu_breath_standby_breath_func(const struct menu_item *m, bool checked);
void menu_breath_stop_func(const struct menu_item *m, bool checked);

void menu_timer0_blink_led_func(const struct menu_item *m, bool checked);
void menu_timer0_blink_led_dim_func(const struct menu_item *m, bool checked);
void menu_timer0_light_led_func(const struct menu_item *m, bool checked);
void menu_timer0_slow_blink_func(const struct menu_item *m, bool checked);
void menu_timer0_turn_off_func(const struct menu_item *m, bool checked);

void menu_qspi_set_quad_func(const struct menu_item *m, bool checked);
void menu_qspi_set_single_func(const struct menu_item *m, bool checked);
void menu_qspi_test_performance_func(const struct menu_item *m, bool checked);
void menu_qspi_div_func(const struct menu_item *m, bool checked);
bool menu_qspi_div_checked_cb_func(const struct menu_item *m);

void menu_timer1_set_pwm_freq_func(const struct menu_item *m, bool checked);
void menu_timer1_set_pwm_dc_func(const struct menu_item *m, bool checked);

void demo_timer2_init(void);
void menu_timer2_pwm_freq_func(const struct menu_item *m, bool checked);
void menu_timer2_pwm_dc_func(const struct menu_item *m, bool checked);
void menu_timer2_pwm_light_rgb_func(const struct menu_item *m, bool checked);
void menu_timer2_pwm_state_func(const struct menu_item *m, bool checked);
void menu_timer2_pause_func(const struct menu_item *m, bool checked);
bool menu_timer2_pause_checked_cb_func(const struct menu_item *m);

#define QUAD_CHANNEL_X_ON     10
#define QUAD_CHANNEL_X_OFF    11
#define QUAD_CHANNEL_Y_ON     20
#define QUAD_CHANNEL_Y_OFF    21
#define QUAD_CHANNEL_Z_ON     30
#define QUAD_CHANNEL_Z_OFF    31
#define QUAD_CHANNEL_XYZ_ON   40
#define QUAD_CHANNEL_XYZ_OFF  41
void demo_quad_init(void);
void menu_quad_channel_ctrl_func(const struct menu_item *m, bool checked);
void menu_quad_set_threshold_func(const struct menu_item *m, bool checked);
void menu_quad_channels_state_func(const struct menu_item *m, bool checked);

/* hw_wkup demo definitions */
void demo_wkup_init(void);
void menu_wkup_pin_enabled_func(const struct menu_item *m, bool checked);
bool menu_wkup_pin_enabled_checked_cb_func(const struct menu_item *m);
void menu_wkup_pin_trigger_func(const struct menu_item *m, bool checked);
bool menu_wkup_pin_trigger_checked_cb_func(const struct menu_item *m);
void menu_wkup_disable_all_func(const struct menu_item *m, bool checked);
void menu_wkup_threshold_func(const struct menu_item *m, bool checked);
void menu_wkup_debounce_func(const struct menu_item *m, bool checked);
void menu_wkup_reset_func(const struct menu_item *m, bool checked);
void menu_wkup_keyhit_func(const struct menu_item *m, bool checked);
void menu_wkup_state_func(const struct menu_item *m, bool checked);

/* hw_gpadc definitions */
void demo_gpadc_init(void);
void menu_gpadc_input_se_func(const struct menu_item *m, bool checked);
bool menu_gpadc_input_se_checked_cb_func(const struct menu_item *m);
void menu_gpadc_input_diff_func(const struct menu_item *m, bool checked);
bool menu_gpadc_input_diff_checked_cb_func(const struct menu_item *m);
void menu_gpadc_cfg_digiclk_func(const struct menu_item *m, bool checked);
bool menu_gpadc_cfg_digiclk_checked_cb_func(const struct menu_item *m);
void menu_gpadc_cfg_attenuator_func(const struct menu_item *m, bool checked);
bool menu_gpadc_cfg_attenuator_checked_cb_func(const struct menu_item *m);
void menu_gpadc_cfg_chopping_func(const struct menu_item *m, bool checked);
bool menu_gpadc_cfg_chopping_checked_cb_func(const struct menu_item *m);
void menu_gpadc_cfg_sign_func(const struct menu_item *m, bool checked);
bool menu_gpadc_cfg_sign_checked_cb_func(const struct menu_item *m);
void menu_gpadc_cfg_sample_func(const struct menu_item *m, bool checked);
bool menu_gpadc_cfg_sample_checked_cb_func(const struct menu_item *m);
void menu_gpadc_cfg_oversampling_func(const struct menu_item *m, bool checked);
bool menu_gpadc_cfg_oversampling_checked_cb_func(const struct menu_item *m);
void menu_gpadc_cfg_interval_func(const struct menu_item *m, bool checked);
bool menu_gpadc_cfg_interval_checked_cb_func(const struct menu_item *m);
void menu_gpadc_calibrate_func(const struct menu_item *m, bool checked);
void menu_gpadc_measure_func(const struct menu_item *m, bool checked);
void menu_gpadc_continuous_func(const struct menu_item *m, bool checked);
bool menu_gpadc_continuous_checked_cb_func(const struct menu_item *m);
void menu_gpadc_state_func(const struct menu_item *m, bool checked);

/* ad_gpadc demo definitions */
#define GPADC_BATTERY_LEVEL_ID          1
#define GPADC_TEMP_SENSOR_ID            2
#define GPADC_LIGHT_SENSOR_ID           3
#define GPADC_ENCODER_SENSOR_ID         4
void demo_ad_gpadc_init(void);
void task_ad_gpadc_worker_func(const struct task_item *task);
void menu_gpadc_read_func(const struct menu_item *m, bool checked);
void menu_gpadc_state_func(const struct menu_item *m, bool checked);
void menu_gpadc_read_cyclic_func(const struct menu_item *m, bool checked);
bool menu_gpadc_read_cyclic_checked_cb_func(const struct menu_item *m);

#define ALARM_RANGE(low, high)  ((int8_t) high << 8 | (int8_t) low)
#define ALARM_GET_LOW(range)    ((int8_t) (range & 0xFF))
#define ALARM_GET_HIGH(range)   ((int8_t) ((range >> 8) & 0xFF))
void demo_i2c_init(void);
void task_i2c_get_temp_init(const struct task_item *task);
void task_i2c_get_temp_func(const struct task_item *task);
void menu_i2c_get_temp_start_func(const struct menu_item *m, bool checked);
bool menu_i2c_get_temp_start_checked_cb_func(const struct menu_item *m);
void menu_i2c_write_to_eeprom_func(const struct menu_item *m, bool checked);
void menu_i2c_read_from_eeprom_func(const struct menu_item *m, bool checked);
void menu_i2c_fm75_set_resolution_func(const struct menu_item *m, bool checked);
void menu_i2c_fm75_set_alarm_limits_func(const struct menu_item *m, bool checked);

void demo_i2c_async_init(void);
void task_i2c_async_get_temp_init(const struct task_item *task);
void task_i2c_async_get_temp_func(const struct task_item *task);
void menu_i2c_async_get_temp_start_func(const struct menu_item *m, bool checked);
bool menu_i2c_async_get_temp_start_checked_cb_func(const struct menu_item *m);
void menu_i2c_async_write_to_eeprom_func(const struct menu_item *m, bool checked);
void menu_i2c_async_read_from_eeprom_func(const struct menu_item *m, bool checked);
void menu_i2c_async_fm75_set_resolution_func(const struct menu_item *m, bool checked);
void menu_i2c_async_fm75_set_alarm_limits_func(const struct menu_item *m, bool checked);

/* ad_uart */
void task_ad_uart_worker_init(const struct task_item *task);
void task_ad_uart_worker_func(const struct task_item *task);
void menu_ad_uart_worker_enable_func(const struct menu_item *m, bool checked);
bool menu_ad_uart_worker_enable_checked_cb_func(const struct menu_item *m);
void menu_ad_uart_prompt_func(const struct menu_item *m, bool checked);
void menu_ad_uart_worker_lock_func(const struct menu_item *m, bool checked);
bool menu_ad_uart_worker_lock_checked_cb_func(const struct menu_item *m);

void task_ad_spi_at45_worker_func(const struct task_item *task);
void menu_ad_spi_at45_write_func(const struct menu_item *m, bool checked);
void menu_ad_spi_at45_erase_func(const struct menu_item *m, bool checked);
void menu_ad_spi_at45_print_log_func(const struct menu_item *m, bool checked);

void menu_ad_i2c_spi_copy_sync_func(const struct menu_item *m, bool checked);
void menu_ad_i2c_spi_copy_async_func(const struct menu_item *m, bool checked);
void menu_ad_spi_i2c_copy_sync_func(const struct menu_item *m, bool checked);
void menu_ad_spi_i2c_copy_async_func(const struct menu_item *m, bool checked);

/* sensor board demo */
void menu_sensor_board_bh1750_read_func(const struct menu_item *m, bool checked);
void menu_sensor_board_bme280_read_func(const struct menu_item *m, bool checked);
void menu_sensor_board_adxl362_read_func(const struct menu_item *m, bool checked);
void menu_sensor_board_bmm150_read_func(const struct menu_item *m, bool checked);
void menu_sensor_board_bmg160_read_func(const struct menu_item *m, bool checked);
void menu_sensor_board_cyclic_read_func(const struct menu_item *m, bool checked);
bool menu_sensor_board_cyclic_read_checked_cb_func(const struct menu_item *m);
void task_sensor_board_worker_func(const struct task_item *task);

/* temperature sensor demo */
void menu_temperature_sensor_read_func(const struct menu_item *m, bool checked);
void menu_temperature_sensor_read_async_func(const struct menu_item *m, bool checked);
void menu_temperature_sensor_read_cyclic_func(const struct menu_item *m, bool checked);
bool menu_temperature_sensor_read_cyclic_checked_cb_func(const struct menu_item *m);
void task_temperature_sensor_worker_func(const struct task_item *task);

/* power mode demo */
void demo_power_mode_init(void);
void menu_power_mode_active_func(const struct menu_item *m, bool checked);
void menu_power_mode_extended_sleep_func(const struct menu_item *m, bool checked);
void menu_power_mode_hibernation_func(const struct menu_item *m, bool checked);

#endif /* COMMON_H_ */
