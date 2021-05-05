/**
 ****************************************************************************************
 *
 * @file UsbApp.c
 *
 * @brief Usb application for for DA1680 USB driver.
 *
 * Copyright (C) 2015-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

/*========================== Include files ==================================*/

#include <string.h>

#include "osal.h"
#include "hw_gpio.h"
#include "hw_usb.h"
#include "UsbApp.h"
#include "UsbAppVars.h"
#include "sys_charger.h"
#include "sys_power_mgr.h"

/*========================== Local macro definitions ========================*/

#define ATTACH_BIT      1
#define DETACH_BIT      2
#define FULL_BIT_MASK   0xFFFFFFFF


/*========================== Global definitions =============================*/

PRIVILEGED_DATA void* HpContext;
PRIVILEGED_DATA static sleep_mode_t SleepMode;

/*========================== Local function prototypes ======================*/

static void usb_enable_app(uint32 context);
static void usb_disable_app(uint32 context);
static void usb_start(void);
static void usb_disable(void);

void usb_init(void);


/*========================== Local data definitions =========================*/

PRIVILEGED_DATA static OS_TASK xUsbInterruptTask;
PRIVILEGED_DATA static OS_TASK xUsbAttachTask;

PRIVILEGED_DATA static HW_USB_DEV_FRAMEWORK_STATE usb_device_state;
PRIVILEGED_DATA static hw_usb_device_framework_ctrl_req_t usb_setup;
PRIVILEGED_DATA static uint8 usb_setup_data[3]; 	//must be 3 for SAMPLING_FREQ_CONTROL
PRIVILEGED_DATA static uint8 *ep0_buffer;

PRIVILEGED_DATA static uint16 class_idle_state;

static OS_EVENT usb_interrupt_event = NULL;

/*==========================================================================================*/
/*========================== Custom APP API Function definitions ===========================*/
/*======================================== START ===========================================*/
/*==========================================================================================*/


/**
 * \brief Once USB interface is configured, the application may begin using it
 *        any additional resources initializations (like queues) to be used from USB
 *        by the application must be placed here (if any)
 *
 * \param[in] context The context (unused).
 *
 */
static void usb_enable_app(uint32 context)
{
        // ADD HERE ANY APP SPECIFIC INITIALIZATIONS LIKE QUEUES AND FLAGS IF NEEDED

        hw_usb_ep_rx_enable(USB_EP_BULK_RX);

        return;
}

/**
 * \brief The USB interface is not configured, so the application must not use it.
 *        it is advised to use some Flags to keep track of the USB interface configuration
 *
 * \param[in] context The context (unused).
 *
 */
static void usb_disable_app(uint32 context)
{
        // ADD HERE ANY APP SPECIFIC DE-INITIALIZATIONS LIKE EMPTY QUEUES AND FLAGS IF NEEDED
        return;
}

/**
 * \brief This is where the RX buffer is returned by the USB driver.
 * \param[in] buffer The pointer to the buffer containing the data.
 * \param[in] size The size of the data in the buffer. Max RX size is USB_BUFFER_SIZE
 *
 *
 */
static void AppUSBRxData(uint8* buffer, uint16 size)
{
        // ADD HERE APP SPECIFIC ACTIONS UPON RX COMPLETION
        // For example to make the RX independent from other tasks use FreeRTOS Queues to add the new buffer
        // and then Notify the data processing task without blocking the next USB RX
        // This function will be called automatically upon Data reception on Bulk EP

        hw_usb_ep_rx_enable(USB_EP_BULK_RX);

        return;
}


/**
 * \brief Call this to send any buffer up to USB_BUFFER_SIZE on Bulk out
 *        The function will not wait for the data to me transmitted physically on the USB to return
 *        When data transmission will be finished the AppUSBTxDataDone() will be called to indicate
 *        the end of transmission
 * \param[in] user_context	void pointer for use from APP code (not in use)
 * \param[in] data uint8 pointer to the buffer to sent
 * \param[in] size uint16 with the length of the buffer to send. Max size is USB_BUFFER_SIZE
 * *
 */
