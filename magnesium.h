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
#include "mg_port.h"

struct mg_list_t {
    struct mg_list_t* next;
    struct mg_list_t* prev;
};

#define mg_list_init(head) ((head)->next = (head)->prev = (head))
#define mg_list_empty(head) ((head)->next == (head))
#define mg_list_first(head) ((head)->next)
#define mg_list_entry(p, type, member) \
    ((type*)((char*)(p) - (size_t)(&((type*)0)->member)))

static inline void mg_list_append(
    struct mg_list_t* head, 
    struct mg_list_t* node) {

    node->next = head;
    node->prev = head->prev;
    node->prev->next = node;
    head->prev = node;
}

static inline void mg_list_remove(struct mg_list_t* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node->prev = 0;
}

struct mg_queue_t {
    struct mg_list_t items;
    int length; /* Positive length - messages, negative - actors */
};

struct mg_message_pool_t {
    struct mg_queue_t queue;
    unsigned char* array;
    size_t total_length;
    size_t block_sz;
    size_t offset;
    bool array_space_available;
};

_Static_assert(
    offsetof(struct mg_message_pool_t, queue) == 0U, 
    "queue must be the first member of the pool"
);

struct mg_message_t {
    struct mg_message_pool_t* parent;
    struct mg_list_t link;
};

struct mg_actor_t {
    struct mg_context_t* parent;
    struct mg_queue_t* (*func)(struct mg_actor_t* self, struct mg_message_t* m);
    unsigned int vect;
    unsigned int prio;
    struct mg_message_t* mailbox;
    struct mg_list_t link;
};

struct mg_context_t {
    struct mg_list_t runq[MG_PRIO_MAX];
};

extern struct mg_context_t g_mg_context;
#define mg_get_context() (&g_mg_context)

static inline void mg_context_init(void) {
    struct mg_context_t* context = mg_get_context();
    const size_t runq_num = (sizeof(context->runq) / sizeof(context->runq[0])); 

    for (size_t i = 0; i < runq_num; ++i) {
        mg_list_init(&context->runq[i]);
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
    size_t block_sz) {

    assert(total_len >= block_sz);
    assert(block_sz >= sizeof(struct mg_message_t));
    mg_queue_init(&pool->queue);
    pool->array = mem;
    pool->total_length = total_len;
    pool->block_sz = block_sz;
    pool->offset = 0;
    pool->array_space_available = true;
}

static inline struct mg_message_t* mg_queue_pop(
    struct mg_queue_t* q, 
    struct mg_actor_t* subscriber) {

    struct mg_message_t* msg = 0;
    mg_queue_lock(q);

    if (q->length > 0) {
        struct mg_list_t* const head = mg_list_first(&q->items);
        mg_list_remove(head);
        msg = mg_list_entry(head, struct mg_message_t, link);
    } 
    else if (subscriber != 0) {
        mg_list_append(&q->items, &subscriber->link);
    }

    --q->length;
    mg_queue_unlock(q);

    return msg;
}

static inline void mg_queue_push(
    struct mg_queue_t* q, 
    struct mg_message_t* msg) {

    struct mg_actor_t* actor = 0;
    mg_queue_lock(q);

    if (q->length >= 0) {
        mg_list_append(&q->items, &msg->link);
    } 
    else {
        struct mg_list_t* const head = mg_list_first(&q->items);
        mg_list_remove(head);
        actor = mg_list_entry(head, struct mg_actor_t, link);
        actor->mailbox = msg;
    }

    ++q->length;
    mg_queue_unlock(q);

    if (actor) {
        struct mg_context_t* const context = actor->parent;
        mg_context_lock(context);
        mg_list_append(&context->runq[actor->prio], &actor->link);
        pic_interrupt_request(actor->vect);
        mg_context_unlock(context);
    }
}

static inline void mg_actor_init(
    struct mg_actor_t* actor, 
    struct mg_queue_t* (*func)(struct mg_actor_t*, struct mg_message_t*), 
    unsigned int vect, 
    struct mg_queue_t* q) {

    struct mg_context_t* const context = mg_get_context();
    const unsigned int prio = pic_vect2prio(vect);
    assert(prio < MG_PRIO_MAX);
    actor->func = func;
    actor->vect = vect;
    actor->prio = prio;
    actor->parent = context;
    actor->mailbox = 0;
    const struct mg_message_t* const msg = mg_queue_pop(q, actor);
    assert(msg == 0);
}

static inline void* mg_message_alloc(struct mg_message_pool_t* pool) {
    struct mg_message_t* msg = 0;
    mg_queue_lock(q);

    if (pool->array_space_available) {
        msg = (void*)(pool->array + pool->offset);
        pool->offset += pool->block_sz;

        if ((pool->offset + pool->block_sz) >= pool->total_length) {
            pool->array_space_available = false;
        }

        msg->parent = pool;
    }

    mg_queue_unlock(q);

    if (!msg) {
        msg = mg_queue_pop(&pool->queue, 0);
    }  

    return msg;
}

static inline void mg_message_free(struct mg_message_t* msg) {
    struct mg_message_pool_t* const pool = msg->parent;
    mg_queue_push(&pool->queue, msg);
}

static inline void mg_context_schedule(unsigned int vect) {
    struct mg_context_t* const context = mg_get_context();
    const unsigned int prio = pic_vect2prio(vect);
    assert(prio < MG_PRIO_MAX);
    struct mg_list_t* const runq = &context->runq[prio];
    mg_context_lock(context);

    while (!mg_list_empty(runq)) {
        struct mg_list_t* const head = mg_list_first(runq);
        struct mg_actor_t* const actor = mg_list_entry(
            head, 
            struct mg_actor_t, 
            link
        );
        mg_list_remove(head);
        mg_context_unlock(context);

        do {
            struct mg_queue_t* const q = actor->func(actor, actor->mailbox);
            assert(q != 0);
            actor->mailbox = mg_queue_pop(q, actor);
        } while (actor->mailbox != 0);

        mg_context_lock(context);
    }

    mg_context_unlock(context);
}

#define MG_ACTOR_START static int mg_state = 0; switch(mg_state) { case 0:
#define MG_ACTOR_END } return NULL
#define MG_AWAIT(queue) mg_state = __LINE__; return (&(queue)); case __LINE__: 

#endif

