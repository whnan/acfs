#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/acfs.h"

// 声明存储设备创建函数
acfs_error_t acfs_create_eeprom_device(storage_device_t* device, uint32_t start_addr, uint32_t size);
void acfs_destroy_storage_device(storage_device_t* device);

/**
 * 测试基本初始化和格式化
 */
void test_init_format()
{
    printf("测试: 初始化和格式化\n");
    
    storage_device_t storage;
    acfs_error_t ret = acfs_create_eeprom_device(&storage, 0x0000, 32 * 1024);
    assert(ret == ACFS_OK);
    
    acfs_config_t config = {
        .cluster_size = 128,
        .reserved_clusters = 2,
        .format_if_invalid = true,
        .enable_crc_check = true
    };
    
    acfs_t acfs;
    ret = acfs_init(&acfs, &storage, &config);
    assert(ret == ACFS_OK);
    
    ret = acfs_deinit(&acfs);
    assert(ret == ACFS_OK);
    
    acfs_destroy_storage_device(&storage);
    printf("✓ 初始化和格式化测试通过\n");
}

/**
 * 测试读写操作
 */
void test_read_write()
{
    printf("测试: 读写操作\n");
    
    storage_device_t storage;
    acfs_create_eeprom_device(&storage, 0x0000, 32 * 1024);
    
    acfs_config_t config = {
        .cluster_size = 128,
        .reserved_clusters = 2,
        .format_if_invalid = true,
        .enable_crc_check = true
    };
    
    acfs_t acfs;
    acfs_init(&acfs, &storage, &config);
    
    // 测试写入
    const char* test_data = "Hello, ACFS Test!";
    acfs_error_t ret = acfs_write(&acfs, "test", test_data, strlen(test_data) + 1);
    assert(ret == ACFS_OK);
    
    // 测试读取
    char read_buffer[64];
    size_t actual_size;
    ret = acfs_read(&acfs, "test", read_buffer, sizeof(read_buffer), &actual_size);
    assert(ret == ACFS_OK);
    assert(strcmp(test_data, read_buffer) == 0);
    assert(actual_size == strlen(test_data) + 1);
    
    acfs_deinit(&acfs);
    acfs_destroy_storage_device(&storage);
    printf("✓ 读写操作测试通过\n");
}

/**
 * 测试存在性检查
 */
void test_exists()
{
    printf("测试: 存在性检查\n");
    
    storage_device_t storage;
    acfs_create_eeprom_device(&storage, 0x0000, 32 * 1024);
    
    acfs_config_t config = {
        .cluster_size = 128,
        .reserved_clusters = 2,
        .format_if_invalid = true,
        .enable_crc_check = true
    };
    
    acfs_t acfs;
    acfs_init(&acfs, &storage, &config);
    
    // 测试不存在的数据
    assert(!acfs_exists(&acfs, "nonexistent"));
    
    // 写入数据
    const char* test_data = "Test data";
    acfs_write(&acfs, "exists_test", test_data, strlen(test_data) + 1);
    
    // 测试存在的数据
    assert(acfs_exists(&acfs, "exists_test"));
    
    acfs_deinit(&acfs);
    acfs_destroy_storage_device(&storage);
    printf("✓ 存在性检查测试通过\n");
}

/**
 * 测试删除操作
 */
void test_delete()
{
    printf("测试: 删除操作\n");
    
    storage_device_t storage;
    acfs_create_eeprom_device(&storage, 0x0000, 32 * 1024);
    
    acfs_config_t config = {
        .cluster_size = 128,
        .reserved_clusters = 2,
        .format_if_invalid = true,
        .enable_crc_check = true
    };
    
    acfs_t acfs;
    acfs_init(&acfs, &storage, &config);
    
    // 写入数据
    const char* test_data = "Data to be deleted";
    acfs_write(&acfs, "delete_test", test_data, strlen(test_data) + 1);
    assert(acfs_exists(&acfs, "delete_test"));
    
    // 删除数据
    acfs_error_t ret = acfs_delete(&acfs, "delete_test");
    assert(ret == ACFS_OK);
    assert(!acfs_exists(&acfs, "delete_test"));
    
    // 尝试删除不存在的数据
    ret = acfs_delete(&acfs, "nonexistent");
    assert(ret == ACFS_ERROR_DATA_NOT_FOUND);
    
    acfs_deinit(&acfs);
    acfs_destroy_storage_device(&storage);
    printf("✓ 删除操作测试通过\n");
}

/**
 * 测试统计信息
 */
void test_stats()
{
    printf("测试: 统计信息\n");
    
    storage_device_t storage;
    acfs_create_eeprom_device(&storage, 0x0000, 32 * 1024);
    
    acfs_config_t config = {
        .cluster_size = 256,
        .reserved_clusters = 4,
        .format_if_invalid = true,
        .enable_crc_check = true
    };
    
    acfs_t acfs;
    acfs_init(&acfs, &storage, &config);
    
    size_t total_size, used_size, free_size;
    uint16_t data_count;
    
    acfs_error_t ret = acfs_get_stats(&acfs, &total_size, &used_size, &free_size, &data_count);
    assert(ret == ACFS_OK);
    assert(data_count == 0);
    assert(used_size == 0);
    
    // 写入一些数据
    const char* test_data = "Statistical test data";
    acfs_write(&acfs, "stats_test", test_data, strlen(test_data) + 1);
    
    ret = acfs_get_stats(&acfs, &total_size, &used_size, &free_size, &data_count);
    assert(ret == ACFS_OK);
    assert(data_count == 1);
    assert(used_size > 0);
    
    acfs_deinit(&acfs);
    acfs_destroy_storage_device(&storage);
    printf("✓ 统计信息测试通过\n");
}

/**
 * 测试错误处理
 */
void test_error_handling()
{
    printf("测试: 错误处理\n");
    
    storage_device_t storage;
    acfs_create_eeprom_device(&storage, 0x0000, 1024);  // 很小的存储空间
    
    acfs_config_t config = {
        .cluster_size = 256,
        .reserved_clusters = 2,
        .format_if_invalid = true,
        .enable_crc_check = true
    };
    
    acfs_t acfs;
    acfs_init(&acfs, &storage, &config);
    
    // 测试无效参数
    acfs_error_t ret = acfs_write(NULL, "test", "data", 4);
    assert(ret == ACFS_ERROR_INVALID_PARAM);
    
    ret = acfs_write(&acfs, NULL, "data", 4);
    assert(ret == ACFS_ERROR_INVALID_PARAM);
    
    ret = acfs_write(&acfs, "test", NULL, 4);
    assert(ret == ACFS_ERROR_INVALID_PARAM);
    
    // 测试读取不存在的数据
    char buffer[64];
    size_t actual_size;
    ret = acfs_read(&acfs, "nonexistent", buffer, sizeof(buffer), &actual_size);
    assert(ret == ACFS_ERROR_DATA_NOT_FOUND);
    
    acfs_deinit(&acfs);
    acfs_destroy_storage_device(&storage);
    printf("✓ 错误处理测试通过\n");
}

int main()
{
    printf("=== ACFS 单元测试 ===\n");
    
    test_init_format();
    test_read_write();
    test_exists();
    test_delete();
    test_stats();
    test_error_handling();
    
    printf("\n所有测试通过！✓\n");
    return 0;
} 