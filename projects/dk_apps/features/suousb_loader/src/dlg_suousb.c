/**
 ****************************************************************************************
 *
 * @file dlg_suousb.c
 *
 * @brief Dialog SUOUSB service implementation
 *
 * Copyright (C) 2016-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "osal.h"
#include "suota.h"
#include "ad_nvms.h"
#include "hw_watchdog.h"
#include "hw_cpm.h"
#include "dlg_suousb.h"

#define FLASH_SECTOR_SIZE       (4096)
#define SUOUSB_MAX_IMAGE_SIZE   (236 * 1024)
#define SUOUSB_BUFFER_SIZE      (512)

typedef enum {
        SUOUSB_STATE_IDLE,
        SUOUSB_STATE_W4_HEADER,
        SUOUSB_STATE_W4_HEADER_EXT,
        SUOUSB_STATE_W4_IMAGE_DATA,
        SUOUSB_STATE_DONE,
        SUOUSB_STATE_ERROR,
} suousb_state_t;

/**
 * SUOUSB status
 *
 * As defined by Dialog SUOTA specification.
 *
 */
typedef enum {
        /* Value zero must not be used !! Notifications are sent when status changes. */
        SUOUSB_SRV_STARTED      = 0x01,     // Valid memory device has been configured by initiator. No sleep state while in this mode
        SUOUSB_CMP_OK           = 0x02,     // SUOUSB process completed successfully.
        SUOUSB_SRV_EXIT         = 0x03,     // Forced exit of SUOUSB service.
        SUOUSB_CRC_ERR          = 0x04,     // Overall Patch Data CRC failed
        SUOUSB_PATCH_LEN_ERR    = 0x05,     // Received patch Length not equal to PATCH_LEN characteristic value
        SUOUSB_EXT_MEM_WRITE_ERR= 0x06,     // External Mem Error (Writing to external device failed)
        SUOUSB_INT_MEM_ERR      = 0x07,     // Internal Mem Error (not enough space for Patch)
        SUOUSB_INVAL_MEM_TYPE   = 0x08,     // Invalid memory device
        SUOUSB_APP_ERROR        = 0x09,     // Application error

        /* SUOUSB application specific error codes */
        SUOUSB_IMG_STARTED      = 0x10,     // SUOUSB started for downloading image (SUOUSB application)
        SUOUSB_INVAL_IMG_BANK   = 0x11,     // Invalid image bank
        SUOUSB_INVAL_IMG_HDR    = 0x12,     // Invalid image header
        SUOUSB_INVAL_IMG_SIZE   = 0x13,     // Invalid image size
        SUOUSB_INVAL_PRODUCT_HDR= 0x14,     // Invalid product header
        SUOUSB_SAME_IMG_ERR     = 0x15,     // Same Image Error
        SUOUSB_EXT_MEM_READ_ERR = 0x16,     // Failed to read from external memory device
} suousb_status_t;

const char *suousb_status_str[] = {
        "INVALID:0",
        "SUOUSB_SRV_STARTED",           //= 0x01,     // Valid memory device has been configured by initiator. No sleep state while in this mode
        "SUOUSB_CMP_OK",                //= 0x02,     // SUOUSB process completed successfully.
        "SUOUSB_SRV_EXIT",              //= 0x03,     // Forced exit of SUOUSB service.
        "SUOUSB_CRC_ERR",               //= 0x04,     // Overall Patch Data CRC failed
        "SUOUSB_PATCH_LEN_ERR",         //= 0x05,     // Received patch Length not equal to PATCH_LEN characteristic value
        "SUOUSB_EXT_MEM_WRITE_ERR",     //= 0x06,     // External Mem Error (Writing to external device failed)
        "SUOUSB_INT_MEM_ERR",           //= 0x07,     // Internal Mem Error (not enough space for Patch)
        "SUOUSB_INVAL_MEM_TYPE",        //= 0x08,     // Invalid memory device
        "SUOUSB_APP_ERROR",             //= 0x09,     // Application error

        "INVALID:A",
        "INVALID:B",
        "INVALID:C",
        "INVALID:D",
        "INVALID:E",
        "INVALID:F",

        /* SUOUSB application specific error codes */
        "SUOUSB_IMG_STARTED",           //= 0x10,     // SUOUSB started for downloading image (SUOUSB application)
        "SUOUSB_INVAL_IMG_BANK",        //= 0x11,     // Invalid image bank
        "SUOUSB_INVAL_IMG_HDR",         //= 0x12,     // Invalid image header
        "SUOUSB_INVAL_IMG_SIZE",        //= 0x13,     // Invalid image size
        "SUOUSB_INVAL_PRODUCT_HDR",     //= 0x14,     // Invalid product header
        "SUOUSB_SAME_IMG_ERR",          //= 0x15,     // Same Image Error
        "SUOUSB_EXT_MEM_READ_ERR",      //= 0x16,     // Failed to read from external memory device

        "INVALID:17",
};

