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
// Errors and exceptions turn onboard LED on.
//
static noreturn void Error_Handler(void)
{
    __disable_irq();
    for(;;);
}

//
// Override hardware exceptions.
//
void HardFault_Handler(void)
{
    Error_Handler();
}

//
// Library dependency...
//
void __assert_func(const char *file, int line, const char *func, const char *expr)
{
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
static struct mg_actor_t g_handler;

struct mg_context_t g_mg_context;

//
// This is name for 0th IRQ which we use for actor execution. 
// Any unused vector may be used.
//
void WWDG_IRQHandler(void)
{
    mg_context_schedule(0);
}

//
// Systick sends notification to queue at every tick.
//
void SysTick_Handler(void)
{
    struct example_msg_t* m = mg_message_alloc(&g_pool);

    if (m) 
    {
        mg_queue_push(&g_queue, &m->header);
    }
}

//
// Actor switches LED state once new message arrives.
//
static struct mg_queue_t* actor(struct mg_actor_t* self, struct mg_message_t* m)
{
    MG_ACTOR_START;
    
    for (;;)
    {      
        GPIOA->BSRR = GPIO_BSRR_BS_4;
        mg_message_free(m);

        MG_AWAIT(g_queue);

        GPIOA->BSRR = GPIO_BSRR_BR_4;
        mg_message_free(m);

        MG_AWAIT(g_queue);
    }

    MG_ACTOR_END;
}

//
// Entry point...
//
int main(void)
{
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
    // Create actor.
    //
    mg_actor_init(&g_handler, &actor, 0, &g_queue);

    //
    // Enable interrupt source.
    // WARNING! Priority 0 is default for all vectors, this is assumed implicitly!!
    //
    NVIC_EnableIRQ(0);

    //
    // Enable Systick to trigger interrupt every 100ms.
    //
    SysTick->LOAD  = 48000U * 100 - 1U;
    SysTick->VAL   = 0;
    SysTick->CTRL  = 7;

    for (;;);
    return 0; /* make compiler happy. */
}

