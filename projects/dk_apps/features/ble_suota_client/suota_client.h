/**
 ****************************************************************************************
 *
 * @file suota_client.h
 *
 * @brief SUOTA 1.2 client header file
 *
 * Copyright (C) 2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef SUOTA_CLIENT_H
#define SUOTA_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include "ble_client.h"

/** Client events */
typedef enum {
        SUOTA_CLIENT_EVENT_STATUS_NOTIF = 0x01, /* SUOTA Status characteristic notifications */
} suota_client_event_t;

/** Remote server capabilities */
typedef enum {
        SUOTA_CLIENT_CAP_L2CAP_PSM = 0x01,              /* L2CAP CoC dynamic PSM value characteristic present */
        SUOTA_CLIENT_CAP_SUOTA_VERSION = 0x02,          /* SUOTA version characteristic present */
} suota_client_cap_t;

/** Target memory device type */
typedef enum {
        SUOTA_CLIENT_MEM_DEV_I2C_EEPROM = 0x12, /* EEPROM connected via I2C */
        SUOTA_CLIENT_MEM_DEV_SPI_FLASH = 0x13,  /* Flash memory connected via (Q)SPI */
} suota_client_mem_dev_t;

typedef void (* suota_client_set_event_state_completed_cb_t) (ble_client_t *client,
                                                                        suota_client_event_t event,
                                                                        att_error_t status);

typedef void (* suota_client_get_event_state_completed_cb_t) (ble_client_t *client,
                                                                        suota_client_event_t event,
                                                                        att_error_t status,
                                                                        bool enabled);

typedef void (* suota_client_read_l2cap_psm_completed_cb_t) (ble_client_t *client,
                                                                att_error_t status, uint16_t psm);

typedef void (* suota_client_get_suota_version_completed_cb_t) (ble_client_t *client,
                                                                att_error_t status, uint8_t suota_version);

typedef void (* suota_client_get_patch_data_char_size_completed_cb_t) (ble_client_t *client,
                                                                att_error_t status, uint16_t patch_data_char_size);

typedef void (* suota_client_generic_write_completed_cb_t) (ble_client_t *client, att_error_t status);

typedef void (* suota_client_status_notif_cb_t) (ble_client_t *client, uint8_t status);

typedef struct {
        suota_client_set_event_state_completed_cb_t             set_event_state_completed;
        suota_client_get_event_state_completed_cb_t             get_event_state_completed;

        suota_client_read_l2cap_psm_completed_cb_t              read_l2cap_psm_completed;
        suota_client_get_suota_version_completed_cb_t           get_suota_version_completed;
        suota_client_get_patch_data_char_size_completed_cb_t    get_patch_data_char_size_completed;
        suota_client_generic_write_completed_cb_t               set_patch_len_completed;
        suota_client_generic_write_completed_cb_t               send_patch_data_completed;

        suota_client_generic_write_completed_cb_t               set_mem_dev_completed;
        suota_client_generic_write_completed_cb_t               send_reboot_cmd_completed;
        suota_client_generic_write_completed_cb_t               send_end_cmd_completed;
        suota_client_generic_write_completed_cb_t               send_abort_cmd_completed;

        suota_client_status_notif_cb_t                          status_notif;
} suota_client_callbacks_t;

/**
 * \brief Initialize SUOTA client
 *
 * This function should be called when SUOTA service is browsed to get client instance which will be
 * used to access SUOTA functionality on remote server.
 *
 * \param [in] cb   application callbacks
 * \param [in] evt  browsed service event
 *
 * \return client instance
 *
 */
ble_client_t *suota_client_init(const suota_client_callbacks_t *cb,
                                                        const ble_evt_gattc_browse_svc_t *evt);

/**
 * \brief Get SUOTA client capabilities
 *
 * \param [in] client  client instance
 *
 * \return client capabilities bitmask
 *
 */
suota_client_cap_t suota_client_get_capabilities(ble_client_t *client);

