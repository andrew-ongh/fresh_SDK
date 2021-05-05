/**
 ****************************************************************************************
 *
 * @file usb_cdc.c
 *
 * @brief USB CDC app implementation
 *
 * Copyright (C) 2016-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */
#include <stdarg.h>
#include "sys_charger.h"
#include "sys_power_mgr.h"
#include "hw_usb.h"
#include "sys_watchdog.h"
#include "USB_CDC.h"

/*********************************************************************
 *
 *       Defines, configurable
 *
 **********************************************************************
 */

#define usb_main_TASK_PRIORITY              ( OS_TASK_PRIORITY_NORMAL )
__RETAINED static OS_TASK usb_cdc_task_handle;
__RETAINED static uint8 run_usb_task/* = 0*/;
static USB_HOOK UsbpHook;
static char usb_cdc_buf[USB_MAX_PACKET_SIZE];
//
//  Information that is used during enumeration.
//
static const USB_DEVICE_INFO _DeviceInfo = {
        0x2DCF,         // VendorId
        0x6002,         // ProductId
        "Dialog Semiconductor",       // VendorName
        "DA1468x/DA1510x CDC",   // ProductName
        "12345678"      // SerialNumber.
};

/*********************************************************************
 *
 *       _AddCDC
 *
 *  Function description
 *    Add communication device class to USB stack
 */
static USB_CDC_HANDLE _AddCDC(void)
{
        static U8 _abOutBuffer[USB_MAX_PACKET_SIZE];
        USB_CDC_INIT_DATA InitData;
        USB_CDC_HANDLE hInst;

        InitData.EPIn = USBD_AddEP(USB_DIR_IN, USB_TRANSFER_TYPE_BULK, 0, NULL, 0);
        InitData.EPOut = USBD_AddEP(USB_DIR_OUT, USB_TRANSFER_TYPE_BULK, 0, _abOutBuffer,
                USB_MAX_PACKET_SIZE);
        InitData.EPInt = USBD_AddEP(USB_DIR_IN, USB_TRANSFER_TYPE_INT, 8, NULL, 0);
        hInst = USBD_CDC_Add(&InitData);

        return hInst;
}

void usb_cdc_state_cb(void * pContext, U8 NewState)
{
        int OldState = USBD_GetState();

        if (((OldState & USB_STAT_ATTACHED) == 0) && (NewState & USB_STAT_ATTACHED)) {
                //Attached
        }

        if ((OldState & USB_STAT_ATTACHED) && ((NewState & USB_STAT_ATTACHED) == 0)) {
                //Detached
        }

        if (((OldState & USB_STAT_READY) == 0) && (NewState & USB_STAT_READY)) {
                //Ready
        }

        if ((OldState & USB_STAT_READY) && ((NewState & USB_STAT_READY) == 0)) {
                //Un-Ready
        }

        if (((OldState & USB_STAT_ADDRESSED) == 0) && (NewState & USB_STAT_ADDRESSED)) {
                //Addressed
        }

        if ((OldState & USB_STAT_ADDRESSED) && ((NewState & USB_STAT_ADDRESSED) == 0)) {
                //Un-Addressed
        }

        if (((OldState & USB_STAT_CONFIGURED) == 0) && (NewState & USB_STAT_CONFIGURED)) {
                //Configured
        }

        if ((OldState & USB_STAT_CONFIGURED) && ((NewState & USB_STAT_CONFIGURED) == 0)) {
                //Un-Configured
        }

        if (((OldState & USB_STAT_SUSPENDED) == 0) && (NewState & USB_STAT_SUSPENDED)) {
                // USB is going to be Suspended - DO NOT USE THIS POINT TO TRIGGER APP CODE!
                debug_print("USB Node State: Suspend (o:%d, n:%d)!\r\n", OldState, NewState);
        }

        if ((OldState & USB_STAT_SUSPENDED) && ((NewState & USB_STAT_SUSPENDED) == 0)) {
                // USB is going to be Resumed - DO NOT USE THIS POINT TO TRIGGER APP CODE!
                debug_print("USB Node State: Resume (o:%d, n:%d)!\r\n", OldState, NewState);
        }
}

/*********************************************************************
 *
 *       usb_is_suspended
 *
 *  Function description
 *    Callback to indicate that the USB Node is going to be suspended
 */