uint16 AppUSBTxData(void* user_context, uint8* data, uint16 size)
{
        if (size > USB_BUFFER_SIZE) size = USB_BUFFER_SIZE;

        if (data != NULL && size > 0) {
                memcpy(usb_tx_buffer, data, size);
                hw_usb_ep_tx_start(USB_EP_BULK_TX, data, size);
        }

        return size;
}

/**
 * \brief This is called to indicate the completion of a transmission on Bulk out
 *
 */
void AppUSBTxDataDone(uint8* buffer)
{
        // APP specific code goes here is any need for that

        return;
}

/*==========================================================================================*/
/*========================== Custom APP API Function definitions ===========================*/
/*========================================= END ============================================*/
/*==========================================================================================*/


/*========================== Function definitions ===========================*/


/**
 * \brief Get the endpoint ID from endpoint address.
 *
 * \param[in] ep_addr The Endpoint address.
 *
 * \return The endpoint ID.
 *
 */
static USB_Endpoint_Id_Type GetEpId(uint8 ep_addr)
{
        return (USB_Endpoint_Id_Type) (ep_addr & ~HW_USB_DEVICE_FRAMEWORK_DIR_IN);
}

/**
 * \brief Transmit data on endpoint zero.
 *
 * \param[in] data The buffer holding the data.
 * \param[in] size The size of the data in the buffer.
 *
 */
static void SendEp0Data(uint8* data, uint8 size)
{
        ep0_buffer = OS_MALLOC(size);

        if (ep0_buffer) {
                memcpy(ep0_buffer, data, size);
                hw_usb_ep_tx_start(USB_EP_DEFAULT, ep0_buffer, size);
        }
        else {
                hw_usb_ep0_stall();
        }
}

/**
 * \brief Convert char string to wide-char, and send.
 *
 * \param[in] str The string.
 *
 * \warning Note that this is not a real ansi-to-wide conversion, just an extension of each
 *        character to 16-bit.
 */
static void SendEp0String(const char* str)
{
        uint8 len = strlen(str);
        uint8 size = 2 + (2 * len);

        ep0_buffer = OS_MALLOC(size);

        if (ep0_buffer) {
                uint8* pb = ep0_buffer;
                const char* ps = str;
                uint8 i;

                *pb++ = size;
                *pb++ = HW_USB_DEVICE_FRAMEWORK_DT_STRING;

                for (i = 0; i < len; i++) {
                        *pb++ = *ps++;
                        *pb++ = 0;
                }

                hw_usb_ep_tx_start(USB_EP_DEFAULT, ep0_buffer,
                                MIN(size, usb_setup.length));
        }
        else {
                hw_usb_ep0_stall();
        }
}


/**
 * \brief Handle USB GET STATUS request.
 *
 */
static void GetStatusReq(void)
{
        uint16 v = 0;

        if (usb_device_state == USB_STATE_ADDRESS
                        || usb_device_state == USB_STATE_CONFIGURED) {
                switch (usb_setup.request_type & HW_USB_DEVICE_FRAMEWORK_RECIP_MASK) {
                case HW_USB_DEVICE_FRAMEWORK_RECIP_DEVICE:
                        SendEp0Data((uint8*) &v, 2);
                        return;

                case HW_USB_DEVICE_FRAMEWORK_RECIP_ENDPOINT:
                {
                        USB_Endpoint_Id_Type EpNr = GetEpId(usb_setup.index);

                        if (EpNr >= USB_EP_MAX) {
                                break;
                        }

                        if (hw_usb_ep_is_stalled(EpNr)) {
                                v = 1;
                        }
                }
                SendEp0Data((uint8*) &v, 2);
                return;
                }
        }

        hw_usb_ep0_stall();
}

