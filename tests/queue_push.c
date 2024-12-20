#include <assert.h>
#include <stdbool.h>
#include "magnesium.h"
#include "mocks.h"
#include <stdio.h>

static struct test_message_t {
    struct mg_message_t header;
    unsigned int payload;
} g_msgs[1];

static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;
static struct mg_actor_t g_actor;
static bool actor_started = false;

struct mg_queue_t* actor_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    struct test_message_t* const msg = (struct test_message_t*) m;
    assert(msg->payload == 0xc0cac01a);
    actor_started = true;
    mg_message_free(m);
    return &g_queue;
}

struct mg_context_t g_mg_context;

int main(void) {

    mg_context_init();
    mg_message_pool_init(&g_pool, &g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);
    mg_actor_init(&g_actor, actor_fn, 0, &g_queue); 
    struct test_message_t* const m = mg_message_alloc(&g_pool);
    assert(m);
    m->payload = 0xc0cac01a;
    assert(!g_req);
    mg_queue_push(&g_queue, &m->header);
    assert(g_req);
    mg_context_schedule(0);
    assert(actor_started);

    return 0;
}
