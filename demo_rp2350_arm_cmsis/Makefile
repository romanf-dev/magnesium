#
# Simple makefile for compiling all .c files in the current folder.
#

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

GCC_PREFIX ?= arm-none-eabi-
MG_DIR ?= ..

.PHONY: all clean

ifndef PICO_SDK_PATH
$(error PICO_SDK_PATH is not set)
endif

ifndef PICOTOOL_PATH
$(error PICOTOOL_PATH is not set)
endif

%.o : %.c
	$(GCC_PREFIX)gcc -ffreestanding -mcpu=cortex-m33 -mcmse -mthumb -O2 -DMG_CPU_MAX=2 \
	-I $(MG_DIR) -I $(MG_DIR)/nvic \
	-I $(PICO_SDK_PATH)/src/rp2_common/cmsis/stub/CMSIS/Device/RP2350/Include \
	-I $(PICO_SDK_PATH)/src/rp2_common/cmsis/stub/CMSIS/Core/Include \
	-I $(PICO_SDK_PATH)/src/rp2_common/cmsis/include \
	-Wall -Wl,--build-id=none -c -o $@ $<

%.o : %.s
	$(GCC_PREFIX)gcc -mcpu=cortex-m33 -mthumb -c -o $@ $<

all : clean $(OBJS) startup_rp2350.o
	$(GCC_PREFIX)ld -nostdlib --gc-sections -T script.ld -o demo.elf $(OBJS) startup_rp2350.o
	$(PICOTOOL_PATH) uf2 convert --verbose demo.elf demo.uf2 --family rp2350-arm-s --abs-block

clean:
	rm -f *.o *.elf *.uf2