/**
 * \brief Handle USB CLEAR FEATURE request.
 *
 */
static void ClearFeatureReq(void)
{
        if (usb_device_state == USB_STATE_ADDRESS
                        || usb_device_state == USB_STATE_CONFIGURED) {
                switch (usb_setup.request_type & HW_USB_DEVICE_FRAMEWORK_RECIP_MASK) {
                case HW_USB_DEVICE_FRAMEWORK_RECIP_ENDPOINT:
                {
                        USB_Endpoint_Id_Type EpNr = GetEpId(usb_setup.index);

                        if (EpNr >= USB_EP_MAX) {
                                break;
                        }

                        if (usb_setup.value != HW_USB_DEVICE_FRAMEWORK_ENDPOINT_HALT) {
                                break;
                        }

                        hw_usb_ep_unstall(EpNr);
                }
                hw_usb_ep_tx_start(USB_EP_DEFAULT, NULL, 0);
                return;
                }
        }
        hw_usb_ep0_stall();
}

/**
 * \brief Handle USB SET FEATURE request.
 *
 */
static void SetFeatureReq(void)
{
        if (usb_device_state == USB_STATE_ADDRESS
                        || usb_device_state == USB_STATE_CONFIGURED) {
                switch (usb_setup.request_type & HW_USB_DEVICE_FRAMEWORK_RECIP_MASK) {
                case HW_USB_DEVICE_FRAMEWORK_RECIP_ENDPOINT:
                {
                        USB_Endpoint_Id_Type EpNr = GetEpId(usb_setup.index);

                        if (EpNr >= USB_EP_MAX) {
                                break;
                        }

                        if (usb_setup.value != HW_USB_DEVICE_FRAMEWORK_ENDPOINT_HALT) {
                                break;
                        }

                        hw_usb_ep_stall(EpNr);
                }
                hw_usb_ep_tx_start(USB_EP_DEFAULT, NULL, 0);
                return;
                }
        }
        hw_usb_ep0_stall();
}

/**
 * \brief Handle USB SET ADDRESS request.
 *
 */
static void SetAddressReq(void)
{
        if ((usb_device_state == USB_STATE_DEFAULT) || (usb_device_state == USB_STATE_ADDRESS)) {
                hw_usb_bus_address(usb_setup.value);
                hw_usb_ep_tx_start(USB_EP_DEFAULT, NULL, 0);
                usb_device_state = USB_STATE_ADDRESS;
                usb_disable_app(0);

                return;
        }
        hw_usb_ep0_stall();
}

/**
 * \brief Handle USB GET DESCRIPTOR request.
 *
 */
