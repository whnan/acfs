# ACFS - Abstract Cluster File System

ACFS是一个轻量级的抽象簇文件系统，专为单片机和嵌入式系统设计。它将各种存储介质（EEPROM、Flash、SDRAM等）统一抽象为簇存储单元，提供简单易用的文件系统接口。

## 特性

- **存储介质抽象**: 支持EEPROM、Flash、SDRAM等多种存储介质
- **簇管理**: 以簇为最小单位进行存储管理，自动分配和回收
- **数据完整性**: 内置CRC32校验确保数据完整性
- **轻量级设计**: 专为资源受限的嵌入式系统优化
- **简单API**: 提供简洁的读写接口，易于集成
- **碎片整理**: 支持存储空间碎片整理（可选）

## 系统架构

```
┌─────────────────┐
│     应用层      │
├─────────────────┤
│      ACFS       │
│  ┌─────────────┐│
│  │   簇管理    ││
│  ├─────────────┤│
│  │  数据管理   ││
│  ├─────────────┤│
│  │  CRC校验    ││
│  └─────────────┘│
├─────────────────┤
│   存储适配层    │
├─────────────────┤
│    存储介质     │
│ EEPROM/Flash... │
└─────────────────┘
```

## 编译和安装

### 依赖

- GCC编译器
- Make工具
- 标准C库

### 编译

```bash
# 编译所有目标
make

# 编译调试版本
make debug

# 编译发布版本
make release

# 交叉编译ARM版本
make arm
```

### 运行测试

```bash
# 运行单元测试
make test

# 运行基本使用示例
make example
```

### 安装

```bash
# 安装到系统
sudo make install

# 卸载
sudo make uninstall
```

## 使用方法

### 基本使用

```c
#include "acfs.h"

int main() {
    // 1. 创建存储设备
    storage_device_t storage;
    acfs_create_eeprom_device(&storage, 0x0000, 64 * 1024);
    
    // 2. 配置ACFS
    acfs_config_t config = {
        .cluster_size = 256,
        .reserved_clusters = 4,
        .format_if_invalid = true,
        .enable_crc_check = true
    };
    
    // 3. 初始化ACFS
    acfs_t acfs;
    acfs_init(&acfs, &storage, &config);
    
    // 4. 写入数据
    const char* data = "Hello, ACFS!";
    acfs_write(&acfs, "greeting", data, strlen(data) + 1);
    
    // 5. 读取数据
    char buffer[64];
    size_t actual_size;
    acfs_read(&acfs, "greeting", buffer, sizeof(buffer), &actual_size);
    printf("读取到: %s\n", buffer);
    
    // 6. 清理资源
    acfs_deinit(&acfs);
    acfs_destroy_storage_device(&storage);
    
    return 0;
}
```

### 存储设备类型

```c
// EEPROM设备
acfs_create_eeprom_device(&storage, start_addr, size);

// Flash设备  
acfs_create_flash_device(&storage, start_addr, size, erase_block_size);

// SDRAM设备（用于测试）
acfs_create_sdram_device(&storage, start_addr, size);
```

### 主要API

```c
// 文件系统管理
acfs_error_t acfs_init(acfs_t* acfs, storage_device_t* storage, const acfs_config_t* config);
acfs_error_t acfs_deinit(acfs_t* acfs);
acfs_error_t acfs_format(acfs_t* acfs, const acfs_config_t* config);

// 数据操作
acfs_error_t acfs_write(acfs_t* acfs, const char* data_id, const void* data, size_t size);
acfs_error_t acfs_read(acfs_t* acfs, const char* data_id, void* data, size_t size, size_t* actual_size);
acfs_error_t acfs_delete(acfs_t* acfs, const char* data_id);
bool acfs_exists(acfs_t* acfs, const char* data_id);

// 状态查询
acfs_error_t acfs_get_size(acfs_t* acfs, const char* data_id, size_t* size);
acfs_error_t acfs_get_free_space(acfs_t* acfs, size_t* free_size);
acfs_error_t acfs_get_stats(acfs_t* acfs, size_t* total_size, size_t* used_size, 
                           size_t* free_size, uint16_t* data_count);

// 维护操作
acfs_error_t acfs_check_integrity(acfs_t* acfs);
acfs_error_t acfs_defragment(acfs_t* acfs);
```

## 配置参数

| 参数 | 描述 | 范围 |
|------|------|------|
| cluster_size | 簇大小（字节） | 64-4096，必须是2的幂 |
| reserved_clusters | 系统保留簇数 | 建议2-8个 |
| format_if_invalid | 无效时是否格式化 | true/false |
| enable_crc_check | 启用CRC校验 | true/false |

## 错误码

| 错误码 | 描述 |
|--------|------|
| ACFS_OK | 成功 |
| ACFS_ERROR_INVALID_PARAM | 无效参数 |
| ACFS_ERROR_NOT_INITIALIZED | 未初始化 |
| ACFS_ERROR_NO_SPACE | 空间不足 |
| ACFS_ERROR_DATA_NOT_FOUND | 数据未找到 |
| ACFS_ERROR_DATA_CORRUPTED | 数据损坏 |
| ACFS_ERROR_IO_ERROR | IO错误 |
| ACFS_ERROR_CRC_MISMATCH | CRC校验失败 |

## 内存占用

- 系统头部: ~24字节
- 每个数据条目: ~40字节 + 簇列表
- 簇位图: 每8个簇占用1字节
- 运行时缓冲: 1个簇大小

## 性能特点

- **读取性能**: O(1) - 直接查找
- **写入性能**: O(n) - 需要簇分配
- **空间利用率**: 95%+ （取决于簇大小）
- **碎片化程度**: 低 - 自动簇管理

## 移植指南

1. 实现存储设备操作接口:
   ```c
   int (*read)(uint32_t addr, void* data, size_t size);
   int (*write)(uint32_t addr, const void* data, size_t size);
   int (*erase)(uint32_t addr, size_t size);  // 可选
   ```

2. 配置存储设备参数:
   ```c
   storage_device_t device = {
       .start_addr = /* 存储起始地址 */,
       .size = /* 存储空间大小 */,
       .type = /* 存储类型 */,
       .ops = /* 操作接口 */
   };
   ```

3. 调整配置参数以适应硬件特性

## 开发工具

```bash
# 代码格式化
make format

# 静态分析
make analyze

# 创建发布包
make dist
```

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request来改进ACFS。

## 联系方式

如有问题或建议，请通过以下方式联系:
- GitHub Issues
- Email: [您的邮箱]

---

*ACFS - 让嵌入式存储管理更简单* 