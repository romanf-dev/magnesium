Magnesium is a simple (~350 lines) header-only kernel implementing CSP-like 
computation model with actors, messages and communication queues for deeply 
embedded systems. 
It maps actors to unused interrupt vectors and utilizes interrupt controller 
hardware for scheduling. All functions have constant interrupt locking time.


Features
--------

- Preemptive multitasking
- Easy integration into any project
- Hardware-assisted scheduling
- Unlimited number of actors and queues
- Zero-copy message-passing communication
- Timer facility
- Multicore support
- Hard real-time capability
- ARMv6-M, ARMv7-M, ARMv8-M are supported at now


API description
---------------

Please note that the kernel itself does not initialize the interrupt 
controller, so it is user responsibility to properly set vector priorities 
before the first actor is created.


Initialization of global context containing runqueues. Don't forget to 
declare g_mg_context as a global variable with type mg_context_t.

        void mg_context_init(void);


Select the next actor to run. Must be called inside vectors designated to 
actor execution.

        void mg_context_schedule(unsigned int this_vector);


Message queue initialization. A queue is always empty after init.

        void mg_queue_init(struct mg_queue_t* q);


Message pool initialization. Each message must contain a header with 
type mg_message_t as its first member.

        void mg_message_pool_init(struct mg_message_pool_t* pool, void* mem, size_t len, size_t msg_sz);


Example:

        static struct { 
            mg_message_t header; 
            unsigned int payload;
        } msgs[10];

        static struct mg_message_pool_t pool;
        mg_message_pool_init(&pool, msgs, sizeof(msgs), sizeof(msgs[0]));


Actor object initialization. Actor is a stackless run-to-completion function.

        void mg_actor_init(
            struct mg_actor_t* actor, 
            struct mg_queue_t* (*func)(struct mg_actor_t* self, struct mg_message_t* msg),
            unsigned int vect, 
            struct mg_queue_t* q);


The actor will be implicitly subscribed to the queue specified. If q == NULL 
then actor's function will be called inside init to obtain queue pointer. 
The rule of thumb here is if you want your actor to be always called with a 
valid message pointer including the first call then use 'subscription on init'. 
Otherwise, if you prefer async-style coding with internal explicit or implicit 
state machine then use NULL here, the actor will be called on init with no 
message to init the state machine.

Note: actor's default CPU is the one where it was initialized. This behavior
may be overridden by explicitly set actor.cpu = N. All actor activations will
happen on that CPU.


Message management. Alloc returns void* to avoid explicit typecasts to 
specific message type. It may be safely assumed that this pointer always 
points to the message header. If a message pool returned NULL it may be 
typecasted to a queue. If an actor subscribes to that queue it will be 
activated when someone returns a message into the pool. This is the way 
to deal with limited memory pools.

        void* mg_message_alloc(struct mg_message_pool_t* pool);
        void mg_message_free(struct mg_message_t* msg);


Sending message to a queue. Queues have no internal storage, they contain 
just head of linked list so sending cannot fail, no need for return status.

        void mg_queue_push(struct mg_queue_t* q, struct mg_message_t* msg);


Synchronous polling of a message queue.

        struct mg_message_t* mg_queue_pop(struct mg_queue_t* q, NULL);


If the system has a tick source you can also use timing facility. The tick
function has to be called periodically.

        void mg_context_tick(void);


Actor execution can be delayed by specified number of ticks by returning the 
special value:

        return mg_sleep_for(<delay in ticks>, self);


The calling actor will be activated with zero-message when the timeout is reached.


**Warning! It is expected that interrupts are enabled on call of all these functions.**


Protothreads
------------

Since actors are stackless all the state should be maintained by the user. 
It usually leads to state machines inside the actor function as C does not 
support async/await-like functionality. To simplify writing these types of
actors three additional macros are provided: MG_ACTOR_START, MG_ACTOR_END 
and MG_AWAIT. They may be used to write actors like protothreads:

        struct mg_queue_t* actor_fn(struct mg_actor_t* self, struct mg_message_t* msg) {
            MG_ACTOR_START;

            for (;;) {
                MG_AWAIT(mg_sleep_for(100, self)); // waiting for 100 ticks
                ...
                MG_AWAIT(queue); // awaiting for messages in the queue
            }

            MG_ACTOR_END;
        }

These macros are optional and provided only for convenience.


How to use
----------

1. Include magnesium.h into your application.
2. Setup include directories to point to appropriate porting header (mg_port.h).
3. Add global variable g_mg_context to some file in your project.
4. Initialize interrupt controller registers and priorities, enable irqs.
5. Initialize context, message pools, queues and objects in your main().
6. Put calls to 'schedule' in interrupt handlers associated with actors.
7. Put calls to 'tick' in interrupt handler of tick source.
8. Put calls to alloc/push in interrupt handlers associated with devices.
9. Implement message handling code in actor's functions.


Demo
----

The demo is a toy example blinking the LED. Use make to build. It is expected 
that arm-none-eabi- compiler is available via the PATH.
Most demos use make for building. For Raspberry Pi Pico 2 SDK version use

    cmake -DPICO_BOARD=pico2
    make


Why 'Magnesium'
---------------

RTOS-less systems are often called 'bare-metal' and magnesium is the 'key 
component of strong lightweight metal alloys'. Also, magnesium is one of 
just seven 'simple' metals contaning only s- and p- electron orbitals.

