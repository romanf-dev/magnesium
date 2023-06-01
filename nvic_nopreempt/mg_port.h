//
// This port does not use preemption and utilizes PendSV as actor-executing vector.
// All actors runs sequentially in context of PendSV handler.
//

#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#if !defined (__GNUC__)
#error This header is intended to be used in GNU GCC only because of non-portable asm functions. 
#endif

#define MG_PRIO_MAX 1

#define mg_context_lock(p) { asm volatile ("cpsid i"); }
#define mg_context_unlock(p) { asm volatile ("cpsie i"); }

#define mg_queue_lock(p) { asm volatile ("cpsid i"); }
#define mg_queue_unlock(p) { asm volatile ("cpsie i"); }

#define pic_vect2prio(v) 0

#define ICSR_ADDR ((volatile unsigned int*) 0xE000ED04)
#define pic_interrupt_request(v) ((*ICSR_ADDR) = 0x10000000)

#endif

