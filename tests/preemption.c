#include <assert.h>
#include <stdbool.h>
#include "magnesium.h"
#include "mocks.h"
#include <stdio.h>

static struct mg_message_t g_msgs[1];
static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;
static struct mg_actor_t g_actor1;
static struct mg_actor_t g_actor2;
struct mg_context_t g_mg_context;

static bool actor1_started = false;
static bool actor2_started = false;

struct mg_queue_t* actor1_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    actor1_started = true;
    assert(!actor2_started);
    mg_queue_push(&g_queue, m);
    assert(actor2_started);
    actor1_started = false;
    return &g_queue;
}

struct mg_queue_t* actor2_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    actor2_started = true;
    assert(actor1_started && actor2_started);
    mg_message_free(m);
    return &g_queue;
}

int main(void) {

    mg_context_init();
    mg_message_pool_init(&g_pool, &g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);
    mg_actor_init(&g_actor1, actor1_fn, 0, &g_queue); 
    mg_actor_init(&g_actor2, actor2_fn, 1, &g_queue); 
    struct mg_message_t* const m = mg_message_alloc(&g_pool);
    assert(m);
    assert(!g_req);
    mg_queue_push(&g_queue, m);
    assert(g_req);
    mg_context_schedule(0);

    return 0;
}

