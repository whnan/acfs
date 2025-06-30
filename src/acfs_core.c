#include "../include/acfs.h"
#include <string.h>
#include <stdlib.h>

/* 内部函数声明 */
static acfs_error_t acfs_load_header(acfs_t* acfs);
static acfs_error_t acfs_save_header(acfs_t* acfs);
static acfs_error_t acfs_load_entries(acfs_t* acfs);
static acfs_error_t acfs_save_entries(acfs_t* acfs);
static acfs_error_t acfs_init_bitmap(acfs_t* acfs);

/**
 * 获取错误描述字符串
 */
const char* acfs_error_string(acfs_error_t error)
{
    switch (error) {
        case ACFS_OK: return "成功";
        case ACFS_ERROR_INVALID_PARAM: return "无效参数";
        case ACFS_ERROR_NOT_INITIALIZED: return "未初始化";
        case ACFS_ERROR_ALREADY_INITIALIZED: return "已初始化";
        case ACFS_ERROR_NO_SPACE: return "空间不足";
        case ACFS_ERROR_DATA_NOT_FOUND: return "数据未找到";
        case ACFS_ERROR_DATA_CORRUPTED: return "数据损坏";
        case ACFS_ERROR_IO_ERROR: return "IO错误";
        case ACFS_ERROR_INVALID_FILESYSTEM: return "无效文件系统";
        case ACFS_ERROR_CLUSTER_FULL: return "簇已满";
        case ACFS_ERROR_CRC_MISMATCH: return "CRC校验失败";
        default: return "未知错误";
    }
}

/**
 * 初始化ACFS
 */
acfs_error_t acfs_init(acfs_t* acfs, storage_device_t* storage, const acfs_config_t* config)
{
    if (!acfs || !storage || !config) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (acfs->initialized) {
        return ACFS_ERROR_ALREADY_INITIALIZED;
    }
    
    // 参数检查
    if (config->cluster_size < ACFS_CLUSTER_SIZE_MIN || 
        config->cluster_size > ACFS_CLUSTER_SIZE_MAX ||
        (config->cluster_size & (config->cluster_size - 1)) != 0) {
        return ACFS_ERROR_INVALID_PARAM;  // 簇大小必须是2的幂
    }
    
    memset(acfs, 0, sizeof(acfs_t));
    acfs->storage = storage;
    
    // 尝试加载现有文件系统头
    acfs_error_t ret = acfs_load_header(acfs);
    if (ret == ACFS_OK) {
        // 验证头部信息
        if (acfs->header.magic != ACFS_MAGIC_NUMBER ||
            acfs->header.cluster_size != config->cluster_size) {
            if (config->format_if_invalid) {
                ret = acfs_format(acfs, config);
                if (ret != ACFS_OK) return ret;
            } else {
                return ACFS_ERROR_INVALID_FILESYSTEM;
            }
        }
    } else {
        // 没有找到有效的文件系统，格式化
        if (config->format_if_invalid) {
            ret = acfs_format(acfs, config);
            if (ret != ACFS_OK) return ret;
        } else {
            return ACFS_ERROR_INVALID_FILESYSTEM;
        }
    }
    
    // 分配内存
    uint16_t max_entries = (acfs->header.sys_clusters * acfs->header.cluster_size - sizeof(acfs_header_t)) / sizeof(acfs_data_entry_t);
    acfs->entries = (acfs_data_entry_t*)calloc(max_entries, sizeof(acfs_data_entry_t));
    if (!acfs->entries) {
        return ACFS_ERROR_NO_SPACE;
    }
    
    size_t bitmap_size = (acfs->header.total_clusters + 7) / 8;
    acfs->cluster_bitmap = (uint8_t*)calloc(bitmap_size, 1);
    if (!acfs->cluster_bitmap) {
        free(acfs->entries);
        return ACFS_ERROR_NO_SPACE;
    }
    
    acfs->cluster_buffer = (uint8_t*)malloc(acfs->header.cluster_size);
    if (!acfs->cluster_buffer) {
        free(acfs->entries);
        free(acfs->cluster_bitmap);
        return ACFS_ERROR_NO_SPACE;
    }
    
    // 加载数据条目和位图
    ret = acfs_load_entries(acfs);
    if (ret != ACFS_OK) {
        free(acfs->entries);
        free(acfs->cluster_bitmap);
        free(acfs->cluster_buffer);
        return ret;
    }
    
    ret = acfs_init_bitmap(acfs);
    if (ret != ACFS_OK) {
        free(acfs->entries);
        free(acfs->cluster_bitmap);
        free(acfs->cluster_buffer);
        return ret;
    }
    
    acfs->initialized = true;
    return ACFS_OK;
}

/**
 * 反初始化ACFS
 */
