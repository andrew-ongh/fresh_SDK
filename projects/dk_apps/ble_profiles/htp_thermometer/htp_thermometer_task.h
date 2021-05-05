/**
 ****************************************************************************************
 *
 * @file htp_thermometer_task.h
 *
 * @brief Command handlers API
 *
 * Copyright (C) 2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>

#ifndef HTP_THERMOMETER_TASK_H_
#define HTP_THERMOMETER_TASK_H_

/**
 * \brief Check interval value and do proper action. Inform if interval has new value.
 *
 * \param [in] interval                 Measurement Interval value
 *
 * \return ATT_ERROR_OK if interval value was set correctly
 *
 */
att_error_t handle_interval_value(uint16_t interval);

#endif /* HTP_THERMOMETER_TASK_H_ */
