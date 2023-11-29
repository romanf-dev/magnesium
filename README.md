Header-only preemptive multitasking framework implementing actor model
======================================================================


Overview
--------

The framework relies on hardware features of the interrupt controller: the ability to set user-defined priorities to IRQs and possibility to trigger interrupts programmatically. Actor's priorities are mapped to interrupt vectors and interrupt controller acts as a hardware scheduler. Therefore, interrupts are used as 'execution engine' for actors with certain priority.

There are only three types of objects: actor, queue and message.

When some event (hardware interrupt) occurs, it allocates and posts the message to a queue, this causes activation of the subscribed actor, moving the message into its incoming mailbox, moving the actor to the list of ready ones and also triggering interrupt vector corresponding to actor's priority. Hardware automatically transfers control to the activated vector, its handler then calls framework's function 'schedule' which eventually calls activated actors.


API description
---------------

Initialization of global context containing runqueues. Don't forget to declare g_mg_context at global variable with type mg_context_t.

        void mg_context_init(void);


Select the next actor to run. Should be called inside vectors designated to actors.

        void mg_context_schedule(unsigned int this_vect);


Message queue initialization.

        void mg_queue_init(struct mg_queue_t* q);


Message pool initialization. Each message must contain header with type mg_message_t as its first member.

        void mg_message_pool_init(struct mg_message_pool_t* pool, void* mem, size_t len, size_t msg_sz);


Example:

        static struct { 
            mg_message_t header; 
            unsigned int our_payload;
        } msgs[10];

        static struct mg_message_pool_t pool;
        mg_message_pool_init(&pool, msgs, sizeof(msgs), sizeof(msgs[0]));


Actor object initialization. The actor will be implicitly subscribed to the queue specified. Function 'func' will be bound to the actor.
If q == NULL then actor's function will be called inside init to obtain queue pointer. The rule of thumb is that if you want your actor to be always called with a valid message pointer including the first call then use 'subscription on init'. Otherwise, if you prefer async-style coding with internal explicit or implicit (MG_AWAIT) state machine then use NULL here, the actor will be called on init with no message to init the state machine.

        void mg_actor_init(
            struct mg_actor_t* actor, 
            struct mg_queue_t* (*func)(struct mg_actor_t*, struct mg_message_t*), 
            unsigned int vect, 
            struct mg_queue_t* q);


Message management:

        void* mg_message_alloc(struct mg_message_pool_t* pool);
        void mg_message_free(struct mg_message_t* msg);


Sending message to queue:

        void mg_queue_push(struct mg_queue_t* q, struct mg_message_t* msg);


If your system has the tick source you can also use timing facility. Raw low-level API works as follows:

        self->timeout = <delay in ticks>;
        return MG_ACTOR_SUSPEND;


The calling actor will be activated with zero-message when the timeout is reached. The more convenient way to use timers
is async/await-like approach:

        MG_AWAIT(mg_sleep_for(10, self));


This will suspend the calling actor for 10 ticks. AWAIT-style API may be used with any queue with the same effect. 
Internal implementation is based on C-coroutines utilizing implicit state machine similar to Duff's device.
Also, don't forget to call tick function in your timer's interrupt handler:

        mg_context_tick();


**Warning! Actor is automatically subscribed to queue it returns, don't call mg_queue_pop manually!**

**Warning! It is expected that interrupts are enabled on call of these functions.**


Tips & tricks: if the message pool returned NULL it may be typecasted to queue. If an actor returns such a queue it will be activated when someone frees message. This is the way to deal with limited memory pools.


How to use
----------

1. Include magnesium.h into your application.
2. Setup include directories to point to appropriate porting header (mg_port.h).
3. Add global variable g_mg_context to some file in your project.
4. Initialize interrupt controller registers and priorities.
5. Initialize context, message pools, queues and objects in your main(). Enable the corresponding interrupt sources.
6. Put calls to 'schedule' in interrupt handlers associated with actors.
7. Put calls to 'tick' in interrupt handler of tick source.
8. Put calls to alloc/push in interrupt handlers associated with devices.
9. Implement message handling code in actor's functions.


Demo
----

The demo is a toy example with just one actor which blinks the LED.
Building:

        make MG_DIR=..


Why 'Magnesium'
---------------

RTOS-less systems are often called 'bare-metal' and magnesium is the 'key component of strong lightweight metal alloys'. Also, magnesium is one of just seven 'simple' metals contaning only s- and p- electron orbitals.

