.PHONY: all run test clean

# 默认：编译
all:
	$(MAKE) -C code

# 运行默认 benchmark（随机数据）
run:
	$(MAKE) -C code run

# 用 testcase 跑（支持传 IMPL / CASE）
test:
	$(MAKE) -C code test IMPL=$(IMPL) CASE=$(CASE)

# 清理
clean:
	$(MAKE) -C code clean
