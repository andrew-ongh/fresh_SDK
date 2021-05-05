/**
 ****************************************************************************************
 *
 * @file eeprom_24xx256.h
 *
 * @brief API to access 24xx256 EEPROM
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef EEPROM_24XX256_H_
#define EEPROM_24XX256_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Read 24xx256 memory
 *
 * \param [in] dev flash device returned from i2c_open()
 * \param [in] addr address in flash to read from
 * \param [out] buf buffer to fill with data from flash
 * \param [in] size number of bytes to read
 *
 * \return 0 on success, otherwise error value from enum HW_I2C_ABORT_SOURCE
 *
 * \sa i2c_open
 */
int eeprom_24XX256_read(i2c_device dev, uint16_t addr, uint8_t *buf, uint16_t size);

/**
 * \brief Read 24xx256 memory asynchronously
 *
 * \param [in] dev flash device returned from i2c_open()
 * \param [in] addr address in flash to read from
 * \param [out] buf buffer to fill with data from EEPROM
 * \param [in] size number of bytes to read
 * \param [in] cb function to call when read is finished
 * \param [in] user_data argument to pass to \p cb
 *
 * \sa i2c_open
 *
 */
void eeprom_24XX256_read_async(i2c_device dev, uint16_t addr, uint8_t *buf, uint16_t size,
                                                                ad_i2c_user_cb cb, void *user_data);

/**
 * \brief Write 24xx256 memory
 *
 * This is complex function that will execute several I2C transactions if needed to allow
 * linear access to EEPROM.
 *
 * \param [in] dev flash device returned from i2c_open()
 * \param [in] addr address in flash to write to
 * \param [in] buf buffer write
 * \param [in] size number of bytes to write
 *
 * \return true on success false otherwise
 *
 * \sa i2c_open
 *
 */
bool eeprom_24XX256_write(i2c_device dev, uint32_t addr, const uint8_t *buf, uint16_t size);

/**
 * \brief Write 24xx256 memory asynchronously
 *
 * This function is suitable to write single page to EEPROM, write will roll over if data exceeds
 * page size.
 * Error condition if any will be notified in user callback \p cb.
 *
 * \param [in] dev flash device returned from i2c_open()
 * \param [in] addr address in flash to write to
 * \param [in] buf buffer write
 * \param [in] size number of bytes to write
 * \param [in] cb function to call when write is finished
 * \param [in] user_data argument to pass to \p cb
 *
 * \sa i2c_open
 */
void eeprom_24XX256_write_async(i2c_device dev, uint32_t addr, const uint8_t *buf,
                                                uint8_t size, ad_i2c_user_cb cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_24XX256_H_ */
