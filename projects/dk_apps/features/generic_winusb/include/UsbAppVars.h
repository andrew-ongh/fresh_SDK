 /**
 ****************************************************************************************
 *
 * @file UsbAppVars.h
 *
 * @brief Header with local variables of the USB application for the DA1680 USB driver.
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef USBAPPVARS_H_
#define USBAPPVARS_H_

#include "hw_usb.h"

/*========================== Local data definitions =========================*/

/* Note: put in here to improve the readability of the code in UsbApp.c as they are too large, even
 *       though this is not a good programming practice. It is done only because the "UsbAppVars.h"
 *       is included only once in the UsbApp.c file.
 */


#define USB_BUFFER_SIZE	2048
uint8 usb_rx_buffer[USB_BUFFER_SIZE];
uint8 usb_tx_buffer[USB_BUFFER_SIZE];

#define UR_GET_IDLE             0x02
#define UR_SET_IDLE             0x0a

/******************************/
/* USB descriptors parameters */
/******************************/
// USB vendor and product ID.
#define USB_VID							0x0ABC
#define USB_PID 						0x0008

// Device class/subclass
#define USB_DT_CLASS					0xFF
#define USB_DT_SUBCLASS					0x00

// USB attributes
#define USB_bmAttr_BusPowered			(1<<7) //Reserved, always set
#define USB_bmAttr_SelfPowered			(1<<6)
#define USB_bmAttr_RemoteWakeUp			(1<<5)

//Microsoft specific IDs
#define MS_OS_STRING_VENODR_CODE		0x10 						//Used in Microsoft OS String Descriptor
#define MS_WINUSB_COMP_ID_FEATURE 		MS_OS_STRING_VENODR_CODE	//Used in Microsoft Compatible ID Descriptor. DO NOT CHANGE THIS

/******************************/
/* Interface parameters       */
/******************************/
// Number of interfaces exposed
#define NUM_OF_INTERFACES				1
//Number of ENDPOINT for IFACE 0
#define USB_IFACE0_NUM_OF_EP			2
//#define NUM_OF_ENDPOINTS				3 //when the INT EP will be added we will have 3 EP
// Size of each EP IO transaction
#define USB_EP_IO_SIZE 					64


//IMPORTANT: Keep the TX/RX EP in that order alternating TX to RX.
typedef enum {
        USB_EP_DEF = USB_EP_DEFAULT,    // Must always be first (zero), don't change.
        USB_EP_BULK_TX,                 // IN
        USB_EP_BULK_RX,                 // OUT
        USB_EP_INT_TX,                  // IN
//        USB_EP_INT_RX,                // OUT
} USB_Endpoint_Id_Type;



