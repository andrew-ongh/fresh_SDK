/**
 ****************************************************************************************
 *
 * @file plt_fw.c
 *
 * @brief PLT Firmware core code.
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
#include <string.h>
#include <stdio.h>

#include "osal.h"
#include "hw_gpio.h"

#include "ble_mgr.h"
#include "co_version.h"

#include "packers.h"
#include "plt_fw.h"
#include "Xtal_TRIM.h"

#include "hw_gpio.h"
#include "hw_spi.h"
#include "hw_i2c.h"
#include "ad_gpadc.h"
#include "hw_uart.h"
#include <platform_devices.h>

#include "dgtl.h"
#include "dgtl_msg.h"
#include "dgtl_pkt.h"

#define MAX_TRIM 2047
#define MIN_TRIM 0

typedef void (*plt_cmd_handler)(dgtl_msg_t *msg);

void xtal_trim(dgtl_msg_t *msg);
void fw_version_get(dgtl_msg_t *msg);
void hci_custom_action(dgtl_msg_t *msg);
void hci_read_adc(dgtl_msg_t *msg);
void hci_sensor_test(dgtl_msg_t *msg);
void hci_gpio_set(dgtl_msg_t *msg);
void hci_gpio_read(dgtl_msg_t *msg);
void hci_uart_loop(dgtl_msg_t *msg);
void hci_uart_baud(dgtl_msg_t *msg);

plt_cmd_handler plt_cmd_handlers[] = {
		NULL,
		NULL,
		xtal_trim,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		fw_version_get,
		NULL,
		hci_custom_action,      /* 0xFE0A */
		hci_read_adc,           /* 0xFE0B */
		hci_sensor_test,        /* 0xFE0C */
		hci_gpio_set,           /* 0xFE0D */
		hci_gpio_read,          /* 0xFE0E */
		hci_uart_loop,          /* 0xFE0F */
		hci_uart_baud           /* 0xFE10 */
};

dgtl_msg_t *init_response_evt(dgtl_pkt_hci_cmd_t *cmd, size_t length)
{
        dgtl_msg_t *msg_evt;
        plt_evt_hdr_t *evt;
        size_t param_len;

        param_len = length - sizeof(dgtl_pkt_hci_evt_t);

        msg_evt = dgtl_msg_prepare_hci_evt(NULL, 0x0E /* Command Complete Event */, param_len, NULL);
        evt = (plt_evt_hdr_t *) msg_evt;

        /* Clear parameters of event packet */
        memset(dgtl_msg_get_param_ptr(msg_evt, NULL), 0, param_len);

        evt->num_hci_cmd_packets = 1;
        evt->opcode = cmd->opcode;

        return msg_evt;
}

void plt_parse_dgtl_msg(dgtl_msg_t *msg)
{
        const dgtl_pkt_hci_cmd_t *pkt = (const dgtl_pkt_hci_cmd_t *) msg;

        /* Get only lower 8-bits of OCF (others are unused) */
        uint8_t cmd = pkt->opcode;

        if ((cmd < (sizeof(plt_cmd_handlers) / sizeof(plt_cmd_handler))) && plt_cmd_handlers[cmd]) {
                plt_cmd_handlers[cmd](msg);
        }

        dgtl_msg_free(msg);
}

static inline void enable_output_xtal(void)
{
        GPIO->GPIO_CLK_SEL = 0x3; /* Select XTAL16 clock */
        hw_gpio_set_pin_function(HW_GPIO_PORT_0, HW_GPIO_PIN_5, HW_GPIO_MODE_OUTPUT, HW_GPIO_FUNC_CLOCK);
}

static inline void disable_output_xtal(void)
{
        hw_gpio_set_pin_function(HW_GPIO_PORT_0, HW_GPIO_PIN_5, HW_GPIO_MODE_INPUT, HW_GPIO_FUNC_GPIO);
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


        if (r < 0)
                return -r;
        else
                return 0;
}