static void GetDescriptorReq(void)
{
        uint16 size;

        switch (usb_setup.value >> 8) {
        case HW_USB_DEVICE_FRAMEWORK_DT_DEVICE:
                SendEp0Data((uint8*) &DeviceDescriptor,
                                MIN(sizeof(DeviceDescriptor), usb_setup.length));
                return;

        case HW_USB_DEVICE_FRAMEWORK_DT_CONFIG:
                size = sizeof(ConfigDescriptor) + SIZE_OF_CONFIGURATION;
                ep0_buffer = OS_MALLOC(size);
                if (ep0_buffer == NULL) {
                        break;
                }

                // Create full configuration descriptor.
                {
                        uint8 *pb = ep0_buffer;
                        uint8 i;

                        // Config descriptor.
                        memcpy(pb, &ConfigDescriptor, sizeof(ConfigDescriptor));

                        ((hw_usb_device_framework_conf_descriptor_t*) pb)->total_length = size;
                        pb += sizeof(ConfigDescriptor);

                        // HID interfaces.
                        for (i = 0; i < NUM_OF_INTERFACES; i++) {
                                memcpy(pb, &Interface0, sizeof(Interface0));
                                ((hw_usb_device_framework_if_descriptor_t*) pb)->interface_number = i;
                                pb += sizeof(Interface0);

                                memcpy(pb, &BulkEpTx, sizeof(BulkEpTx));
                                ((hw_usb_device_framework_ep_descriptor_t*) pb)->endpoint_address =
                                        HW_USB_DEVICE_FRAMEWORK_DIR_IN | (USB_EP_BULK_TX + (2 * i));
                                pb += sizeof(BulkEpTx);

                                memcpy(pb, &BulkEpRx, sizeof(BulkEpRx));
                                ((hw_usb_device_framework_ep_descriptor_t*) pb)->endpoint_address =
                                        HW_USB_DEVICE_FRAMEWORK_DIR_OUT | (USB_EP_BULK_RX + (2 * i));
                                pb += sizeof(BulkEpRx);
                        }
                }

                // Send it.
                hw_usb_ep_tx_start(USB_EP_DEFAULT, ep0_buffer, MIN(size, usb_setup.length));
                return;

        case HW_USB_DEVICE_FRAMEWORK_DT_STRING:
                switch (usb_setup.value & 0xFF) {
                case 0:
                        SendEp0Data((uint8*) UsbLanguageId,
                                        MIN(sizeof(UsbLanguageId), usb_setup.length));
                        return;

                case MANUFACTURER_STR_INDEX:
                        SendEp0String(UsbManufacturerString);
                        return;

                case PRODUCT_STR_INDEX:
                        SendEp0String(UsbProductString);
                        return;

                case CONFIGURATION_STR_INDEX:
                        SendEp0String(UsbConfigurationfString);
                        return;

                case SERIAL_STR_INDEX:
                        SendEp0String(UsbSerialNumberString);
                        return;

                case IFACE_STR_INDEX:
                        SendEp0String(UsbInterfaceString);
                        return;

                case MSFT_OS_STR_INDEX:
                        SendEp0Data((uint8*) UsbMSOSString,
                                        MIN(UsbMSOSString[0], usb_setup.length));
                        return;
                }
                break;

                case HW_USB_DEVICE_FRAMEWORK_DT_DEVICE_QUALIFIER:

                        break;
        }
        hw_usb_ep0_stall();
}

/**
 * \brief Handle USB GET CONFIGURATION request.
 *
 */
static void GetConfigurationReq(void)
{
        if ((usb_device_state == USB_STATE_ADDRESS) || (usb_device_state == USB_STATE_CONFIGURED)) {
                uint8 v = 0;

                if (usb_device_state == USB_STATE_CONFIGURED) {
                        v = 1;
                }

                SendEp0Data(&v, 1);
                return;
        }
        hw_usb_ep0_stall();
}

/**
 * \brief Handle USB SET CONFIGURATION request.
 *
 */
static void SetConfigurationReq(void)
{
        if ((usb_device_state == USB_STATE_ADDRESS) || (usb_device_state == USB_STATE_CONFIGURED)) {
                if (usb_setup.value <= DeviceDescriptor.num_configurations) {
                        uint8 i;

                        if (usb_setup.value) {
                                for (i = 2; i < USB_EP_MAX; i += 2) {
                                        hw_usb_ep_rx_enable(i);
                                }

                                usb_device_state = USB_STATE_CONFIGURED;
                                usb_enable_app(0);
                        }
                        else {
                                for (i = 1; i < USB_EP_MAX; i++) {
                                        hw_usb_ep_disable(i, true);
                                }

                                usb_device_state = USB_STATE_ADDRESS;
                                usb_disable_app(0);
                        }
                        hw_usb_ep_tx_start(USB_EP_DEFAULT, NULL, 0);

                        usb_charger_connected(dg_configBATTERY_CHARGE_CURRENT);

                        return;
                }
        }
        hw_usb_ep0_stall();
}



/**
 * \brief Handle USB standard request.
 *
 */
