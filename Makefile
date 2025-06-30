# ACFS Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude
SRCDIR = src
OBJDIR = obj
BINDIR = bin
TESTDIR = test
EXAMPLEDIR = examples

# 源文件
SOURCES = $(SRCDIR)/acfs_core.c $(SRCDIR)/acfs_crc.c $(SRCDIR)/acfs_storage.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# 目标文件
LIBRARY = $(BINDIR)/libacfs.a
TEST_BIN = $(BINDIR)/test_acfs
EXAMPLE_BIN = $(BINDIR)/basic_usage

# 默认目标
all: $(LIBRARY) $(TEST_BIN) $(EXAMPLE_BIN)

# 创建目录
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# 编译目标文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 创建静态库
$(LIBRARY): $(OBJECTS) | $(BINDIR)
	ar rcs $@ $(OBJECTS)

# 编译测试程序
$(TEST_BIN): $(TESTDIR)/test_acfs.c $(LIBRARY) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BINDIR) -lacfs -o $@

# 编译示例程序
$(EXAMPLE_BIN): $(EXAMPLEDIR)/basic_usage.c $(LIBRARY) | $(BINDIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BINDIR) -lacfs -o $@

# 运行测试
test: $(TEST_BIN)
	./$(TEST_BIN)

# 运行示例
example: $(EXAMPLE_BIN)
	./$(EXAMPLE_BIN)

# 清理
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# 安装
install: $(LIBRARY)
	mkdir -p /usr/local/lib
	mkdir -p /usr/local/include
	cp $(LIBRARY) /usr/local/lib/
	cp include/acfs.h /usr/local/include/

# 卸载
uninstall:
	rm -f /usr/local/lib/libacfs.a
	rm -f /usr/local/include/acfs.h

# 打包发布
dist: clean
	tar -czf acfs.tar.gz --exclude='.git' .

# 格式化代码
format:
	find src include test examples -name "*.c" -o -name "*.h" | xargs clang-format -i

# 静态分析
analyze:
	cppcheck --enable=all --suppress=missingIncludeSystem src/ include/

# 调试版本
debug: CFLAGS += -DDEBUG -O0
debug: all

# 发布版本
release: CFLAGS += -DNDEBUG -O3
release: all

# 交叉编译（ARM）
arm: CC = arm-linux-gnueabihf-gcc
arm: all

# 帮助信息
help:
	@echo "ACFS 构建系统"
	@echo ""
	@echo "可用目标:"
	@echo "  all      - 编译所有目标 (默认)"
	@echo "  test     - 编译并运行测试"
	@echo "  example  - 编译并运行示例"
	@echo "  clean    - 清理构建文件"
	@echo "  install  - 安装到系统"
	@echo "  uninstall- 从系统卸载"
	@echo "  dist     - 创建发布包"
	@echo "  format   - 格式化代码"
	@echo "  analyze  - 静态代码分析"
	@echo "  debug    - 编译调试版本"
	@echo "  release  - 编译发布版本"
	@echo "  arm      - 交叉编译ARM版本"
	@echo "  help     - 显示此帮助信息"

.PHONY: all test example clean install uninstall dist format analyze debug release arm help 