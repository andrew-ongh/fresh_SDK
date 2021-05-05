/**
 ****************************************************************************************
 *
 * @file 512K/suota/partition_table.h
 *
 * Copyright (C) 2016-2017 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

PARTITION2( 0x000000 , 0x01E000 , NVMS_FIRMWARE_PART        , 0 )
PARTITION2( 0x01E000 , 0x001000 , NVMS_PRODUCT_HEADER_PART  , 0 )
PARTITION2( 0x01F000 , 0x001000 , NVMS_IMAGE_HEADER_PART    , 0 )
PARTITION2( 0x020000 , 0x020000 , NVMS_FW_EXEC_PART         , 0 )
PARTITION2( 0x040000 , 0x021000 , NVMS_FW_UPDATE_PART       , 0 )
PARTITION2( 0x061000 , 0x00F000 , NVMS_GENERIC_PART         , PARTITION_FLAG_VES )
PARTITION2( 0x070000 , 0x00C000 , NVMS_LOG_PART             , 0 )
PARTITION2( 0x07C000 , 0x001000 , NVMS_PARAM_PART           , 0 )
PARTITION2( 0x07D000 , 0x002000 , NVMS_PLATFORM_PARAMS_PART , PARTITION_FLAG_READ_ONLY )
PARTITION2( 0x07F000 , 0x001000 , NVMS_PARTITION_TABLE      , PARTITION_FLAG_READ_ONLY )

