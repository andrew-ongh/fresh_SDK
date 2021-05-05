/**
 ****************************************************************************************
 *
 * @file serial_console.c
 *
 * @brief Serial console
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <string.h>
#include <stdio.h>
#include <osal.h>
#include <resmgmt.h>
#include "platform_devices.h"
#include "interrupts.h"

typedef struct {
        uart_device_id uart;    /**< Serial to pass to ad_uart_open */
        OS_MUTEX mutex;         /**< Mutex for reading clients */
        OS_TASK task;           /**< Console task */
        OS_EVENT fifo_not_full; /**< Event to wake up waiting writers */
        OS_EVENT read_finished; /**< Event to wake up readers */
        int16_t read_size;      /**< Number of requested bytes */
        int16_t fifo_wrix;      /**< write ring buffer index */
        int16_t fifo_rdix;      /**< read ring buffer index */
        int16_t fifo_free;      /**< number of free bytes in fifo */
        int16_t fifo_size;      /**< size of ring buffer */
        uint32_t drop_count;    /**< number of bytes already dropped */
        char *ring_buf;         /**< ring buffer */
        char *read_buf;         /**< user buffer provided for read */
} console_data_t;

PRIVILEGED_DATA console_data_t *console;

#define CONSOLE_WRITE_REQUEST           0x01
#define CONSOLE_WRITE_DONE              0x02
#define CONSOLE_READ_REQUEST            0x04
#define CONSOLE_READ_DONE               0x08
#define CONSOLE_EXIT                    0x80

/*
 * Copy data to console ring buffer, caller must ensure that data will fit.
 * This function will be called inside critical section.
 */
static void console_write_to_ring_buffer(const char *ptr, int len)
{
        console->fifo_free -= len;
        if (console->fifo_wrix + len > console->fifo_size) {
                /*
                 * This is case when some data must be written at the end of ring buffer.
                 * and some from the beginning.
                 */
                int left;
                memcpy(console->ring_buf + console->fifo_wrix, ptr,
                                                        console->fifo_size - console->fifo_wrix);
                left  = len - (console->fifo_size - console->fifo_wrix);
                memcpy(console->ring_buf, ptr + console->fifo_size - console->fifo_wrix, left);
                console->fifo_wrix = left;
        } else {
                /* Simple case without overlap */
                memcpy(console->ring_buf + console->fifo_wrix, ptr, len);
                console->fifo_wrix += len;
                if (console->fifo_wrix >= console->fifo_size) {
                        console->fifo_wrix = 0;
                }
        }
}

int console_write(const char *buf, int len)
{
        int dropped;
        int left = len;

        for (;;) {
                dropped = 0;
                /*
                 * Put as much as possible data into ring buffer.
                 */
                OS_ENTER_CRITICAL_SECTION();
                if (left > console->fifo_free) {
                        /* not all can fit in ring buffer */
                        dropped = left - console->fifo_free;
                        left = console->fifo_free;
                }
                /* There was something to write this time, just put it in buffer */
                if (left) {
                        console_write_to_ring_buffer(buf, left);
                }
                /*
                 * If something was not fitting in ring buffer but we are in interrupt, bad luck.
                 * Data will be just dropped forever.
                 */
                if (dropped && in_interrupt()) {
                        console->drop_count += dropped;
                        dropped = 0;
                }
                OS_LEAVE_CRITICAL_SECTION();

                /* If something was put in ring buffer notify task to take over printing */
                if (left) {
                        OS_TASK_NOTIFY_FROM_ISR(console->task, CONSOLE_WRITE_REQUEST,
                                                                                OS_NOTIFY_SET_BITS);
                }

                buf += left;
                left = 0;

                if (dropped) {
                        /*
                         * ring buffer did not took everything, let's wait for a while and try
                         * again. We can do this since this code is not interrupt.
                         */
                        left = dropped;
                        if (OS_EVENT_WAIT(console->fifo_not_full, 0x2000) == OS_EVENT_SIGNALED) {
                                /*
                                 * Now some space should show up in ring buffer.
                                 */
                                continue;
                        }
                        /*
                         * Wait failed with timeout, don't try again. Just count dropped data.
                         */
                        console->drop_count += left;
                }
                break;
        }
        return len - left;
}

int console_read(char *ptr, int len)
{
        /*
         * Only one client can request read at a time.
         */
        OS_MUTEX_GET(console->mutex, OS_MUTEX_FOREVER);

        /*
         * Pass read request parameters to task.
         */
        console->read_size = len;
        console->read_buf = ptr;
        OS_TASK_NOTIFY(console->task, CONSOLE_READ_REQUEST, OS_NOTIFY_SET_BITS);

        /*
         * Let's wait for ad_uart_read to finish in console task context.
         */
        OS_EVENT_WAIT(console->read_finished, OS_EVENT_FOREVER);

        OS_MUTEX_PUT(console->mutex);

        return console->read_size;
}

/*
 * Callback function called when single write to UART finished.
 * This callback will wake up console task so next writes can start.
 */
static void console_write_cb(void *user_data, uint16_t transferred)
{
        console_data_t *console = (console_data_t *) user_data;

        OS_ENTER_CRITICAL_SECTION();

        /* Move read index and increase free FIFO counter */
        console->fifo_rdix += transferred;
        if (console->fifo_rdix >= console->fifo_size) {
                console->fifo_rdix -= console->fifo_size;
        }
        console->fifo_free += transferred;

        OS_LEAVE_CRITICAL_SECTION();

        OS_TASK_NOTIFY_FROM_ISR(console->task, CONSOLE_WRITE_DONE, OS_NOTIFY_SET_BITS);
}

