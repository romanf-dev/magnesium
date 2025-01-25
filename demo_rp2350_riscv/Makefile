#
# Simple makefile for compiling all .c files in the current folder.
#

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

GCC_PREFIX ?= riscv32-unknown-elf-
PICOTOOL_PATH ?= echo "\e[33mPICOTOOL_PATH is not defined.\e[0m" To get uf2 use picotool
MG_PATH ?= ..

.PHONY: clean
.PHONY: bin

%.o: %.c
	$(GCC_PREFIX)gcc -ffreestanding -march=rv32ima_zicsr_zbb -Wall -Wl,--build-id=none -O2 \
	-DMG_CPU_MAX=2 -I $(MG_PATH) -I . -c -o $@ $<

%.o: %.s
	$(GCC_PREFIX)gcc -march=rv32ima_zicsr -c -I . -o $@ $<

all: clean $(OBJS) startup.o
	$(GCC_PREFIX)ld -nostdlib -T rp2350.ld -o demo.elf $(OBJS) startup.o
	$(PICOTOOL_PATH) uf2 convert --verbose demo.elf demo.uf2 --family 0xe48bff5a

clean:
	rm -f *.o *.elf *.uf2