/**
 * SUOUSB commands
 *
 * As defined by Dialog SUOTA specification.
 *
 */
typedef enum {
        /* SUOUSB is used to send entire image */
        SUOUSB_IMG_INT_SYSRAM = 0x10,
        SUOUSB_IMG_INT_RETRAM = 0x11,
        SUOUSB_IMG_I2C_EEPROM = 0x12,
        SUOUSB_IMG_SPI_FLASH  = 0x13,

        /* DO NOT move. Must be before commands */
        SUOUSB_MEM_INVAL_DEV  = 0x14,

        /* SUOUSB commands */
        SUOUSB_REBOOT         = 0xFD,
        SUOUSB_IMG_END        = 0xFE,

        /*
         * When initiator selects 0xff, it wants to exit SUOUSB service.
         * This is used in case of unexplained failures. If SUOUSB process
         * finishes correctly it will exit automatically.
         */
        SUOUSB_MEM_SERVICE_EXIT   = 0xFF,
} suousb_commands_t;

typedef struct suousb_service suousb_service_t;

/** SUOUSB status callback during image transfer */
typedef void (* suousb_error_cb_t) (suousb_service_t *suousb, suousb_status_t status);

/** SUOUSB callback after full chunk is received during image transfer */
typedef void (* suousb_chunk_cb_t) (suousb_service_t *suousb);

typedef struct suousb_service {
        suousb_state_t state;            // image transfer state
        suousb_chunk_cb_t chunk_cb;      // called on every 'patch_len' bytes of data received
        suousb_error_cb_t error_cb;      // called in case of error during image transfer

        uint8_t *buffer;
        uint16_t buffer_len;

        suota_image_header_t header;    // copy of image header

        uint16_t chunk_len;             // length of data received in current chunk (depends on 'patch_len')
        uint32_t image_crc;             // CRC of received image
        uint32_t recv_total_len;        // length of received data
        uint32_t recv_image_len;        // length of received image
        uint32_t flash_write_addr;      // flash address where data will be written to
        uint32_t flash_erase_addr;      // flash address which is not yet erased (assume everything prior to this address is erased)
        uint16_t pending_credits;       // number of credits to give back to app

        uint16_t patch_len;
        suousb_active_img_t active_img;
        nvms_t  nvms;
} suousb_service_t;

typedef struct {
        void *next;
        uint8_t status;
} client_status_notif_t;

static suousb_service_t *suousb_svc;
static suousb_notify_cb_t ext_notify_cb;
static uint32_t suousb_buff_sz;
static uint16_t suousb_external_status;

