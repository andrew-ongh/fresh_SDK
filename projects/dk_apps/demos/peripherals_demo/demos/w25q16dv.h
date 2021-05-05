/**
 ****************************************************************************************
 *
 * @file w25q16dv.h
 *
 * @brief API to access W25Q16DV flash memory
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef W25Q16DV_H_
#define W25Q16DV_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Manufacturer ID
 */
#define W25Q16DV_MANUFACTURER        0xEF

/*
 * Device chip ID
 */
#define W25Q16DV_DEVICE_ID           0x14

/*
 * Device ID as returned by w25q_read_id()
 */
#define W25Q16DV_ID                  (((W25Q16DV_DEVICE_ID) << 8) | W25Q16DV_MANUFACTURER)

/*
 * Commands for serial flash memory with dual and quad spi
 */
#define W25Q_WRITE_ENABLE            0x06
#define W25Q_WRITE_DISABLE           0x04
#define W25Q_READ_STATUS_REGISTER1   0x05
#define W25Q_READ_STATUS_REGISTER2   0x35
#define W25Q_WRITE_STATUS_REGISTER   0x01
#define W25Q_PAGE_PROGRAM            0x02
#define W25Q_QUAD_PAGE_PROGRAM       0x32
#define W25Q_SECTOR_ERASE            0x20
#define W25Q_BLOCK_ERASE             0x52
#define W25Q_BLOCK_ERASE_64K         0xD8
#define W25Q_CHIP_ERASE              0xC7
#define W25Q_ERASE_PROGRAM_SUSPEND   0x75
#define W25Q_ERASE_PROGRAM_RESUME    0x7A

#define W25Q_READ_DATA               0x03
#define W25Q_FAST_READ               0x0B
#define W25Q_FAST_READ_DUAL_OUTPUT   0x3B
#define W25Q_FAST_READ_QUAD_OUTPUT   0x6B
#define W25Q_FAST_READ_DUAL_IO       0xBB
#define W25Q_FAST_READ_QUAD_IO       0xEB
#define W25Q_WORD_READ_QUAD_IO       0xE7
#define W25Q_OCTAL_WORD_READ_QUAD_IO 0xE7
#define W25Q_SET_BURST_WITH_WRAP     0x77
#define W25Q_RELEASE_POWER_DOWN      0xAB
#define W25Q_MANUFACTURER_ID         0x90
#define W25Q_ENABLE_RESET            0x66
#define W25Q_RESET                   0x99
#define W25Q_POWER_DOWN              0xB9
#define W25Q_RELEASE_POWER_DOWN      0xAB

/* Erase/Write in progress */
#define W25Q_STATUS1_BUSY            0x01
/* Write enable latch */
#define W25Q_STATUS1_WEL             0x02

/* QUAD enable (not volatile) */
#define W25Q_STATUS2_QE              0x02

/**
 * \brief Read W25Q16DV status register 1
 *
 * \return value of status register 1
 *
 */
uint8_t w25q_read_status_register_1(void);

/**
 * \brief Read W25Q16DV status register 2
 *
 * \return value of status register 2
 *
 */
uint8_t w25q_read_status_register_2(void);

/**
 * \brief Set write enable bit in W25Q16DV
 *
 * This function must be called before any erase or page program.
 *
 */
void w25q_write_enable(void);

/**
 * \brief Read W25Q16DV memory
 *
 * \param [in] addr address in flash to read from
 * \param [out] buf buffer to fill with data from flash
 * \param [in] size number of bytes to read
 *
 */
void w25q_read(uint32_t addr, uint8_t *buf, uint16_t size);

/**
 * \brief Erase W25Q16DV sector (4kB)
 *
 * \note After calling this function memory is busy for some time.
 * Use w25q_read_status_register_1() to check if busy flag is off before accessing memory again.
 *
 * \param [in] addr starting address of sector to erase
 *
 * \sa w25q_read_status_register_1
 *
 */
void w25q_erase_sector(uint32_t addr);

/**
 * \brief Write W25Q16DV page (256 bytes)
 *
 * \note After calling this function memory is busy for some time.
 * Use w25q_read_status_register_1() to check if busy flag is off before accessing memory again.
 * \note if number of bytes to write is bigger than 256 programming will wrap so first data will be
 * overwritten. If starting address does not point to start of page wrapping can also occur even
 * if size is less than 256.
 *
 * \param [in] addr starting address of page to program
 * \param [in] buf data to write
 * \param [in] size number of bytes to write
 *
 * \sa w25q_read_status_register_1
 *
 */
void w25q_write_page(uint32_t addr, const uint8_t *buf, uint16_t size);

/**
 * \brief Enable quad mode in W25Q16DV
 *
 * This will send command to flash changing purpose of lines D2 D3 to bidirectional data lines.
 * If this command is not send D2 is treated as hardware write protect line, D3 is used as HOLD
 * line see W25Q16DV documentation for details.
 *
 */
void w25q_quad_enable(void);

/**
 * \brief Exit W25Q16DV continuous read mode
 *
 * Continuous read mode allows to send only one read command (which is transmitted in single bus
 * mode). In this mode every data send after chip select is activated is treated as address.
 * Sending reset sequence tells flash to read commands again.
 *
 */
void w25_reset_continuous_mode(void);

/**
 * \brief Enter W25Q16DV power down mode
 *
 */
void w25q_power_down(void);

/**
 * \brief Release W25Q16DV power down mode
 *
 */
void w25q_release_power_down(void);

/**
 * \brief Read W25Q16DV manufacturer and device IDs
 *
 * \return should return \p W25Q16DV_ID if device is powered up, 0 when it's not
 *
 */
uint16_t w25q_read_id(void);

#ifdef __cplusplus
}
#endif

#endif /* W25Q16DV_H_ */