/*
 * Callback function called when read on UART ended.
 * This callback will wake up console task.
 */
static void console_read_cb(void *user_data, uint16_t transferred)
{
        console_data_t *console = (console_data_t *) user_data;
        console->read_size = transferred;
        OS_TASK_NOTIFY_FROM_ISR(console->task, CONSOLE_READ_DONE, OS_NOTIFY_SET_BITS);
}

static void console_task_fun(void *param)
{
        console_data_t *console = (console_data_t *) param;
        uint32_t pending_requests = 0;
        uint32_t current_requests;
        uint32_t mask = CONSOLE_WRITE_REQUEST | CONSOLE_READ_REQUEST | CONSOLE_EXIT;
        uart_device uart = ad_uart_open(console->uart);

        for (;;) {
                /*
                 * If there are some unmasked requests already, no need to wait for new ones.
                 */
                if ((pending_requests & mask) == 0) {
                        uint32_t new_bits = 0;
                        OS_TASK_NOTIFY_WAIT(0, ~0, &new_bits, OS_TASK_NOTIFY_FOREVER);
                        pending_requests |= new_bits;
                }
                /*
                 * Filter requests that are not masked, and remove ones that will be handled.
                 */
                current_requests = pending_requests & mask;
                pending_requests ^= current_requests;

                /*
                 * Ring buffer has some new data that should go to UART.
                 */
                if (0 != (current_requests & CONSOLE_WRITE_REQUEST) &&
                                                        console->fifo_free < console->fifo_size) {
                        uint16_t wx = console->fifo_wrix;
                        uint16_t size = 0;
                        if (console->fifo_rdix < wx) {
                                /*
                                 * In case when data to write is in one block in ring buffer
                                 * just print it in one run
                                 */
                                size = wx - console->fifo_rdix;
                        } else {
                                /*
                                 * This time data to print starts at the end of ring buffer.
                                 * UART will print this part first, and after writing that,
                                 * data at the beginning will be printed.
                                 */
                                size = console->fifo_size - console->fifo_rdix;
                                /*
                                 * Write request was already cleared, but here asked for it again.
                                 * This request will be masked till UART writes finishes.
                                 */
                                pending_requests |= CONSOLE_WRITE_REQUEST;
                        }

                        if (size > 0) {
                                /*
                                 * There was something to print, mask write request and wait for
                                 * write done, then start sending data.
                                 */
                                mask ^= CONSOLE_WRITE_REQUEST | CONSOLE_WRITE_DONE;
                                ad_uart_write_async(uart, console->ring_buf + console->fifo_rdix,
                                                                size, console_write_cb, console);
                        }
                }

                if (0 != (current_requests & CONSOLE_WRITE_DONE)) {
                        /*
                         * UART finished printing, enable write request again, and notify clients.
                         */
                        mask ^= CONSOLE_WRITE_REQUEST | CONSOLE_WRITE_DONE;
                        OS_EVENT_SIGNAL(console->fifo_not_full);
                }

                if (0 != (current_requests & CONSOLE_READ_REQUEST)) {
                        /*
                         * Some task want to read from UART. Start reading, block read requests
                         * and wait for read done.
                         */
                        mask ^= CONSOLE_READ_DONE | CONSOLE_READ_REQUEST;
                        ad_uart_read_async(uart, console->read_buf, console->read_size,
                                                                        console_read_cb, console);
                }

                if (0 != (current_requests & CONSOLE_READ_DONE)) {
                        /*
                         * Something was received. Enable read request again, and notify reader.
                         */
                        mask ^= CONSOLE_READ_DONE | CONSOLE_READ_REQUEST;
                        OS_EVENT_SIGNAL(console->read_finished);
                }

                if (current_requests & CONSOLE_EXIT) {
                        ad_uart_abort_read_async(uart);
                        break;
                }
        }

        OS_EVENT_DELETE(console->fifo_not_full);
        OS_EVENT_DELETE(console->read_finished);
        OS_MUTEX_DELETE(console->mutex);
        OS_FREE(console->ring_buf);
        OS_TASK_DELETE(NULL);
}

void console_init(uart_device_id id, uint16_t write_fifo_size)
{
        if (NULL == console) {
                console = OS_MALLOC(sizeof(*console));
                OS_ASSERT(console != NULL);
                console->uart = id;
                console->ring_buf = OS_MALLOC(write_fifo_size);
                OS_ASSERT(console->ring_buf != NULL);
                console->fifo_size = write_fifo_size;
                console->fifo_free = write_fifo_size;
                OS_MUTEX_CREATE(console->mutex);
                OS_EVENT_CREATE(console->fifo_not_full);
                OS_EVENT_CREATE(console->read_finished);
                OS_TASK_CREATE("console", console_task_fun, console, 400, 1, console->task);
        }
}

void console_done(void)
{
        if (console) {
                OS_TASK_NOTIFY(console->task, CONSOLE_EXIT, OS_NOTIFY_SET_BITS);
        }
}

#if SERIAL_CONSOLE_RETARGET
int _write (int fd, char *ptr, int len)
{
        console_write(ptr, len);

        return len;
}

int _read (int fd, char *ptr, int len)
{
        return console_read(ptr, 1);
}
#endif
