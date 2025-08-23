#
# Simple makefile for compiling all .c and .s files in the current folder.
#

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
GCC_PREFIX ?= arm-none-eabi-
MG_PATH ?= ../..

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -ffunction-sections -mcpu=cortex-m3 -Wall -O2 \
	-DMG_NVIC_PRIO_BITS=4 -DSTM32F103xB -mthumb -I . -I $(MG_PATH) -I $(MG_PATH)/nvic -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m3 -mthumb -c -o $@ $<

all : clean $(OBJS) startup_stm32f103c8tx.o
	$(GCC_PREFIX)ld -nostdlib --gc-sections -T LinkerScript.ld -o demo.elf $(OBJS) startup_stm32f103c8tx.o

clean:
	rm -f *.o *.elf

