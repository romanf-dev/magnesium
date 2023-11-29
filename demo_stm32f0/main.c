/** 
  ******************************************************************************
  *  @file   main.c
  *  @brief  Toy example of magnesium actor framework.
  ******************************************************************************
  *  License: Public domain.
  *****************************************************************************/

#include <stdint.h>
#include <stdnoreturn.h>
#include "stm32f0xx.h"
#include "magnesium.h"

//
// Errors and exceptions are mapped to infinite loop.
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
void __assert_func(const char *file, int line, const char *func, const char *expr) {
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
// This is the name for 0th IRQ which we use for actor execution. 
// Any unused vector may be used.
//
void WWDG_IRQHandler(void) {
    mg_context_schedule(0);
}

//
// Systick is used to supply internal timers.
//
void SysTick_Handler(void) {
    mg_context_tick();
}

//
// Actor sends messages to another actor.
//
static struct mg_queue_t* sender(struct mg_actor_t* self, struct mg_message_t* m) {
    MG_ACTOR_START;
    
    for (;;) {      
        MG_AWAIT(mg_sleep_for(50, self));
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
static struct mg_queue_t* receiver(struct mg_actor_t* self, struct mg_message_t* m) {
    MG_ACTOR_START;
    
    for (;;) {
        MG_AWAIT(&g_queue);     
        GPIOA->BSRR = GPIO_BSRR_BS_4;
        mg_message_free(m);

        MG_AWAIT(&g_queue);
        GPIOA->BSRR = GPIO_BSRR_BR_4;
        mg_message_free(m);
    }

    MG_ACTOR_END;
}

//
// Entry point...
//
int main(void) {
    //
    // Enable HSE and wait until it is ready.
    //
    RCC->CR |= RCC_CR_HSEON;

    while(!(RCC->CR & RCC_CR_HSERDY)) {
    }
    
    //
    // Setup flash prefetch.
    //
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;

    //
    // Set PLL multiplier to 6, set HSE as PLL source and enable the PLL.
    //
    RCC->CFGR |= RCC_CFGR_PLLMUL6;
    RCC->CFGR |= RCC_CFGR_PLLSRC_1;
    RCC->CR |= RCC_CR_PLLON;

    while(!(RCC->CR & RCC_CR_PLLRDY)) {
    }
    
    //
    // Switch to PLL and wait until switch succeeds.
    //
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;

    while(!(RCC->CFGR & RCC_CFGR_SWS_PLL)) {
    }

    //
    // Disable HSI.
    //
    RCC->CR &= ~RCC_CR_HSION;

    //
    // Enable LED.
    //
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= GPIO_MODER_MODER4_0;

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
    // WARNING! Priority 0 is default for all vectors, this is assumed implicitly!!
    //
    NVIC_EnableIRQ(0);

    //
    // Enable Systick to trigger interrupt every 100ms.
    //
    SysTick->LOAD  = 48000U - 1;
    SysTick->VAL   = 0;
    SysTick->CTRL  = 7;

    for (;;);
    return 0; /* make compiler happy. */
}

