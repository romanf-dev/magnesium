#include <assert.h>
#include <stdbool.h>
#include "magnesium.h"
#include "mocks.h"
#include <stdio.h>

static struct mg_actor_t g_actor1;
static struct mg_actor_t g_actor2;
struct mg_context_t g_mg_context;

static unsigned actor1_counter = 0;
static unsigned actor2_counter = 0;

struct mg_queue_t* actor1_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    UNUSED_ARG(m);
    MG_ACTOR_START;

    for (;;) {
        MG_AWAIT(mg_sleep_for(1, self));
        ++actor1_counter;
        MG_AWAIT(mg_sleep_for(0, self));
        assert((actor1_counter == 1) && (actor2_counter == 1));
        ++actor1_counter;
        MG_AWAIT(mg_sleep_for(0, self));
        assert((actor1_counter == 2) && (actor2_counter == 2));
        ++actor1_counter;
        MG_AWAIT(mg_sleep_for(0, self));
        assert((actor1_counter == 3) && (actor2_counter == 3));
    }
    
    MG_ACTOR_END;
}

struct mg_queue_t* actor2_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    UNUSED_ARG(m);
    MG_ACTOR_START;

    for (;;) {
        MG_AWAIT(mg_sleep_for(1, self));
        assert((actor1_counter == 1) && (actor2_counter == 0));
        ++actor2_counter;
        MG_AWAIT(mg_sleep_for(0, self));
        ++actor2_counter;
        assert((actor1_counter == 2) && (actor2_counter == 2));
        MG_AWAIT(mg_sleep_for(0, self));
        ++actor2_counter;
        assert((actor1_counter == 3) && (actor2_counter == 3));
        MG_AWAIT(mg_sleep_for(0, self));
    }
    
    MG_ACTOR_END;
}

int main(void) {
    mg_context_init();
    mg_actor_init(&g_actor1, actor1_fn, 0, NULL); 
    mg_actor_init(&g_actor2, actor2_fn, 0, NULL); 
    mg_context_tick();
    assert(g_req);
    mg_context_schedule(0);
    assert((actor1_counter == 3) && (actor2_counter == 3));
    return 0;
}

