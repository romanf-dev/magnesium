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
#include "RP2350.h"

static inline bool multicore_fifo_rvalid(void) {
    return SIO->FIFO_ST & 1;
}

static inline bool multicore_fifo_wready(void) {
    return SIO->FIFO_ST & 2;
}

static inline void multicore_fifo_push_blocking(uint32_t data) {
    while (!multicore_fifo_wready()) {
        asm volatile ("nop");
    }

    SIO->FIFO_WR = data;
    asm volatile ("sev");
}

static inline void multicore_fifo_drain(void) {
    while (multicore_fifo_rvalid()) {
        (void) SIO->FIFO_RD;
    }
}

static inline uint32_t multicore_fifo_pop_blocking(void) {
    while (!multicore_fifo_rvalid()) {
       asm volatile ("wfe");
    }

    return SIO->FIFO_RD;
}

void core1_start(void (*fn)(void)) {
    extern uint8_t _estack1;
    extern uint8_t _vector_table;
    const uintptr_t core1_stack = (uintptr_t) &_estack1;
    const uintptr_t core1_vtor = (uintptr_t) &_vector_table;
    const uint32_t cmd_sequence[] = { 
        0, 0, 1, core1_vtor, core1_stack, (uintptr_t) fn
    };
    const size_t n = sizeof(cmd_sequence) / sizeof(cmd_sequence[0]);
    unsigned int seq = 0;

    do {
        const unsigned int cmd = cmd_sequence[seq];

        if (!cmd) {
            multicore_fifo_drain();
            asm volatile ("sev");
        }

        multicore_fifo_push_blocking(cmd);
        const uint32_t response = multicore_fifo_pop_blocking();
        seq = cmd == response ? seq + 1 : 0;
    } while (seq < n);
}

