/** 
  * @file  magnesium.h
  * @brief Interrupt-based preemptive multitasking.
  * License: Public domain. The code is provided as is without any warranty.
  */

#ifndef _MAGNESIUM_H_
#define _MAGNESIUM_H_

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include "mg_port.h"

struct mg_list_t {
    struct mg_list_t* next;
    struct mg_list_t* prev;
};

#define mg_list_init(head) ((head)->next = (head)->prev = (head))
#define mg_list_empty(head) ((head)->next == (head))
#define mg_list_first(head) ((head)->next)
#define mg_list_last(head) ((head)->prev)
#define mg_list_entry(p, type, member) \
    ((type*)((char*)(p) - (size_t)(&((type*)0)->member)))

static inline void mg_list_append(
    struct mg_list_t* head, 
    struct mg_list_t* node
) {
    node->next = head;
    node->prev = head->prev;
    node->prev->next = node;
    head->prev = node;
}

static inline void mg_list_unlink(struct mg_list_t* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node->prev = 0;
}

struct mg_queue_t {
    struct mg_list_t items;
    int length; /* Positive length - messages, negative - actors. */
};

struct mg_message_pool_t {
    struct mg_queue_t queue; /* Must be the first member. */
    unsigned char* array;
    size_t total_length;
    size_t block_sz;
    size_t offset;
    bool array_space_available;
};

struct mg_message_t {
    struct mg_message_pool_t* parent;
    struct mg_list_t link;
};

struct mg_actor_t {
    struct mg_context_t* parent;
    struct mg_queue_t* (*func)(struct mg_actor_t* self, struct mg_message_t* m);
    unsigned vect;
    unsigned prio;
    uint32_t timeout;
    struct mg_message_t* mailbox;
    struct mg_list_t link;
};

struct mg_context_t {
    struct mg_list_t runq[MG_PRIO_MAX];
    struct mg_list_t timerq[MG_TIMERQ_MAX];
    uint32_t ticks;
    uint32_t gray_ticks;
};

extern struct mg_context_t g_mg_context;
#define MG_GLOBAL_CONTEXT() (&g_mg_context)
#define MG_ACTOR_SUSPEND ((struct mg_queue_t*) 1)

static inline void mg_context_init(void) {
    struct mg_context_t* self = MG_GLOBAL_CONTEXT();
    const size_t nqueues = sizeof(self->runq) / sizeof(self->runq[0]);
    const size_t nbuckets = sizeof(self->timerq) / sizeof(self->timerq[0]);
    self->ticks = self->gray_ticks = 0;
    
    for (size_t i = 0; i < nbuckets; ++i) {
        mg_list_init(&self->timerq[i]);
    }

    for (size_t i = 0; i < nqueues; ++i) {
        mg_list_init(&self->runq[i]);
    }
}

static inline void mg_queue_init(struct mg_queue_t* q) {
    mg_list_init(&q->items);
    q->length = 0;
}

static inline void mg_message_pool_init(
    struct mg_message_pool_t* pool, 
    void* mem, 
    size_t total_len, 
    size_t block_sz
) {
    assert(total_len >= block_sz);
    assert(block_sz >= sizeof(struct mg_message_t));
    mg_queue_init(&pool->queue);
    pool->array = mem;
    pool->total_length = total_len;
    pool->block_sz = block_sz;
    pool->offset = 0;
    pool->array_space_available = true;
}

static inline void _mg_actor_activate(struct mg_actor_t* actor) {
    struct mg_context_t* const context = actor->parent;
    mg_list_append(&context->runq[actor->prio], &actor->link);
    pic_interrupt_request(actor->vect);
}

static inline struct mg_message_t* mg_queue_pop(
    struct mg_queue_t* q, 
    struct mg_actor_t* subscriber
) {
    struct mg_message_t* msg = 0;
    mg_critical_section_enter();

    if (q->length > 0) {
        struct mg_list_t* const head = mg_list_first(&q->items);
        mg_list_unlink(head);
        msg = mg_list_entry(head, struct mg_message_t, link);
        --q->length;
    } else if (subscriber != 0) {
        mg_list_append(&q->items, &subscriber->link);
        --q->length;
    }

    mg_critical_section_leave();
    return msg;
}

static inline void mg_queue_push(
    struct mg_queue_t* q, 
    struct mg_message_t* msg
) {
    struct mg_actor_t* actor = 0;
    mg_critical_section_enter();

    if (q->length++ >= 0) {
        mg_list_append(&q->items, &msg->link);
    } else {
        struct mg_list_t* const head = mg_list_first(&q->items);
        mg_list_unlink(head);
        actor = mg_list_entry(head, struct mg_actor_t, link);
        actor->mailbox = msg;
        _mg_actor_activate(actor);
    }

    mg_critical_section_leave();
}

static inline void* mg_message_alloc(struct mg_message_pool_t* pool) {
    struct mg_message_t* msg = 0;
    mg_critical_section_enter();

    if (pool->array_space_available) {
        msg = (void*)(pool->array + pool->offset);
        pool->offset += pool->block_sz;

        if ((pool->offset + pool->block_sz) >= pool->total_length) {
            pool->array_space_available = false;
        }

        msg->parent = pool;
    }

    mg_critical_section_leave();

    if (!msg) {
        msg = mg_queue_pop(&pool->queue, 0);
    }  

    return msg;
}

