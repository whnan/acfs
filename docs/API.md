# ACFS API 参考文档

## 概述

ACFS（Abstract Cluster File System）提供了一套简洁的API接口，用于在嵌入式系统中管理存储数据。本文档详细描述了所有可用的API函数。

## 核心数据结构

### acfs_t
ACFS实例结构体，包含文件系统的所有状态信息。

### storage_device_t
存储设备描述符，定义了存储介质的基本属性和操作接口。

### acfs_config_t
初始化配置结构体，用于配置文件系统参数。

## 初始化和清理

### acfs_init()
```c
acfs_error_t acfs_init(acfs_t* acfs, storage_device_t* storage, const acfs_config_t* config);
```

**功能**: 初始化ACFS文件系统

**参数**:
- `acfs`: ACFS实例指针
- `storage`: 存储设备指针
- `config`: 配置参数指针

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效
- `ACFS_ERROR_ALREADY_INITIALIZED`: 已初始化
- `ACFS_ERROR_NO_SPACE`: 内存不足

**示例**:
```c
acfs_t acfs;
storage_device_t storage;
acfs_config_t config = {
    .cluster_size = 256,
    .reserved_clusters = 4,
    .format_if_invalid = true,
    .enable_crc_check = true
};

acfs_error_t ret = acfs_init(&acfs, &storage, &config);
if (ret != ACFS_OK) {
    printf("初始化失败: %s\n", acfs_error_string(ret));
}
```

### acfs_deinit()
```c
acfs_error_t acfs_deinit(acfs_t* acfs);
```

**功能**: 反初始化ACFS文件系统，释放资源

**参数**:
- `acfs`: ACFS实例指针

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效
- `ACFS_ERROR_NOT_INITIALIZED`: 未初始化

### acfs_format()
```c
acfs_error_t acfs_format(acfs_t* acfs, const acfs_config_t* config);
```

**功能**: 格式化存储介质，创建新的文件系统

**参数**:
- `acfs`: ACFS实例指针
- `config`: 配置参数指针

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效
- `ACFS_ERROR_IO_ERROR`: IO错误

## 数据操作

### acfs_write()
```c
acfs_error_t acfs_write(acfs_t* acfs, const char* data_id, const void* data, size_t size);
```

**功能**: 写入数据到文件系统

**参数**:
- `acfs`: ACFS实例指针
- `data_id`: 数据标识符（字符串）
- `data`: 数据指针
- `size`: 数据大小

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效
- `ACFS_ERROR_NO_SPACE`: 空间不足
- `ACFS_ERROR_IO_ERROR`: IO错误

**注意**: 
- 数据标识符长度不能超过 `ACFS_MAX_DATA_ID_LEN`
- 如果数据已存在，将会覆盖原数据
- 系统会自动分配足够的簇来存储数据

### acfs_read()
```c
acfs_error_t acfs_read(acfs_t* acfs, const char* data_id, void* data, size_t size, size_t* actual_size);
```

**功能**: 从文件系统读取数据

**参数**:
- `acfs`: ACFS实例指针
- `data_id`: 数据标识符
- `data`: 数据缓冲区
- `size`: 缓冲区大小
- `actual_size`: 实际读取的数据大小（可选）

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效
- `ACFS_ERROR_DATA_NOT_FOUND`: 数据未找到
- `ACFS_ERROR_CRC_MISMATCH`: CRC校验失败

### acfs_delete()
```c
acfs_error_t acfs_delete(acfs_t* acfs, const char* data_id);
```

**功能**: 删除指定的数据

**参数**:
- `acfs`: ACFS实例指针
- `data_id`: 数据标识符

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效
- `ACFS_ERROR_DATA_NOT_FOUND`: 数据未找到

### acfs_exists()
```c
bool acfs_exists(acfs_t* acfs, const char* data_id);
```

**功能**: 检查数据是否存在

**参数**:
- `acfs`: ACFS实例指针
- `data_id`: 数据标识符

**返回值**: 
- `true`: 数据存在
- `false`: 数据不存在或参数无效

## 状态查询

### acfs_get_size()
```c
acfs_error_t acfs_get_size(acfs_t* acfs, const char* data_id, size_t* size);
```

**功能**: 获取指定数据的大小

**参数**:
- `acfs`: ACFS实例指针
- `data_id`: 数据标识符
- `size`: 返回数据大小

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效
- `ACFS_ERROR_DATA_NOT_FOUND`: 数据未找到

### acfs_get_free_space()
```c
acfs_error_t acfs_get_free_space(acfs_t* acfs, size_t* free_size);
```