void usb_is_suspended(void)
{
        debug_print("App: USB Suspend!\r\n", 1);
}

/*********************************************************************
 *
 *       usb_is_resumed
 *
 *  Function description
 *    Callback to indicate that the USB Node is going to be resumed
 */
void usb_is_resumed(void)
{
        debug_print("App: USB Resume!\r\n", 1);
}

/*********************************************************************
 *
 *       usb_cdc_task
 *
 */
void usb_cdc_task(void *params)
{
        USB_CDC_HANDLE hInst;
        int NumBytesReceived;
#if dg_configUSE_WDOG
        int8_t wdog_id;

        /* register usb_cdc_task task to be monitored by watchdog */
        wdog_id = sys_watchdog_register(false);
#endif

        USBD_Init();
        USBD_CDC_Init();
        USBD_RegisterSCHook(&UsbpHook, usb_cdc_state_cb, NULL);
        hInst = _AddCDC();
        USBD_SetDeviceInfo(&_DeviceInfo);
        USBD_Start();

        while (1) {
#if dg_configUSE_WDOG
                /* notify watchdog on each loop */
                sys_watchdog_notify(wdog_id);
#endif

                //
                // Wait for configuration
                //
                while ((USBD_GetState() & (USB_STAT_CONFIGURED | USB_STAT_SUSPENDED))
                        != USB_STAT_CONFIGURED) {
                        OS_DELAY(50);
                }

#if dg_configUSE_WDOG
                /* suspend watchdog while blocking on USBD_CDC_Receive */
                sys_watchdog_suspend(wdog_id);
#endif
                //
                // Receive at maximum of sizeof(usb_cdc_buf) Bytes
                // If less data has been received,
                // this should be OK.
                //
                NumBytesReceived = USBD_CDC_Receive(hInst, usb_cdc_buf, sizeof(usb_cdc_buf), 0);
#if dg_configUSE_WDOG
                /* resume watchdog */
                sys_watchdog_notify_and_resume(wdog_id);
#endif

                if (NumBytesReceived > 0) {
                        USBD_CDC_Write(hInst, usb_cdc_buf, NumBytesReceived, 0);
                }
        }
}

void usb_cdc_start()
{
        OS_BASE_TYPE status;

        /* Start the USB CDC application task. */
        status = OS_TASK_CREATE("UsbCdcTask",   /* The text name assigned to the task, for
                                                   debug only; not used by the kernel. */
                        usb_cdc_task,           /* The function that implements the task. */
                        NULL,                   /* The parameter passed to the task. */
                        512,                    /* The number of bytes to allocate to the
                                                   stack of the task. */
                        usb_main_TASK_PRIORITY, /* The priority assigned to the task. */
                        usb_cdc_task_handle);   /* The task handle. */

        OS_ASSERT(status == OS_TASK_CREATE_SUCCESS);
}

void usb_cdc_stop()
{
        USBD_UnregisterSCHook(&UsbpHook);
        OS_TASK_DELETE(usb_cdc_task_handle);
        USBD_DeInit();
}

/*********************************************************************
 *
 *       usb_start_enumeration_cb
 *
 *  Function description
 *    Event callback called from the usbcharger task to notify
 *    the application about to allow enumeration.
 *    Note: The USB charger task is started before the application task. Thus, these
 *          call-backs may be called before the application task is started.
 *          The application code should handle this case, if need be.
 */
void usb_start_enumeration_cb(void)
{
        if (run_usb_task == 0) {
                pm_stay_alive();
                run_usb_task = 1;
                cm_sys_clk_set(sysclk_PLL96);
                hw_usb_init();
                hw_usb_bus_attach();
                usb_cdc_start();
        }

}
/*********************************************************************
 *
 *       usb_detach_cb
 *
 *  Function description
 *    Event callback called from the usbcharger task to notify
 *    the application that a detach of the USB cable was detected.
 *
 *    Note: The USB charger task is started before the application task. Thus, these
 *          call-backs may be called before the application task is started.
 *          The application code should handle this case, if need be.
 */
void usb_detach_cb(void)
{
        if (run_usb_task == 1) {
                hw_usb_bus_detach();
                usb_cdc_stop();
                cm_sys_clk_set(sysclk_XTAL16M);
                run_usb_task = 0;
                pm_resume_sleep();
        }
}
