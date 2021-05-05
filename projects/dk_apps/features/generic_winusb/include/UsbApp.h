 /**
 ****************************************************************************************
 *
 * @file UsbApp.h
 *
 * @brief header for USB application for the DA1680 USB driver.
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef USBAPP_H_
#define USBAPP_H_

#define USB_INTERRUPT_PRIORITY 	1
#define USB_INTERRPT_TASK_PRIO 	1
#define USB_ATTACH_PRIORITY		2


/**
 * \brief Initialize USB function and driver.
 *
 */
void usb_init(void);

#endif // USBAPP_H_
