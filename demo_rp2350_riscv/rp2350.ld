/*
 * Linker script for RP2350 (RISC-V).
 * This script uses 'fake' FLASH at the beginning of the RAM to keep 
 * whole image in RAM (data copying from 'flash' to ram is also kept).
 * Adjust flash address to actual flash to make it persistent.
 */

ENTRY(reset_entry)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x20000000, LENGTH = 64k
    RAM(rwx) : ORIGIN =  0x20010000, LENGTH = 448k
    SCRATCH_X(rwx) : ORIGIN = 0x20080000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20081000, LENGTH = 4k
}

_estack0 = ORIGIN(SCRATCH_X) + LENGTH(SCRATCH_X);
_estack1 = ORIGIN(SCRATCH_Y) + LENGTH(SCRATCH_Y);

SECTIONS
{
    .text :
    {
        KEEP(*(.entry_point))
        KEEP(*(.metadata_block))
        *(.text*)
        *(.rodata*)
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        _sdata = .;
        *(.data*)
        *(.sdata*)
        *(.tdata*)
        _edata = .;
    } > RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        PROVIDE(__global_pointer$ = . + 2K);
        *(.bss*)
        *(COMMON)
        *(.sbss*)
        _ebss = .;
    } > RAM
}