static void HandleStandardReq(void)
{
        switch (usb_setup.request) {
        case HW_USB_DEVICE_FRAMEWORK_REQ_GET_STATUS:
                GetStatusReq();
                break;

        case HW_USB_DEVICE_FRAMEWORK_REQ_CLEAR_FEATURE:
                ClearFeatureReq();
                break;

        case HW_USB_DEVICE_FRAMEWORK_REQ_SET_FEATURE:
                SetFeatureReq();
                break;

        case HW_USB_DEVICE_FRAMEWORK_REQ_SET_ADDRESS:
                SetAddressReq();
                break;

        case HW_USB_DEVICE_FRAMEWORK_REQ_GET_DESCRIPTOR:
                GetDescriptorReq();
                break;

        case HW_USB_DEVICE_FRAMEWORK_REQ_GET_CONFIGURATION:
                GetConfigurationReq();
                break;

        case HW_USB_DEVICE_FRAMEWORK_REQ_SET_CONFIGURATION:
                SetConfigurationReq();
                break;

        default:
                hw_usb_ep0_stall();
                break;
        }
}



static void MSGetWinusbCompIDFeatureReq(void)
{
        switch (usb_setup.value & 0xFF) {
        case MS_WINUSB:
                if (usb_setup.index == MS_COMPATIBLE_ID_FEATURE)
                {
                        SendEp0Data((uint8*) UsbMSOSCompatibleID,
                                        MIN(UsbMSOSCompatibleID[0], usb_setup.length));
                }
                break;

        default:
                hw_usb_ep0_stall();
                break;
        }
}

/**
 * \brief Handle USB VENDOR request.
 *
 */
static void HandleVendorReq(void)
{
        switch (usb_setup.request) {
        case MS_WINUSB_COMP_ID_FEATURE:
                MSGetWinusbCompIDFeatureReq();
                break;

        default:
                hw_usb_ep0_stall();
                break;
        }
}


/**
 * \brief Handle class GET IDLE request.
 *
 */
static void ClassGetIdleReq(void)
{
        if (usb_device_state == USB_STATE_CONFIGURED) {
                uint8 v = class_idle_state >> 8;

                SendEp0Data(&v, 1);
                return;
        }
        hw_usb_ep0_stall();
}

/**
 * \brief Handle class SET IDLE request.
 *
 */
static void ClassSetIdleReq(void)
{
        if (usb_device_state == USB_STATE_CONFIGURED) {
                class_idle_state = usb_setup.value;
                hw_usb_ep_tx_start(USB_EP_DEFAULT, NULL, 0);
                return;
        }
        hw_usb_ep0_stall();
}

/**
 * \brief Handle USB class specific request.
 *
 */
static void HandleClassReq(void)
{
        switch (usb_setup.request) {

        case UR_GET_IDLE:
                ClassGetIdleReq();
                break;

        case UR_SET_IDLE:
                ClassSetIdleReq();
                break;

        default:
                hw_usb_ep0_stall();
                break;
        }
}

void hw_usb_bus_event(usb_bus_event_type event)
{
        uint8 i;

        switch (event) {
        case UBE_SUSPEND:
        case UBE_RWKUP_OK:
                if (usb_device_state == USB_STATE_CONFIGURED) {
                        usb_device_state = USB_STATE_SUSPENDED;
                        for (i = 0; i < USB_EP_MAX; i++) {
                                hw_usb_ep_disable(i, false);
                        }
                        usb_disable_app(0);
                }
                break;

        case UBE_RESUME:
                if (usb_device_state == USB_STATE_SUSPENDED) {
                        usb_device_state = USB_STATE_CONFIGURED;
                        hw_usb_ep_rx_enable(USB_EP_DEFAULT);
                }
                else {
                        usb_device_state = USB_STATE_POWERED;
                }
                break;

        case UBE_RESET:
                if (usb_device_state != USB_STATE_DEFAULT) {
                        usb_device_state = USB_STATE_DEFAULT;
                        for (i = 0; i < USB_EP_MAX; i++) {
                                hw_usb_ep_disable(i, true);
                        }
                        hw_usb_bus_address(0);
                        hw_usb_ep_rx_enable(USB_EP_DEFAULT);
                        usb_disable_app(0);
                }
                break;
        default:
                break;
        }
}