void xtal_trim(dgtl_msg_t *msg)
{
        plt_cmd_xtrim_t *cmd = (plt_cmd_xtrim_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_xtrim_t *evt;

        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_xtrim_t *) msg_evt;

        switch (cmd->operation) {
        case 0: /* Read trim value */
                evt->trim_value = CRG_TOP->CLK_FREQ_TRIM_REG;
                break;
        case 1: /* Write trim value */
                CRG_TOP->CLK_FREQ_TRIM_REG = cmd->value;
                break;
        case 2: /* Enable output xtal on P05 */
                enable_output_xtal();
                break;
        case 3: /* Increase trim value by delta */
                CRG_TOP->CLK_FREQ_TRIM_REG = CRG_TOP->CLK_FREQ_TRIM_REG + cmd->value;
                break;
        case 4: /* Decrease trim value by delta */
                CRG_TOP->CLK_FREQ_TRIM_REG = CRG_TOP->CLK_FREQ_TRIM_REG - cmd->value;
                break;
        case 5: /* Disable output xtal on P05 */
                disable_output_xtal();
                break;
        case 6: /* Auto calibration test */
                evt->trim_value = auto_xtal_trim(cmd->value);
                break;
        }

        dgtl_send(msg_evt);
}

void fw_version_get(dgtl_msg_t *msg)
{
        plt_cmd_hci_firmware_version_get_t *cmd = (plt_cmd_hci_firmware_version_get_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_firmware_version_get_t *evt;

        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_hci_firmware_version_get_t *) msg_evt;

        evt->ble_version_length = snprintf(evt->ble_fw_version, sizeof(evt->ble_fw_version),
                                                "%d.%d.%d.%d", RWBLE_SW_VERSION_MAJOR,
                                                RWBLE_SW_VERSION_MINOR, RWBLE_SW_VERSION_BUILD,
                                                RWBLE_SW_VERSION_SUB_BUILD ) + 1;

        evt->app_version_length = strlen(PLT_VERSION_STR) + 1;
        memcpy(evt->app_fw_version, PLT_VERSION_STR, evt->app_version_length);

        dgtl_send(msg_evt);
}

void hci_custom_action(dgtl_msg_t *msg)
{
        plt_cmd_hci_custom_action_t *cmd = (plt_cmd_hci_custom_action_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_custom_action_t *evt;

        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_hci_custom_action_t *) msg_evt;

        evt->custom_action = cmd->custom_action;

        dgtl_send(msg_evt);
}

void hci_read_adc(dgtl_msg_t *msg)
{
        plt_cmd_hci_read_adc_t *cmd = (plt_cmd_hci_read_adc_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_read_adc_t *evt;
        uint16_t adc_value;
        uint16_t adc_offs_p = 0x200;
        uint16_t adc_offs_n = 0x200;
        uint32_t sum = 0;
        int i;

        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_hci_read_adc_t *) msg_evt;

        /* Hack: We can't really use ADC adapter here, since it only
         * returns the 10-bit result, not the 16-bit oversampled one
         */
        resource_acquire(RES_MASK(RES_ID_GPADC), RES_WAIT_FOREVER);

        adc_offs_p = hw_gpadc_get_offset_positive();
        adc_offs_n = hw_gpadc_get_offset_negative();

        hw_gpadc_reset();

        hw_gpadc_set_input(HW_GPADC_INPUT_SE_VBAT);
        hw_gpadc_set_input_mode(HW_GPADC_INPUT_MODE_SINGLE_ENDED);
        hw_gpadc_set_ldo_constant_current(true);
        hw_gpadc_set_ldo_dynamic_current(true);
        hw_gpadc_adc_measure(); // dummy (fast) measurement
        hw_gpadc_set_sample_time(15);

        hw_gpadc_set_offset_positive(0x200);
        hw_gpadc_set_offset_negative(0x200);

        hw_gpadc_set_chopping(true);
        hw_gpadc_set_oversampling(7); // 128 samples
        for (volatile int i = 10; i > 0; i--)
                ; // Make sure 1usec has passed since the mode setting (VBAT).

        OS_ENTER_CRITICAL_SECTION();
        for (i = 0; i < cmd->samples_nr; i++) {
                hw_gpadc_adc_measure();
                adc_value = hw_gpadc_get_raw_value();
                sum += adc_value;
                if (cmd->samples_period > 0)
                        OS_DELAY_MS(cmd->samples_period);
        }
        OS_LEAVE_CRITICAL_SECTION();

        hw_gpadc_set_offset_positive(adc_offs_p);
        hw_gpadc_set_offset_negative(adc_offs_n);

        resource_release(RES_MASK(RES_ID_GPADC));

        evt->result = ((cmd->samples_nr != 0) ? sum / cmd->samples_nr : 0);

        dgtl_send(msg_evt);
}

