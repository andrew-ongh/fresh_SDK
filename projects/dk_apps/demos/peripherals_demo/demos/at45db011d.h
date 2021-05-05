/**
 ****************************************************************************************
 *
 * @file at45db011d.h
 *
 * @brief API to access at45db011 flash memory
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef AT45DB011_H_
#define AT45DB011_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Commands for serial flash memory.
 */
#define AT45DB_MAIN_MEMORY_PAGE_READ        0xD2
#define AT45DB_CONTINUOUS_ARRAY_READ        0x0B
#define AT45DB_BUFFER_READ                  0xD4

#define AT45DB_BUFFER_WRITE                 0x84
#define AT45DB_PAGE_PROGRAM_THROUGH_BUFFER  0x82
#define AT45DB_BUFFER_PAGE_PROGRAM          0x83

#define AT45DB_PAGE_ERASE                   0x81
#define AT45DB_BLOCK_ERASE                  0x50
#define AT45DB_SECTOR_ERASE                 0x7C
#define AT45DB_CHIP_ERASE                   0x80
#define AT45DB_MEMORY_2_BUFFER              0x53
#define AT45DB_MEMORY_2_BUFFER_COMPARE      0x60
#define AT45DB_READ_STATUS_REGISTER         0xD7
#define AT45DB_MANUFACTURER_AND_ID          0x9F


/* Device has 256 byte page */
#define AT45DB_STATUS_PAGE_SIZE_256         0x01
/* Device is ready for next command */
#define AT45DB_STATUS_READY                 0x80
/* Most recent buffer memory compare result */
#define AT45DB_STATUS_COMP                  0x40

/**
 * \brief Read AT45DB011 status register
 *
 * \param [in] dev flash device returned from spi_open()
 *
 * \return value of status register
 *
 */
uint8_t at45db_read_status_register(spi_device dev);

/**
 * \brief Read AT45DB011 memory
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr address in flash to read from
 * \param [out] buf buffer to fill with data from flash
 * \param [in] size number of bytes to read
 *
 */
void at45db_read(spi_device dev, uint16_t addr, uint8_t *buf, uint16_t size);

/**
 * \brief Read AT45DB011 memory asynchronously
 *
 * This function block for 5 command bytes then it returns. User callback is executed in interrupt
 * context when all data was received.
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr address in flash to read from
 * \param [out] buf buffer to fill with data from flash
 * \param [in] size number of bytes to read
 * \param [in] cb function to call when read is finished
 * \param [in] user_data argument to pass to \p cb
 *
 */
void at45db_read_async(spi_device dev, uint16_t addr, uint8_t *buf, uint16_t size,
                                                                ad_spi_user_cb cb, void *user_data);

/**
 * \brief Write AT45DB011 memory
 *
 * This is complex function that will execute several SPI transaction, with this function
 * user does not have to worry about erasing memory before write. Even single byte can be written.
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr address in flash to write to
 * \param [in] buf buffer write
 * \param [in] size number of bytes to write
 *
 */
void at45db_write(spi_device dev, uint32_t addr, const uint8_t *buf, uint16_t size);

/**
 * \brief Erase AT45DB011 page
 *
 * \note After calling this function memory is busy for some time.
 * Use at45db_read_status_register() to check if busy flag is off before accessing memory again.
 * Address in any address in page to erase.
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr address of page to erase
 *
 * \sa at45db_read_status_register
 *
 */
void at45db_erase_page(spi_device dev, uint32_t addr);

/**
 * \brief Write AT45DB011 page
 *
 * This function will write data to internal buffer first, then it will write contents of buffer
 * to flash main memory. Refer to AT45DB011 reference to see working details.
 * If details are not important use \sa at45db_write() that will take care of all cases.
 *
 * \note After calling this function memory is busy for some time.
 * Use at45db_read_status_register() to check if busy flag is off before accessing memory again.
 * \note if number of bytes to write is bigger than page size programming will wrap so first data
 * will be overwritten. If starting address does not point to start of page wrapping can also occur
 * even if size is less than page size.
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr starting address of page to program
 * \param [in] buf data to write
 * \param [in] size number of bytes to write
 *
 * \sa at45db_read_status_register
 *
 */
void at45db_write_page(spi_device dev, uint32_t addr, const uint8_t *buf, uint16_t size);

/**
 * \brief Write AT45DB011 page asynchronously
 *
 * This function will write data to internal buffer first, then it will write contents of buffer
 * to flash main memory. Refer to AT45DB011 reference to see working details.
 * This function is intended to write single page only.
 * Use synchronous \sa at45db_write() in case of unaligned or multi-page writes.
 * Function is synchronous while sending 4 command bytes, then it returns and caller can wait for
 * callback after all data pointed by \p buf was sent.
 *
 * \note After calling this function memory is busy for some time.
 * Use at45db_read_status_register() to check if busy flag is off before accessing memory again.
 * \note if number of bytes to write is bigger than page size programming will wrap so first data
 * will be overwritten. If starting address does not point to start of page wrapping can also occur
 * even if size is less than page size.
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr starting address of page to program
 * \param [in] buf data to write
 * \param [in] size number of bytes to write
 * \param [in] cb function to call when read is finished
 * \param [in] user_data argument to pass to \p cb
 *
 * \sa at45db_read_status_register
 *
 */
void at45db_write_page_async(spi_device dev, uint32_t addr, const uint8_t *buf, uint16_t size,
                                                                ad_spi_user_cb cb, void *user_data);

/**
 * \brief Wait while flash is busy
 *
 * This is convenience function that can be used to wait for flash memory to finish erase, write
 * or any other long lasting operations.
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr starting address of page to program
 * \param [in] buf data to write
 * \param [in] size number of bytes to write
 *
 * \sa at45db_read_status_register
 *
 */
void at45db_wait_while_busy(spi_device dev);

/**
 * \brief Read main flash memory to flash data buffer
 *
 * Flash has internal ram buffer that is used during writes. It's possible to modify fragment
 * of a page by first reading flash memory to this internal buffer, then writing some part of
 * buffer and then writing modified buffer to flash.
 * This is more efficient than reading whole page through SPI modifying it on processor and then
 * sending whole page back to flash.
 *
 * \param [in] dev flash device returned from spi_open()
 * \param [in] addr starting address of page to program
 *
 * \sa spi_open
 *
 */
void at45db_read_memory_to_buffer(spi_device dev, uint32_t addr);

#ifdef __cplusplus
}
#endif

#endif /* AT45DB011_H_ */
