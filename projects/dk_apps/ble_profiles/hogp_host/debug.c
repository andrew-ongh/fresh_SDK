/**
 ****************************************************************************************
 *
 * @file debug.c
 *
 * @brief Debug utilities
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include "osal.h"
#include "ble_client.h"
#include "hids_client.h"
#include "hogp_host_task.h"
#include "debug.h"

typedef void (* debug_callback_t) (client_t *client, int argc, const char **argv);

/**
 * Get Protocol debug command handler. Command should be in format: "hogp get_protocol <client_id>"
 */
static void hogp_get_protocol_cb(client_t *client, int argc, const char **argv)
{
        bool status;

        printf("Get protocol mode\r\n");
        status = hids_client_get_protocol_mode(client->client);
        printf("\tStatus: %s\r\n", status ? "success" : "failure");
}

/**
 * Read Boot debug command handler. Command should be in format:
 * "hogp boot_read <client_id> <boot_report_type>"
 */
static void hogp_boot_read_cb(client_t *client, int argc, const char **argv)
{
        hids_client_boot_report_type type;
        bool status;

        if (argc < 2) {
                printf("Report type is not provided\r\n");
                return;
        }

        type = atoi(argv[1]);

        printf("Boot report read request\r\n");
        printf("\tReport type: %d\r\n", type);
        status = hids_client_boot_report_read(client->client, type);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Enable Notifications for Boot Report debug command handler. Command should be in format:
 * "hogp boot_notif <client_id> <boot_report_type> <enable>"
 */
static void hogp_boot_notif_cb(client_t *client, int argc, const char **argv)
{
        hids_client_boot_report_type type;
        bool status, enable;

        if (argc < 3) {
                printf("Missing arguments\r\n");
                return;
        }

        type = atoi(argv[1]);
        enable = atoi(argv[2]);

        printf("Boot report set notif state\r\n");
        printf("\tAction: %s notifications\r\n", enable ? "Register for" : "Unregister");
        printf("\tReport type provided: %d\r\n", type);
        status = hids_client_boot_report_set_notif_state(client->client, type, enable);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Read CCC of Boot Report debug command handler. Command should be in format:
 * "hogp boot_read_ccc <client_id> <boot_report_type>"
 */
static void hogp_boot_read_ccc_cb(client_t *client, int argc, const char **argv)
{
        hids_client_boot_report_type type;
        bool status;

        if (argc < 2) {
                printf("Report type is not provided\r\n");
                return;
        }

        type = atoi(argv[1]);

        printf("Boot report get notif state\r\n");
        printf("\tReport type provided: %d\r\n", type);
        status = hids_client_boot_report_get_notif_state(client->client, type);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Helper converting string in format "0ae34d..." to uint8_t array
 */
static int str_to_hex(const char *str, uint8_t *buf, int buf_size)
{
        int str_len;
        int i, j;
        char c;
        uint8_t b;

        str_len = strlen(str);

        if (str_len % 2)
                return -1;

        for (i = 0, j = 0; i < buf_size && j < str_len; i++, j++) {
                c = str[j];

                if (c >= 'a' && c <= 'f')
                        c += 'A' - 'a';

                if (c >= '0' && c <= '9')
                        b = c - '0';
                else if (c >= 'A' && c <= 'F')
                        b = 10 + c - 'A';
                else
                        return 0;

                j++;

                c = str[j];

                if (c >= 'a' && c <= 'f')
                        c += 'A' - 'a';

                if (c >= '0' && c <= '9')
                        b = b * 16 + c - '0';
                else if (c >= 'A' && c <= 'F')
                        b = b * 16 + 10 + c - 'A';
                else
                        return 0;

                buf[i] = b;
        }

        return i;
}

/**
 * Write Boot Report debug command handler. Command should be in format:
 * "hogp boot_write <client_id> <boot_report_type> <data in hex>"
 */
static void hogp_boot_write_cb(client_t *client, int argc, const char **argv)
{
        hids_client_boot_report_type type;
        uint16_t data_length;
        uint8_t data[32];
        bool status;

        if (argc < 3) {
                printf("Missing arguments\r\n");
                return;
        }

        type = atoi(argv[1]);
        data_length = str_to_hex(argv[2], data, sizeof(data));
        if (data_length == 0) {
                printf("No data to write\r\n");
                return;
        }

        printf("Boot report write\r\n");
        printf("\tReport type provided: %d\r\n", type);
        printf("\tRequire response: true\r\n");
        printf("\tValue length: %d\r\n", data_length);
        status = hids_client_boot_report_write(client->client, type, true, data_length, data);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Read Report debug command handler. Command should be in format:
 * "hogp report_read <client_id> <report_type> <report_id>"
 */
static void hogp_report_read_cb(client_t *client, int argc, const char **argv)
{
        hids_client_report_type_t type;
        uint8_t report_id;
        bool status;

        if (argc < 3) {
                printf("Missing arguments\r\n");
                return;
        }

        type = atoi(argv[1]);
        report_id = atoi(argv[2]);

        printf("Report read\r\n");
        printf("\tReport type provided: %d\r\n", type);
        printf("\tReport Id provided: %d\r\n", report_id);
        status = hids_client_report_read(client->client, type, report_id);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Enable Input Report Notifications debug command handler. Command should be in format:
 * "hogp report_notif <client_id> <report_id> <enable>"
 */
static void hogp_report_notif_cb(client_t *client, int argc, const char **argv)
{
        bool status, enable;
        uint8_t report_id;

        if (argc < 3) {
                printf("Missing arguments\r\n");
                return;
        }

        report_id = atoi(argv[1]);
        enable = atoi(argv[2]);

        printf("Report set notif state\r\n");
        printf("\tAction: %s notifications\r\n", enable ? "Register for" : "Unregister");
        printf("\tReport Id provided: %d\r\n", report_id);
        status = hids_client_input_report_set_notif_state(client->client, report_id, enable);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Write Report debug command handler. Command should be in format:
 * hogp report_write <client_id> <report_type> <report_id> <response> <data in hex>"
 */
static void hogp_report_write_cb(client_t *client, int argc, const char **argv)
{
        hids_client_report_type_t type;
        uint8_t report_id;
        bool status, response;
        uint16_t data_length;
        uint8_t data[32];

        if (argc < 5) {
                printf("Missing arguments\r\n");
                return;
        }

        type = atoi(argv[1]);
        report_id = atoi(argv[2]);
        response = atoi(argv[3]);
        data_length = str_to_hex(argv[4], data, sizeof(data));
        if (data_length == 0) {
                printf("No data to write\r\n");
                return;
        }

        printf("Report write\r\n");
        printf("\tReport type provided: %d\r\n", type);
        printf("\tReport Id provided: %d\r\n", report_id);
        printf("\tRequire response: %s\r\n", response ? "true" : "false");
        printf("\tReport length: %d\r\n", data_length);
        status = hids_client_report_write(client->client, type, report_id, response, data_length,
                                                                                        data);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Write Control Point debug command handler. Command should be in format:
 * hogp cp_command <client_id> <command>"
 */
static void hogp_cp_command_cb(client_t *client, int argc, const char **argv)
{
        hids_client_cp_command_t command;
        bool status;

        if (argc < 2) {
                printf("Missing command\r\n");
                return;
        }

        command = atoi(argv[1]);

        printf("Control command write\r\n");
        printf("\tCommand provided: %d\r\n", command);
        status = hids_client_cp_command(client->client, command);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

/**
 * Read Input Report CCC descriptor. Command should be in format:
 * hogp report_read_ccc <client_id> <report_id>
 */
static void hogp_report_read_ccc_cb(client_t *client, int argc, const char **argv)
{
        uint8_t report_id;
        bool status;

        if (argc < 2) {
                printf("Missing report Id\r\n");
                return;
        }

        report_id = atoi(argv[1]);

        printf("Report get notif state\r\n");
        printf("\tReport Id provided: %d\r\n", report_id);
        status = hids_client_input_report_get_notif_state(client->client, report_id);
        printf("\tRequest status: %s\r\n", status ? "success" : "failure");
}

static void hogp_connect_cb(int argc, const char **argv, void *user_data)
{
        hogp_connect();
}

static void hogp_disconnect_cb(int argc, const char **argv, void *user_data)
{
        hogp_disconnect();
}

static void hids_client_cb(int argc, const char *argv[], void *user_data)
{
        debug_callback_t callback = user_data;
        uint8_t client_id;
        client_t *client;

        if (argc < 2) {
                printf("Invalid arguments\r\n");
                return;
        }

        client_id = atoi(argv[1]);
        client = get_client(client_id, CLIENT_TYPE_HIDS);

        if (!client) {
                printf("Client with Id: %d not found\r\n", client_id);
                return;
        }

        callback(client, argc - 1, (argc == 1 ? NULL : &argv[1]));
}

INITIALISED_PRIVILEGED_DATA static cli_command_t debug_handlers[] = {
        { "get_protocol",       hids_client_cb,         hogp_get_protocol_cb },
        { "boot_read",          hids_client_cb,         hogp_boot_read_cb },
        { "boot_notif",         hids_client_cb,         hogp_boot_notif_cb },
        { "boot_read_ccc",      hids_client_cb,         hogp_boot_read_ccc_cb },
        { "boot_write",         hids_client_cb,         hogp_boot_write_cb },
        { "report_read",        hids_client_cb,         hogp_report_read_cb },
        { "report_notif",       hids_client_cb,         hogp_report_notif_cb },
        { "report_read_ccc",    hids_client_cb,         hogp_report_read_ccc_cb },
        { "report_write",       hids_client_cb,         hogp_report_write_cb },
        { "cp_command",         hids_client_cb,         hogp_cp_command_cb },
        { "connect",            hogp_connect_cb,        NULL },
        { "disconnect",         hogp_disconnect_cb,     NULL },
        { NULL },
};

static void default_handler(int argc, const char **argv, void *user_data)
{
        printf("Unknown command\r\n");
}

cli_t register_debug(uint32_t notif_mask)
{
        return cli_register(notif_mask, debug_handlers, default_handler);
}
