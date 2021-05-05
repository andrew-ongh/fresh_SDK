/**
 ****************************************************************************************
 *
 * @file wsp_weightscale_config.h
 *
 * @brief Application configuration
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef WSP_WEIGHTSCALE_CONFIG_H_
#define WSP_WEIGHTSCALE_CONFIG_H_

#define CFG_MULTIPLE_CLIENTS    (0)     // allow couple of users to connect
#define CFG_UDS_MAX_USERS       (3)     // max number of users in database for UDS
#define CFG_MAX_MEAS_TO_STORE   (25)    // max number of measurements to store for each client who
                                        // is registered and have a proper consent, according to
                                        // specification server should store at least 25 data meas
#endif /* WSP_WEIGHTSCALE_CONFIG_H_ */
