#ifndef ACFS_H
#define ACFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 版本信息 */
#define ACFS_VERSION_MAJOR    1
#define ACFS_VERSION_MINOR    0
#define ACFS_VERSION_PATCH    0

/* 配置参数 */
#define ACFS_MAX_DATA_ID_LEN  32    // 数据标识最大长度
#define ACFS_CLUSTER_SIZE_MIN 64    // 最小簇大小
#define ACFS_CLUSTER_SIZE_MAX 4096  // 最大簇大小
#define ACFS_MAX_CLUSTERS     65535 // 最大簇数量
#define ACFS_MAGIC_NUMBER     0x41434653  // "ACFS"

/* 错误码定义 */
typedef enum {
    ACFS_OK = 0,                    // 成功
    ACFS_ERROR_INVALID_PARAM,       // 无效参数
    ACFS_ERROR_NOT_INITIALIZED,     // 未初始化
    ACFS_ERROR_ALREADY_INITIALIZED, // 已初始化
    ACFS_ERROR_NO_SPACE,           // 空间不足
    ACFS_ERROR_DATA_NOT_FOUND,     // 数据未找到
    ACFS_ERROR_DATA_CORRUPTED,     // 数据损坏
    ACFS_ERROR_IO_ERROR,           // IO错误
    ACFS_ERROR_INVALID_FILESYSTEM, // 无效文件系统
    ACFS_ERROR_CLUSTER_FULL,       // 簇已满
    ACFS_ERROR_CRC_MISMATCH        // CRC校验失败
} acfs_error_t;

/* 存储介质类型 */
typedef enum {
    STORAGE_TYPE_EEPROM = 0,
    STORAGE_TYPE_FLASH,
    STORAGE_TYPE_SDRAM,
    STORAGE_TYPE_CUSTOM
} storage_type_t;

/* 存储介质操作接口 */
typedef struct {
    int (*read)(uint32_t addr, void* data, size_t size);
    int (*write)(uint32_t addr, const void* data, size_t size);
    int (*erase)(uint32_t addr, size_t size);  // 可选，某些存储介质需要
} storage_ops_t;

/* 存储介质描述符 */
typedef struct {
    uint32_t start_addr;        // 起始地址
    uint32_t size;              // 总大小
    storage_type_t type;        // 存储类型
    storage_ops_t ops;          // 操作接口
    bool need_erase;            // 是否需要擦除操作
    uint32_t erase_block_size;  // 擦除块大小
} storage_device_t;

/* 系统信息头 */
typedef struct {
    uint32_t magic;             // 魔数
    uint16_t version;           // 版本号
    uint16_t cluster_size;      // 簇大小
    uint16_t total_clusters;    // 总簇数
    uint16_t sys_clusters;      // 系统信息区簇数
    uint16_t data_entries;      // 数据条目数
    uint16_t free_clusters;     // 空闲簇数
    uint32_t crc32;             // 头部CRC32
} __attribute__((packed)) acfs_header_t;

/* 数据条目信息 */
typedef struct {
    char data_id[ACFS_MAX_DATA_ID_LEN];  // 数据标识
    uint32_t data_size;                   // 数据大小
    uint16_t cluster_count;               // 占用簇数
    uint16_t *cluster_list;               // 簇列表
    uint32_t crc32;                       // 数据CRC32
    bool is_valid;                        // 是否有效
} acfs_data_entry_t;

/* ACFS实例 */
typedef struct {
    storage_device_t* storage;      // 存储设备
    acfs_header_t header;           // 系统头
    acfs_data_entry_t* entries;     // 数据条目表
    uint8_t* cluster_bitmap;        // 簇位图
    bool initialized;               // 初始化标志
    uint8_t* cluster_buffer;        // 簇缓冲区
} acfs_t;

/* 初始化配置 */
typedef struct {
    uint16_t cluster_size;          // 簇大小
    uint16_t reserved_clusters;     // 保留系统信息区簇数
    bool format_if_invalid;         // 如果无效是否格式化
    bool enable_crc_check;          // 是否启用CRC校验
} acfs_config_t;

/* 核心API接口 */

/**
 * 初始化ACFS
 * @param acfs ACFS实例
 * @param storage 存储设备
 * @param config 配置参数
 * @return 错误码
 */
acfs_error_t acfs_init(acfs_t* acfs, storage_device_t* storage, const acfs_config_t* config);

/**
 * 反初始化ACFS
 * @param acfs ACFS实例
 * @return 错误码
 */
acfs_error_t acfs_deinit(acfs_t* acfs);

/**
 * 格式化存储介质
 * @param acfs ACFS实例
 * @param config 配置参数
 * @return 错误码
 */
acfs_error_t acfs_format(acfs_t* acfs, const acfs_config_t* config);

/**
 * 写入数据
 * @param acfs ACFS实例
 * @param data_id 数据标识
 * @param data 数据指针
 * @param size 数据大小
 * @return 错误码
 */
acfs_error_t acfs_write(acfs_t* acfs, const char* data_id, const void* data, size_t size);

/**
 * 读取数据
 * @param acfs ACFS实例
 * @param data_id 数据标识
 * @param data 数据缓冲区
 * @param size 缓冲区大小
 * @param actual_size 实际读取大小
 * @return 错误码
 */
acfs_error_t acfs_read(acfs_t* acfs, const char* data_id, void* data, size_t size, size_t* actual_size);

/**
 * 删除数据
 * @param acfs ACFS实例
 * @param data_id 数据标识
 * @return 错误码
 */
acfs_error_t acfs_delete(acfs_t* acfs, const char* data_id);

/**
 * 检查数据是否存在
 * @param acfs ACFS实例
 * @param data_id 数据标识
 * @return true存在，false不存在
 */
bool acfs_exists(acfs_t* acfs, const char* data_id);

/**
 * 获取数据大小
 * @param acfs ACFS实例
 * @param data_id 数据标识
 * @param size 数据大小输出
 * @return 错误码
 */
acfs_error_t acfs_get_size(acfs_t* acfs, const char* data_id, size_t* size);

/**
 * 获取空闲空间
 * @param acfs ACFS实例
 * @param free_size 空闲空间大小输出
 * @return 错误码
 */
acfs_error_t acfs_get_free_space(acfs_t* acfs, size_t* free_size);

/**
 * 获取系统统计信息
 * @param acfs ACFS实例
 * @param total_size 总空间
 * @param used_size 已用空间
 * @param free_size 空闲空间
 * @param data_count 数据条目数
 * @return 错误码
 */
acfs_error_t acfs_get_stats(acfs_t* acfs, size_t* total_size, size_t* used_size, 
                           size_t* free_size, uint16_t* data_count);

/**
 * 数据完整性检查
 * @param acfs ACFS实例
 * @return 错误码
 */
acfs_error_t acfs_check_integrity(acfs_t* acfs);

/**
 * 碎片整理
 * @param acfs ACFS实例
 * @return 错误码
 */
acfs_error_t acfs_defragment(acfs_t* acfs);

/* 工具函数 */

/**
 * 获取错误描述字符串
 * @param error 错误码
 * @return 错误描述
 */
const char* acfs_error_string(acfs_error_t error);

/**
 * 计算CRC32
 * @param data 数据
 * @param size 数据大小
 * @return CRC32值
 */
uint32_t acfs_crc32(const void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* ACFS_H */ 