static const uint32_t crc32_tab[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static inline uint16_t get_u16(const uint8_t *buffer)
{
        return (buffer[0]) | (buffer[1] << 8);
}

static inline uint32_t get_u32(const uint8_t *buffer)
{
        return (buffer[0]) | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
}

static uint32_t get_update_addr(suousb_service_t *suousb)
{
        return 0;
}

static void suousb_notify_client_status(suousb_service_t *suousb, uint8_t status)
{
        if (ext_notify_cb) {
                ext_notify_cb(suousb_status_str[status]);
        }
}

static suousb_error_t do_bl_ccc_write(suousb_service_t *suousb, uint16_t offset,
        uint16_t length, const uint8_t *value)
{
        if (offset) {
                return SUOUSB_ERROR_ATTRIBUTE_NOT_LONG;
        }

        if (length != sizeof(suousb_external_status)) {
                return SUOUSB_ERROR_APPLICATION_ERROR;
        }

        suousb_external_status = get_u16(value);

        return SUOUSB_ERROR_OK;
}

static bool set_active_img_ptr(suousb_service_t *suousb)
{
        int written;

        suousb->header.flags |= SUOTA_1_1_IMAGE_FLAG_VALID;
        written = ad_nvms_write(suousb->nvms, get_update_addr(suousb), (uint8_t *) &suousb->header,
                                                                sizeof(suota_image_header_t));
        return written == sizeof(suota_image_header_t);
}

static bool prepare_flash(suousb_service_t *suousb, size_t write_size)
{
        uint32_t end_addr = suousb->flash_write_addr + write_size - 1;
        size_t erase_size;

        /* If flash is already erased in required range, do nothing */
        if (end_addr < suousb->flash_erase_addr) {
                return true;
        }

        erase_size = end_addr - suousb->flash_erase_addr + 1;

        if (!ad_nvms_erase_region(suousb->nvms, suousb->flash_erase_addr, erase_size)) {
                return false;
        }

        /*
         * Assume everything up to sector boundary is erased now, next erase address is at the
         * beginning of following sector
         */
        suousb->flash_erase_addr += erase_size;
        suousb->flash_erase_addr |= (FLASH_SECTOR_SIZE - 1);
        suousb->flash_erase_addr++;

        return true;
}

static void suousb_error_cb(suousb_service_t *suousb, suousb_status_t status)
{
        OS_ASSERT(0);

        suousb_notify_client_status(suousb, status);
        suousb->state = SUOUSB_STATE_ERROR;
}

static void suousb_chunk_cb(suousb_service_t *suousb)
{
        suousb_notify_client_status(suousb, SUOUSB_CMP_OK);
}

static size_t pull_to_buffer(suousb_service_t *suousb, const uint8_t *data, size_t len,
        size_t expected_len)
{
        /* Caller guarantees that we'll pull data only up to buffer capacity */
        OS_ASSERT(expected_len <= suousb_buff_sz);

        /* We shall *never* have more than expected_len bytes of data in buffer */
        OS_ASSERT(suousb->buffer_len <= expected_len);

        /* Required amount data is already in buffer */
        if (suousb->buffer_len >= expected_len) {
                return 0;
        }

        expected_len -= suousb->buffer_len;
        if (expected_len > len) {
                expected_len = len;
        }

        memcpy(suousb->buffer + suousb->buffer_len, data, expected_len);
        suousb->buffer_len += expected_len;

        return expected_len;
}

static bool suousb_state_w4_header(suousb_service_t *suousb)
{
        memcpy(&suousb->header, suousb->buffer, sizeof(suousb->header));

        if (suousb->header.signature[0] != SUOTA_1_1_IMAGE_HEADER_SIGNATURE_B1 ||
                        suousb->header.signature[1] != SUOTA_1_1_IMAGE_HEADER_SIGNATURE_B2) {
                suousb->error_cb(suousb, SUOUSB_INVAL_IMG_HDR);
                return false;
        }

        if (sizeof(suousb->header) + suousb->header.code_size > SUOUSB_MAX_IMAGE_SIZE) {
                suousb->error_cb(suousb, SUOUSB_INVAL_IMG_SIZE);
                return false;
        }

        /* Erase flash for header, but don't write now - postpone until image is downloaded */
        if (!prepare_flash(suousb, sizeof(suousb->header))) {
                suousb->error_cb(suousb, SUOUSB_INT_MEM_ERR);
                return false;
        }
        suousb->flash_write_addr += sizeof(suousb->header);

        suousb->state = SUOUSB_STATE_W4_HEADER_EXT;

        return true;
}

static bool suousb_state_w4_header_ext(suousb_service_t *suousb)
{
        /* Just ignore data and go to next state */

        suousb->state = SUOUSB_STATE_W4_IMAGE_DATA;

        return true;
}

static bool suousb_state_w4_image_data(suousb_service_t *suousb)
{
        int written;

        if (!prepare_flash(suousb, suousb->buffer_len)) {
                return false;
        }

        written = ad_nvms_write(suousb->nvms, suousb->flash_write_addr, suousb->buffer, suousb->buffer_len);

        suousb->flash_write_addr += written;

        suousb->image_crc = suousb_update_crc(suousb->image_crc, suousb->buffer, suousb->buffer_len);

        suousb->recv_image_len += suousb->buffer_len;

        if (suousb->recv_image_len == suousb->header.code_size) {
                suousb->state = SUOUSB_STATE_DONE;
        }

        return (written == suousb->buffer_len);
}

static bool process_patch_data(suousb_service_t *suousb, const uint8_t *data, size_t len, size_t *consumed)
{
        size_t expected_len;
        bool ret = false;

        /*
         * First make sure data buffer holds proper number of bytes required in current state.
         * We will only fetch exactly the number of bytes required, this makes processing simpler.
         */

        switch (suousb->state) {
        case SUOUSB_STATE_W4_HEADER:
                expected_len = sizeof(suota_image_header_t);
                break;
        case SUOUSB_STATE_W4_HEADER_EXT:
                expected_len = suousb->header.exec_location - sizeof(suota_image_header_t);
                break;
        case SUOUSB_STATE_W4_IMAGE_DATA:
                /* Fetch as much as possible, until expected end of image */
                expected_len = suousb->header.code_size - suousb->recv_image_len;

                if (expected_len > suousb_buff_sz) {
                        expected_len = suousb_buff_sz;
                }
                break;
        case SUOUSB_STATE_DONE:
                /* Just ignore any trailing data */
                *consumed = len;
                return true;
        case SUOUSB_STATE_IDLE:
        case SUOUSB_STATE_ERROR:
        default:
                return false;
        }

        *consumed = pull_to_buffer(suousb, data, len, expected_len);
        if (suousb->buffer_len < expected_len) {
                return true;
        }

        /*
         * Now we have data fetched into buffer. State handler is expected to consume all available
         * data from buffer so we don't need to check for this.
         */

        switch (suousb->state) {
        case SUOUSB_STATE_W4_HEADER:
                ret = suousb_state_w4_header(suousb);
                break;
        case SUOUSB_STATE_W4_HEADER_EXT:
                ret = suousb_state_w4_header_ext(suousb);
                break;
        case SUOUSB_STATE_W4_IMAGE_DATA:
                ret = suousb_state_w4_image_data(suousb);
                break;
        case SUOUSB_STATE_IDLE:
        case SUOUSB_STATE_DONE:
        case SUOUSB_STATE_ERROR:
                /* We should never reach this since these states should already return above */
                OS_ASSERT(0);
                break;
        }

        suousb->buffer_len = 0;

        return ret;
}

static bool handle_patch_data(suousb_service_t *suousb, const uint8_t *data, size_t recv_len)
{
        size_t len = 0;
        bool ret;

        if (!suousb->buffer) {
                return false;
        }

        suousb->recv_total_len += recv_len;

        do {
                size_t consumed = 0;

                ret = process_patch_data(suousb, data + len, recv_len - len, &consumed);
                len += consumed;
        } while (ret && len < recv_len);

        if (suousb->chunk_cb) {
                suousb->chunk_len += len;

                while (suousb->chunk_len >= suousb->patch_len) {
                        suousb->chunk_cb(suousb);
                        suousb->chunk_len -= suousb->patch_len;
                }
        }

        return ret;
}

static suousb_error_t do_mem_dev_write(suousb_service_t *suousb, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        uint8_t cmd;

        if (offset) {
                return SUOUSB_ERROR_ATTRIBUTE_NOT_LONG;
        }

        if (length != sizeof(uint32_t)) {
                return SUOUSB_ERROR_APPLICATION_ERROR;
        }

        cmd = get_u32(value) >> 24;

        if (cmd < SUOUSB_MEM_INVAL_DEV) {
                suousb->flash_write_addr = get_update_addr(suousb);
                suousb->flash_erase_addr = suousb->flash_write_addr;
        }

        switch (cmd) {
        case SUOUSB_IMG_SPI_FLASH:
                if (!suousb->buffer)
                        suousb->buffer = OS_MALLOC(sizeof(uint8_t) * suousb_buff_sz);

                if (!suousb->buffer) {
                        suousb_notify_client_status(suousb, SUOUSB_SRV_EXIT);
                        return SUOUSB_ERROR_OK;
                }

                suousb->buffer_len = 0;
                suousb_notify_client_status(suousb, SUOUSB_IMG_STARTED);
                suousb->state = SUOUSB_STATE_W4_HEADER;
                suousb->chunk_len = 0;
                suousb->recv_total_len = 0;
                suousb->recv_image_len = 0;
                suousb->image_crc = 0xFFFFFFFF;

                break;

        case SUOUSB_REBOOT:
                hw_cpm_reboot_system();

                break;

        case SUOUSB_IMG_END:
                suousb->image_crc ^= 0xFFFFFFFF;
                if (suousb->image_crc != suousb->header.crc) {
                        suousb_notify_client_status(suousb, SUOUSB_CRC_ERR);
                } else {
                        if (!set_active_img_ptr(suousb)) {
                                suousb_notify_client_status(suousb, SUOUSB_APP_ERROR);
                                return SUOUSB_ERROR_APPLICATION_ERROR;
                        } else {
                                suousb_notify_client_status(suousb, SUOUSB_CMP_OK);
                        }
                }
                break;

        case SUOUSB_MEM_SERVICE_EXIT:
                if (suousb->buffer) {
                        OS_FREE(suousb->buffer);
                        suousb->buffer = NULL;
                }

                suousb_notify_client_status(suousb, SUOUSB_SRV_EXIT);
                break;
        }

        return SUOUSB_ERROR_OK;
}

static suousb_error_t do_gpio_map_write(suousb_service_t *suousb, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        if (offset) {
                return SUOUSB_ERROR_ATTRIBUTE_NOT_LONG;
        }

        if (length != sizeof(uint32_t)) {
                return SUOUSB_ERROR_APPLICATION_ERROR;
        }

        return SUOUSB_ERROR_OK;
}

static suousb_error_t do_patch_len_write(suousb_service_t *suousb, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        if (offset) {
                return SUOUSB_ERROR_ATTRIBUTE_NOT_LONG;
        }

        if (length != sizeof(suousb->patch_len)) {
                return SUOUSB_ERROR_APPLICATION_ERROR;
        }

        /* Client writes patch_len only in GATT mode, set proper callbacks here */
        suousb->error_cb = suousb_error_cb;
        suousb->chunk_cb = suousb_chunk_cb;
        suousb->patch_len = get_u16(value);

        return SUOUSB_ERROR_OK;
}

static suousb_error_t do_patch_data_write(suousb_service_t *suousb, uint16_t offset,
                uint16_t length, const uint8_t *value)
{
        bool ret;

        ret = handle_patch_data(suousb, value, length);

        return ret ? SUOUSB_ERROR_OK : SUOUSB_ERROR_APPLICATION_ERROR;
}

suousb_error_t suousb_read_req(suousb_write_request_t req, uint32_t *value)
{
        suousb_error_t status = SUOUSB_ERROR_READ_NOT_PERMITTED;

        switch (req) {
        case SUOUSB_READ_STATUS:
                *value = suousb_external_status;
                status = SUOUSB_ERROR_OK;
                break;
        case SUOUSB_READ_MEMINFO:
                *value = suousb_svc->recv_total_len;
                status = SUOUSB_ERROR_OK;
                break;
        default:
                break;
        }

        return status;
}

suousb_error_t suousb_write_req(suousb_write_request_t req, uint16_t offset, uint16_t length,
        const uint8_t *value)
{
        suousb_error_t status = SUOUSB_ERROR_ATTRIBUTE_NOT_FOUND;

        switch (req) {
        case SUOUSB_WRITE_STATUS:
                status = do_bl_ccc_write(suousb_svc, offset, length, value);
                break;
        case SUOUSB_WRITE_MEMDEV:
                status = do_mem_dev_write(suousb_svc, offset, length, value);
                break;
        case SUOUSB_WRITE_GPIO_MAP:
                status = do_gpio_map_write(suousb_svc, offset, length, value);
                break;
        case SUOUSB_WRITE_PATCH_LEN:
                status = do_patch_len_write(suousb_svc, offset, length, value);
                break;
        case SUOUSB_WRITE_PATCH_DATA:
                status = do_patch_data_write(suousb_svc, offset, length, value);
                break;
        default:
                break;
        }

        return status;
}

uint32_t suousb_update_crc(uint32_t crc, const uint8_t *data, size_t len)
{
        while (len--) {
                crc = crc32_tab[(crc ^ *data++) & 0xff] ^ (crc >> 8);
        }
        return crc;
}

int suousb_init(suousb_notify_cb_t cb, uint32_t buffsize)
{
        nvms_t nvms;

        nvms = ad_nvms_open(NVMS_FW_UPDATE_PART);
        if (!nvms) {
                return -1;
        }

        suousb_svc = OS_MALLOC(sizeof(*suousb_svc));
        OS_ASSERT(suousb_svc);

        memset(suousb_svc, 0, sizeof(*suousb_svc));

        suousb_svc->state = SUOUSB_STATE_W4_HEADER;
        suousb_svc->active_img = SUOUSB_ACTIVE_IMG_FIRST;
        suousb_svc->nvms = nvms;

        ext_notify_cb = cb;
        suousb_buff_sz = buffsize;

        return 0;
}
