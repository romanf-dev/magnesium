//
// Simple port for test purposes.
//

#ifndef _MG_PORT_H_
#define _MG_PORT_H_

#define mg_context_lock(p)
#define mg_context_unlock(p)

#define mg_queue_lock(p)
#define mg_queue_unlock(p)

#define pic_vect2prio(v) 0
#define pic_interrupt_request(v)

#endif