void hw_usb_bus_frame(uint16 frame_nr)
{
	return;
}

void hw_usb_ep_nak(uint8 ep_nr)
{
        if (ep_nr == USB_EP_DEFAULT) {
                hw_usb_ep_disable(USB_EP_DEFAULT, false);
                hw_usb_ep_rx_enable(USB_EP_DEFAULT);
        }
}

uint8* hw_usb_ep_get_rx_buffer(uint8 ep_nr, bool is_setup, uint16* buffer_size)
{
        if (ep_nr == USB_EP_DEFAULT) {
                if (is_setup) {
                        *buffer_size = sizeof(usb_setup);
                        return (uint8*) &usb_setup;
                }

                // SETUP with OUT data stage.
                if (usb_setup.length <= sizeof(usb_setup_data)) {
                        *buffer_size = usb_setup.length;
                        return usb_setup_data;
                }
                hw_usb_ep0_stall();
                return NULL;
        }

        if (ep_nr == USB_EP_BULK_RX) {
                *buffer_size = USB_BUFFER_SIZE;
                return usb_rx_buffer;
        }

        return NULL;
}

bool hw_usb_ep_rx_read_by_driver(uint8 ep_nr)
{
        switch (ep_nr) {

        default:
                return true;
        }
}

bool hw_usb_ep_rx_done(uint8 ep_nr, uint8* buffer, uint16 size)
{
        if (ep_nr == USB_EP_DEFAULT) {
                if (size == 0) {
                        return true;
                }

                if (buffer == (uint8*) &usb_setup) {
                        if ((usb_setup.request_type & HW_USB_DEVICE_FRAMEWORK_DIR_IN) == 0) {
                                if (usb_setup.length) {
                                        if (usb_setup.length > sizeof(usb_setup_data)) {
                                                hw_usb_ep0_stall();
                                                return true;
                                        }
                                        // Wait for OUT data.
                                        return true;
                                }
                        }
                }

                switch (usb_setup.request_type & HW_USB_DEVICE_FRAMEWORK_TYPE_MASK) {
                case HW_USB_DEVICE_FRAMEWORK_TYPE_STANDARD:
                        HandleStandardReq();
                        break;

                case HW_USB_DEVICE_FRAMEWORK_TYPE_CLASS:
                        HandleClassReq();
                        break;

                case HW_USB_DEVICE_FRAMEWORK_TYPE_VENDOR:
                        HandleVendorReq();
                        break;

                case HW_USB_DEVICE_FRAMEWORK_TYPE_RESERVED:
                        /* Should never request this, it's RESERVED */
                        /* Code should never reach this point as this is a standard WinSYS (WinUSB) device
                         * This is not a fatal error though
                         */
                        ASSERT_WARNING(0);
                        break;

                default:
                        hw_usb_ep0_stall();
                        break;
                }
        }

        if (ep_nr == USB_EP_BULK_RX) {
                AppUSBRxData(buffer, size);
                return false;
        }

        return false;
}

void hw_usb_ep_tx_done(uint8 ep_nr, uint8* buffer)
{
        if (ep_nr == USB_EP_DEFAULT) {
                if (buffer && buffer == ep0_buffer) {
                        OS_FREE(ep0_buffer);
                        ep0_buffer = NULL;
                }
                hw_usb_ep_rx_enable(USB_EP_DEFAULT);
        }

        if (ep_nr == USB_EP_BULK_TX) {
                AppUSBTxDataDone(buffer);
        }
}

/**
 * \brief Disable USB.
 *
 */
static void usb_disable(void)
{
        hw_usb_disable();
}

