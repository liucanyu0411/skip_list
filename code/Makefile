CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11

TARGET  := bench

SRCS := \
	benchmark.c \
	bptree.c \
	nodestore.c \
	nodestore_array.c \
	nodestore_list.c \
	nodestore_skip.c \
	skiplist.c

# ===== 默认 benchmark 参数（从 testcases 文件读）=====
M      := 32
ROUNDS := 1
IMPL   := skip
TAG    :=

# ===== testcase 相关 =====
# 约定：$(TESTCASE_DIR)/$(CASE)/ 下包含三份文件：ins.txt / q.txt / del.txt
TESTCASE_DIR := ../testcases
CASE         := 1

INS_FILE := $(TESTCASE_DIR)/$(CASE)/ins.txt
QRY_FILE := $(TESTCASE_DIR)/$(CASE)/q.txt
DEL_FILE := $(TESTCASE_DIR)/$(CASE)/del.txt

# CSV 输出：默认 stdout；想写文件就 make run CSV=out.csv
CSV :=

.PHONY: all run test clean print

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

# 打印当前会用到的路径，方便你确认 testcases 组织结构是否匹配
print:
	@echo "TARGET   = $(TARGET)"
	@echo "M        = $(M)"
	@echo "ROUNDS   = $(ROUNDS)"
	@echo "IMPL     = $(IMPL)"
	@echo "TAG      = $(TAG)"
	@echo "CASE     = $(CASE)"
	@echo "INS_FILE = $(INS_FILE)"
	@echo "QRY_FILE = $(QRY_FILE)"
	@echo "DEL_FILE = $(DEL_FILE)"
	@echo "CSV      = $(CSV)"

# make run：跑一次（默认 CASE=1，文件为 ../testcases/1/ins.txt q.txt del.txt）
# 示例：
#   make run IMPL=array M=128 ROUNDS=5 CASE=3
#   make run IMPL=skip  M=64  ROUNDS=3 CASE=1 CSV=out.csv TAG=exp1
run: $(TARGET)
	@if [ ! -f "$(INS_FILE)" ]; then echo "Missing insert file: $(INS_FILE)"; exit 1; fi
	@if [ ! -f "$(QRY_FILE)" ]; then echo "Missing search file: $(QRY_FILE)"; exit 1; fi
	@if [ ! -f "$(DEL_FILE)" ]; then echo "Missing delete file: $(DEL_FILE)"; exit 1; fi
	@if [ -n "$(CSV)" ]; then \
		./$(TARGET) --m $(M) --impl $(IMPL) --insert "$(INS_FILE)" --search "$(QRY_FILE)" --delete "$(DEL_FILE)" --rounds $(ROUNDS) --csv "$(CSV)" --tag "$(TAG)"; \
	else \
		./$(TARGET) --m $(M) --impl $(IMPL) --insert "$(INS_FILE)" --search "$(QRY_FILE)" --delete "$(DEL_FILE)" --rounds $(ROUNDS) --tag "$(TAG)"; \
	fi

# make test：保留一个别名，等价于 run（为了你之前的使用习惯）
test: run

clean:
	rm -f $(TARGET)