/**
 * \brief Enable/disable GATT event
 *
 * This function enables/disables GATT event (i.e. notification or indication) on remote by writing
 * the CCC descriptor of the appropriate characteristic (specified by \p event).
 *
 * \p suota_client_callbacks_t::set_event_state_completed callback is called when operation is
 * finished.
 *
 * \param [in] client  client instance
 * \param [in] event   event identifier
 * \param [in] enable  new state of event
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_set_event_state(ble_client_t *client, suota_client_event_t event, bool enable);

/**
 * \brief Read GATT event state
 *
 * This function reads current state of GATT event (i.e. notification or indication) on remote by
 * reading the CCC descriptor of the appropriate characteristic (specified by \p event).
 *
 * \p suota_client_callbacks_t::get_event_state_completed callback is called when operation is
 * finished.
 *
 * \param [in] client  client instance
 * \param [in] event   event identifier
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_get_event_state(ble_client_t *client, suota_client_event_t event);

/**
 * \brief Read dynamic PSM value for L2CAP CoC transfer
 *
 * This function reads the dynamic PSM value of the L2CAP CoC transfer, if remote supports it. It
 * should be only called when client has \p SUOTA_CLIENT_CAP_L2CAP_PSM capability.
 *
 * \p suota_client_callbacks_t::read_l2cap_psm_completed callback is called when operation is
 * finished.
 *
 * \param [in] client  client instance
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_read_l2cap_psm(ble_client_t *client);

/**
 * \brief Read SUOTA version
 *
 * This function reads SUOTA version from the remote server, if the specific characteristic exists.
 *
 * \p suota_client_callbacks_t::get_suota_version_completed callback is called when operation is
 * finished.
 *
 * \param [in] client  client instance
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_get_suota_version(ble_client_t *client);

/**
 * \brief Read the size of the patch data characteristic
 *
 * This function reads the size of the patch data characteristic from the remote server, if the specific characteristic exists.
 *
 * \p suota_client_callbacks_t::get_patch_data_char_size_completed callback is called when operation is
 * finished.
 *
 * \param [in] client  client instance
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_get_patch_data_char_size(ble_client_t *client);

/**
 * \brief Select memory device for storing image on remote
 *
 * \note Only SUOTA_CLIENT_MEM_DEV_SPI_FLASH is supported by SUOTA server versions 1.1 & 1.2 at the
 * moment.
 *
 * \param [in] client        client instance
 * \param [in] dev           memory device to select
 * \param [in] base_address  base address to write image to
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_set_mem_dev(ble_client_t *client, suota_client_mem_dev_t dev, uint32_t base_address);

/**
 * \brief Set patch chunk length to be used during transfer
 *
 * This functions shall be used before starting image data transfer to remote server to inform of
 * the length of data chunk that will be sent before waiting for acknowledgment from server.
 *
 * Application is responsible for adjusting this value depending on amount of data to be sent. To
 * achieve best compatibility with Android/iOS application patch length should be set to 240 and
 * then adjusted to lower value for last data chunk.
 *
 * \note This is only applicable when using GATT for SUOTA image transfer
 *
 * \param [in] client     client instance
 * \param [in] patch_len  new patch chunk len
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_set_patch_len(ble_client_t *client, uint16_t patch_len);

/**
 * \brief Send image data to remote server
 *
 * This function shall be used to send image data to remote server. It is application responsibility
 * to make sure that the amount of data sent does not exceed the value set by suota_client_set_patch_len().
 *
 * \note This is only applicable if using GATT for SUOTA image transfer
 *
 * \param [in] client  client instance
 * \param [in] length  data length
 * \param [in] data    data
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_send_patch_data(ble_client_t *client, size_t length, const void *data);

/**
 * \brief Send "SUOTA reboot" command to remote
 *
 * This function should be used after SUOTA process is completed to reboot remote.
 *
 * \p suota_client_callbacks_t::send_reboot_cmd_completed callback is called when operation is finished.
 *
 * \param [in] client  client instance
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_send_reboot_cmd(ble_client_t *client);

/**
 * \brief Send "SUOTA end" command to remote
 *
 * This function should be used to indicate that image transfer has been completed.
 *
 * \p suota_client_callbacks_t::send_end_cmd_completed callback is called when operation is finished.
 *
 * \param [in] client  client instance
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_send_end_cmd(ble_client_t *client);

/**
 * \brief Send "SUOTA abort" command to remote
 *
 * This function can be used to abort ongoing SUOTA process.
 *
 * \p suota_client_callbacks_t::send_abort_cmd_completed callback is called when operation is finished.
 *
 * \param [in] client  client instance
 *
 * \return true if operation started successfully, false otherwise
 *
 */
bool suota_client_send_abort_cmd(ble_client_t *client);

#endif /* SUOTA_CLIENT_H */
