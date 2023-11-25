
/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);	/* end of "RAM" Ram type memory */

/* Memories definition */
MEMORY
{
    RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 4K
    FLASH    (rx)   : ORIGIN = 0x8000000,   LENGTH = 16K
}

/* Sections */
SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)    
        *(.text*)   
        . = ALIGN(4);
        _etext = .; 
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)  
        *(.rodata*) 
        . = ALIGN(4);
    } >FLASH

    /* Used by the startup to initialize data */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;        /* create a global symbol at data start */
        *(.data)           /* .data sections */
        *(.data*)          /* .data* sections */
        . = ALIGN(4);
        _edata = .;        /* define a global symbol at data end */

    } >RAM AT> FLASH

    . = ALIGN(4);
    .bss :
    {
        _sbss = .;         /* define a global symbol at bss start */
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;         /* define a global symbol at bss end */
        __bss_end__ = _ebss;
    } >RAM
}

