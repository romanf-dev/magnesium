
ENTRY(elf_entry)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 4096k
    RAM(rwx) : ORIGIN =  0x20000000, LENGTH = 512k
    SCRATCH_X(rwx) : ORIGIN = 0x20080000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20081000, LENGTH = 4k
}

_estack0 = ORIGIN(SCRATCH_X) + LENGTH(SCRATCH_X);
_estack1 = ORIGIN(SCRATCH_Y) + LENGTH(SCRATCH_Y);

SECTIONS
{
    .text :
    {
        PROVIDE(_vector_table = ABSOLUTE(.));
        KEEP(*(.vectors))
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
        _sbss = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        _ebss = .;
    } > RAM
}

