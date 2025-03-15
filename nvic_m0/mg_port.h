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

/*
 * GCC does not generate MUL for multiplication because of possibly inaccurate
 * result as its higher part is not stored, so use inline asm.
 */
static inline uint32_t _mul(uint32_t a, uint32_t b) {
    asm volatile ("mul %0, %1, %2" : "=r" (a) : "r" (a), "Il" (b));
    return a;
}

/*
 * Software CLZ based on deBruijn seqence. It is assumed v != 0.
 */
unsigned int mg_port_clz(uint32_t v) {
    static const uint8_t hash[] = {
        31, 30, 3, 29, 2, 17, 7, 28, 1, 9, 11, 16, 6, 14, 27, 23,
        0, 4, 18, 8, 10, 12, 15, 24, 5, 19, 13, 25, 20, 26, 21, 22
    };
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v -= v >> 1;
    return hash[_mul(v, 0x077CB531U) >> 27];
}

#define mg_critical_section_enter() { asm volatile ("cpsid i"); }
#define mg_critical_section_leave() { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) \
    ((((volatile unsigned char*)0xE000E400)[v]) >> (8 - MG_NVIC_PRIO_BITS))

#define ISPR_ADDR ((volatile unsigned int*) 0xE000E200)
#define pic_interrupt_request(cpu, v) ((*ISPR_ADDR) = 1U << (v))

#endif

