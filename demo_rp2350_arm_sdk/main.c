/** 
  ******************************************************************************
  *  @file   main.c
  *  @brief  Toy example for the SMP version of the magnesium framework.
  ******************************************************************************
  *  License: BSD-2-Clause.
  *****************************************************************************/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "magnesium.h"

const uint LED_PIN = 25;

static atomic_uint g_ipi_request[MG_CPU_MAX];
static int g_doorbell_id;

unsigned int mg_cpu_this(void) {
    return get_core_num();
}

void pic_interrupt_request(unsigned int cpu, unsigned int vect) {
    if (cpu != mg_cpu_this()) {
        const unsigned int prio = pic_vect2prio(vect);
        (void) atomic_fetch_or(&g_ipi_request[cpu], 1U << prio);
        multicore_doorbell_set_other_core(g_doorbell_id);
    } else {
        irq_set_pending(vect);
    }
}

void isr_sio_bell(void) {   
    multicore_doorbell_clear_current_core(g_doorbell_id);
    const unsigned int cpu = mg_cpu_this();
    unsigned int prev = atomic_exchange(&g_ipi_request[cpu], 0);

    while (prev) {
        const unsigned int prio = 31 - mg_port_clz(prev);
        const unsigned int vect = prio + SPARE_IRQ_0;
        prev &= ~(1U << prio);
        irq_set_pending(vect);
    }
}

void isr_spare_0(void) {
    mg_context_schedule(SPARE_IRQ_0);
}

bool timer_callback(repeating_timer_t* unused) {
    mg_context_tick();
    return true;
}

struct test_message_t {
    struct mg_message_t header;
    unsigned int payload;
};

struct mg_context_t g_mg_context;
static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;

static inline void cpu_init(void) {
    uint32_t irq = multicore_doorbell_irq_num(g_doorbell_id);
    irq_set_priority(irq, 0);
    irq_set_enabled(irq, true);
    irq_set_priority(SPARE_IRQ_0, 0);
    irq_set_enabled(SPARE_IRQ_0, true);
}

struct mg_queue_t* actor1_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    static bool s_initialized = false;

    if (s_initialized) {
        struct test_message_t* const msg = mg_message_alloc(&g_pool);

        if (msg) {        
            mg_queue_push(&g_queue, &msg->header);
        }
    } else {
        s_initialized = true;
    }

    return mg_sleep_for(50, self);
}

struct mg_queue_t* actor2_fn(struct mg_actor_t *self, struct mg_message_t* restrict m) {
    static int s_led_state = 0;
    s_led_state ^= 1;
    gpio_put(LED_PIN, s_led_state);
    mg_message_free(m);
    return &g_queue;
}

void core1_entry(void) {
    cpu_init();
    
    while (1) {
        tight_loop_contents();
    }
}

int main() {
    bi_decl(bi_program_description("This is a test binary."));
    bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    g_doorbell_id = multicore_doorbell_claim_unused((1 << NUM_CORES) - 1, true);

    static struct mg_actor_t g_actor1;
    static struct mg_actor_t g_actor2;
    static struct test_message_t g_msgs[1];

    cpu_init();
    multicore_launch_core1(core1_entry);

    mg_context_init();
    mg_message_pool_init(&g_pool, &g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);
    mg_actor_init(&g_actor1, actor1_fn, SPARE_IRQ_0, 0);
    mg_actor_init(&g_actor2, actor2_fn, SPARE_IRQ_0, &g_queue);
    g_actor2.cpu = 1;
    
    repeating_timer_t timer;

    // negative timeout means exact delay (rather than delay between callbacks)
    add_repeating_timer_ms(-10, timer_callback, NULL, &timer);

    while (1) {
        tight_loop_contents();
    }
}

