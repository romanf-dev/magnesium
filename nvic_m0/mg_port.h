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

#if !defined MG_PRIO_MAX
#error Define MG_PRIO_MAX as maximum number of supported priorities.
#endif 

#if !defined MG_NVIC_PRIO_BITS
#error Define MG_NVIC_PRIO_BITS as maximum number of supported preemption priorities for the target chip.
#endif

#define mg_critical_section_enter() { asm volatile ("cpsid i"); }
#define mg_critical_section_leave() { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) \
    ((((volatile unsigned char*)0xE000E400)[v]) >> (8 - MG_NVIC_PRIO_BITS))

#define ISPR_ADDR ((volatile unsigned int*) 0xE000E200)
#define pic_interrupt_request(v) ((*ISPR_ADDR) = 1U << (v))

#endif

