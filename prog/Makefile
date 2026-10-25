CROSS ?= riscv64-unknown-elf-
PROGS := hello stress

.PHONY: all clean distclean $(PROGS)

all: $(PROGS:%=%.bin)

$(PROGS): %: %.bin

%.elf: %.s
	$(CROSS)gcc -march=rv32i -mabi=ilp32 -nostdlib -static \
	    -Wl,-Ttext=0x0 -Wl,--no-relax $< -o $@

%.bin: %.elf
	$(CROSS)objcopy -O binary $< $@

clean:
	rm -f *.elf

distclean: clean
	rm -f *.bin
