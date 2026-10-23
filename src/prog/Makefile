CROSS ?= riscv64-unknown-elf-

hello: hello.bin

hello.bin: hello.s
	$(CROSS)gcc -march=rv32i -mabi=ilp32 -nostdlib -static \
	    -Wl,-Ttext=0x0 -Wl,--no-relax hello.s -o hello.elf
	$(CROSS)objcopy -O binary hello.elf hello.bin