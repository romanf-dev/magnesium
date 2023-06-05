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
    node->next = node->prev = 0;
}

struct mg_queue_t {
    struct mg_list_t items;
    bool item_type_is_msg;
};

struct mg_message_pool_t {
    struct mg_queue_t queue;
    unsigned char* array;
    size_t total_length;
    size_t block_sz;
    size_t offset;
    bool array_space_available;
};

_Static_assert(offsetof(struct mg_message_pool_t, queue) == 0U, "queue must be the first member of the pool");

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
    struct mg_actor_t* _Atomic active_actor;
};

extern struct mg_context_t g_mg_context;
#define mg_get_context() (&g_mg_context)

void mg_context_init(void) {
    struct mg_context_t* c = mg_get_context();

    c->active_actor = 0;
    for (size_t i = 0; i < (sizeof(c->runq) / sizeof(c->runq[0])); ++i) {
        mg_list_init(&c->runq[i]);
    }
}

void mg_queue_init(struct mg_queue_t* q) {
    mg_list_init(&q->items);
    q->item_type_is_msg = true;
}

void mg_message_pool_init(struct mg_message_pool_t* pool, void* mem, size_t total_len, size_t block_sz) {
    assert(total_len >= block_sz);
    pool->array = mem;
    pool->total_length = total_len;
    pool->block_sz = block_sz;
    pool->offset = 0;
    pool->array_space_available = true;
    mg_queue_init(&pool->queue);
}

struct mg_actor_t* mg_actor_self(void) {
    return mg_get_context()->active_actor;
}

struct mg_message_t* mg_queue_pop(struct mg_queue_t* q, bool subscribe) {
    struct mg_message_t* msg = 0;

    mg_queue_lock(q);

    if (!mg_list_empty(&q->items) && q->item_type_is_msg) {
        struct mg_list_t* const head = mg_list_first(&q->items);
        mg_list_remove(head);
        msg = mg_list_entry(head, struct mg_message_t, link);
    } 
    else if (subscribe) {
        struct mg_context_t* const context = mg_get_context();
        struct mg_actor_t* const actor = context->active_actor;
        assert(actor != 0);
        mg_list_append(&q->items, &actor->link);
        q->item_type_is_msg = false;
    }

    mg_queue_unlock(q);

    return msg;
}

void mg_queue_push(struct mg_queue_t* q, struct mg_message_t* msg) {
    struct mg_actor_t* actor = 0;

    mg_queue_lock(q);

    if (q->item_type_is_msg) {
        mg_list_append(&q->items, &msg->link);
    } 
    else {
        struct mg_list_t* const head = mg_list_first(&q->items);

        mg_list_remove(head);
        actor = mg_list_entry(head, struct mg_actor_t, link);
        actor->mailbox = msg;

        if (mg_list_empty(&q->items)) {
            q->item_type_is_msg = true;
        }
    }

    mg_queue_unlock(q);

    if (actor) {
        struct mg_context_t* const context = actor->parent;

        mg_context_lock(context);
        mg_list_append(&context->runq[actor->prio], &actor->link);
        pic_interrupt_request(actor->vect);
        mg_context_unlock(context);
    }
}

static inline void _mg_call(struct mg_context_t* context, struct mg_actor_t* actor) {
    struct mg_actor_t* const prev = context->active_actor;
    context->active_actor = actor;
    while ((actor->mailbox = mg_queue_pop(actor->func(actor, actor->mailbox), true)) != 0) {
    }
    context->active_actor = prev;
}

void mg_actor_init(
    struct mg_actor_t* actor, 
    struct mg_queue_t* (*func)(struct mg_actor_t*, struct mg_message_t*), 
    unsigned int vect, 
    struct mg_message_t* msg) {

    struct mg_context_t* const context = mg_get_context();

    actor->func = func;
    actor->vect = vect;
    actor->prio = pic_vect2prio(vect);
    actor->parent = context;
    actor->mailbox = msg;
    _mg_call(context, actor);
}

void* mg_message_alloc(struct mg_message_pool_t* pool) {
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
        msg = mg_queue_pop(&pool->queue, false);
    }  

    return msg;
}

void mg_message_free(struct mg_message_t* msg) {
    struct mg_message_pool_t* const pool = msg->parent;

    mg_queue_push(&pool->queue, msg);
}

void mg_context_schedule(unsigned int vect) {
    struct mg_context_t* const context = mg_get_context();
    struct mg_list_t* const runq = &context->runq[pic_vect2prio(vect)];

    mg_context_lock(context);

    while (!mg_list_empty(runq)) {
        struct mg_list_t* const head = mg_list_first(runq);
        struct mg_actor_t* const actor = mg_list_entry(head, struct mg_actor_t, link);

        mg_list_remove(head);
        mg_context_unlock(context);
        _mg_call(context, actor);
        mg_context_lock(context);
    }

    mg_context_unlock(context);
}

#endif

