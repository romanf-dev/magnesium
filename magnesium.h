/** 
  * @file  magnesium.h
  * @brief Interrupt-based preemptive multitasking.
  * License: BSD-2-Clause.
  */

#ifndef MAGNESIUM_H
#define MAGNESIUM_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include "mg_port.h"

#ifndef MG_CPU_MAX
struct mg_smp_protect_t {
    unsigned int dummy;
};

#   define MG_CPU_MAX 1
#   define mg_cpu_this() 0
#   define mg_smp_protect_init(s)
#   define mg_smp_protect_acquire(s) mg_critical_section_enter()
#   define mg_smp_protect_release(s) mg_critical_section_leave()
#else
#   include <stdatomic.h>

struct mg_smp_protect_t {
    atomic_uint spinlock;
};

static inline void mg_smp_protect_init(struct mg_smp_protect_t* s) {
    atomic_init(&s->spinlock, 0);
}

static inline void mg_smp_protect_acquire(struct mg_smp_protect_t* s) {
    mg_critical_section_enter();

    while (!atomic_compare_exchange_weak_explicit(
        &s->spinlock, 
        &(unsigned) { 0 },
        1,
        memory_order_acquire,
        memory_order_relaxed)
    ) {
        mg_port_wait_event();
    }
}

static inline void mg_smp_protect_release(struct mg_smp_protect_t* s) {
    atomic_store_explicit(&s->spinlock, 0, memory_order_release);
    mg_port_send_event();
    mg_critical_section_leave();
}
#endif

struct mg_list_t {
    struct mg_list_t* next;
    struct mg_list_t* prev;
};

#define mg_list_init(head) ((head)->next = (head)->prev = (head))
#define mg_list_empty(head) ((head)->next == (head))
#define mg_list_first(head) ((head)->next)
#define mg_list_last(head) ((head)->prev)
#define mg_list_unlink_head(head) mg_list_unlink(mg_list_first(head))
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

static inline struct mg_list_t* mg_list_unlink(struct mg_list_t* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node->prev = 0;
    return node;
}

struct mg_queue_t {
    struct mg_smp_protect_t lock;
    struct mg_list_t items;
    int length; /* Positive length - messages, negative - actors. */
};

struct mg_message_pool_t {
    struct mg_queue_t queue; /* Must be the first member. */
    unsigned char* array;
    size_t total_length;
    size_t block_sz;
    size_t offset;
    volatile bool array_space_available;
};

struct mg_message_t {
    struct mg_message_pool_t* parent;
    struct mg_list_t link;
};

struct mg_cpu_context_t {
    struct mg_smp_protect_t lock;
    struct mg_list_t runq[MG_PRIO_MAX];
    struct mg_list_t timerq[MG_TIMERQ_MAX];
    uint32_t ticks;
};

struct mg_actor_t {
    struct mg_queue_t* (*func)(struct mg_actor_t*, struct mg_message_t* restrict);
    unsigned vect;
    unsigned cpu;
    unsigned prio;
    uint32_t timeout;
    struct mg_message_t* restrict mailbox;
    struct mg_list_t link;
};

struct mg_context_t {
    struct mg_cpu_context_t per_cpu_data[MG_CPU_MAX];
};

extern struct mg_context_t g_mg_context;
#define MG_CPU_CONTEXT(cpu) (&g_mg_context.per_cpu_data[cpu])
#define MG_ACTOR_SUSPEND ((struct mg_queue_t*) 1)
#define MG_ACTOR_START static int _mg_state = 0; switch(_mg_state) { case 0:
#define MG_ACTOR_END } return NULL
#define MG_AWAIT(q) _mg_state = __LINE__; return (q); case __LINE__:

static inline void mg_context_init(void) {
    for (unsigned cpu = 0; cpu < MG_CPU_MAX; ++cpu) {
        struct mg_cpu_context_t* const self = MG_CPU_CONTEXT(cpu);
        self->ticks = 0;
        mg_smp_protect_init(&self->lock);
        
        for (size_t i = 0; i < MG_TIMERQ_MAX; ++i) {
            mg_list_init(&self->timerq[i]);
        }

        for (size_t i = 0; i < MG_PRIO_MAX; ++i) {
            mg_list_init(&self->runq[i]);
        }
    }
}

