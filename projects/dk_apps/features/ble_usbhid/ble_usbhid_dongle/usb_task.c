/**
 ****************************************************************************************
 *
 * @file usb_task.c
 *
 * @brief USB HID task implementation
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdarg.h>
#include "sys_charger.h"
#include "sys_power_mgr.h"
#include "sys_watchdog.h"
#include "hw_usb.h"
#include "USB.h"
#include "USB_HID.h"
#include "hid_ble_config.h"
#include "hid_usb_config.h"
#include "common.h"

/* This value cannot be larger that queue length in emUSB library (which is set to 3 by default)! */
#define USB_MAX_OUT_BUFS        3

static const USB_DEVICE_INFO usb_dev_info = {
        .VendorId = HID_USB_VID,
        .ProductId = HID_USB_PID,
        .sVendorName = HID_USB_VENDOR_NAME,
        .sProductName = HID_USB_PRODUCT_NAME,
        .sSerialNumber = HID_USB_SERIAL_NUMBER,
};

__RETAINED static struct {
        OS_TASK handle;
        int8_t wdog_id;
} task;

__RETAINED static USB_HOOK usb_hook_h;

__RETAINED static USB_HID_HANDLE usb_hid_h;

__RETAINED static struct {
        uint8_t read[USB_MAX_PACKET_SIZE];

        uint8_t in[USB_MAX_PACKET_SIZE];

        uint8_t out[USB_MAX_OUT_BUFS + 1][USB_MAX_PACKET_SIZE];
        size_t out_idx;
} usb_bufs;

__RETAINED bool usb_available;

static void set_state(bool new_state)
{
        OS_ENTER_CRITICAL_SECTION();

        usb_available = new_state;

        OS_LEAVE_CRITICAL_SECTION();

        notify_state_changed_to_ble();
}

static USB_HID_HANDLE usbd_hid_add(void)
{
        USB_HID_INIT_DATA hid_data = { };
        USB_HID_HANDLE hid_h;

        hid_data.EPIn = USBD_AddEP(USB_DIR_IN, USB_TRANSFER_TYPE_INT, 0, NULL, 0);
        hid_data.EPOut = USBD_AddEP(USB_DIR_OUT, USB_TRANSFER_TYPE_INT, 0,
                                                                usb_bufs.in, sizeof(usb_bufs.in));

        hid_data.pReport = hid_ble_config.report_map;
        hid_data.NumBytesReport = hid_ble_config.report_map_length;

        hid_h = USBD_HID_Add(&hid_data);

        return hid_h;
}

static void usb_state_changed_cb(void *context, uint8_t new_state)
{
}

void usb_is_suspended(void)
{
}

void usb_is_resumed(void)
{
}

static int hid_on_getreport_request_cb(USB_HID_REPORT_TYPE type, unsigned id,
                                                        const uint8_t **data, uint32_t *num_bytes)
{
        bool ble_state = get_ble_state();
        int ret;

        ret = usb_on_get_report(ble_state, type, id, num_bytes, usb_bufs.in);
        if (ret) {
                return 1;
        }

        if (!ble_state) {
                return 0;
        }

        send_to_ble(HID_OP_GET_REPORT, type, id, *num_bytes, NULL);

        return -1;
}

static void hid_on_setreport_request_cb(USB_HID_REPORT_TYPE type, unsigned id, uint32_t num_bytes)
{
        bool ble_state = get_ble_state();
        int ret;

        ret = usb_on_set_report(ble_state, type, id, num_bytes, usb_bufs.in);
        if (ret) {
                return;
        }

        send_to_ble(HID_OP_SET_REPORT, type, id, num_bytes, usb_bufs.in);
}

static void hid_on_data_cb(USB_HID_REPORT_TYPE type, unsigned id, uint32_t num_bytes)
{
        bool ble_state = get_ble_state();
        int ret;

        ret = usb_on_data(ble_state, type, id, num_bytes, &usb_bufs.read[1]);
        if (ret) {
                return;
        }

        send_to_ble(HID_OP_DATA, type, id, num_bytes, &usb_bufs.read[1]);
}