//USB strings
const char* UsbProductString = "DA1468x USB WICD";
const char* UsbSerialNumberString = "Data Channel";
const char* UsbConfigurationfString = "DA14681";
const char* UsbInterfaceString = "DA1468x WICD";
const char  UsbMSOSString[] = {0x12, HW_USB_DEVICE_FRAMEWORK_DT_STRING, 'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0, MS_OS_STRING_VENODR_CODE, 0x00};
const char  UsbMSOSCompatibleID[] = {0x28, 0, 0, 0, 0x00, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 'W','I','N','U','S','B',0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const char* UsbManufacturerString = "Dialog Semiconductor";

#define PRODUCT_STR_INDEX		1
#define SERIAL_STR_INDEX		2
#define CONFIGURATION_STR_INDEX	3
#define IFACE_STR_INDEX			4
#define MANUFACTURER_STR_INDEX  5
#define MSFT_OS_STR_INDEX		0xEE //Microsoft specific

//Microsoft specific USB requests
#define	MS_WINUSB					0
#define	MS_COMPATIBLE_ID_FEATURE	0x0004

//Device Descriptor
const hw_usb_device_framework_dev_descriptor_t DeviceDescriptor = {
        {
                sizeof(hw_usb_device_framework_dev_descriptor_t),     // uint8  bLength;
                HW_USB_DEVICE_FRAMEWORK_DT_DEVICE       // uint8  bDescriptorType;
        },
        0x0200,                                         // uint16 bcdUSB;
        USB_DT_CLASS,                                   // uint8  bDeviceClass;
        USB_DT_SUBCLASS,                                // uint8  bDeviceSubClass;
        0x00,                                           // uint8  bDeviceProtocol;
        0x08,                                           // uint8  bMaxPacketSize0;
        USB_VID,                                        // uint16 idVendor;
        USB_PID,                                        // uint16 idProduct;
        0x0100,                                         // uint16 bcdDevice;
        MANUFACTURER_STR_INDEX,                         // uint8  iManufacturer;
        PRODUCT_STR_INDEX,                              // uint8  iProduct;
        SERIAL_STR_INDEX,                               // uint8  iSerialNumber;
        0x01,                                           // uint8  bNumConfigurations;
};

//Configuration Descriptor
const hw_usb_device_framework_conf_descriptor_t ConfigDescriptor = {
        {
                sizeof(hw_usb_device_framework_conf_descriptor_t),     // uint8  bLength;
                HW_USB_DEVICE_FRAMEWORK_DT_CONFIG       // uint8  bDescriptorType;
        },
        0x0BAD,                                         // uint16 wTotalLength;
        NUM_OF_INTERFACES,             		        // uint8  bNumInterfaces;
        0x01,                                           // uint8  bConfigurationValue;
        CONFIGURATION_STR_INDEX,                        // uint8  iConfiguration;
        USB_bmAttr_BusPowered,                          // uint8  bmAttributes;
        0xFA,                                           // uint8  bMaxPower = 500mA;
};

//Interface 0 descriptor
const hw_usb_device_framework_if_descriptor_t Interface0 = {
        {
                sizeof(hw_usb_device_framework_if_descriptor_t),        // uint8  bLength;
                HW_USB_DEVICE_FRAMEWORK_DT_INTERFACE                    // uint8  bDescriptorType;
        },
        0x00,                                                           // uint8  bInterfaceNumber;
        0x00,                                                           // uint8  bAlternateSetting;
        USB_IFACE0_NUM_OF_EP,                                           // uint8  bNumEndpoints;
        0x00,                                  	                        // uint8  bInterfaceClass;
        0x00,                                                           // uint8  bInterfaceSubClass;
        0x00,                                                           // uint8  bInterfaceProtocol;
        IFACE_STR_INDEX                                                 // uint8  iInterface;
};

// TX Bulk EP descriptor (DA --> host)
const hw_usb_device_framework_ep_descriptor_t BulkEpTx = {
        {
                sizeof(hw_usb_device_framework_ep_descriptor_t),        // uint8  bLength;
                HW_USB_DEVICE_FRAMEWORK_DT_ENDPOINT                     // uint8  bDescriptorType;
        },
        HW_USB_DEVICE_FRAMEWORK_DIR_IN | USB_EP_BULK_TX,                // uint8  bEndpointAddress;
        HW_USB_DEVICE_FRAMEWORK_ENDPOINT_XFER_BULK,                     // uint8  bmAttributes;
        USB_EP_IO_SIZE,                                                 // uint16 wMaxPacketSize;
        0x01,                                                           // uint8  bInterval;
};

// RX Bulk EP descriptor (Host --> DA)
const hw_usb_device_framework_ep_descriptor_t BulkEpRx = {
        {
                sizeof(hw_usb_device_framework_ep_descriptor_t),        // uint8  bLength;
                HW_USB_DEVICE_FRAMEWORK_DT_ENDPOINT                     // uint8  bDescriptorType;
        },
        HW_USB_DEVICE_FRAMEWORK_DIR_OUT | USB_EP_BULK_RX,               // uint8  bEndpointAddress;
        HW_USB_DEVICE_FRAMEWORK_ENDPOINT_XFER_BULK,                     // uint8  bmAttributes;
        USB_EP_IO_SIZE,                                                 // uint16 wMaxPacketSize;
        0x01,                                                           // uint8  bInterval;
};


#define SIZE_OF_CONFIGURATION				(NUM_OF_INTERFACES * (sizeof(Interface0) + sizeof(BulkEpTx) + sizeof(BulkEpRx)))


//Language ID descriptor
const uint8 UsbLanguageId[] = { 0x04, HW_USB_DEVICE_FRAMEWORK_DT_STRING, 0x09, 0x04 };

#endif // USBAPPVARS_H_
