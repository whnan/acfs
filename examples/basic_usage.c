#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/acfs.h"

// 声明存储设备创建函数
acfs_error_t acfs_create_eeprom_device(storage_device_t* device, uint32_t start_addr, uint32_t size);
void acfs_destroy_storage_device(storage_device_t* device);

int main()
{
    printf("=== ACFS 基本使用示例 ===\n");
    
    // 创建存储设备
    storage_device_t storage;
    acfs_error_t ret = acfs_create_eeprom_device(&storage, 0x0000, 64 * 1024);  // 64KB EEPROM
    if (ret != ACFS_OK) {
        printf("创建存储设备失败: %s\n", acfs_error_string(ret));
        return 1;
    }
    
    // 配置ACFS
    acfs_config_t config;
    config.cluster_size = 256;          // 256字节簇大小
    config.reserved_clusters = 4;       // 保留4个簇用于系统信息
    config.format_if_invalid = true;    // 如果无效则格式化
    config.enable_crc_check = true;     // 启用CRC校验
    
    // 初始化ACFS
    acfs_t acfs;
    ret = acfs_init(&acfs, &storage, &config);
    if (ret != ACFS_OK) {
        printf("初始化ACFS失败: %s\n", acfs_error_string(ret));
        acfs_destroy_storage_device(&storage);
        return 1;
    }
    
    printf("ACFS初始化成功\n");
    
    // 获取系统统计信息
    size_t total_size, used_size, free_size;
    uint16_t data_count;
    ret = acfs_get_stats(&acfs, &total_size, &used_size, &free_size, &data_count);
    if (ret == ACFS_OK) {
        printf("文件系统统计:\n");
        printf("  总空间: %zu 字节\n", total_size);
        printf("  已用空间: %zu 字节\n", used_size);
        printf("  空闲空间: %zu 字节\n", free_size);
        printf("  数据条目数: %u\n", data_count);
    }
    
    // 写入测试数据
    const char* test_data1 = "Hello, ACFS! This is test data 1.";
    const char* test_data2 = "ACFS is a lightweight filesystem for embedded systems.";
    
    printf("\n=== 写入测试数据 ===\n");
    
    ret = acfs_write(&acfs, "test1", test_data1, strlen(test_data1) + 1);
    if (ret == ACFS_OK) {
        printf("成功写入 'test1' 数据\n");
    } else {
        printf("写入 'test1' 失败: %s\n", acfs_error_string(ret));
    }
    
    ret = acfs_write(&acfs, "test2", test_data2, strlen(test_data2) + 1);
    if (ret == ACFS_OK) {
        printf("成功写入 'test2' 数据\n");
    } else {
        printf("写入 'test2' 失败: %s\n", acfs_error_string(ret));
    }
    
    // 检查数据是否存在
    printf("\n=== 检查数据存在性 ===\n");
    
    if (acfs_exists(&acfs, "test1")) {
        printf("'test1' 数据存在\n");
    } else {
        printf("'test1' 数据不存在\n");
    }
    
    if (acfs_exists(&acfs, "test2")) {
        printf("'test2' 数据存在\n");
    } else {
        printf("'test2' 数据不存在\n");
    }
    
    if (acfs_exists(&acfs, "test3")) {
        printf("'test3' 数据存在\n");
    } else {
        printf("'test3' 数据不存在\n");
    }
    
    // 读取数据
    printf("\n=== 读取测试数据 ===\n");
    
    char read_buffer[256];
    size_t actual_size;
    
    ret = acfs_read(&acfs, "test1", read_buffer, sizeof(read_buffer), &actual_size);
    if (ret == ACFS_OK) {
        printf("成功读取 'test1': %s (大小: %zu)\n", read_buffer, actual_size);
    } else {
        printf("读取 'test1' 失败: %s\n", acfs_error_string(ret));
    }
    
    ret = acfs_read(&acfs, "test2", read_buffer, sizeof(read_buffer), &actual_size);
    if (ret == ACFS_OK) {
        printf("成功读取 'test2': %s (大小: %zu)\n", read_buffer, actual_size);
    } else {
        printf("读取 'test2' 失败: %s\n", acfs_error_string(ret));
    }
    
    // 获取数据大小
    printf("\n=== 获取数据大小 ===\n");
    
    size_t data_size;
    ret = acfs_get_size(&acfs, "test1", &data_size);
    if (ret == ACFS_OK) {
        printf("'test1' 数据大小: %zu 字节\n", data_size);
    } else {
        printf("获取 'test1' 大小失败: %s\n", acfs_error_string(ret));
    }
    
    ret = acfs_get_size(&acfs, "test2", &data_size);
    if (ret == ACFS_OK) {
        printf("'test2' 数据大小: %zu 字节\n", data_size);
    } else {
        printf("获取 'test2' 大小失败: %s\n", acfs_error_string(ret));
    }
    
    // 更新数据
    printf("\n=== 更新测试数据 ===\n");
    
    const char* updated_data = "Updated test data for test1.";
    ret = acfs_write(&acfs, "test1", updated_data, strlen(updated_data) + 1);
    if (ret == ACFS_OK) {
        printf("成功更新 'test1' 数据\n");
        
        ret = acfs_read(&acfs, "test1", read_buffer, sizeof(read_buffer), &actual_size);
        if (ret == ACFS_OK) {
            printf("读取更新后的 'test1': %s\n", read_buffer);
        }
    } else {
        printf("更新 'test1' 失败: %s\n", acfs_error_string(ret));
    }
    
    // 删除数据
    printf("\n=== 删除测试数据 ===\n");
    
    ret = acfs_delete(&acfs, "test2");
    if (ret == ACFS_OK) {
        printf("成功删除 'test2' 数据\n");
    } else {
        printf("删除 'test2' 失败: %s\n", acfs_error_string(ret));
    }
    
    // 验证删除
    if (acfs_exists(&acfs, "test2")) {
        printf("'test2' 仍然存在\n");
    } else {
        printf("'test2' 已被删除\n");
    }
    
    // 获取最终统计信息
    printf("\n=== 最终统计信息 ===\n");
    
    ret = acfs_get_stats(&acfs, &total_size, &used_size, &free_size, &data_count);
    if (ret == ACFS_OK) {
        printf("文件系统统计:\n");
        printf("  总空间: %zu 字节\n", total_size);
        printf("  已用空间: %zu 字节\n", used_size);
        printf("  空闲空间: %zu 字节\n", free_size);
        printf("  数据条目数: %u\n", data_count);
    }
    
    // 数据完整性检查
    printf("\n=== 数据完整性检查 ===\n");
    
    ret = acfs_check_integrity(&acfs);
    if (ret == ACFS_OK) {
        printf("数据完整性检查通过\n");
    } else {
        printf("数据完整性检查失败: %s\n", acfs_error_string(ret));
    }
    
    // 清理资源
    ret = acfs_deinit(&acfs);
    if (ret == ACFS_OK) {
        printf("\nACFS反初始化成功\n");
    } else {
        printf("\nACFS反初始化失败: %s\n", acfs_error_string(ret));
    }
    
    acfs_destroy_storage_device(&storage);
    
    printf("\n=== 示例程序结束 ===\n");
    return 0;
} 