/**
 * \brief FreeRTOS task for handling USB interrupts.
 *
 * \param[in] pvParameter FreeRTOS task parameter.
 *
 */
static void UsbInterruptTaskFunction(void *pvParameter)
{
        for (;;) {
                if (OS_EVENT_WAIT(usb_interrupt_event, OS_EVENT_FOREVER)) {
                        hw_usb_interrupt_handler();
                        REG_SET_BIT(USB, USB_MAMSK_REG, USB_M_INTR);
                }
        }
}

/**
 * \brief FreeRTOS task for attaching / detaching the device to / from the bus.
 *
 * \param[in] pvParameter FreeRTOS task parameter.
 *
 */
void UsbAttachTaskFunction(void *pvParameter)
{
        sys_clk_t system_clock = sysclk_LP;
        uint32_t ulNotifiedValue;

        while (1) {
                OS_TASK_NOTIFY_WAIT(pdFALSE, FULL_BIT_MASK, &ulNotifiedValue,
                                OS_TASK_NOTIFY_FOREVER);

                if (ulNotifiedValue & ATTACH_BIT) {
                        system_clock = cm_sys_clk_get();
                        cm_sys_clk_set(sysclk_PLL96);

                        SleepMode = pm_get_sleep_mode();
                        pm_set_sleep_mode(pm_mode_active);

                        usb_start();
                }

                if (ulNotifiedValue & DETACH_BIT) {
                        usb_disable();
                        pm_set_sleep_mode(SleepMode);
                        if (system_clock != sysclk_LP) {
                                cm_sys_clk_set(system_clock);
                        }
                }
        }
}

/**
 * \brief Initialize USB FreeRTOS tasks.
 *
 */
static void UsbRtosInit(uint8 task_priority)
{
        OS_EVENT_CREATE(usb_interrupt_event);

        OS_TASK_CREATE("UsbIH",
                        UsbInterruptTaskFunction,
                        NULL,
                        1024,
                        task_priority,
                        xUsbInterruptTask);

        OS_TASK_CREATE("UsbAt",
                        UsbAttachTaskFunction,
                        NULL,
                        256,
                        USB_ATTACH_PRIORITY,
                        xUsbAttachTask);
}

void usb_init(void)
{
        UsbRtosInit(USB_INTERRPT_TASK_PRIO);

        hw_usb_disable_interrupt();
}

/**
 * \brief Start the USB function.
 *
 */
static void usb_start(void)
{
        usb_device_state = USB_STATE_NOTATTACHED;

        // Initialize hardware
        hw_usb_init();

        // Endpoint 0 (default control EP).
        hw_usb_ep_configure(USB_EP_DEFAULT, true, NULL);

        // BULK endpoints.
        hw_usb_ep_configure(USB_EP_BULK_TX, false,
                        (hw_usb_device_framework_ep_descriptor_t*) &BulkEpTx);

        hw_usb_ep_configure(USB_EP_BULK_RX, false,
                        (hw_usb_device_framework_ep_descriptor_t*) &BulkEpRx);

        // Now that everything is ready, announce device presence to the USB host.
        usb_device_state = USB_STATE_ATTACHED;
        hw_usb_bus_attach();

        hw_usb_enable_interrupt();
}

void USB_Handler(void)
{
        OS_BASE_TYPE xResult;

        REG_CLR_BIT(USB, USB_MAMSK_REG, USB_M_INTR);

        xResult = OS_EVENT_SIGNAL_FROM_ISR(usb_interrupt_event);
        OS_ASSERT(xResult);//Check if signaling was OK
}

void usb_start_enumeration_cb(void)
{
        OS_TASK_NOTIFY(xUsbAttachTask, ATTACH_BIT, OS_NOTIFY_SET_BITS);
}

void usb_detach_cb(void)
{
        OS_TASK_NOTIFY(xUsbAttachTask, DETACH_BIT, OS_NOTIFY_SET_BITS);
}

// End of file.