**功能**: 获取可用空间大小

**参数**:
- `acfs`: ACFS实例指针
- `free_size`: 返回可用空间大小

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效

### acfs_get_stats()
```c
acfs_error_t acfs_get_stats(acfs_t* acfs, size_t* total_size, size_t* used_size, 
                           size_t* free_size, uint16_t* data_count);
```

**功能**: 获取文件系统统计信息

**参数**:
- `acfs`: ACFS实例指针
- `total_size`: 总空间大小（可选）
- `used_size`: 已用空间大小（可选）
- `free_size`: 可用空间大小（可选）
- `data_count`: 数据条目数（可选）

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效

**注意**: 所有输出参数都是可选的，传入NULL将跳过该参数

## 维护操作

### acfs_check_integrity()
```c
acfs_error_t acfs_check_integrity(acfs_t* acfs);
```

**功能**: 检查文件系统完整性

**参数**:
- `acfs`: ACFS实例指针

**返回值**: 
- `ACFS_OK`: 完整性检查通过
- `ACFS_ERROR_DATA_CORRUPTED`: 数据损坏
- `ACFS_ERROR_INVALID_PARAM`: 参数无效

### acfs_defragment()
```c
acfs_error_t acfs_defragment(acfs_t* acfs);
```

**功能**: 对文件系统进行碎片整理

**参数**:
- `acfs`: ACFS实例指针

**返回值**: 
- `ACFS_OK`: 成功
- `ACFS_ERROR_INVALID_PARAM`: 参数无效

**注意**: 碎片整理操作可能耗时较长，建议在系统空闲时执行

## 工具函数

### acfs_error_string()
```c
const char* acfs_error_string(acfs_error_t error);
```

**功能**: 获取错误码对应的描述字符串

**参数**:
- `error`: 错误码

**返回值**: 错误描述字符串

### acfs_crc32()
```c
uint32_t acfs_crc32(const void* data, size_t size);
```

**功能**: 计算数据的CRC32校验值

**参数**:
- `data`: 数据指针
- `size`: 数据大小

**返回值**: CRC32校验值

## 存储设备管理

### acfs_create_eeprom_device()
```c
acfs_error_t acfs_create_eeprom_device(storage_device_t* device, uint32_t start_addr, uint32_t size);
```

**功能**: 创建EEPROM存储设备

### acfs_create_flash_device()
```c
acfs_error_t acfs_create_flash_device(storage_device_t* device, uint32_t start_addr, 
                                     uint32_t size, uint32_t erase_block_size);
```

**功能**: 创建Flash存储设备

### acfs_destroy_storage_device()
```c
void acfs_destroy_storage_device(storage_device_t* device);
```

**功能**: 销毁存储设备，释放资源

## 错误处理

所有ACFS函数都返回 `acfs_error_t` 类型的错误码，应用程序应该检查返回值以确保操作成功。

### 常见错误码

- `ACFS_OK`: 操作成功
- `ACFS_ERROR_INVALID_PARAM`: 传入了无效参数
- `ACFS_ERROR_NOT_INITIALIZED`: 文件系统未初始化
- `ACFS_ERROR_NO_SPACE`: 存储空间不足
- `ACFS_ERROR_DATA_NOT_FOUND`: 指定的数据不存在
- `ACFS_ERROR_IO_ERROR`: 底层存储设备IO错误
- `ACFS_ERROR_CRC_MISMATCH`: CRC校验失败，数据可能损坏

### 错误处理示例

```c
acfs_error_t ret = acfs_write(&acfs, "config", data, size);
switch (ret) {
    case ACFS_OK:
        printf("写入成功\n");
        break;
    case ACFS_ERROR_NO_SPACE:
        printf("存储空间不足\n");
        break;
    case ACFS_ERROR_IO_ERROR:
        printf("存储设备IO错误\n");
        break;
    default:
        printf("未知错误: %s\n", acfs_error_string(ret));
        break;
}
```

## 性能考虑

- **簇大小选择**: 较大的簇可以减少系统开销，但可能浪费存储空间
- **CRC校验**: 启用CRC校验会略微影响性能，但能提高数据可靠性
- **碎片整理**: 定期进行碎片整理可以提高存储效率
- **批量操作**: 对于大量小数据，考虑合并后批量操作

## 线程安全

ACFS不是线程安全的，如果在多线程环境中使用，需要应用程序自行实现同步机制。

## 移植注意事项

1. 确保存储设备操作接口正确实现
2. 根据硬件特性调整配置参数
3. 考虑字节序和内存对齐问题
4. 测试在目标平台上的性能表现 