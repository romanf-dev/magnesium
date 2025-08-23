/** 
  ******************************************************************************
  *  @file   mtimer.h
  *  @brief  Helper functions RISC-V mtimer from Pico SDK.
  ******************************************************************************
  *  Copyright (c) Raspberry Pi 2024
  *  License: BSD-3-Clause.
  *****************************************************************************/

#ifndef MTIMER_H
#define MTIMER_H

static inline uint64_t riscv_timer_get_mtime(void) {
    // Read procedure from RISC-V ISA manual to avoid being off by 2**32 on
    // low half rollover -- note this loop generally executes only once, and
    // should never execute more than twice:
    uint32_t h0, l, h1;
    do {
        h0 = sio_hw->mtimeh;
        l  = sio_hw->mtime;
        h1 = sio_hw->mtimeh;
    } while (h0 != h1);
    return l | (uint64_t)h1 << 32;
}

static inline void riscv_timer_set_mtime(uint64_t mtime) {
    // This ought really only be done when the timer is stopped, but we can
    // make things a bit safer by clearing the low half of the counter, then
    // writing high half, then low half. This protects against the low half
    // rolling over, and largely avoids getting an intermediate value that is
    // higher than either the original or new value, if the timer is running.
    //
    // Note that on RP2350, mtime is shared between the two cores!(mtimcemp is
    // core-local however.)
    sio_hw->mtime  = 0;
    sio_hw->mtimeh = mtime >> 32;
    sio_hw->mtime  = mtime & 0xffffffffu;
}

static inline void riscv_timer_set_mtimecmp(uint64_t mtimecmp) {
    // Use write procedure from RISC-V ISA manual to avoid causing a spurious
    // interrupt when updating the two halves of mtimecmp.
    // No lower than original:
    sio_hw->mtimecmp  = -1u;
    // No lower than original, no lower than new (assuming new >= original):
    sio_hw->mtimecmph = mtimecmp >> 32;
    // Equal to new:
    sio_hw->mtimecmp  = mtimecmp & 0xffffffffu;
}

#endif

