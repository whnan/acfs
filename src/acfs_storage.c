#include "../include/acfs.h"
#include <string.h>

/* 模拟的EEPROM存储 */
static uint8_t* eeprom_buffer = NULL;
static uint32_t eeprom_size = 0;

/* 模拟的Flash存储 */
static uint8_t* flash_buffer = NULL;
static uint32_t flash_size = 0;

/**
 * EEPROM读取操作
 */
static int eeprom_read(uint32_t addr, void* data, size_t size)
{
    if (!eeprom_buffer || addr + size > eeprom_size) {
        return -1;
    }
    
    memcpy(data, eeprom_buffer + addr, size);
    return 0;
}

/**
 * EEPROM写入操作
 */
static int eeprom_write(uint32_t addr, const void* data, size_t size)
{
    if (!eeprom_buffer || addr + size > eeprom_size) {
        return -1;
    }
    
    memcpy(eeprom_buffer + addr, data, size);
    return 0;
}

/**
 * EEPROM擦除操作（EEPROM通常不需要擦除）
 */
static int eeprom_erase(uint32_t addr, size_t size)
{
    if (!eeprom_buffer || addr + size > eeprom_size) {
        return -1;
    }
    
    memset(eeprom_buffer + addr, 0xFF, size);
    return 0;
}

/**
 * Flash读取操作
 */
static int flash_read(uint32_t addr, void* data, size_t size)
{
    if (!flash_buffer || addr + size > flash_size) {
        return -1;
    }
    
    memcpy(data, flash_buffer + addr, size);
    return 0;
}

/**
 * Flash写入操作
 */
static int flash_write(uint32_t addr, const void* data, size_t size)
{
    if (!flash_buffer || addr + size > flash_size) {
        return -1;
    }
    
    // Flash写入前需要检查是否已擦除
    for (size_t i = 0; i < size; i++) {
        if (flash_buffer[addr + i] != 0xFF) {
            return -1;  // 需要先擦除
        }
    }
    
    memcpy(flash_buffer + addr, data, size);
    return 0;
}

/**
 * Flash擦除操作
 */
static int flash_erase(uint32_t addr, size_t size)
{
    if (!flash_buffer || addr + size > flash_size) {
        return -1;
    }
    
    memset(flash_buffer + addr, 0xFF, size);
    return 0;
}

/**
 * 创建EEPROM存储设备
 * @param device 存储设备结构体
 * @param start_addr 起始地址
 * @param size 大小
 * @return 错误码
 */
acfs_error_t acfs_create_eeprom_device(storage_device_t* device, uint32_t start_addr, uint32_t size)
{
    if (!device || size == 0) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    // 分配模拟的EEPROM缓冲区
    eeprom_buffer = (uint8_t*)malloc(size);
    if (!eeprom_buffer) {
        return ACFS_ERROR_NO_SPACE;
    }
    
    // 初始化为0xFF（EEPROM擦除状态）
    memset(eeprom_buffer, 0xFF, size);
    eeprom_size = size;
    
    // 配置设备
    device->start_addr = start_addr;
    device->size = size;
    device->type = STORAGE_TYPE_EEPROM;
    device->need_erase = false;
    device->erase_block_size = 0;
    
    device->ops.read = eeprom_read;
    device->ops.write = eeprom_write;
    device->ops.erase = eeprom_erase;
    
    return ACFS_OK;
}

/**
 * 创建Flash存储设备
 * @param device 存储设备结构体
 * @param start_addr 起始地址
 * @param size 大小
 * @param erase_block_size 擦除块大小
 * @return 错误码
 */
acfs_error_t acfs_create_flash_device(storage_device_t* device, uint32_t start_addr, 
                                     uint32_t size, uint32_t erase_block_size)
{
    if (!device || size == 0 || erase_block_size == 0) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    // 分配模拟的Flash缓冲区
    flash_buffer = (uint8_t*)malloc(size);
    if (!flash_buffer) {
        return ACFS_ERROR_NO_SPACE;
    }
    
    // 初始化为0xFF（Flash擦除状态）
    memset(flash_buffer, 0xFF, size);
    flash_size = size;
    
    // 配置设备
    device->start_addr = start_addr;
    device->size = size;
    device->type = STORAGE_TYPE_FLASH;
    device->need_erase = true;
    device->erase_block_size = erase_block_size;
    
    device->ops.read = flash_read;
    device->ops.write = flash_write;
    device->ops.erase = flash_erase;
    
    return ACFS_OK;
}

/**
 * 创建SDRAM存储设备（用于测试）
 * @param device 存储设备结构体
 * @param start_addr 起始地址
 * @param size 大小
 * @return 错误码
 */
acfs_error_t acfs_create_sdram_device(storage_device_t* device, uint32_t start_addr, uint32_t size)
{
    if (!device || size == 0) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    // SDRAM使用与EEPROM相同的模拟方式
    return acfs_create_eeprom_device(device, start_addr, size);
}

/**
 * 销毁存储设备
 * @param device 存储设备结构体
 */
void acfs_destroy_storage_device(storage_device_t* device)
{
    if (!device) {
        return;
    }
    
    if (device->type == STORAGE_TYPE_EEPROM || device->type == STORAGE_TYPE_SDRAM) {
        if (eeprom_buffer) {
            free(eeprom_buffer);
            eeprom_buffer = NULL;
            eeprom_size = 0;
        }
    } else if (device->type == STORAGE_TYPE_FLASH) {
        if (flash_buffer) {
            free(flash_buffer);
            flash_buffer = NULL;
            flash_size = 0;
        }
    }
    
    memset(device, 0, sizeof(storage_device_t));
}

/**
 * 存储设备完整性测试
 * @param device 存储设备结构体
 * @return 错误码
 */
acfs_error_t acfs_test_storage_device(storage_device_t* device)
{
    if (!device) {
        return ACFS_ERROR_INVALID_PARAM;
    }
    
    uint8_t test_data[] = {0x55, 0xAA, 0x33, 0xCC};
    uint8_t read_data[4];
    
    // 测试写入和读取
    if (device->ops.write(device->start_addr, test_data, sizeof(test_data)) != 0) {
        return ACFS_ERROR_IO_ERROR;
    }
    
    if (device->ops.read(device->start_addr, read_data, sizeof(read_data)) != 0) {
        return ACFS_ERROR_IO_ERROR;
    }
    
    if (memcmp(test_data, read_data, sizeof(test_data)) != 0) {
        return ACFS_ERROR_DATA_CORRUPTED;
    }
    
    // 测试擦除（如果支持）
    if (device->need_erase) {
        if (device->ops.erase(device->start_addr, device->erase_block_size) != 0) {
            return ACFS_ERROR_IO_ERROR;
        }
        
        if (device->ops.read(device->start_addr, read_data, sizeof(read_data)) != 0) {
            return ACFS_ERROR_IO_ERROR;
        }
        
        for (int i = 0; i < 4; i++) {
            if (read_data[i] != 0xFF) {
                return ACFS_ERROR_DATA_CORRUPTED;
            }
        }
    }
    
    return ACFS_OK;
} 