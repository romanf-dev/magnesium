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
#define MG_NVIC_PRIO_BITS 2 /* ARMv6-M supports only two priority bits. */
#endif

#if !defined MG_PRIO_MAX
#define MG_PRIO_MAX (1U << MG_NVIC_PRIO_BITS)
#endif 

#if !defined MG_TIMERQ_MAX
#define MG_TIMERQ_MAX 10
#endif

static inline unsigned int mg_port_clz(uint32_t x) {
    unsigned int i = 0;

    for (; i < 32; ++i) {
        if (x & (1U << (31 - i))) {
            break;
        }
    }
    
    return i;
}

#define mg_critical_section_enter() { asm volatile ("cpsid i"); }
#define mg_critical_section_leave() { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) \
    ((((volatile unsigned char*)0xE000E400)[v]) >> (8 - MG_NVIC_PRIO_BITS))

#define ISPR_ADDR ((volatile unsigned int*) 0xE000E200)
#define pic_interrupt_request(cpu, v) ((*ISPR_ADDR) = 1U << (v))

#endif