static inline void mg_queue_init(struct mg_queue_t* q) {
    mg_list_init(&q->items);
    mg_smp_protect_init(&q->lock);
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

static inline void _mg_actor_insert(struct mg_actor_t* actor) {
    assert(actor->cpu < MG_CPU_MAX);
    struct mg_cpu_context_t* const context = MG_CPU_CONTEXT(actor->cpu);
    mg_smp_protect_acquire(&context->lock);
    mg_list_append(&context->runq[actor->prio], &actor->link);
    mg_smp_protect_release(&context->lock);
}

static inline void _mg_actor_activate(struct mg_actor_t* actor) {
    _mg_actor_insert(actor);
    pic_interrupt_request(actor->cpu, actor->vect);
}

static inline struct mg_message_t* restrict mg_queue_pop(
    struct mg_queue_t* q, 
    struct mg_actor_t* subscriber
) {
    struct mg_message_t* restrict msg = 0;
    mg_smp_protect_acquire(&q->lock);

    if (q->length > 0) {
        struct mg_list_t* const head = mg_list_unlink_head(&q->items);
        msg = mg_list_entry(head, struct mg_message_t, link);
        --q->length;
    } else if (subscriber != 0) {
        mg_list_append(&q->items, &subscriber->link);
        --q->length;
    }

    mg_smp_protect_release(&q->lock);
    return msg;
}

static inline void mg_queue_push(
    struct mg_queue_t* q, 
    struct mg_message_t* restrict msg
) {
    struct mg_actor_t* actor = 0;
    mg_smp_protect_acquire(&q->lock);

    if (q->length++ >= 0) {
        mg_list_append(&q->items, &msg->link);
    } else {
        struct mg_list_t* const head = mg_list_unlink_head(&q->items);
        actor = mg_list_entry(head, struct mg_actor_t, link);
        actor->mailbox = msg;    
    }

    mg_smp_protect_release(&q->lock);

    if (actor) {
        _mg_actor_activate(actor);
    }
}

static inline void* mg_message_alloc(struct mg_message_pool_t* pool) {
    struct mg_message_t* msg = 0;

    if (pool->array_space_available) {
        mg_smp_protect_acquire(&pool->queue.lock);

        if (pool->array_space_available) {
            msg = (void*)(pool->array + pool->offset);
            msg->parent = pool;
            pool->offset += pool->block_sz;
            pool->array_space_available = 
                ((pool->offset + pool->block_sz) <= pool->total_length);
        }

        mg_smp_protect_release(&pool->queue.lock);
    }

    if (!msg) {
        msg = mg_queue_pop(&pool->queue, 0);
    }  

    return msg;
}

static inline void mg_message_free(struct mg_message_t* restrict msg) {
    struct mg_message_pool_t* const pool = msg->parent;
    mg_queue_push(&pool->queue, msg);
}

static inline unsigned _mg_diff_msb(uint32_t x, uint32_t y) {
    assert(x != y);
    const unsigned width = sizeof(uint32_t) * CHAR_BIT;
    const unsigned msb = width - mg_port_clz(x ^ y) - 1;
    return (msb < MG_TIMERQ_MAX) ? msb : MG_TIMERQ_MAX - 1;
}

static inline void mg_context_tick(void) {
    struct mg_cpu_context_t* const context = MG_CPU_CONTEXT(mg_cpu_this());
    mg_smp_protect_acquire(&context->lock);
    const uint32_t oldticks = context->ticks++;
    const unsigned i = _mg_diff_msb(oldticks, context->ticks);
    struct mg_list_t* const last = mg_list_last(&context->timerq[i]);

    while (!mg_list_empty(&context->timerq[i])) {
        struct mg_actor_t* actor_to_wake = 0;
        struct mg_list_t* const head = mg_list_unlink_head(&context->timerq[i]);
        struct mg_actor_t* const actor = mg_list_entry(head, struct mg_actor_t, link);

        if (actor->timeout == context->ticks) {
            actor->timeout = 0;
            actor_to_wake = actor;
        } else {
            const unsigned j = _mg_diff_msb(actor->timeout, context->ticks);
            mg_list_append(&context->timerq[j], &actor->link);
        }

        mg_smp_protect_release(&context->lock);

        if (actor_to_wake) {    
            _mg_actor_activate(actor_to_wake);
        }

        mg_smp_protect_acquire(&context->lock);

        if (head == last) {
            break;
        }
    }

    mg_smp_protect_release(&context->lock);
}

static inline void _mg_actor_timeout(struct mg_actor_t* actor) {
    assert((actor->timeout != 0) && (actor->timeout < INT32_MAX));
    struct mg_cpu_context_t* const context = MG_CPU_CONTEXT(mg_cpu_this());
    mg_smp_protect_acquire(&context->lock);
    actor->timeout += context->ticks;
    const unsigned i = _mg_diff_msb(context->ticks, actor->timeout);
    mg_list_append(&context->timerq[i], &actor->link);
    mg_smp_protect_release(&context->lock);
}

static inline void mg_actor_call(struct mg_actor_t* actor) {
    do {
        struct mg_queue_t* const q = actor->func(actor, actor->mailbox);
        assert(q != 0);

        if (q == MG_ACTOR_SUSPEND) {
            actor->mailbox = 0;

            if (actor->timeout != 0) {
                _mg_actor_timeout(actor);    
            } else {                    
                _mg_actor_insert(actor);
            }

            break;
        }

        actor->mailbox = mg_queue_pop(q, actor);
    } while (actor->mailbox != 0);
}

static inline void mg_actor_init(
    struct mg_actor_t* actor, 
    struct mg_queue_t* (*func)(struct mg_actor_t*, struct mg_message_t* restrict),
    unsigned int vect,
    struct mg_queue_t* q
) {
    actor->prio = pic_vect2prio(vect);
    assert(actor->prio < MG_PRIO_MAX);
    actor->func = func;
    actor->vect = vect;
    actor->cpu = mg_cpu_this();
    actor->timeout = 0;
    actor->mailbox = 0;

    if (q) {
        struct mg_message_t* msg = mg_queue_pop(q, actor);
        assert(msg == 0);
    } else if (func) {
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

static inline struct mg_actor_t* _mg_context_pop_head(unsigned vect, bool* last) {
    struct mg_cpu_context_t* const context = MG_CPU_CONTEXT(mg_cpu_this());
    const unsigned prio = pic_vect2prio(vect);
    assert(prio < MG_PRIO_MAX);
    struct mg_list_t* const runq = &context->runq[prio];
    struct mg_actor_t* actor = 0;
    mg_smp_protect_acquire(&context->lock);

    if (!mg_list_empty(runq)) {
        struct mg_list_t* const head = mg_list_unlink_head(runq);
        actor = mg_list_entry(head, struct mg_actor_t, link);
        *last = mg_list_empty(runq);
    }

    mg_smp_protect_release(&context->lock);
    return actor;
}

static inline void mg_context_schedule(unsigned vect) {
    struct mg_actor_t* actor = 0;

    while ((actor = _mg_context_pop_head(vect, &(bool) { false }))) {
        mg_actor_call(actor);
    }
}

#endif
