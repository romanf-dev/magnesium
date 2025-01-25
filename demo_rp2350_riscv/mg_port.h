/** 
  * @file  mg_port.h
  * @brief Magnesium porting layer for Hazard3.
  * License: BSD-2-Clause.
  */

#ifndef MG_PORT_H
#define MG_PORT_H

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable asm functions. 
#endif

#if !defined MG_PRIO_MAX
#define MG_PRIO_MAX 4
#endif 

#if !defined MG_TIMERQ_MAX
#define MG_TIMERQ_MAX 10
#endif

#define mg_port_clz(x) __builtin_clz(x)
#define mg_critical_section_enter() asm volatile ("csrc mstatus, 8")
#define mg_critical_section_leave() asm volatile ("csrs mstatus, 8")

static inline unsigned int mg_cpu_this(void) {
    uint32_t cpu;
    asm volatile ("csrr %0, mhartid" : "=r" (cpu));
    return cpu;
}

extern unsigned int pic_vect2prio(unsigned int vect);
extern void pic_interrupt_request(unsigned int cpu, unsigned int vect);

#if __STDC_NO_ATOMICS__ == 1
#error Compiler does not support atomic types.
#endif

#include <stdatomic.h>

struct mg_smp_protect_t {
    atomic_uint spinlock;
};

static inline void mg_smp_protect_init(struct mg_smp_protect_t* s) {
    atomic_init(&s->spinlock, 0);
}

static inline void mg_smp_protect_acquire(struct mg_smp_protect_t* s) {
    unsigned int expected = 0;
    mg_critical_section_enter();

    while (!atomic_compare_exchange_weak_explicit(
        &s->spinlock, 
        &expected, 
        1,
        memory_order_acquire,
        memory_order_relaxed)
    ) {
        expected = 0;
        asm volatile ("slt x0, x0, x0" : : : "memory");
    }
}

static inline void mg_smp_protect_release(struct mg_smp_protect_t* s) {
    atomic_store_explicit(&s->spinlock, 0, memory_order_release);
    asm volatile ("slt x0, x0, x1" : : : "memory");
    mg_critical_section_leave();
}

#endif

