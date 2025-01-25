/** 
  * @file  startup.s
  * @brief Startup code for RISC-V core in RP2350.
  * License: BSD-2-Clause.
  */

.section .entry_point
    csrc    mstatus, 8
    la      a0, _vectors + 1
    csrw    mtvec, a0
    call    gp_init
    csrr    a0, mhartid
    bnez    a0, reset_entry
    la      sp, _estack0
    la      t0, _sbss
    la      t1, _ebss
bss_loop:
    beq     t0, t1, data_loop
    sb      zero, 0(t0)
    addi    t0, t0, 1
    j       bss_loop
data_loop:
    la      t0, _sidata
    la      t1, _sdata
    la      t2, _edata
data_init:
    beq     t1, t2, init_done
    lb      t3, (t0)
    sb      t3, (t1)
    addi    t0, t0, 1
    addi    t1, t1, 1
    j       data_init
init_done:
    call    main

.global gp_init
gp_init:
.option push
.option norelax
    la      gp, __global_pointer$
.option pop
    ret

.global reset_entry
reset_entry:
    li      a0, 0x7dfc      /* Bootrom entry. */
    jr      a0

.section .metadata_block    /* See 5.9.5.2 in the datasheet. */

.word   0xffffded3
.word   0x11010142
.word   0x000001ff
.word   0x00000000
.word   0xab123579

.section .data              /* Vector table is in SRAM. */

.global _vectors
_vectors:
.option push
.option norvc
.option norelax
    j       machine_exception_entry
.word       0
.word       0
    j       .   /* softirqs aren't used. */
.word       0
.word       0
.word       0
    j       .   /* mtimer isn't used. */
.word       0
.word       0
.word       0
.option pop

.equ meicontext, 0x0be5
.equ meinext, 0x0be4

    addi    sp, sp, -80
    sw      ra,  0(sp)
    sw      t0,  4(sp)
    sw      t1,  8(sp)
    sw      t2, 12(sp)
    sw      a0, 16(sp)
    sw      a1, 20(sp)
    sw      a2, 24(sp)
    sw      a3, 28(sp)
    sw      a4, 32(sp)
    sw      a5, 36(sp)
    sw      a6, 40(sp)
    sw      a7, 44(sp)
    sw      t3, 48(sp)
    sw      t4, 52(sp)
    sw      t5, 56(sp)
    sw      t6, 60(sp)
    csrr    a0, mepc
    csrr    a1, mstatus
    sw      a0, 64(sp)
    sw      a1, 68(sp)
save_meicontext:
    csrr    a2, meicontext
    sw      a2, 72(sp)
get_first_irq:
    csrrsi  a0, meinext, 1   /* meinext.update */
    bltz    a0, no_more_irqs
dispatch_irq:
    csrsi   mstatus, 8
    call    isr_handler
    csrci   mstatus, 8
get_next_irq:
    csrrsi  a0, meinext, 1   /* meinext.update */
    bgez    a0, dispatch_irq
no_more_irqs:
    lw      a0, 64(sp)
    lw      a1, 68(sp)
    lw      a2, 72(sp)
    csrw    mepc, a0
    csrw    mstatus, a1
    csrw    meicontext, a2
    lw      ra,  0(sp)
    lw      t0,  4(sp)
    lw      t1,  8(sp)
    lw      t2, 12(sp)
    lw      a1, 20(sp)
    lw      a2, 24(sp)
    lw      a3, 28(sp)
    lw      a4, 32(sp)
    lw      a5, 36(sp)
    lw      a6, 40(sp)
    lw      a7, 44(sp)
    lw      t3, 48(sp)
    lw      t4, 52(sp)
    lw      t5, 56(sp)
    lw      t6, 60(sp)
check_irq_before_exit:
    csrr    a0, meinext
    bgez    a0, save_meicontext
    lw      a0, 16(sp)
    addi    sp, sp, 80
    mret

machine_exception_entry:
    call    exception

