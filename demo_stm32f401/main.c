/** 
  ******************************************************************************
  *  @file   main.c
  *  @brief  Toy example of magnesium actor framework.
  ******************************************************************************
  *  License: Public domain.
  *****************************************************************************/

#include <stdint.h>
#include <stdalign.h>
#include <stdnoreturn.h>
#include "stm32f4xx.h"
#include "magnesium.h"

//
// Errors and exceptions turn onboard LED on.
//
static noreturn void Error_Handler(void) {
    __disable_irq();

    for(;;);
}

//
// Override hardware exceptions.
//
void HardFault_Handler(void) {
    Error_Handler();
}

//
// Library dependency...
//
void __assert_func(const char *file, int line, const char *fn, const char *expr) {
    Error_Handler();
}

//
// Global data.
//
static struct example_msg_t {
    struct mg_message_t header;
} g_msgs[1];

static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;

struct mg_context_t g_mg_context;

//
// This is name for 0th IRQ which we use for actor execution. 
// Any unused vector may be used.
//
void WWDG_IRQHandler(void) {
    mg_context_schedule(0);
}

//
// Systick sends notification to queue at every tick.
//
void SysTick_Handler(void) {
    mg_context_tick();
}

//
// Actor sends messages to another actor.
//
static struct mg_queue_t* sender(struct mg_actor_t* self, struct mg_message_t* restrict m) {
    MG_ACTOR_START;
    
    for (;;) {      
        MG_AWAIT(mg_sleep_for(100, self));
        struct example_msg_t* msg = mg_message_alloc(&g_pool);

        if (msg) {
            mg_queue_push(&g_queue, &msg->header);
        }
    }

    MG_ACTOR_END;
}

//
// Actor switches LED state once new message arrives.
//
static struct mg_queue_t* receiver(struct mg_actor_t* self, struct mg_message_t* restrict m) {
    MG_ACTOR_START;
    
    for (;;) {
        MG_AWAIT(&g_queue);     
        GPIOC->BSRRL = 1U << 13;
        mg_message_free(m);

        MG_AWAIT(&g_queue);
        GPIOC->BSRRH = 1U << 13;
        mg_message_free(m);
    }

    MG_ACTOR_END;
}

//
// Entry point...
//
int main(void) {
    RCC->CR |= RCC_CR_HSEON;

    while((RCC->CR & RCC_CR_HSERDY) == 0) {
        ;
    }
    
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
    RCC->PLLCFGR = (0x27U << 24) | RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLP_0 | (336U << 6) | 25U;
    RCC->CR |= RCC_CR_PLLON;

    while((RCC->CR & RCC_CR_PLLRDY) == 0) {
        ;
    }
    
    RCC->CFGR = (RCC->CFGR | RCC_CFGR_SW_PLL) & ~RCC_CFGR_SW_HSE;

    while((RCC->CFGR & RCC_CFGR_SWS_PLL) == 0) {
        ;
    }

    RCC->CR &= ~RCC_CR_HSION;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER |= GPIO_MODER_MODER13_0;

    mg_context_init();
    mg_message_pool_init(&g_pool, g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);

    //
    // Create sender actor. It sends message every 50ms to the receiver.
    //
    static struct mg_actor_t actor_sender;
    mg_actor_init(&actor_sender, &sender, 0, 0);

    //
    // Receiver. It blinks the LED.
    //
    static struct mg_actor_t actor_receiver;
    mg_actor_init(&actor_receiver, &receiver, 0, 0);

    //
    // Enable interrupt source.
    //
    NVIC_SetPriorityGrouping(4);
    NVIC_EnableIRQ(0);

    //
    // Enable Systick to trigger interrupt every 1ms.
    //
    SysTick_Config(84000);

    for (;;);

    return 0; /* make compiler happy. */
}

