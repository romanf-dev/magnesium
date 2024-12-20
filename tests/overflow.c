#include <assert.h>
#include <stdbool.h>
#include "magnesium.h"
#include "mocks.h"
#include <stdio.h>

static struct mg_actor_t g_actor;
static int actor_calls = 0;

struct mg_queue_t* actor_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    actor_calls++;
    return mg_sleep_for(2, self);
}

struct mg_context_t g_mg_context;

int main(void) {
    mg_context_init();
    g_mg_context.per_cpu_data[0].ticks = 0xffffffff;
    mg_actor_init(&g_actor, actor_fn, 0, NULL);
    assert(actor_calls == 1);
    
    for (unsigned i = 0; i < 2; ++i) {
        mg_context_tick();
    }

    assert(g_req);
    mg_context_schedule(0);
    assert(actor_calls == 2);

    return 0;
}
