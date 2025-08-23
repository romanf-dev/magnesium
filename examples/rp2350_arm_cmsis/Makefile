#
# Simple makefile for compiling all .c files in the current folder.
#

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
GCC_PREFIX ?= arm-none-eabi-
MG_PATH ?= ../..
PICOTOOL_PATH ?= echo To get uf2 use picotool 

.PHONY: all clean

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m33 -mcmse -mthumb -O2 -DMG_CPU_MAX=2 \
	-I $(MG_PATH) -I $(MG_PATH)/nvic -I CMSIS/Device/RP2350/Include -I CMSIS/Core/Include \
	-Wall -Wl,--build-id=none -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m33 -mthumb -c -o $@ $<

all : clean $(OBJS) startup_rp2350.o
	$(GCC_PREFIX)ld -nostdlib --gc-sections -T script.ld -o demo.elf $(OBJS) startup_rp2350.o
	$(PICOTOOL_PATH) uf2 convert --verbose demo.elf demo.uf2 --family rp2350-arm-s

clean:
	rm -f *.o *.elf *.uf2

