/**
 ****************************************************************************************
 *
 * @file usb_handler.c
 *
 * @brief USB report handers functions
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stdint.h>

/**
 * Handle GET_REPORT request
 *
 * This function is called when GET_REPORT reuqest is received from USB host. It can be handled
 * internally or passed further to BLE device.
 *
 * \param [in]  has_ble true if BLE device is available, false otherwise
 * \param [in]  type    report type
 * \param [in]  id      report id
 * \param [out] length  report data length
 * \param [out] data    report data
 *
 * \return == 0 request is passed further to BLE (if connected)
 *         != 0 request is handled here and will not be passed to BLE
 *
 */
int usb_on_get_report(bool has_ble, uint8_t type, uint8_t id, uint32_t *length, void *data)
{
        /*
         * We pass all data to BLE - output parameters are ignored in this case. If BLE is not
         * available, caller will reply with zero-length packet.
         *
         * If some special handling is required, application can probide proper reply by setting
         * length and data output parameters and returning non-zero value.
         */
        return 0;
}

/*
 * Handle SET_REPORT request
 *
 * This function is called when SET_REPORT request is received from USB host. It can be handled
 * internally or passed further to BLE device.
 *
 * \param [in]  has_ble true if BLE device is available, false otherwise
 * \param [in]  type    report type
 * \param [in]  id      report id
 * \param [in]  length  report data length
 * \param [in]  data    report data
 *
 * \return == 0 request is passed further to BLE (if connected)
 *         != 0 request is handled here and will not be passed to BLE
 *
 */
int usb_on_set_report(bool has_ble, uint8_t type, uint8_t id, uint32_t length, void *data)
{
        return 0;
}

/*
 * Handle data output
 *
 * This function is called when data is written from USB host. It can be handled internally or
 * passed further to BLE device.
 *
 * \param [in]  has_ble true if BLE device is available, false otherwise
 * \param [in]  type    report type
 * \param [in]  id      report id
 * \param [in]  length  report data length
 * \param [in]  data    report data
 *
 * \return == 0 request is passed further to BLE (if connected)
 *         != 0 request is handled here and will not be passed to BLE
 *
 */
int usb_on_data(bool has_ble, uint8_t type, uint8_t id, uint32_t length, void *data)
{
        return 0;
}
