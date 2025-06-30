#ifndef ACFS_CONFIG_H
#define ACFS_CONFIG_H

/* 编译时配置选项 */

/* 调试选项 */
#ifdef DEBUG
#define ACFS_DEBUG_ENABLED      1
#define ACFS_VERBOSE_LOGGING    1
#else
#define ACFS_DEBUG_ENABLED      0
#define ACFS_VERBOSE_LOGGING    0
#endif

/* 性能调优 */
#define ACFS_ENABLE_CACHE       1    // 启用缓存
#define ACFS_CACHE_SIZE         4    // 缓存簇数量
#define ACFS_ENABLE_WEAR_LEVEL  0    // 启用磨损均衡（Flash专用）

/* 功能开关 */
#define ACFS_ENABLE_DEFRAG      1    // 启用碎片整理
#define ACFS_ENABLE_COMPRESSION 0    // 启用数据压缩
#define ACFS_ENABLE_ENCRYPTION  0    // 启用数据加密

/* 内存配置 */
#define ACFS_STATIC_MEMORY      0    // 使用静态内存分配
#define ACFS_MAX_STATIC_ENTRIES 64   // 静态分配的最大条目数

/* 平台特定配置 */
#define ACFS_LITTLE_ENDIAN      1    // 小端字节序
#define ACFS_ALIGN_SIZE         4    // 内存对齐大小

/* 限制参数 */
#define ACFS_MAX_FILENAME_LEN   32   // 最大文件名长度
#define ACFS_MAX_OPEN_FILES     8    // 最大同时打开文件数
#define ACFS_MAX_DEVICES        4    // 最大设备数量

/* 错误处理 */
#define ACFS_ENABLE_ASSERT      1    // 启用断言
#define ACFS_ENABLE_ERROR_DESC  1    // 启用错误描述字符串

/* 日志级别 */
typedef enum {
    ACFS_LOG_NONE = 0,
    ACFS_LOG_ERROR,
    ACFS_LOG_WARNING, 
    ACFS_LOG_INFO,
    ACFS_LOG_DEBUG
} acfs_log_level_t;

#define ACFS_LOG_LEVEL ACFS_LOG_INFO

/* 调试宏 */
#if ACFS_DEBUG_ENABLED
#define ACFS_DEBUG(fmt, ...) printf("[ACFS DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define ACFS_DEBUG(fmt, ...)
#endif

#if ACFS_VERBOSE_LOGGING
#define ACFS_VERBOSE(fmt, ...) printf("[ACFS VERBOSE] " fmt "\n", ##__VA_ARGS__)
#else
#define ACFS_VERBOSE(fmt, ...)
#endif

/* 断言宏 */
#if ACFS_ENABLE_ASSERT
#include <assert.h>
#define ACFS_ASSERT(expr) assert(expr)
#else
#define ACFS_ASSERT(expr)
#endif

/* 内存分配宏 */
#if ACFS_STATIC_MEMORY
extern uint8_t acfs_static_memory_pool[];
#define ACFS_MALLOC(size) acfs_static_malloc(size)
#define ACFS_FREE(ptr) acfs_static_free(ptr)
#else
#include <stdlib.h>
#define ACFS_MALLOC(size) malloc(size)
#define ACFS_FREE(ptr) free(ptr)
#endif

/* 编译器属性 */
#if defined(__GNUC__)
#define ACFS_PACKED __attribute__((packed))
#define ACFS_ALIGNED(n) __attribute__((aligned(n)))
#define ACFS_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
#define ACFS_PACKED
#define ACFS_ALIGNED(n) __declspec(align(n))
#define ACFS_UNUSED
#else
#define ACFS_PACKED
#define ACFS_ALIGNED(n)
#define ACFS_UNUSED
#endif

/* 平台检测 */
#if defined(__arm__) || defined(__aarch64__)
#define ACFS_PLATFORM_ARM 1
#elif defined(__x86_64__) || defined(_M_X64)
#define ACFS_PLATFORM_X64 1
#elif defined(__i386__) || defined(_M_IX86)
#define ACFS_PLATFORM_X86 1
#else
#define ACFS_PLATFORM_OTHER 1
#endif

/* 版本信息 */
#define ACFS_VERSION_STRING "1.0.0"
#define ACFS_BUILD_DATE __DATE__
#define ACFS_BUILD_TIME __TIME__

#endif /* ACFS_CONFIG_H */ 