void hci_sensor_test(dgtl_msg_t *msg)
{
        plt_cmd_hci_sensor_test_t *cmd = (plt_cmd_hci_sensor_test_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_sensor_test_t *evt;
        size_t i2c_status = 0;
        HW_I2C_ABORT_SOURCE abrt_src = HW_I2C_ABORT_NONE;

        /* SPI initialization structure. */
        const spi_config spi_init = {
                .cs_pad.port = cmd->spi_cs_port,
                .cs_pad.pin = cmd->spi_cs_pin,
                .word_mode = HW_SPI_WORD_8BIT,
                .smn_role = HW_SPI_MODE_MASTER,
                .polarity_mode = HW_SPI_POL_HIGH,
                .phase_mode = HW_SPI_PHA_MODE_1,
                .mint_mode = HW_SPI_MINT_DISABLE,
                .xtal_freq = HW_SPI_FREQ_DIV_2,
                .fifo_mode = HW_SPI_FIFO_RX_TX,
                .disabled = 0,
                .use_dma = 0,
                .rx_dma_channel = 0,
                .tx_dma_channel = 0
        };

        /* I2C initialization structure. */
        const i2c_config i2c_init = {
                .speed = HW_I2C_SPEED_STANDARD,
                .mode = HW_I2C_MODE_MASTER,
                .addr_mode = HW_I2C_ADDRESSING_7B,
        };

        /* Prepare the UART response message. */
        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_hci_sensor_test_t *) msg_evt;
        evt->error = 0x0; // Initialize with no error.

        /* If DRDY/INT GPIO test is enabled, then initialize it. */
        if (cmd->int_gpio_check == 0x01) {
                hw_gpio_configure_pin(cmd->int_port, cmd->int_pin, HW_GPIO_MODE_INPUT_PULLUP, HW_GPIO_FUNC_GPIO, false);
                hw_gpio_configure_pin_power(cmd->int_port, cmd->int_pin, cmd->gpio_lvl ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);
        }

        /* Sensor interface is SPI. */
        if (cmd->iface == 0)
        {
                /* Initialize the SPI interface. */
                hw_spi_init(HW_SPI1, &spi_init);

                /* Configure the SPI GPIOs. */
                hw_gpio_configure_pin(cmd->spi_cs_port, cmd->spi_cs_pin, HW_GPIO_MODE_OUTPUT_PUSH_PULL, HW_GPIO_FUNC_SPI_EN, true);
                hw_gpio_configure_pin(cmd->spi_clk_i2c_scl_port, cmd->spi_clk_i2c_scl_pin, HW_GPIO_MODE_OUTPUT_PUSH_PULL, HW_GPIO_FUNC_SPI_CLK, true);
                hw_gpio_configure_pin(cmd->spi_do_port, cmd->spi_do_pin, HW_GPIO_MODE_OUTPUT_PUSH_PULL, HW_GPIO_FUNC_SPI_DO, true);
                hw_gpio_configure_pin(cmd->spi_di_i2c_sda_port, cmd->spi_di_i2c_sda_pin, HW_GPIO_MODE_INPUT_PULLUP, HW_GPIO_FUNC_SPI_DI, true);
                hw_gpio_configure_pin_power(cmd->spi_cs_port, cmd->spi_cs_pin, cmd->gpio_lvl ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);
                hw_gpio_configure_pin_power(cmd->spi_clk_i2c_scl_port, cmd->spi_clk_i2c_scl_pin, cmd->gpio_lvl ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);
                hw_gpio_configure_pin_power(cmd->spi_do_port, cmd->spi_do_pin, cmd->gpio_lvl ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);
                hw_gpio_configure_pin_power(cmd->spi_di_i2c_sda_port, cmd->spi_di_i2c_sda_pin, cmd->gpio_lvl ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);

                /* Toggle CS. Needed for some devices to enter SPI mode. */
                hw_spi_set_cs_low(HW_SPI1);
                hw_cpm_delay_usec(1000);
                hw_spi_set_cs_high(HW_SPI1);
                hw_cpm_delay_usec(1000);

                hw_spi_set_cs_low(HW_SPI1);

                /* Check if we want to write to the sensor. */
                if (cmd->rd_wr == 0x01) {
                        cmd->reg_addr = cmd->reg_addr & 0x7F;
                        hw_spi_write_buf(HW_SPI1, &cmd->reg_addr, 1, NULL, NULL);
                        hw_spi_write_buf(HW_SPI1, &cmd->reg_data, 1, NULL, NULL);
                }

                /* Read from the sensor. */
                cmd->reg_addr = cmd->reg_addr | 0x80;
                hw_spi_write_buf(HW_SPI1, &cmd->reg_addr, 1, NULL, NULL);
                hw_spi_read_buf(HW_SPI1, &evt->data, 1, NULL, NULL);

                /* Set the SPI CS high. */
                hw_spi_set_cs_high(HW_SPI1);

                /* Check the status of the DRDY/INT GPIO. */
                if (cmd->int_gpio_check == 0x01) {
                    hw_cpm_delay_usec(1000);
                    evt->data = hw_gpio_get_pin_status((HW_GPIO_PORT) cmd->int_port, (HW_GPIO_PIN) cmd->int_pin);
                }

                /* Reset the SPI GPIOs. */
                hw_gpio_configure_pin(cmd->spi_cs_port, cmd->spi_cs_pin, HW_GPIO_MODE_INPUT_PULLUP, HW_GPIO_FUNC_SPI_EN, true);
                hw_gpio_configure_pin(cmd->spi_clk_i2c_scl_port, cmd->spi_clk_i2c_scl_pin, HW_GPIO_MODE_INPUT_PULLDOWN, HW_GPIO_FUNC_SPI_CLK, true);
                hw_gpio_configure_pin(cmd->spi_do_port, cmd->spi_do_pin, HW_GPIO_MODE_INPUT_PULLDOWN, HW_GPIO_FUNC_SPI_DO, true);
                hw_gpio_configure_pin(cmd->spi_di_i2c_sda_port, cmd->spi_di_i2c_sda_pin, HW_GPIO_MODE_INPUT_PULLDOWN, HW_GPIO_FUNC_SPI_DI, true);

                /* Reset and disable the SPI1 interface. */
                hw_spi_reset(HW_SPI1);
                hw_spi_enable(HW_SPI1, 0);

        /* Sensor interface is I2C. */
        } else {
                /* Initialize the I2C interface. */
                hw_i2c_init(HW_I2C1, &i2c_init);

                /* Set the address. */
                hw_i2c_disable(HW_I2C1);
                hw_i2c_set_target_address(HW_I2C1, cmd->i2c_slave_addr);
                hw_i2c_enable(HW_I2C1);

                /* Configure the I2C GPIOs. */
                hw_gpio_configure_pin(cmd->spi_clk_i2c_scl_port, cmd->spi_clk_i2c_scl_pin, HW_GPIO_MODE_INPUT, HW_GPIO_FUNC_I2C_SCL, true);
                hw_gpio_configure_pin(cmd->spi_di_i2c_sda_port, cmd->spi_di_i2c_sda_pin, HW_GPIO_MODE_INPUT, HW_GPIO_FUNC_I2C_SDA, true);
                hw_gpio_configure_pin_power(cmd->spi_clk_i2c_scl_port, cmd->spi_clk_i2c_scl_pin, cmd->gpio_lvl ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);
                hw_gpio_configure_pin_power(cmd->spi_di_i2c_sda_port, cmd->spi_di_i2c_sda_pin, cmd->gpio_lvl ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);

                /* Check if we want to write to the sensor. */
                if (cmd->rd_wr == 0x01) {
                        hw_i2c_write_byte(HW_I2C1, cmd->reg_addr);
                        i2c_status = hw_i2c_write_buffer_sync(HW_I2C1, &cmd->reg_data, 1, &abrt_src, HW_I2C_F_WAIT_FOR_STOP);
                        if ((i2c_status < 1) || (abrt_src != HW_I2C_ABORT_NONE)) {
                                evt->data = 0xFF;
                                evt->error = 0x01;
                                dgtl_send(msg_evt);
                                return;
                        }
                }

                /* Read from the sensor. */
                hw_i2c_write_byte(HW_I2C1, cmd->reg_addr);
                i2c_status = hw_i2c_read_buffer_sync(HW_I2C1, &evt->data, 1, &abrt_src, HW_I2C_F_NONE);
                if ((i2c_status < 1) || (abrt_src != HW_I2C_ABORT_NONE)) {
                        evt->data = 0xFF;
                        evt->error = 0x01;
                        dgtl_send(msg_evt);
                        return;
                }

                /* Check the status of the DRDY/INT GPIO. */
                if (cmd->int_gpio_check == 0x01) {
                    hw_cpm_delay_usec(1000);
                    evt->data = hw_gpio_get_pin_status((HW_GPIO_PORT) cmd->int_port, (HW_GPIO_PIN) cmd->int_pin);
                }

                /* Reset the I2C GPIOs. */
                hw_gpio_configure_pin(cmd->spi_clk_i2c_scl_port, cmd->spi_clk_i2c_scl_pin, HW_GPIO_MODE_INPUT_PULLDOWN, HW_GPIO_FUNC_I2C_SCL, true);
                hw_gpio_configure_pin(cmd->spi_di_i2c_sda_port, cmd->spi_di_i2c_sda_pin, HW_GPIO_MODE_INPUT_PULLDOWN, HW_GPIO_FUNC_I2C_SDA, true);

                /* Reset and disable the I2C interface. */
                hw_i2c_reset_abort_source(HW_I2C1);
                hw_i2c_reset_int_all(HW_I2C1);
                hw_i2c_disable(HW_I2C1);
        }
        /* Send the UART response. */
        dgtl_send(msg_evt);
}

