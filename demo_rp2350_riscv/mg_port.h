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

#if __STDC_NO_ATOMICS__ == 1
#error This platform supports SMP but compiler does not provide atomic types.
#endif

#if !defined MG_PRIO_MAX
#define MG_PRIO_MAX 4 /* Only 4 spare vectors 48-51 are used. */
#endif 

#if !defined MG_TIMERQ_MAX
#define MG_TIMERQ_MAX 10
#endif

#define mg_port_clz(x) __builtin_clz(x)
#define mg_critical_section_enter() asm volatile ("csrc mstatus, 8")
#define mg_critical_section_leave() asm volatile ("csrs mstatus, 8")
#define mg_port_wait_event() asm volatile ("slt x0, x0, x0" : : : "memory")
#define mg_port_send_event() asm volatile ("slt x0, x0, x1" : : : "memory")

static inline unsigned int mg_cpu_this(void) {
    uint32_t cpu;
    asm volatile ("csrr %0, mhartid" : "=r" (cpu));
    return cpu;
}

extern unsigned int pic_vect2prio(unsigned int vect);
extern void pic_interrupt_request(unsigned int cpu, unsigned int vect);

#endif