static inline void mg_message_free(struct mg_message_t* msg) {
    struct mg_message_pool_t* const pool = msg->parent;
    mg_queue_push(&pool->queue, msg);
}

static inline uint32_t mg_gray_code(uint32_t x) {
    return x ^ (x >> 1);
}

static inline unsigned mg_diff_msb(uint32_t x, uint32_t y) {
    assert(x != y);
    const unsigned width = sizeof(uint32_t) * CHAR_BIT;
    const unsigned msb = width - mg_port_clz(x ^ y) - 1;
    return (msb < MG_TIMERQ_MAX) ? msb : MG_TIMERQ_MAX - 1;
}

static inline void mg_context_tick() {
    struct mg_context_t* const context  = MG_GLOBAL_CONTEXT();
    mg_critical_section_enter();
    const uint32_t gray_ticks = mg_gray_code(++context->ticks);
    const unsigned i = mg_diff_msb(context->gray_ticks, gray_ticks);
    context->gray_ticks = gray_ticks;
    struct mg_list_t* const last = mg_list_last(&context->timerq[i]);

    while (!mg_list_empty(&context->timerq[i])) {
        struct mg_list_t* const head = mg_list_first(&context->timerq[i]);
        mg_list_unlink(head);
        struct mg_actor_t* const actor = mg_list_entry(head, struct mg_actor_t, link);

        if (actor->timeout == gray_ticks) {
            actor->timeout = 0;
            _mg_actor_activate(actor);
        } else {
            const unsigned j = mg_diff_msb(actor->timeout, gray_ticks);
            mg_list_append(&context->timerq[j], &actor->link);
        }

        mg_critical_section_leave(); /* Interrupt window . */
        mg_critical_section_enter();

        if (head == last) {
            break;
        }
    }

    mg_critical_section_leave();
}

static inline void mg_actor_timeout(
    struct mg_context_t* context,
    struct mg_actor_t* actor
) {
    assert((actor->timeout != 0) && (actor->timeout < INT32_MAX));
    mg_critical_section_enter();
    const uint32_t tout = context->ticks + actor->timeout;
    const uint32_t gray_tout = mg_gray_code(tout);
    const unsigned i = mg_diff_msb(context->gray_ticks, gray_tout);
    actor->timeout = gray_tout;
    mg_list_append(&context->timerq[i], &actor->link);
    mg_critical_section_leave();
}

static inline void mg_actor_call(struct mg_actor_t* actor) {
    do {
        struct mg_queue_t* const q = actor->func(actor, actor->mailbox);
        assert(q != 0);

        if (q == MG_ACTOR_SUSPEND) {
            actor->mailbox = 0;

            if (actor->timeout) {
                mg_actor_timeout(actor->parent, actor);
                break;
            } else {                    
                continue; /* Zero timeout, just call the actor again. */
            }
        }

        actor->mailbox = mg_queue_pop(q, actor);
    } while (actor->mailbox != 0);
}

static inline void mg_actor_init(
    struct mg_actor_t* actor, 
    struct mg_queue_t* (*func)(struct mg_actor_t*, struct mg_message_t*),
    unsigned int vect,
    struct mg_queue_t* q
) {
    struct mg_context_t* const context = MG_GLOBAL_CONTEXT();
    const unsigned int prio = pic_vect2prio(vect);
    assert(prio < MG_PRIO_MAX);
    actor->func = func;
    actor->vect = vect;
    actor->prio = prio;
    actor->parent = context;
    actor->timeout = 0;
    actor->mailbox = 0;

    if (q) {
        struct mg_message_t* msg = mg_queue_pop(q, actor);
        assert(msg == 0);
    } else {
        mg_actor_call(actor);
    }
}

static inline struct mg_queue_t* mg_sleep_for(
    uint32_t delay, 
    struct mg_actor_t* self
) {
    self->timeout = delay;
    return MG_ACTOR_SUSPEND;
}

static inline void mg_context_schedule(unsigned int vect) {
    struct mg_context_t* const context = MG_GLOBAL_CONTEXT();
    const unsigned int prio = pic_vect2prio(vect);
    assert(prio < MG_PRIO_MAX);
    struct mg_list_t* const runq = &context->runq[prio];
    mg_critical_section_enter();

    while (!mg_list_empty(runq)) {
        struct mg_list_t* head = mg_list_first(runq);
        struct mg_actor_t* actor = mg_list_entry(head, struct mg_actor_t, link);
        mg_list_unlink(head);
        mg_critical_section_leave();
        mg_actor_call(actor);
        mg_critical_section_enter();
    }

    mg_critical_section_leave();
}

#define MG_ACTOR_START static int mg_state = 0; switch(mg_state) { case 0:
#define MG_ACTOR_END } return NULL
#define MG_AWAIT(q) mg_state = __LINE__; return (q); case __LINE__:

#endif