void hci_gpio_set(dgtl_msg_t *msg)
{
        plt_cmd_hci_gpio_set_t *cmd = (plt_cmd_hci_gpio_set_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_gpio_set_t *evt;
        uint8_t err = 0x0;
        HW_GPIO_PORT port;
        HW_GPIO_PIN pin;
        HW_GPIO_MODE mode;
        uint8_t volt_rail;
        uint8_t state;

        /* Prepare the UART response message. */
        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_hci_gpio_set_t *) msg_evt;

        /* Transform the input. */
        port = (HW_GPIO_PORT) (cmd->gpio_pad / 10);
        pin = (HW_GPIO_PIN) (cmd->gpio_pad % 10);
        volt_rail = cmd->gpio_lvl;
        state = cmd->val;

        /* Validate the GPIO port. */
        if (!(port >= HW_GPIO_PORT_0 && port <= HW_GPIO_PORT_4))
                err = 1;

        /* Validate the GPIO pin. */
        if (!(pin >= HW_GPIO_PIN_0 && pin <= HW_GPIO_PIN_7) || (port == HW_GPIO_PORT_2 && pin >= HW_GPIO_PIN_4))
                err = 1;

        /* Transform and validate the GPIO mode. */
        switch (cmd->mode) {
        case 0: mode = HW_GPIO_MODE_INPUT; break;
        case 1: mode = HW_GPIO_MODE_INPUT_PULLUP; break;
        case 2: mode = HW_GPIO_MODE_INPUT_PULLDOWN; break;
        case 3: mode = HW_GPIO_MODE_OUTPUT; break;
        case 4: mode = HW_GPIO_MODE_OUTPUT_PUSH_PULL; break;
        case 5: mode = HW_GPIO_MODE_OUTPUT_OPEN_DRAIN; break;
        default: err = 1; break;
        }

        /* Validate the GPIO voltage rail. 0 = 3.3V, 1 = 1.8V. */
        if (volt_rail > 1) err = 1;

        /* 0=Reset, 1=Set. Only valid in output mode. */
        if ((state > 1) && (cmd->mode >= 3)) err = 1;

        /* If error exists in the received message, reply with error. */
        if (err) {
                evt->error = 0xFF;
                dgtl_send(msg_evt);
                return;
        }

        /* P1_1 and P2_2 are the USB port pins.
           They need special handling to power them and use them as GPIOs.
        */
        if ((port == HW_GPIO_PORT_1 && pin == HW_GPIO_PIN_1) ||
                (port == HW_GPIO_PORT_2 && pin == HW_GPIO_PIN_2)) {
                REG_CLR_BIT(USB, USB_MCTRL_REG, USBEN);
                REG_SET_BIT(CRG_PER, USBPAD_REG, USBPAD_EN);
        }

        /* Configure the pin voltage rail. */
        hw_gpio_configure_pin_power(port, pin, volt_rail ? HW_GPIO_POWER_VDD1V8P : HW_GPIO_POWER_V33);

        /* Configure the pin. */
        hw_gpio_configure_pin(port, pin, mode, HW_GPIO_FUNC_GPIO, state==1 ? true : false);

        /* No error. */
        evt->error = 0x0;
        /* Send the UART response. */
        dgtl_send(msg_evt);
}

