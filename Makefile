.PHONY: all clean run

all:
	$(MAKE) -C code

run:
	$(MAKE) -C code run

clean:
	$(MAKE) -C code clean
