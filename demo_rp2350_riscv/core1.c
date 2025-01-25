/** 
  ******************************************************************************
  *  @file   core1.c
  *  @brief  Helper functions for core1 startup from Pico SDK.
  ******************************************************************************
  *  Copyright (c) Raspberry Pi 2024
  *  License: BSD-3-Clause.
  *****************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <hardware/structs/sio.h>

#define __NOP() asm volatile ("nop")
#define __WFE() asm volatile ("slt x0, x0, x0" : : : "memory")
#define __SEV() asm volatile ("slt x0, x0, x1" : : : "memory")

static inline bool multicore_fifo_rvalid(void) {
    return sio_hw->fifo_st & 1;
}

static inline bool multicore_fifo_wready(void) {
    return sio_hw->fifo_st & 2;
}

static inline void multicore_fifo_push_blocking(uint32_t data) {
    while (!multicore_fifo_wready()) {
        __NOP();
    }

    sio_hw->fifo_wr = data;
    __SEV();
}

static inline void multicore_fifo_drain(void) {
    while (multicore_fifo_rvalid()) {
        (void) sio_hw->fifo_rd;
    }
}

static inline uint32_t multicore_fifo_pop_blocking(void) {
    while (!multicore_fifo_rvalid()) {
       __WFE();
    }

    return sio_hw->fifo_rd;
}

void core1_start(void (*entry)(void)) {
    extern uint32_t _estack1;
    extern uint32_t _vectors;
    const uintptr_t core1_stack = (uintptr_t) &_estack1;
    const uintptr_t core1_mtvec = (uintptr_t) (&_vectors) + 1;
    const uint32_t cmd_sequence[] = {
        0, 0, 1, core1_mtvec, core1_stack, (uintptr_t) entry
    };
    const size_t n = sizeof(cmd_sequence) / sizeof(cmd_sequence[0]);
    unsigned int seq = 0;

    do {
        const unsigned int cmd = cmd_sequence[seq];

        if (!cmd) {
            multicore_fifo_drain();
            __SEV();
        }

        multicore_fifo_push_blocking(cmd);
        const uint32_t response = multicore_fifo_pop_blocking();
        seq = cmd == response ? seq + 1 : 0;
    } while (seq < n);
}

