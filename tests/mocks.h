/** 
  * @file  mocks.h
  * @brief Mocks for unit testing.
  */
#ifndef MOCKS_H
#define MOCKS_H

static bool g_req = false;

//
// By default all actors in unit tests must use single priority 0. Interrupt
// requests just set a flag to ease unit testing. The only case when priority 
// 1 is used is preemption tests. The porting layer has to be designed in a 
// way when activation of actor with priority 1 causes immediate preemption.
//
void pic_interrupt_request(unsigned int cpu, unsigned int v) {
    if (v == 1) {
        mg_context_schedule(1);        
    } else {
        g_req = true;
    }
}

#endif

