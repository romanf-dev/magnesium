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
#include "stm32f1xx.h"
#include "magnesium.h"

#define EXAMPLE_VECTOR 20

//
// Errors and exceptions turn onboard LED on.
//
static noreturn void Error_Handler(void)
{
    __disable_irq();
    GPIOC->BSRR = GPIO_BSRR_BR13;
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
// Library
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
    unsigned int led_state;
} g_msgs[1];
static struct mg_message_pool_t g_pool;
static struct mg_queue_t g_queue;
static struct mg_actor_t g_handler;

struct mg_context_t g_mg_context;

//
// This is name for 20th IRQ which we use for actor execution. 
// Any unused vector may be used.
//
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    mg_context_schedule(EXAMPLE_VECTOR);
}

//
// Systick sends new LED state to queue at every tick.
//
void SysTick_Handler(void)
{
    static unsigned int led_state = 0;

    led_state ^= 1;

    struct example_msg_t* m = mg_message_alloc(&g_pool);
    m->led_state = led_state;
    mg_queue_push(&g_queue, &m->header);
}

//
// Actor reads new LED state from the message and program GPIO.
//
static struct mg_queue_t* actor(struct mg_actor_t* self, struct mg_message_t* m)
{
    const struct example_msg_t* msg = (struct example_msg_t*) m;

    if (msg->led_state == 0)
        GPIOC->BSRR = GPIO_BSRR_BR13;
    else
        GPIOC->BSRR = GPIO_BSRR_BS13;

    mg_message_free(m);
    return &g_queue;
}

//
// Entry point...
//
int main(void)
{
    RCC->CR |= RCC_CR_HSEON;            
    while(!(RCC->CR & RCC_CR_HSERDY))
        ;
    
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;

    RCC->CFGR |= RCC_CFGR_SW_HSE;
    RCC->CFGR |= RCC_CFGR_PLLMULL9;
    RCC->CFGR |= RCC_CFGR_PLLSRC;

    RCC->CR |= RCC_CR_PLLON;
    while(!(RCC->CR & RCC_CR_PLLRDY))
        ;
    
    RCC->CFGR = (RCC->CFGR | RCC_CFGR_SW_PLL) & ~RCC_CFGR_SW_HSE;
    while(!(RCC->CFGR & RCC_CFGR_SWS_PLL))
        ;

    RCC->CR &= ~RCC_CR_HSION;

    //
    // Enable LED.
    //
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH |= GPIO_CRH_CNF13_0 | GPIO_CRH_MODE13_1;

    //
    // Enable 20th vector. Its priority is defaulted to 0 after MCU reset.
    // 
    NVIC_SetPriorityGrouping(3);
    NVIC_EnableIRQ(EXAMPLE_VECTOR);

    mg_context_init();
    mg_message_pool_init(&g_pool, g_msgs, sizeof(g_msgs), sizeof(g_msgs[0]));
    mg_queue_init(&g_queue);

    //
    // Send first message to initialize actor.
    //
    struct example_msg_t* m = mg_message_alloc(&g_pool);
    m->led_state = 0;
    mg_actor_init(&g_handler, &actor, EXAMPLE_VECTOR, &m->header);

    //
    // Enable Systick to trigger interrupt every 100ms.
    //
    SysTick->LOAD  = 72000U * 100 - 1U;
    SysTick->VAL   = 0;
    SysTick->CTRL  = 7;

    for (;;)
    {
    }

    return 0; /* make compiler happy. */
}
