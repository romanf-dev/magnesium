Magnesium is a simple header-only framework implementing actor model for deeply embedded systems. It uses interrupt controller hardware for scheduling and supports preemptive multitasking.


Features
--------

- Preemptive multitasking
- Hardware-assisted scheduling
- Unlimited number of actors and queues
- Message-passing communication
- Timer facility
- Hard real-time capability
- Only ARMv6-M and ARMv7-M are supported at now


API description
---------------

Please note that the framework itself does not initialize the interrupt controller, so it is user responsibility to properly set vector priorities before the first actor is created.

Initialization of global context containing runqueues. Don't forget to declare g_mg_context as a global variable with type mg_context_t.

        void mg_context_init(void);


Select the next actor to run. Must be called inside vectors designated to actors.

        void mg_context_schedule(unsigned int this_vect);


Message queue initialization.

        void mg_queue_init(struct mg_queue_t* q);


Message pool initialization. Each message must contain a header with type mg_message_t as its first member.

        void mg_message_pool_init(struct mg_message_pool_t* pool, void* mem, size_t len, size_t msg_sz);


Example:

        static struct { 
            mg_message_t header; 
            unsigned int our_payload;
        } msgs[10];

        static struct mg_message_pool_t pool;
        mg_message_pool_init(&pool, msgs, sizeof(msgs), sizeof(msgs[0]));


Actor object initialization. The actor will be implicitly subscribed to the queue specified.
If q == NULL then actor's function will be called inside init to obtain queue pointer. The rule of thumb is that if you want your actor to be always called with a valid message pointer including the first call then use 'subscription on init'. Otherwise, if you prefer async-style coding with internal explicit or implicit (MG_AWAIT) state machine then use NULL here, the actor will be called on init with no message to init the state machine.

        void mg_actor_init(
            struct mg_actor_t* actor, 
            struct mg_queue_t* (*func)(struct mg_actor_t*, struct mg_message_t*), 
            unsigned int vect, 
            struct mg_queue_t* q);


Message management. Alloc returns void* to avoid explicit typecast to specific message type. It may be safely assumed that this pointer always points to the message header. If the message pool returned NULL it may be typecasted to queue. If an actor subscribes to this queue it will be activated when someone returns a message into that pool. This is the way to deal with limited memory pools.

        void* mg_message_alloc(struct mg_message_pool_t* pool);
        void mg_message_free(struct mg_message_t* msg);


Sending message to a queue. Queues have no internal storage, they contain just head of linked list so sending cannot fail, no need for return status.

        void mg_queue_push(struct mg_queue_t* q, struct mg_message_t* msg);


If your system has a tick source you can also use timing facility.

        return mg_sleep_for(<delay in ticks>, self);


The calling actor will be activated with zero-message when the timeout is reached.
Also, don't forget to call tick function in your timer's interrupt handler:

        mg_context_tick();


**Warning! Actor is automatically subscribed to queue it returns, don't call mg_queue_pop manually!**

**Warning! It is expected that interrupts are enabled on call of all these functions.**


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

        make


Why 'Magnesium'
---------------

RTOS-less systems are often called 'bare-metal' and magnesium is the 'key component of strong lightweight metal alloys'. Also, magnesium is one of just seven 'simple' metals contaning only s- and p- electron orbitals.

