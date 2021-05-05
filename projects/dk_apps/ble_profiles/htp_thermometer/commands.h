/**
 ****************************************************************************************
 *
 * @file commands.h
 *
 * @brief Command handlers API
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

#include "cli.h"

/**
 * \brief Register command handlers
 *
 * \param [in] notif_mask       bit mask for task notification
 *
 * \return CLI instance
 *
 */
cli_t register_command_handlers(uint32_t notif_mask);

#endif /* COMMANDS_H_ */