void hci_gpio_read(dgtl_msg_t *msg)
{
        plt_cmd_hci_gpio_read_t *cmd = (plt_cmd_hci_gpio_read_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_gpio_read_t *evt;
        uint8_t err = 0x0;
        HW_GPIO_PORT port;
        HW_GPIO_PIN pin;

        /* Prepare the UART response message. */
        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_hci_gpio_read_t *) msg_evt;

        /* Transform the input. */
        port = cmd->gpio_pad / 10;
        pin = cmd->gpio_pad % 10;

        /* Validate the GPIO port. */
        if (!(port >= HW_GPIO_PORT_0 && port <= HW_GPIO_PORT_4))
                err = 1;

        /* Validate the GPIO pin. */
        if (!(pin >= HW_GPIO_PIN_0 && pin <= HW_GPIO_PIN_7) ||(port == HW_GPIO_PORT_2 && pin >= HW_GPIO_PIN_4))
                err = 1;

        /* If error exists in the received message, reply with error. */
        if (err) {
                evt->data = 0xFF;
                dgtl_send(msg_evt);
                return;
        }

        evt->data = hw_gpio_get_pin_status(port, pin);

        /* Send the UART response. */
        dgtl_send(msg_evt);
}

void hci_uart_loop(dgtl_msg_t *msg)
{
        plt_cmd_hci_uart_loop_t *cmd = (plt_cmd_hci_uart_loop_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_uart_loop_t *evt;

        /* Get the length of the received data. */
        size_t pkt_len = (size_t)(cmd->hci_cmd.length);

        /* Prepare the UART response message. */
        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt) + pkt_len);
        evt = (plt_evt_hci_uart_loop_t *) msg_evt;

        /* Copy the received data to the output UART buffer. */
        memcpy(evt->data, cmd->data, pkt_len);

        /* Send the UART response. */
        dgtl_send(msg_evt);
}

