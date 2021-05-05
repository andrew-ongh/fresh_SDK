/**
 ****************************************************************************************
 *
 * @file demo_uart_printf.c
 *
 * @brief printf-like capability over UART2 demo (hw_uart driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <msg_queues.h>
#include <platform_devices.h>
#include <resmgmt.h>
#include <ad_uart.h>
#include "config.h"
#include "common.h"

/* message id and type, can be any numbers picked by application */
#define MSG_ID_PRINTFLN         ((MSG_ID) 1)
#define MSG_TYPE_PRINTFLN       ((MSG_TYPE) 1)

/* queue of messages for asynchronous printf */
static msg_queue queue;
/* overrun counter, i.e. how many times queue was full on attempt to put message */
static int queue_overrun = 0;
static uart_device uart;

/**
 * \brief Print formatted line of text to UART
 *
 * In blocking mode, text line is formatted and output immediately.
 * In non-blocking mode, text line is formatted and added to queue to be printed at idle time. In
 * this mode there's no guarantee that line will be ever printed, i.e. message will be dropped if
 * queue is already full.
 *
 * Output text line can hold up to 127 characters, excessive text will be stripped.
 *
 * No newline should be put at the end of \p fmt as it is always added automatically.
 *
 * \param [in] async true for non-blocking mode, false otherwise
 * \param [in] fmt format specified in printf syntax
 *
 */
void uart_printfln(bool async, const char *fmt, ...)
{
        char buf[127 + NEWLINE_SIZE];
        va_list ap;
        int len;

        /*
         * Format line of text into buffer and save space for newline. vsnprintf() reserves 1 byte
         * for \0, but we'll overwrite it with newline anyway - see below.
         */
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf) - NEWLINE_SIZE + 1, fmt, ap);
        va_end(ap);

        /*
         * Append newline characters at the end of string. It's ok to overwrite \0 since string does
         * not need to be null-terminated for sending over UART (it's handled as binary buffer).
         */
        len = strlen(buf);
        memcpy(&buf[len], NEWLINE, NEWLINE_SIZE);
        len += NEWLINE_SIZE;

        if (async) {
                /*
                 * In async (a.k.a. non-blocking) mode we just put message into print queue and
                 * return - this will be printed by uart_printf task which is running with low
                 * priority.
                 *
                 * Since buffer with data is allocated on stack, we're using send-with-copy version
                 * of send function, but zero-copy version is also available - in such case only
                 * pointer to existing buffer is passed in message so application have more control
                 * on allocation and freeing data buffer, for example:
                 *
                 * // this function is optional - if not specified, application have full control
                 * // on when buffer is freed
                 * void buf_free_func(void *buf)
                 * {
                 *     free(buf);
                 * }
                 *
                 * void send_message(char *data, int len)
                 * {
                 *     void *buf = malloc(len);
                 *     memcpy(buf, data, len);
                 *     if (msq_queue_send_zero_copy(&queue, MSG_ID_PRINTFLN, MSG_TYPE_PRINTFLN,
                 *                                              buf, len, OS_QUEUE_NO_WAIT,
                 *                                              buf_free_func) != OS_QUEUE_OK) {
                 *         // message not sent, buf_free_func() is called automatically if present!
                 *     }
                 * }
                 *
                 * In case queue is full, this just increments overrun counter. We can use this
                 * information later to see how good or bad things are going, but at the moment it's
                 * not used.
                 */
                if (msg_queue_send(&queue, MSG_ID_PRINTFLN, MSG_TYPE_PRINTFLN, buf, len,
                                                                OS_QUEUE_NO_WAIT) != OS_QUEUE_OK) {
                        queue_overrun++;
                }
        } else {
                /*
                 * In sync (a.k.a. blocking) mode we write to UART2 waiting if necessary for access.
                 * All synchronization is done internally in ad_uart_write.
                 */
                ad_uart_write(uart, buf, len);
        }
}

void task_uart_printf_init_func(const struct task_item *t)
{
        /*
         * Open serial device that will be used for prints.
         */
        uart = ad_uart_open(SERIAL2);

        msg_queue_create(&queue, 10, DEFAULT_OS_ALLOCATOR);
}

void task_uart_printf_func(const struct task_item *t)
{
        msg m;

        /*
         * Since this task does nothing but printing messages from queue, it can wait indefinitely
         * for message to be put into queue.
         */
        msg_queue_get(&queue, &m, OS_QUEUE_FOREVER);

        /*
         * As for now, only one id and type of message is expected to be put into queue, so it's
         * redundant to check for it, but for sanity purposes we'll check anyway.
         */
        if (m.id == MSG_ID_PRINTFLN && m.type == MSG_TYPE_PRINTFLN) {
                /*
                 * Write to UART locking it for print time.
                 */
                ad_uart_write(uart, (const char *) m.data, m.size);
        }

        /*
         * Receiver task is responsible for releasing message. Whether this will also free buffer
         * passed in message or not, it's decided by message queue logic and caller does not need
         * to know about it.
         */
        msg_release(&m);
}
