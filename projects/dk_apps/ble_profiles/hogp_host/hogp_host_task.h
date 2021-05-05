/**
 ****************************************************************************************
 *
 * @file hogp_host_task.h
 *
 * @brief HOGP Host task header
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

/**
 * Client types
 */
typedef enum {
        CLIENT_TYPE_NONE,
        CLIENT_TYPE_HIDS,
        CLIENT_TYPE_BAS,
        CLIENT_TYPE_SCPS,
        CLIENT_TYPE_GATT,
        CLIENT_TYPE_DIS,
} client_type_t;

/**
 * Client struct
 */
typedef struct {
        void *next;
        uint8_t id;
        client_type_t type;
        ble_client_t *client;
} client_t;

/**
 * Function returns client struct with given type and id
 */
client_t *get_client(uint8_t id, client_type_t type);

void hogp_connect(void);

void hogp_disconnect(void);
