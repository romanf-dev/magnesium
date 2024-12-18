#
# Simple makefile for compiling all .c and .s files in the current folder.
# No dependency tracking, use make clean if a header is changed.
#

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

GCC_PREFIX ?= arm-none-eabi-
MG_DIR ?= ..

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -ffunction-sections -mcpu=cortex-m4 -Wall -O2 -mthumb -I . -I $(MG_DIR) -I $(MG_DIR)/nvic -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m4 -mthumb -c -o $@ $<

all : clean $(OBJS) startup_stm32f401xc.o
	$(GCC_PREFIX)ld -nostdlib --gc-sections -T LinkerScript.ld -o demo.elf $(OBJS) startup_stm32f401xc.o

clean:
	rm -f *.o *.elf

