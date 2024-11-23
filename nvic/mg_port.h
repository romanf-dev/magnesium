/** 
  * @file  mg_port.h
  * @brief Magnesium porting layer for ARM NVIC interrupt controller.
  * License: Public domain. The code is provided as is without any warranty.
  */
#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable asm functions. 
#endif

#if !defined MG_NVIC_PRIO_BITS
#define MG_NVIC_PRIO_BITS 3 /* Three bits are guaranteed to be implemented in ARMv7-M. */
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

#define STIR_ADDR ((volatile unsigned int*) 0xE000EF00)
#define pic_interrupt_request(cpu, v) ((*STIR_ADDR) = v)

#endif

