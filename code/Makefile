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

# 默认 benchmark 参数（你之后可以直接改这里）
N      := 10000
M      := 32
ROUNDS := 1
# IMPL   := array
# IMPL   := list
IMPL   := skip

.PHONY: all run clean

# make
all: $(TARGET)

# 编译
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

# make run
run: $(TARGET)
	./$(TARGET) --n $(N) --m $(M) --rounds $(ROUNDS) --impl $(IMPL)

# make clean
clean:
	rm -f $(TARGET)