acfs_error_t acfs_deinit(acfs_t* acfs)
{
    if (!acfs) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (!acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    // 释放内存
    if (acfs->entries) {
        for (uint16_t i = 0; i < acfs->header.data_entries; i++) {
            if (acfs->entries[i].cluster_list) {
                free(acfs->entries[i].cluster_list);
            }
        }
        free(acfs->entries);
    }
    
    if (acfs->cluster_bitmap) {
        free(acfs->cluster_bitmap);
    }
    
    if (acfs->cluster_buffer) {
        free(acfs->cluster_buffer);
    }
    
    memset(acfs, 0, sizeof(acfs_t));
    return ACFS_OK;
}

/**
 * 格式化存储介质
 */
acfs_error_t acfs_format(acfs_t* acfs, const acfs_config_t* config)
{
    if (!acfs || !config || !acfs->storage) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    // 计算文件系统参数
    uint16_t total_clusters = acfs->storage->size / config->cluster_size;
    if (total_clusters == 0) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    uint16_t sys_clusters = config->reserved_clusters;
    if (sys_clusters == 0) {
        // 自动计算系统区域大小
        sys_clusters = (sizeof(acfs_header_t) + config->cluster_size - 1) / config->cluster_size;
        if (sys_clusters < 2) sys_clusters = 2;  // 至少保留2个簇
    }
    
    if (sys_clusters >= total_clusters) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    // 初始化头部
    memset(&acfs->header, 0, sizeof(acfs_header_t));
    acfs->header.magic = ACFS_MAGIC_NUMBER;
    acfs->header.version = (ACFS_VERSION_MAJOR << 8) | ACFS_VERSION_MINOR;
    acfs->header.cluster_size = config->cluster_size;
    acfs->header.total_clusters = total_clusters;
    acfs->header.sys_clusters = sys_clusters;
    acfs->header.data_entries = 0;
    acfs->header.free_clusters = total_clusters - sys_clusters;
    
    // 计算头部CRC
    acfs->header.crc32 = acfs_crc32(&acfs->header, sizeof(acfs_header_t) - sizeof(uint32_t));
    
    // 写入头部
    acfs_error_t ret = acfs_save_header(acfs);
    if (ret != ACFS_OK) {
        return ret;
    }
    
    // 清空系统信息区域的其余部分
    uint8_t* zero_buffer = (uint8_t*)calloc(config->cluster_size, 1);
    if (!zero_buffer) {
        return ACFS_ERROR_NO_SPACE;
    }
    
    for (uint16_t i = 1; i < sys_clusters; i++) {
        uint32_t addr = acfs->storage->start_addr + i * config->cluster_size;
        if (acfs->storage->ops.write(addr, zero_buffer, config->cluster_size) != 0) {
            free(zero_buffer);
            return ACFS_ERROR_IO_ERROR;
        }
    }
    
    free(zero_buffer);
    return ACFS_OK;
}

/* 内部函数实现 */

static acfs_error_t acfs_load_header(acfs_t* acfs)
{
    uint32_t addr = acfs->storage->start_addr;
    if (acfs->storage->ops.read(addr, &acfs->header, sizeof(acfs_header_t)) != 0) {
        return ACFS_ERROR_IO_ERROR;
    }
    
    // 验证魔数
    if (acfs->header.magic != ACFS_MAGIC_NUMBER) {
        return ACFS_ERROR_INVALID_FILESYSTEM;
    }
    
    // 验证CRC
    uint32_t crc = acfs_crc32(&acfs->header, sizeof(acfs_header_t) - sizeof(uint32_t));
    if (crc != acfs->header.crc32) {
        return ACFS_ERROR_DATA_CORRUPTED;
    }
    
    return ACFS_OK;
}

static acfs_error_t acfs_save_header(acfs_t* acfs)
{
    acfs->header.crc32 = acfs_crc32(&acfs->header, sizeof(acfs_header_t) - sizeof(uint32_t));
    
    uint32_t addr = acfs->storage->start_addr;
    if (acfs->storage->ops.write(addr, &acfs->header, sizeof(acfs_header_t)) != 0) {
        return ACFS_ERROR_IO_ERROR;
    }
    
    return ACFS_OK;
}

static acfs_error_t acfs_load_entries(acfs_t* acfs)
{
    // 简化版本：假设数据条目数为0
    return ACFS_OK;
}

static acfs_error_t acfs_save_entries(acfs_t* acfs)
{
    // 简化版本：暂不实现
    return ACFS_OK;
}

static acfs_error_t acfs_init_bitmap(acfs_t* acfs)
{
    size_t bitmap_size = (acfs->header.total_clusters + 7) / 8;
    memset(acfs->cluster_bitmap, 0, bitmap_size);
    
    // 标记系统簇为已使用
    for (uint16_t i = 0; i < acfs->header.sys_clusters; i++) {
        uint16_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        acfs->cluster_bitmap[byte_idx] |= (1 << bit_idx);
    }
    
    return ACFS_OK;
} 