void hci_uart_baud(dgtl_msg_t *msg)
{
        plt_cmd_hci_uart_baud_t *cmd = (plt_cmd_hci_uart_baud_t *) msg;
        dgtl_msg_t *msg_evt;
        plt_evt_hci_uart_baud_t *evt;

        /* Prepare the UART response message. */
        msg_evt = init_response_evt(&cmd->hci_cmd, sizeof(*evt));
        evt = (plt_evt_hci_uart_baud_t *) msg_evt;

        if ((cmd->data == 4) ||
            (cmd->data == 3) ||
            (cmd->data == 2) ||
            (cmd->data == 1) ||
            (cmd->data == 0))
                evt->error = 0;
        else evt->error = 1;

        /* Send the UART response. */
        dgtl_send(msg_evt);

        /* Wait some time for UART to end transfer. */
        hw_cpm_delay_usec(2000);

        switch (cmd->data)
        {
            case 4:
                hw_uart_baudrate_set(HW_UART2, HW_UART_BAUDRATE_1000000);
                break;
            case 3:
                hw_uart_baudrate_set(HW_UART2, HW_UART_BAUDRATE_115200);
                break;
            case 2:
                hw_uart_baudrate_set(HW_UART2, HW_UART_BAUDRATE_57600);
                break;
            case 1:
                hw_uart_baudrate_set(HW_UART2, HW_UART_BAUDRATE_19200);
                break;
            case 0:
                hw_uart_baudrate_set(HW_UART2, HW_UART_BAUDRATE_9600);
                break;
            default:
                hw_uart_baudrate_set(HW_UART2, HW_UART_BAUDRATE_115200);
                break;
        }
}

