/**
 ****************************************************************************************
 *
 * @file serial_console.h
 *
 * @brief Declarations for serial console
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef SERIAL_CONSOLE_H_
#define SERIAL_CONSOLE_H_

#include "ad_uart.h"

/*
 * Serial console provides input and output through one of the UART's.
 * It uses UART adapter for reading and writing.
 * Unlike UART adapter functions, writes to serial port can be started in interrupt context, so
 * console can be used to allow prints from interrupts.
 * To allow writes from tasks and interrupts, console uses it's own task that does actual
 * UART hardware access through UART adapter.
 * Console does not use additional RAM for printing. RAM that is used is allocated during
 * initialization only.
 *
 * Passing data to print is done using circular buffer that is filled by console_write calls.
 * If data flow is too fast for UART, calls from task will wait, while calls from interrupt can
 * lose some data.
 *
 * Serial console can provide _read and _write functions that are used by printf.
 * To redirect printf (and other I/O for stdout) user should provide definition:
 * #define SERIAL_CONSOLE_RETARGET      1
 */

/**
 * \brief Initialize console to use with specified serial device
 *
 * This function will allocate all necessary resources for serial console (RAM, task,
 * synchronization primitives).
 *
 * \param [in] id uart id (usually SERIAL1 or SERIAL2, see platform_devices.h for exact name).
 * \param [in] write_fifo_size size of buffer that will hold data before it's written to UART
 *             this buffer will be allocated for internal usage.
 *
 */
void console_init(uart_device_id id, uint16_t write_fifo_size);

/**
 * \brief De-initialize console
 *
 * This function frees resource used by serial console.
 *
 */
void console_done(void);

/**
 * \brief Write to UART
 *
 * This function can be called from normal task as well as interrupts.
 * When called from interrupts this function will not block. If buffer can't hold all requested
 * data some data will be dropped and will never leave UART.
 * When called from a task, this function can block if there is no space in buffer to hold data.
 *
 * \param [in] buf pointer to data to send to UART
 * \param [in] len number of bytes to print
 *
 * \return number of bytes written
 */
int console_write(const char *buf, int len);

/**
 * \brief Read from UART
 *
 * Call this function to read data from UART.
 *
 * \param [out] buf pointer to buffer to fill with data from UART
 * \param [in] len number of bytes to read
 *
 */
int console_read(char *buf, int len);

#endif /* SERIAL_CONSOLE_H_ */
