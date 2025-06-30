#include "../include/acfs.h"
#include <string.h>
#include <stdlib.h>

/* 内部函数声明 */
static acfs_error_t acfs_load_header(acfs_t* acfs);
static acfs_error_t acfs_save_header(acfs_t* acfs);
static acfs_error_t acfs_load_entries(acfs_t* acfs);
static acfs_error_t acfs_save_entries(acfs_t* acfs);
static acfs_error_t acfs_init_bitmap(acfs_t* acfs);
static acfs_error_t acfs_allocate_clusters(acfs_t* acfs, uint16_t count, uint16_t* cluster_list);
static void acfs_free_clusters(acfs_t* acfs, uint16_t* cluster_list, uint16_t count);
static acfs_data_entry_t* acfs_find_entry(acfs_t* acfs, const char* data_id);
static uint16_t acfs_calculate_clusters_needed(uint16_t cluster_size, size_t data_size);
static acfs_error_t acfs_read_clusters(acfs_t* acfs, uint16_t* cluster_list, uint16_t count, void* data);
static acfs_error_t acfs_write_clusters(acfs_t* acfs, uint16_t* cluster_list, uint16_t count, const void* data);

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

/**
 * 写入数据
 */
acfs_error_t acfs_write(acfs_t* acfs, const char* data_id, const void* data, size_t size)
{
    if (!acfs || !data_id || !data || size == 0) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (!acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    if (strlen(data_id) >= ACFS_MAX_DATA_ID_LEN) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    uint16_t clusters_needed = acfs_calculate_clusters_needed(acfs->header.cluster_size, size);
    
    // 查找现有条目
    acfs_data_entry_t* entry = acfs_find_entry(acfs, data_id);
    
    if (entry) {
        // 更新现有数据
        if (entry->cluster_count != clusters_needed) {
            // 重新分配簇
            acfs_free_clusters(acfs, entry->cluster_list, entry->cluster_count);
            free(entry->cluster_list);
            
            entry->cluster_list = (uint16_t*)malloc(clusters_needed * sizeof(uint16_t));
            if (!entry->cluster_list) {
                return ACFS_ERROR_NO_SPACE;
            }
            
            acfs_error_t ret = acfs_allocate_clusters(acfs, clusters_needed, entry->cluster_list);
            if (ret != ACFS_OK) {
                free(entry->cluster_list);
                entry->cluster_list = NULL;
                return ret;
            }
            
            entry->cluster_count = clusters_needed;
        }
    } else {
        // 创建新条目
        if (acfs->header.data_entries >= (acfs->header.sys_clusters * acfs->header.cluster_size - sizeof(acfs_header_t)) / sizeof(acfs_data_entry_t)) {
            return ACFS_ERROR_CLUSTER_FULL;
        }
        
        entry = &acfs->entries[acfs->header.data_entries];
        strncpy(entry->data_id, data_id, ACFS_MAX_DATA_ID_LEN - 1);
        entry->data_id[ACFS_MAX_DATA_ID_LEN - 1] = '\0';
        
        entry->cluster_list = (uint16_t*)malloc(clusters_needed * sizeof(uint16_t));
        if (!entry->cluster_list) {
            return ACFS_ERROR_NO_SPACE;
        }
        
        acfs_error_t ret = acfs_allocate_clusters(acfs, clusters_needed, entry->cluster_list);
        if (ret != ACFS_OK) {
            free(entry->cluster_list);
            entry->cluster_list = NULL;
            return ret;
        }
        
        entry->cluster_count = clusters_needed;
        entry->is_valid = true;
        acfs->header.data_entries++;
    }
    
    // 写入数据
    entry->data_size = size;
    entry->crc32 = acfs_crc32(data, size);
    
    acfs_error_t ret = acfs_write_clusters(acfs, entry->cluster_list, entry->cluster_count, data);
    if (ret != ACFS_OK) {
        return ret;
    }
    
    // 更新头部和条目表
    ret = acfs_save_header(acfs);
    if (ret != ACFS_OK) {
        return ret;
    }
    
    ret = acfs_save_entries(acfs);
    return ret;
}

/**
 * 读取数据
 */
acfs_error_t acfs_read(acfs_t* acfs, const char* data_id, void* data, size_t size, size_t* actual_size)
{
    if (!acfs || !data_id || !data) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (!acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    acfs_data_entry_t* entry = acfs_find_entry(acfs, data_id);
    if (!entry || !entry->is_valid) {
        return ACFS_ERROR_DATA_NOT_FOUND;
    }
    
    if (size < entry->data_size) {
        if (actual_size) {
            *actual_size = entry->data_size;
        }
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    // 读取数据
    acfs_error_t ret = acfs_read_clusters(acfs, entry->cluster_list, entry->cluster_count, data);
    if (ret != ACFS_OK) {
        return ret;
    }
    
    // 校验CRC
    uint32_t crc = acfs_crc32(data, entry->data_size);
    if (crc != entry->crc32) {
        return ACFS_ERROR_CRC_MISMATCH;
    }
    
    if (actual_size) {
        *actual_size = entry->data_size;
    }
    
    return ACFS_OK;
}

/**
 * 删除数据
 */
acfs_error_t acfs_delete(acfs_t* acfs, const char* data_id)
{
    if (!acfs || !data_id) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (!acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    acfs_data_entry_t* entry = acfs_find_entry(acfs, data_id);
    if (!entry || !entry->is_valid) {
        return ACFS_ERROR_DATA_NOT_FOUND;
    }
    
    // 释放簇
    acfs_free_clusters(acfs, entry->cluster_list, entry->cluster_count);
    free(entry->cluster_list);
    
    // 移动其他条目
    int entry_index = entry - acfs->entries;
    for (int i = entry_index; i < acfs->header.data_entries - 1; i++) {
        acfs->entries[i] = acfs->entries[i + 1];
    }
    
    acfs->header.data_entries--;
    memset(&acfs->entries[acfs->header.data_entries], 0, sizeof(acfs_data_entry_t));
    
    // 保存更改
    acfs_error_t ret = acfs_save_header(acfs);
    if (ret != ACFS_OK) {
        return ret;
    }
    
    ret = acfs_save_entries(acfs);
    return ret;
}

/**
 * 检查数据是否存在
 */
bool acfs_exists(acfs_t* acfs, const char* data_id)
{
    if (!acfs || !data_id || !acfs->initialized) {
        return false;
    }
    
    acfs_data_entry_t* entry = acfs_find_entry(acfs, data_id);
    return (entry && entry->is_valid);
}

/**
 * 获取数据大小
 */
acfs_error_t acfs_get_size(acfs_t* acfs, const char* data_id, size_t* size)
{
    if (!acfs || !data_id || !size) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (!acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    acfs_data_entry_t* entry = acfs_find_entry(acfs, data_id);
    if (!entry || !entry->is_valid) {
        return ACFS_ERROR_DATA_NOT_FOUND;
    }
    
    *size = entry->data_size;
    return ACFS_OK;
}

/**
 * 获取空闲空间
 */
acfs_error_t acfs_get_free_space(acfs_t* acfs, size_t* free_size)
{
    if (!acfs || !free_size) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (!acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    *free_size = (size_t)acfs->header.free_clusters * acfs->header.cluster_size;
    return ACFS_OK;
}

/**
 * 获取系统统计信息
 */
acfs_error_t acfs_get_stats(acfs_t* acfs, size_t* total_size, size_t* used_size, 
                           size_t* free_size, uint16_t* data_count)
{
    if (!acfs) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    if (!acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    size_t data_area_size = (size_t)(acfs->header.total_clusters - acfs->header.sys_clusters) * acfs->header.cluster_size;
    
    if (total_size) {
        *total_size = data_area_size;
    }
    
    if (free_size) {
        *free_size = (size_t)acfs->header.free_clusters * acfs->header.cluster_size;
    }
    
    if (used_size) {
        *used_size = data_area_size - ((size_t)acfs->header.free_clusters * acfs->header.cluster_size);
    }
    
    if (data_count) {
        *data_count = acfs->header.data_entries;
    }
    
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
    uint32_t entries_addr = acfs->storage->start_addr + sizeof(acfs_header_t);
    uint32_t entries_size = acfs->header.data_entries * sizeof(acfs_data_entry_t);
    
    if (entries_size == 0) {
        return ACFS_OK;
    }
    
    // 读取条目数据（不包括簇列表）
    if (acfs->storage->ops.read(entries_addr, acfs->entries, entries_size) != 0) {
        return ACFS_ERROR_IO_ERROR;
    }
    
    // 为每个条目分配和读取簇列表
    for (uint16_t i = 0; i < acfs->header.data_entries; i++) {
        acfs_data_entry_t* entry = &acfs->entries[i];
        
        if (entry->cluster_count > 0) {
            entry->cluster_list = (uint16_t*)malloc(entry->cluster_count * sizeof(uint16_t));
            if (!entry->cluster_list) {
                return ACFS_ERROR_NO_SPACE;
            }
            
            // 从存储中读取簇列表（存储在条目数据之后）
            uint32_t cluster_list_addr = entries_addr + entries_size + i * ACFS_MAX_CLUSTERS * sizeof(uint16_t);
            if (acfs->storage->ops.read(cluster_list_addr, entry->cluster_list, 
                                       entry->cluster_count * sizeof(uint16_t)) != 0) {
                return ACFS_ERROR_IO_ERROR;
            }
        }
    }
    
    return ACFS_OK;
}

static acfs_error_t acfs_save_entries(acfs_t* acfs)
{
    uint32_t entries_addr = acfs->storage->start_addr + sizeof(acfs_header_t);
    uint32_t entries_size = acfs->header.data_entries * sizeof(acfs_data_entry_t);
    
    // 写入条目数据（不包括簇列表）
    if (acfs->storage->ops.write(entries_addr, acfs->entries, entries_size) != 0) {
        return ACFS_ERROR_IO_ERROR;
    }
    
    // 写入每个条目的簇列表
    for (uint16_t i = 0; i < acfs->header.data_entries; i++) {
        acfs_data_entry_t* entry = &acfs->entries[i];
        
        if (entry->cluster_count > 0 && entry->cluster_list) {
            uint32_t cluster_list_addr = entries_addr + entries_size + i * ACFS_MAX_CLUSTERS * sizeof(uint16_t);
            if (acfs->storage->ops.write(cluster_list_addr, entry->cluster_list, 
                                        entry->cluster_count * sizeof(uint16_t)) != 0) {
                return ACFS_ERROR_IO_ERROR;
            }
        }
    }
    
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
    
    // 标记数据簇为已使用
    for (uint16_t i = 0; i < acfs->header.data_entries; i++) {
        acfs_data_entry_t* entry = &acfs->entries[i];
        if (entry->is_valid && entry->cluster_list) {
            for (uint16_t j = 0; j < entry->cluster_count; j++) {
                uint16_t cluster = entry->cluster_list[j];
                uint16_t byte_idx = cluster / 8;
                uint8_t bit_idx = cluster % 8;
                acfs->cluster_bitmap[byte_idx] |= (1 << bit_idx);
            }
        }
    }
    
    return ACFS_OK;
}

static acfs_error_t acfs_allocate_clusters(acfs_t* acfs, uint16_t count, uint16_t* cluster_list)
{
    if (acfs->header.free_clusters < count) {
        return ACFS_ERROR_NO_SPACE;
    }
    
    uint16_t allocated = 0;
    
    for (uint16_t i = acfs->header.sys_clusters; i < acfs->header.total_clusters && allocated < count; i++) {
        uint16_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        
        if (!(acfs->cluster_bitmap[byte_idx] & (1 << bit_idx))) {
            // 标记为已使用
            acfs->cluster_bitmap[byte_idx] |= (1 << bit_idx);
            cluster_list[allocated++] = i;
        }
    }
    
    if (allocated < count) {
        // 回滚已分配的簇
        for (uint16_t i = 0; i < allocated; i++) {
            uint16_t cluster = cluster_list[i];
            uint16_t byte_idx = cluster / 8;
            uint8_t bit_idx = cluster % 8;
            acfs->cluster_bitmap[byte_idx] &= ~(1 << bit_idx);
        }
        return ACFS_ERROR_NO_SPACE;
    }
    
    acfs->header.free_clusters -= count;
    return ACFS_OK;
}

static void acfs_free_clusters(acfs_t* acfs, uint16_t* cluster_list, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        uint16_t cluster = cluster_list[i];
        uint16_t byte_idx = cluster / 8;
        uint8_t bit_idx = cluster % 8;
        acfs->cluster_bitmap[byte_idx] &= ~(1 << bit_idx);
    }
    
    acfs->header.free_clusters += count;
}

static acfs_data_entry_t* acfs_find_entry(acfs_t* acfs, const char* data_id)
{
    for (uint16_t i = 0; i < acfs->header.data_entries; i++) {
        if (acfs->entries[i].is_valid && 
            strncmp(acfs->entries[i].data_id, data_id, ACFS_MAX_DATA_ID_LEN) == 0) {
            return &acfs->entries[i];
        }
    }
    return NULL;
}

static uint16_t acfs_calculate_clusters_needed(uint16_t cluster_size, size_t data_size)
{
    return (data_size + cluster_size - 1) / cluster_size;
}

static acfs_error_t acfs_read_clusters(acfs_t* acfs, uint16_t* cluster_list, uint16_t count, void* data)
{
    uint8_t* data_ptr = (uint8_t*)data;
    
    for (uint16_t i = 0; i < count; i++) {
        uint32_t addr = acfs->storage->start_addr + cluster_list[i] * acfs->header.cluster_size;
        if (acfs->storage->ops.read(addr, data_ptr, acfs->header.cluster_size) != 0) {
            return ACFS_ERROR_IO_ERROR;
        }
        data_ptr += acfs->header.cluster_size;
    }
    
    return ACFS_OK;
}

static acfs_error_t acfs_write_clusters(acfs_t* acfs, uint16_t* cluster_list, uint16_t count, const void* data)
{
    const uint8_t* data_ptr = (const uint8_t*)data;
    
    for (uint16_t i = 0; i < count; i++) {
        uint32_t addr = acfs->storage->start_addr + cluster_list[i] * acfs->header.cluster_size;
        if (acfs->storage->ops.write(addr, data_ptr, acfs->header.cluster_size) != 0) {
            return ACFS_ERROR_IO_ERROR;
        }
        data_ptr += acfs->header.cluster_size;
    }
    
    return ACFS_OK;
}

acfs_error_t acfs_check_integrity(acfs_t* acfs)
{
    if (!acfs || !acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    // 检查每个数据条目的CRC
    for (uint16_t i = 0; i < acfs->header.data_entries; i++) {
        acfs_data_entry_t* entry = &acfs->entries[i];
        if (!entry->is_valid) continue;
        
        // 读取数据
        uint8_t* temp_data = (uint8_t*)malloc(entry->data_size);
        if (!temp_data) {
            return ACFS_ERROR_NO_SPACE;
        }
        
        acfs_error_t ret = acfs_read_clusters(acfs, entry->cluster_list, entry->cluster_count, temp_data);
        if (ret != ACFS_OK) {
            free(temp_data);
            return ret;
        }
        
        // 验证CRC
        uint32_t crc = acfs_crc32(temp_data, entry->data_size);
        free(temp_data);
        
        if (crc != entry->crc32) {
            return ACFS_ERROR_DATA_CORRUPTED;
        }
    }
    
    return ACFS_OK;
}

acfs_error_t acfs_defragment(acfs_t* acfs)
{
    // 简单的碎片整理实现：重新分配连续的簇
    if (!acfs || !acfs->initialized) {
        return ACFS_ERROR_NOT_INITIALIZED;
    }
    
    // TODO: 实现更复杂的碎片整理算法
    return ACFS_OK;
} 