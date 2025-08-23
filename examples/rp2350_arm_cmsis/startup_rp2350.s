/** 
  ******************************************************************************
  *  @file   startup_rp2350.s
  *  @brief  Startup code and vector table for RP2350.
  ******************************************************************************
  *  License: BSD-2-Clause.
  *****************************************************************************/

.syntax unified
.cpu cortex-m33
.fpu fpv5-sp-d16
.thumb
.global elf_entry

.section .vectors
    .word _estack0
    .word reset_entry
    .word unused_isr
    .word hard_fault_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word 0
    .word 0
    .word 0
    .word 0
    .word unused_isr
    .word unused_isr
    .word 0
    .word unused_isr
    .word systick_isr

    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word doorbell_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word unused_isr
    .word scheduling_isr
    .word scheduling_isr
    .word scheduling_isr
    .word scheduling_isr
    .word unused_isr
    .word unused_isr

.section .metadata_block
    .word 0xffffded3
    .word 0x10210142
    .word 0x000001ff
    .word 0x00000000
    .word 0xab123579

.section .text

.type reset_entry,%function
.thumb_func
reset_entry:
    ldr     r0, =_sidata
    ldr     r1, =_sdata
    ldr     r2, =_edata
data_copying:
    cmp     r1, r2
    beq     data_copy_done
    ldr     r3, [r0], #4
    str     r3, [r1], #4
    b       data_copying
data_copy_done:
    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
bss_zeroing:
    cmp     r0, r1
    beq     bss_zero_done
    str     r2, [r0], #4
    b       bss_zeroing
bss_zero_done:
    b       main

.type unused_isr,%function
.thumb_func
unused_isr:
    b       .

.type elf_entry,%function
.thumb_func
elf_entry:
    mov     r0, #0          // bootrom vtable offset
    ldr     r1, =0xE000ED08 // VTOR address
    str     r0, [r1]
    ldmia   r0!, {r1, r2}
    msr     msp, r1
    bx      r2

