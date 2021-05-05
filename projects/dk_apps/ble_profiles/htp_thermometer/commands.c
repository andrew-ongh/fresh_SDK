/**
 ****************************************************************************************
 *
 * @file commands.c
 *
 * @brief Command handlers API
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include "ble_gap.h"
#include "commands.h"
#include "htp_thermometer_task.h"
#include "htp_thermometer_config.h"

static void clicmd_not_recognized_command(int argc, const char *argv[])
{
        int i;

        printf("Command: ");

        for (i = 0; i < argc; i++) {
                printf("%s ", argv[i]);
        }

        printf("is not recognized\r\n\r\n");
}

static void clicmd_user_usage(bool print_usage)
{
        if (print_usage) {
                printf("Usage:\r\n");
        }

        printf("\tinterval <value>\r\n");
}

static void clicmd_set_interval(int argc, const char *argv[], void *user_data)
{
        uint16_t interval;

        if (argc < 2) {
                clicmd_not_recognized_command(argc, argv);
                clicmd_user_usage(true);
                return;
        }

        if (!strcasecmp("interval", argv[0])) {
                interval = atoi(argv[1]);

                if ((interval < CFG_HTS_MEAS_INTERVAL_LOW_BOUND ||
                                interval > CFG_HTS_MEAS_INTERVAL_HIGH_BOUND) && interval != 0) {
                        printf("Interval value out of boundaries <%d, %d>\r\n",
                                                                CFG_HTS_MEAS_INTERVAL_LOW_BOUND,
                                                                CFG_HTS_MEAS_INTERVAL_HIGH_BOUND);
                        return;
                }

                handle_interval_value(interval);
                printf("Measurement Interval set to: %d seconds\r\n", interval);
        } else {
                clicmd_not_recognized_command(argc, argv);
                clicmd_user_usage(true);
        }
}

static void clicmd_default_handler(int argc, const char *argv[], void *user_data)
{
        int i;

        printf("Invalid command: ");

        for (i = 0; i < argc; i++) {
                printf("%s", argv[i]);
        }

        printf("\r\n");

        printf("Valid commands:\r\n");
        clicmd_user_usage(false);
}

static const cli_command_t clicmd[] = {
        { .name = "interval",   .handler = clicmd_set_interval, .user_data = NULL},
        { NULL },
};

cli_t register_command_handlers(uint32_t notif_mask)
{
        return cli_register(notif_mask, clicmd, clicmd_default_handler);
}
