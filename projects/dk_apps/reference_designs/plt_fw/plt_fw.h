/*
 * plt_fw.h
 *
 *  Created on: Sep 11, 2015
 *      Author: akostop
 */

#ifndef PLT_FW_H_
#define PLT_FW_H_

#include "dgtl_msg.h"
#include "dgtl_pkt.h"

#define PLT_VERSION_STR "1.1"

/* Common event header */
typedef struct {
        dgtl_pkt_hci_evt_t hci_evt;
        uint8_t            num_hci_cmd_packets;
        uint16_t           opcode;
} __attribute__((packed)) plt_evt_hdr_t;

/* xtrim HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t hci_cmd;
        uint8_t            operation;
        uint16_t           value;
} __attribute__((packed)) plt_cmd_xtrim_t;

/* xtrim HCI event response */
typedef struct {
        plt_evt_hdr_t hdr;
        uint16_t      trim_value;
} __attribute__((packed)) plt_evt_xtrim_t;

/* hci_firmware_version_get HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t hci_cmd;
} __attribute__((packed)) plt_cmd_hci_firmware_version_get_t;

/* hci_firmware_version_get HCI event response */
typedef struct {
        plt_evt_hdr_t hdr;
        uint8_t       ble_version_length;
        uint8_t       app_version_length;
        char          ble_fw_version[32];
        char          app_fw_version[32];
} __attribute__((packed)) plt_evt_hci_firmware_version_get_t;

/* hci_custom_action HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t hci_cmd;
        uint8_t            custom_action;
} __attribute__((packed)) plt_cmd_hci_custom_action_t;

/* hci_custom_action HCI event response */
typedef struct {
        plt_evt_hdr_t hdr;
        uint8_t       custom_action;
} __attribute__((packed)) plt_evt_hci_custom_action_t;

/* hci_read_adc HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t hci_cmd;
        uint8_t            samples_nr;
        uint8_t            samples_period;
} __attribute__((packed)) plt_cmd_hci_read_adc_t;

/* hci_read_adc HCI event response */
typedef struct {
        plt_evt_hdr_t hdr;
        uint16_t      result;
} __attribute__((packed)) plt_evt_hci_read_adc_t;

/* hci_sensor_test HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t      hci_cmd;
        uint8_t                 iface;                  /* 0 = SPI, 1 = I2C */
        uint8_t                 rd_wr;                  /* 0 = RD, 1 = WR */
        uint8_t                 spi_clk_i2c_scl_port;
        uint8_t                 spi_clk_i2c_scl_pin;
        uint8_t                 spi_di_i2c_sda_port;
        uint8_t                 spi_di_i2c_sda_pin;
        uint8_t                 spi_do_port;
        uint8_t                 spi_do_pin;
        uint8_t                 spi_cs_port;
        uint8_t                 spi_cs_pin;
        uint8_t                 reg_addr;               /* The sensor register address to read or write. */
        uint8_t                 reg_data;               /* The data to write if rd_wr=1. */
        uint8_t                 i2c_slave_addr;
        uint8_t                 int_gpio_check;         /* 0 = Do not test DRDY/INT GPIO, 1 = Test DRDY/INT GPIO. */
        uint8_t                 int_port;               /* The DRDY/INT GPIO port. */
        uint8_t                 int_pin;                /* The DRDY/INT GPIO pin. */
        uint8_t                 gpio_lvl;               /* 0 = 3.3V, 1 = 1.8V. Used for SPI and DRDY/INT GPIOs. */
} __attribute__((packed)) plt_cmd_hci_sensor_test_t;

/* hci_sensor_test HCI event response */
typedef struct {
        plt_evt_hdr_t           hdr;
        uint8_t                 data;
        uint8_t                 error;                  /* 0=Command succeeded. Valid data. 0xFF=Command error. Invalid data. */
} __attribute__((packed)) plt_evt_hci_sensor_test_t;

/* hci_gpio_set HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t      hci_cmd;
        uint8_t                 gpio_pad;               /* An enumeration of all available GPIOs. E.g. P0_0=0, P0_1=1, ..., etc. */
        uint8_t                 mode;                   /* 0=HW_GPIO_MODE_INPUT, 1=HW_GPIO_MODE_INPUT_PULLUP, 2=HW_GPIO_MODE_INPUT_PULLDOWN, 3=HW_GPIO_MODE_OUTPUT, 4=HW_GPIO_MODE_OUTPUT_PUSH_PULL, 5=HW_GPIO_MODE_OUTPUT_OPEN_DRAIN. */
        uint8_t                 gpio_lvl;               /* The voltage rail. 0 = 3.3V, 1 = 1.8V. */
        uint8_t                 val;                    /* 0=Reset, 1=Set. Only valid in Output mode. */
} __attribute__((packed)) plt_cmd_hci_gpio_set_t;

/* hci_gpio_set HCI event response */
typedef struct {
        plt_evt_hdr_t           hdr;
        uint8_t                 error;                  /* 0=Succeeded. 0xFF=Error. */
} __attribute__((packed)) plt_evt_hci_gpio_set_t;

/* hci_gpio_read HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t      hci_cmd;
        uint8_t                 gpio_pad;               /* An enumeration of all available GPIOs. E.g. P0_0=0, P0_1=1, ..., etc. */
} __attribute__((packed)) plt_cmd_hci_gpio_read_t;

/* hci_gpio_read HCI event response */
typedef struct {
        plt_evt_hdr_t           hdr;
        uint8_t                 data;                   /* 0=Low, 1=High, 0xFF=Error. */
} __attribute__((packed)) plt_evt_hci_gpio_read_t;

/* hci_uart_loop HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t      hci_cmd;
        uint8_t                 data[0];                /* Variable length received data to be looped back to UART. */
} __attribute__((packed)) plt_cmd_hci_uart_loop_t;

/* hci_uart_loop HCI event response */
typedef struct {
        plt_evt_hdr_t           hdr;
        uint8_t                 data[0];                /* Variable length loop back UART data. */
} __attribute__((packed)) plt_evt_hci_uart_loop_t;

/* hci_uart_baud HCI command */
typedef struct {
        dgtl_pkt_hci_cmd_t      hci_cmd;
        uint8_t                 data;                   /* The UART baud rate. 0=9600, 1=19200, 2=57600, 3=115200, 4=1000000. */
} __attribute__((packed)) plt_cmd_hci_uart_baud_t;

/* hci_uart_baud HCI event response */
typedef struct {
        plt_evt_hdr_t           hdr;
        uint8_t                 error;                  /* 1=error, 0=Succeed. */
} __attribute__((packed)) plt_evt_hci_uart_baud_t;

void plt_parse_dgtl_msg(dgtl_msg_t *msg);

#endif /* PLT_FW_H_ */
