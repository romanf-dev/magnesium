/** 
  ******************************************************************************
  *  @file   main.c
  *  @brief  Toy example for the SMP version of the magnesium framework.
  ******************************************************************************
  *  License: BSD-2-Clause.
  *****************************************************************************/

#include <stdint.h>
#include <stdalign.h>
#include <stdnoreturn.h>
#include "RP2350.h"
#include "magnesium.h"

enum {
    SPAREIRQ_IRQ_0 = 46, // Spare irq vector. See 3.8.6.1.2 in the datasheet.
    SYSTICK_VAL = 12000,
    EXTEXCLALL = 1 << 29,// See ACTLR bits in 3.7.5 in the datasheet.
};

//
// Since RP2350 can't send arbitrary interrupts from once core to another the 
// process is two-staged:
// - Requesting core atomically sets a bit corresponding to a target actor
//   priority. Since maximum supported priority is single uint is enough.
// - Requesting core sends doorbell interrupt to the target core.
// - Target core inside its doorbell ISR handler atomically gets request bits
//   and sets the variable to 0. If any other requests are happen between 
//   the request and doorbell ISR activation they would be preserved in the 
//   bitmask.
// - Doorbell ISR maps priorities back to vectors and use standard NVIC 
//   mechanisms to make appropriate interrupts pending as the ISR runs on the
//   target core.
//

static atomic_uint g_ipi_request[MG_CPU_MAX];

unsigned int mg_cpu_this(void) {
    return SIO->CPUID & 1;
}

void pic_interrupt_request(unsigned int cpu, unsigned int vect) {
    if (cpu != mg_cpu_this()) {
        const unsigned int prio = pic_vect2prio(vect);
        (void) atomic_fetch_or(&g_ipi_request[cpu], 1U << prio);
        SIO->DOORBELL_OUT_SET = 1;
    } else {
        NVIC->STIR = vect;
    }
}

void doorbell_isr(void) {   
    SIO->DOORBELL_IN_CLR = 1;
    const unsigned int cpu = mg_cpu_this();
    unsigned int prev = atomic_exchange(&g_ipi_request[cpu], 0);

    while (prev) {
        const unsigned int prio = 31 - mg_port_clz(prev);
        const unsigned int vect = prio + SPAREIRQ_IRQ_0;
        prev &= ~(1U << prio);
        NVIC->STIR = vect;
    }
}

struct test_message_t {
    struct mg_message_t header;
    unsigned int payload;
};

struct mg_context_t g_mg_context;
static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;

//
// Scheduling ISR must be installed on all vectors reserved for actors.
//
void scheduling_isr(void) {
    const unsigned int vect = (__get_IPSR() & IPSR_ISR_Msk) - 16;
    mg_context_schedule(vect);
}

//
// Systick is used as a tick source on both cores.
//
void systick_isr(void) {
    mg_context_tick();
}

//
// It is crucial to have identical priorities for scheduling vectors on both
// cores so it is a good idea to have separate function for this purpose.
//
static inline void cpu_init(void) {
    SCnSCB->ACTLR |= EXTEXCLALL;
    NVIC_SetPriorityGrouping(3);
    NVIC_EnableIRQ(SIO_IRQ_BELL_IRQn);
    NVIC_EnableIRQ(SPAREIRQ_IRQ_0);   
}

//
// Actor1 runs on core0 and sends messages to actor2 every 100 ticks.
// Message payload is not used at now it is just a signal.
//
struct mg_queue_t* actor1_fn(struct mg_actor_t *self, struct mg_message_t* m) {
    static bool s_initialized = false;

    if (s_initialized) {
        struct test_message_t* const msg = mg_message_alloc(&g_pool);

        if (msg) {        
            mg_queue_push(&g_queue, &msg->header);
        }
    } else {
        s_initialized = true;
    }

    return mg_sleep_for(100, self);
}

//
// Actor2 runs on core1 and toggles the LED on each message it receives.
//
struct mg_queue_t* actor2_fn(struct mg_actor_t *self, struct mg_message_t* m) {
    SIO->GPIO_OUT_XOR = 1U << 25;
    mg_message_free(m);
    return &g_queue;
}

void core1_start(void (*entry)(void));

void core1_entry() {
    cpu_init();
    __enable_irq();
    SysTick_Config(SYSTICK_VAL);

    while(1) {
        __WFI(); 
    }
}

static noreturn void panic(void) {
    for(;;);
}

void hard_fault_isr(void) {
    panic();
}

void __assert_func(const char *file, int ln, const char *fn, const char *expr) {
    panic();
}

void main(void) {
    IO_BANK0->GPIO25_CTRL = 5;
    PADS_BANK0->GPIO25 = 0x34;
    SIO->GPIO_OE_SET = 1U << 25;
    SIO->GPIO_OUT_SET = 1U << 25;

    core1_start(core1_entry);
    cpu_init();

    static struct mg_actor_t g_actor1;
    static struct mg_actor_t g_actor2;
    static struct test_message_t g_msgs[1];

    mg_context_init();
    mg_message_pool_init(&g_pool, &g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);
    mg_actor_init(&g_actor1, actor1_fn, SPAREIRQ_IRQ_0, 0);
    mg_actor_init(&g_actor2, actor2_fn, SPAREIRQ_IRQ_0, &g_queue);
    g_actor2.cpu = 1;

    SysTick_Config(SYSTICK_VAL);

    for (;;) {
        __WFI();
    }
}

