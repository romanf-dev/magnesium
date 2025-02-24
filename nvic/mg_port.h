/** 
  * @file  mg_port.h
  * @brief Magnesium porting layer for ARM NVIC interrupt controller.
  * License: Public domain. The code is provided as is without any warranty.
  */
#ifndef MG_PORT_H
#define MG_PORT_H

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable asm functions. 
#endif

#if !defined MG_NVIC_PRIO_BITS
#define MG_NVIC_PRIO_BITS 3 /* Three bits are guaranteed to be implemented. */
#endif

#if !defined MG_PRIO_MAX
#define MG_PRIO_MAX (1U << MG_NVIC_PRIO_BITS)
#endif 

#if !defined MG_TIMERQ_MAX
#define MG_TIMERQ_MAX 10
#endif

#define mg_port_clz(x) __builtin_clz(x)
#define mg_critical_section_enter() { asm volatile ("cpsid i"); }
#define mg_critical_section_leave() { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) \
    ((((volatile unsigned char*)0xE000E400)[v]) >> (8 - MG_NVIC_PRIO_BITS))

#ifndef MG_CPU_MAX
#   define STIR_ADDR ((volatile unsigned*) 0xE000EF00)
#   define pic_interrupt_request(cpu, v) ((*STIR_ADDR) = v)
#else
#   if __STDC_NO_ATOMICS__ == 1
#   error Compiler does not support atomic types.
#   endif
#   define mg_port_wait_event() asm volatile("wfe")
#   define mg_port_send_event() asm volatile("sev")

extern unsigned int mg_cpu_this(void);
extern void pic_interrupt_request(unsigned int cpu, unsigned int vect);

#endif
#endif

