/** 
  * @file  magnesium.h
  * @brief Interrupt-based preemptive multitasking.
  * License: Public domain. The code is provided as is without any warranty.
  */

#ifndef _MAGNESIUM_H_
#define _MAGNESIUM_H_

#include <stddef.h>
#include <assert.h>
#include "mg_port.h"

struct mg_list_t {
    struct mg_list_t* next;
    struct mg_list_t* prev;
};

#define mg_list_init(head) ((head)->next = (head)->prev = (head))
#define mg_list_empty(head) ((head)->next == (head))
#define mg_list_first(head) ((head)->next)
#define mg_list_entry(p, type, member) ((type*)((char*)(p) - (size_t)(&((type*)0)->member)))

static inline void mg_list_append(struct mg_list_t* head, struct mg_list_t* node) {
    node->next = head;
    node->prev = head->prev;
    node->prev->next = node;
    head->prev = node;
}

static inline void mg_list_remove(struct mg_list_t* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node->prev = (void*)0;
}

struct mg_queue_t {
    struct mg_list_t items;
    unsigned int ready;
};

struct mg_message_pool_t {
    struct mg_queue_t queue;
    unsigned char* bytes;
    size_t length;
    size_t size;
    size_t offset;
    unsigned int avail;
};

struct mg_message_t {
    struct mg_message_pool_t* parent;
    struct mg_list_t link;
};

struct mg_actor_t {
    struct mg_context_t* parent;
    struct mg_queue_t* (*func)(struct mg_actor_t* self, struct mg_message_t* msg);
    unsigned int vect;
    unsigned int prio;
    struct mg_message_t* mailbox;
    struct mg_list_t link;
};

struct mg_context_t {
    struct mg_list_t runq[MG_PRIO_MAX];
    struct mg_actor_t* _Atomic active;
};

extern struct mg_context_t g_mg_context;
#define mg_get_context() (&g_mg_context)

void mg_context_init(void) {
    struct mg_context_t* c = mg_get_context();

    c->active = (void*)0;
    for (size_t i = 0; i < (sizeof(c->runq) / sizeof(c->runq[0])); ++i) {
        mg_list_init(&c->runq[i]);
    }
}

void mg_queue_init(struct mg_queue_t* q) {
    mg_list_init(&q->items);
    q->ready = 1;
}

void mg_message_pool_init(struct mg_message_pool_t* pool, void* mem, size_t len, size_t blk_sz) {
    assert(len >= blk_sz);
    pool->bytes = mem;
    pool->length = len;
    pool->size = blk_sz;
    pool->offset = 0;
    pool->avail = 1;
    mg_queue_init(&pool->queue);
}

struct mg_actor_t* mg_actor_self(void) {
    return mg_get_context()->active;
}

struct mg_message_t* mg_queue_pop(struct mg_queue_t* q, unsigned int subscribe) {
    struct mg_message_t* m = (void*)0;
    mg_queue_lock(q);
    if (!mg_list_empty(&q->items) && q->ready) {
        struct mg_list_t* const t = mg_list_first(&q->items);
        mg_list_remove(t);
        m = mg_list_entry(t, struct mg_message_t, link);
    } else if (subscribe) {
        struct mg_context_t* const c = mg_get_context();
        struct mg_actor_t* const a = c->active;
        assert(a != (void*)0);
        mg_list_append(&q->items, &a->link);
        q->ready = 0;
    }
    mg_queue_unlock(q);

    return m;
}

void mg_queue_push(struct mg_queue_t* q, struct mg_message_t* m) {
    struct mg_actor_t* a = (void*)0;
    mg_queue_lock(q);
    if (q->ready) {
        mg_list_append(&q->items, &m->link);
    } else {
        struct mg_list_t* const t = mg_list_first(&q->items);
        mg_list_remove(t);
        a = mg_list_entry(t, struct mg_actor_t, link);
        a->mailbox = m;
        if (mg_list_empty(&q->items)) {
            q->ready = 1;
        }
    }
    mg_queue_unlock(q);

    if (a) {
        struct mg_context_t* const c = a->parent;
        mg_context_lock(c);
        mg_list_append(&c->runq[a->prio], &a->link);
        pic_interrupt_request(a->vect);
        mg_context_unlock(c);
    }
}

static inline void _mg_call(struct mg_context_t* c, struct mg_actor_t* a) {
    struct mg_actor_t* const prev = c->active;
    c->active = a;
    while ((a->mailbox = mg_queue_pop(a->func(a, a->mailbox), 1)) != (void*)0) {
    }
    c->active = prev;
}

void mg_actor_init(
    struct mg_actor_t* a, 
    struct mg_queue_t* (*f)(struct mg_actor_t*, struct mg_message_t*), 
    unsigned int vect, 
    struct mg_message_t* m) {

    struct mg_context_t* const c = mg_get_context();
    a->func = f;
    a->vect = vect;
    a->prio = pic_vect2prio(vect);
    a->parent = c;
    a->mailbox = m;
    _mg_call(c, a);
}

struct mg_message_t* mg_message_alloc(struct mg_message_pool_t* pool) {
    struct mg_message_t* m = (void*)0;
    mg_queue_lock(q);
    if (pool->avail) {
        m = (void*)(pool->bytes + pool->offset);
        pool->offset += pool->size;
        if ((pool->offset + pool->size) >= pool->length) {
            pool->avail = 0;
        }
        m->parent = pool;
    } 
    mg_queue_unlock(q);
    if (!m) {
        m = mg_queue_pop(&pool->queue, 0);
    }  

    return m;
}

void mg_message_free(struct mg_message_t* msg) {
    struct mg_message_pool_t* const pool = msg->parent;
    mg_queue_push(&pool->queue, msg);
}

void mg_context_schedule(unsigned int vect) {
    struct mg_context_t* const c = mg_get_context();
    struct mg_list_t* const runq = &c->runq[pic_vect2prio(vect)];
    mg_context_lock(c);
    while (!mg_list_empty(runq)) {
        struct mg_list_t* const t = mg_list_first(runq);
        struct mg_actor_t* const a = mg_list_entry(t, struct mg_actor_t, link);
        mg_list_remove(t);
        mg_context_unlock(c);
        _mg_call(c, a);
        mg_context_lock(c);
    }
    mg_context_unlock(c);
}

#endif