static void usb_task(void *params)
{
        int recv;
        int i;

        /* Register usb_hid_task task to be monitored by watchdog */
        task.wdog_id = sys_watchdog_register(false);

        /* Initialize USB */
        USBD_Init();
        USBD_RegisterSCHook(&usb_hook_h, usb_state_changed_cb, NULL);
        USBD_SetDeviceInfo(&usb_dev_info);
        USBD_HID_Init();
        usb_hid_h = usbd_hid_add();
        USBD_HID_SetOnGetReportRequest(usb_hid_h, hid_on_getreport_request_cb);
        USBD_HID_SetOnSetReportRequest(usb_hid_h, hid_on_setreport_request_cb);
        USBD_Start();

        set_state(true);

        while (1) {
                /* Notify watchdog on each loop */
                sys_watchdog_notify(task.wdog_id);

                /* Wait for configuration */
                if ((USBD_GetState() & (USB_STAT_CONFIGURED | USB_STAT_SUSPENDED))
                                                                        != USB_STAT_CONFIGURED) {
                        OS_DELAY(50);
                        continue;
                }

                /* Suspend watchdog while blocking on USBD_HID_Receive */
                sys_watchdog_suspend(task.wdog_id);

                /* Read Report ID */
                recv = USBD_HID_Read(usb_hid_h, usb_bufs.read, 1, 0);
                if (recv == 1) {
                        for (i = 0; i < hid_ble_config.num_reports; i++) {
                                const hids_report_t *report = &hid_ble_config.reports[i];

                                if (report->type != HIDS_REPORT_TYPE_OUTPUT ||
                                                        report->report_id != usb_bufs.read[0]) {
                                        continue;
                                }

                                /* Read remaining report data */
                                recv = USBD_HID_Read(usb_hid_h, &usb_bufs.read[1],
                                                                                report->length, 0);

                                hid_on_data_cb(report->type, report->report_id, recv);
                        }
                }

                /* Resume watchdog */
                sys_watchdog_notify_and_resume(task.wdog_id);

                OS_DELAY(10);
        }
}

static void usbhid_start(void)
{
        OS_BASE_TYPE status;

        /* Start the USB HID application task. */
        status = OS_TASK_CREATE("usbhid",       /* The text name assigned to the task, for
                                                   debug only; not used by the kernel. */
                        usb_task,               /* The function that implements the task. */
                        NULL,                   /* The parameter passed to the task. */
                        512,                    /* The number of bytes to allocate to the
                                                   stack of the task. */
                        OS_TASK_PRIORITY_HIGHEST,/* The priority assigned to the task. */
                        task.handle);           /* The task handle. */

        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);
}

static void usbhid_stop(void)
{
        USBD_UnregisterSCHook(&usb_hook_h);
        USBD_DeInit();

        OS_TASK_DELETE(task.handle);
        task.handle = 0;

        sys_watchdog_unregister(task.wdog_id);
}

void usb_start_enumeration_cb(void)
{
        if (task.handle) {
                return;
        }

        pm_stay_alive();
        cm_sys_clk_set(sysclk_PLL96);
        hw_usb_init();
        hw_usb_bus_attach();

        usbhid_start();
}

void usb_detach_cb(void)
{
        if (task.handle == 0) {
                return;
        }

        set_state(false);

        usbhid_stop();

        hw_usb_bus_detach();
        cm_sys_clk_set(sysclk_XTAL16M);
        pm_resume_sleep();
}

void send_to_usb(enum hid_op op, uint8_t type, uint8_t id, uint16_t length, const uint8_t *data)
{
        uint8_t *buf;
        int ret;

        if (task.handle == 0) {
                return;
        }

        buf = usb_bufs.out[usb_bufs.out_idx++];

        if (usb_bufs.out_idx > USB_MAX_OUT_BUFS) {
                usb_bufs.out_idx = 0;
        }

        buf[0] = id;
        memcpy(&buf[1], data, length);

        /*
         * Try to send data in a busy loop - we only need to wait for free buffer in the driver so
         * won't take long. If we return with other error or there is no USB task, just drop this
         * packet.
         */
        do {
                if (!task.handle) {
                        break;
                }

                ret = USBD_HID_Write(usb_hid_h, buf, length + 1, -1);
        } while (ret == USB_STATUS_EP_BUSY);
}

void notify_state_changed_to_usb(void)
{
        /* This can be ignored for now */
}

bool get_usb_state(void)
{
        bool state;

        OS_ENTER_CRITICAL_SECTION();

        state = usb_available;

        OS_LEAVE_CRITICAL_SECTION();

        return state;
}
