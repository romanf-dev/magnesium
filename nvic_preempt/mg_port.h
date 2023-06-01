//
// This port uses preemption. Setup interrupt priorities in your main().
//

#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable asm functions. 
#endif

#if !defined MG_PRIO_MAX
#error Define MG_PRIO_MAX as maximum number of supported priorities.
#endif 

#define mg_context_lock(p) { asm volatile ("cpsid i"); }
#define mg_context_unlock(p) { asm volatile ("cpsie i"); }

#define mg_queue_lock(p) { asm volatile ("cpsid i"); }
#define mg_queue_unlock(p) { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) (((volatile unsigned char*)0xE000E400)[v])

#define STIR_ADDR ((volatile unsigned int*) 0xE000EF00)
#define pic_interrupt_request(v) ((*STIR_ADDR) = v)

#endif

