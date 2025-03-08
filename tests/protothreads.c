#include <assert.h>
#include <stdbool.h>
#include "magnesium.h"
#include "mocks.h"
#include <stdio.h>

static struct mg_message_t g_msgs[1];
static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;
static struct mg_actor_t g_actor;
struct mg_context_t g_mg_context;

static bool actor_started = false;
static bool msg_received = false;

struct mg_queue_t* actor_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    UNUSED_ARG(self);
    UNUSED_ARG(m);
    MG_ACTOR_START;

    for (;;) {
        actor_started = true;
        MG_AWAIT(&g_queue);
        msg_received = true;
        MG_AWAIT(&g_queue);
    }
    
    MG_ACTOR_END;
}

int main(void) {
    mg_context_init();
    mg_message_pool_init(&g_pool, &g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);
    mg_actor_init(&g_actor, actor_fn, 0, 0); 
    struct mg_message_t* const m = mg_message_alloc(&g_pool);
    assert(m);
    assert(actor_started);
    assert(!g_req);
    mg_queue_push(&g_queue, m);
    assert(g_req && !msg_received);
    mg_context_schedule(0);
    assert(msg_received);

    return 0;
}

