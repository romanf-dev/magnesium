/** 
  * @file  mg_port.h
  * @brief Porting layer for unit tests.
  * License: Public domain. The code is provided as is without any warranty.
  */
#ifndef MG_PORT_H
#define MG_PORT_H

#define MG_PRIO_MAX 2
#define MG_TIMERQ_MAX 10

#define mg_port_clz(x) __builtin_clz(x)
#define pic_vect2prio(v) (v)
#define mg_critical_section_enter()
#define mg_critical_section_leave() 
#define mg_cpu_this() 0

extern void pic_interrupt_request(unsigned int cpu, unsigned int vect);

#endif

