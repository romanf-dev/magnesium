/** 
  * @file  main.h
  * @brief Demo application for RP2350 in RISC-V mode.
  * License: BSD-2-Clause.
  */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <hardware/structs/sio.h>
#include <hardware/structs/iobank0.h>
#include <hardware/structs/padsbank0.h>
#include "mtimer.h"
#include "magnesium.h"

#define csrr(csr, data) asm volatile ("csrr %0, " #csr : "=r" (data))
#define _csrw(csr, data) asm volatile ("csrw " #csr ", %0" : : "r" (data))
#define csrw(csr, data) _csrw(csr, data)
#define _csrrsi(csr, rd, bits) \
    asm volatile ("csrrsi %0, " #csr ", %1": "=r" (rd) : "i" (bits))
#define csrrsi(csr, rd, bits) _csrrsi(csr, rd, bits)
#define irq_disable() asm volatile ("csrc mstatus, 8")
#define irq_enable() asm volatile ("csrs mstatus, 8")

#define meiea  0x0be0
#define meifa  0x0be2
#define meipra 0x0be3

enum {
    SPARE_IRQ_MIN = 48, /* Use 48-51 range as it is within a single meiea CSR.*/
    SPARE_IRQ_MAX = 51,
    MEIPRA_WINDOW_SZ = 16,
    MEIPRA_BIT_PER_PRIO = 4,
    MEIPRA_PRIO_PER_WINDOW = MEIPRA_WINDOW_SZ / MEIPRA_BIT_PER_PRIO,
    MEIPRA_WINDOW = SPARE_IRQ_MIN / MEIPRA_PRIO_PER_WINDOW,
    MEIFA_IRQ_PER_WINDOW = 16,
    MEIEA_IRQ_PER_WINDOW = 16,
    GPIO_LED = 25,
    DOORBELL_IRQ = 26,
    MTIMER_IRQ = 29,
    MEIEA_DOORBELL_ON = 1 << ((DOORBELL_IRQ % MEIEA_IRQ_PER_WINDOW) + 16),
    MEIEA_MTIMER_ON = 1 << ((MTIMER_IRQ % MEIEA_IRQ_PER_WINDOW) + 16),
    MEI_MEIE = 1U << 11,
    CLK_PER_TICK = 12000,
};

static_assert(
    (SPARE_IRQ_MIN / MEIPRA_PRIO_PER_WINDOW) == 
        (SPARE_IRQ_MAX / MEIPRA_PRIO_PER_WINDOW)
);

static noreturn void panic(void) {
    irq_disable();
    sio_hw->gpio_set = 1U << GPIO_LED;
    for (;;);
}

void exception(void) {
    panic();
}

void __assert_func(const char *file, int ln, const char *fn, const char *expr) {
    panic();
}

unsigned pic_vect2prio(unsigned vec) {
    assert((vec >= SPARE_IRQ_MIN) && (vec <= SPARE_IRQ_MAX));
    uint32_t prio_array;
    csrrsi(meipra, prio_array, MEIPRA_WINDOW);
    const unsigned offset = (vec % MEIPRA_PRIO_PER_WINDOW) * MEIPRA_BIT_PER_PRIO;
    return (prio_array >> (offset + 16)) & ((1U << MEIPRA_BIT_PER_PRIO) - 1);
}

static atomic_uint g_ipi_request[MG_CPU_MAX];

static inline void request_local_interrupt(unsigned vect) {
    assert((vect >= SPARE_IRQ_MIN) && (vect <= SPARE_IRQ_MAX));
    const unsigned window = vect / MEIFA_IRQ_PER_WINDOW;
    const unsigned mask = 1 << (vect % MEIFA_IRQ_PER_WINDOW);
    csrw(meifa, ((mask << 16) | window));
}

void pic_interrupt_request(unsigned cpu, unsigned vect) {
    if (cpu != mg_cpu_this()) {
        const unsigned prio = pic_vect2prio(vect);
        (void) atomic_fetch_or(&g_ipi_request[cpu], 1U << prio);
        sio_hw->doorbell_out_set = 1;
    } else {
        request_local_interrupt(vect);
    }
}

static inline void doorbell_handler(void) {   
    sio_hw->doorbell_in_clr = 1;
    const unsigned cpu = mg_cpu_this();
    unsigned req = atomic_exchange(&g_ipi_request[cpu], 0);

    while (req) {
        const unsigned prio = sizeof(unsigned) * CHAR_BIT - 1 - mg_port_clz(req);
        const unsigned vect = prio + SPARE_IRQ_MIN;
        req &= ~(1U << prio);
        request_local_interrupt(vect);
    }
}

void isr_handler(uint32_t offset) {
    const unsigned vect = offset >> 2;
    
    switch (vect) {
    case DOORBELL_IRQ:
        doorbell_handler();
        break;
    case MTIMER_IRQ:
        const uint64_t current = riscv_timer_get_mtime();
        const uint64_t next = current + CLK_PER_TICK;
        riscv_timer_set_mtimecmp(next);
        mg_context_tick();
        break;
    default:
        mg_context_schedule(vect);
    };
}

static inline void per_cpu_init(void) {
    irq_disable();
    csrw(mie, MEI_MEIE);
    csrw(
        meiea, 
        MEIEA_MTIMER_ON | MEIEA_DOORBELL_ON | (MTIMER_IRQ / MEIEA_IRQ_PER_WINDOW)
    );
    const uint32_t spare_mask = (1 << (SPARE_IRQ_MAX - SPARE_IRQ_MIN + 1)) - 1;
    const uint32_t offset = (SPARE_IRQ_MIN % MEIEA_IRQ_PER_WINDOW);
    csrw(
        meiea, 
        (spare_mask << (offset + 16)) | (SPARE_IRQ_MIN / MEIFA_IRQ_PER_WINDOW)
    );
    riscv_timer_set_mtimecmp(CLK_PER_TICK);
    irq_enable();
}

struct test_message_t {
    struct mg_message_t header;
    unsigned int payload;
};

struct mg_context_t g_mg_context;
static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;

static struct mg_queue_t* actor1_fn(
    struct mg_actor_t *self, 
    struct mg_message_t* restrict m
) {
    static bool s_initialized = false;

    if (s_initialized) {
        struct test_message_t* const msg = mg_message_alloc(&g_pool);

        if (msg) {        
            mg_queue_push(&g_queue, &msg->header);
        }
    } else {
        s_initialized = true;
    }

    return mg_sleep_for(500, self);
}

static struct mg_queue_t* actor2_fn(
    struct mg_actor_t *self, 
    struct mg_message_t* restrict m
) {
    sio_hw->gpio_togl = 1U << GPIO_LED;
    mg_message_free(m);
    return &g_queue;
}

extern void gp_init(void);
extern void core1_start(void (*entry)(void));

void noreturn core1_main(void) {
    gp_init();
    per_cpu_init();

    for (;;) {
        ;
    }
}

int noreturn main(void) {
    io_bank0_hw->io[GPIO_LED].ctrl = 5;
    pads_bank0_hw->io[GPIO_LED] = 0x34;
    sio_hw->gpio_oe_set = 1U << GPIO_LED;
    sio_hw->gpio_clr = 1U << GPIO_LED;

    static struct mg_actor_t g_actor1;
    static struct mg_actor_t g_actor2;
    static struct test_message_t g_msgs[1];

    mg_context_init();
    mg_message_pool_init(&g_pool, &g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);
    mg_actor_init(&g_actor1, actor1_fn, SPARE_IRQ_MIN, 0);
    mg_actor_init(&g_actor2, actor2_fn, SPARE_IRQ_MIN, &g_queue);
    g_actor2.cpu = 1;

    sio_hw->mtime_ctrl &= ~1U;
    riscv_timer_set_mtime(0);
    sio_hw->mtime_ctrl |= 1 | (1 << 1);

    core1_start(core1_main);
    per_cpu_init();

    for (;;) {
        ;
    }
}

