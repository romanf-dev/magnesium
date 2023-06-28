Header-only preemptive multitasking framework implementing actor model
======================================================================


Overview
--------

The framework relies on hardware features of the interrupt controller: its ability to set user-defined priorities to IRQs and possibility to trigger interrupts programmatically. Actor's priorities are mapped to interrupt vectors and interrupt controller acts as hardware scheduler.

There are only three types of objects: actor, queue and message.

After initialization it is expected that all actors are subscribed to some queues. When hardware interrupt occurs, it allocates and post message to a queue, this causes activation of a subscribed actor. 


API description
---------------

Initialization of global context containing runqueues. Don't forget to declare g_mg_context at global variable with type mg_context_t.

        void mg_context_init(void);


Initialization of queue.

        void mg_queue_init(struct mg_queue_t* q);


Initialization of message pool. 

        void mg_message_pool_init(struct mg_message_pool_t* pool, void* mem, size_t len, size_t msg_sz);


Example:

        struct { 
            mg_message_t header; 
            unsigned int our_payload;
        } msgs[10];

        struct mg_message_pool_t pool;
        mg_message_pool_init(&pool, msgs, sizeof(msgs), sizeof(msgs[0]));


Initialization of actor object. The actor will be implicitly subscribed to the queue specified.

        void mg_actor_init(struct mg_actor_t* a, struct mg_queue_t* (*f)(struct mg_actor_t*, struct mg_message_t*), unsigned int vect, struct mg_queue_t* q);


Selects next actor to run. Should be called inside vectors designated to actors.

        void mg_context_schedule(unsigned int vect);


Message management:

        void* mg_message_alloc(struct mg_message_pool_t* pool);
        void mg_message_free(struct mg_message_t* msg);


Sending message to the queue:

        void mg_queue_push(struct mg_queue_t* q, struct mg_message_t* msg);


Warning! It is expected that interrupts are enabled when using these functions.


Tips & tricks: if message pool returned NULL it may be typecasted to queue. If some actor returns pointer to such queue it will be activated when someone returns message to the pool. This is a way to deal with limited memory pools.


How to use
----------

1. Include magnesium.h into your application
2. Setup include directories to point to appropriate porting header (mg_port.h)
3. Initialize message pools, queues and objects in your main(), initialize interrupt controller registers and priorities
4. Put calls to alloc/push in interrupt handlers associated with devices
5. Put calls to schedule in interrupt handlers associated with actors


Demo
----

The demo is a toy example with just one actor which blinks the LED on the STM32 Bluepill board.
Building:

        make MG_DIR=..


Why 'Magnesium'
---------------

RTOS-less systems are often called 'bare-metal' and magnesium is the 'key component of strong lightweight metal alloys'. Also, magnesium is one of just seven 'simple' metals contaning only s- and p- electron orbitals.

