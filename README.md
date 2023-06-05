Lightweight header-only preemptive multitasking framework implementing actor model
==================================================================================


Theory of operation
-------------------

The framework relies on hardware features of an interrupt controller: its ability to set user-defined priorities to IRQs and possibility to trigger interrupts programmatically. Actor's priorities are mapped to interrupt vectors and interrupt controller acts as hardware scheduler.

There are only three types of objects: actor, queue and message. 
After initialization it is expected that all actors are subscribed to some queues. When some event (hardware interrupt) occurs, it allocates and post message to some queue, this causes activation of subscribed actor, moving the message into its incoming mailbox, moving actor to the list of ready ones and also triggering interrupt vector corresponding to actor's priority. After leaving this handler hardware automatically transfers control to our activated vector, its handler then calls framework's function 'schedule' which eventually calls activated actors which starts processing its message. 

During this process the system remains fully preemptable and asynchronous: another interrupts may post messages to other queues, if corresponding actors have less priority then messages will just being accumulated in queues. When some actor or interrupt activate another high-priority actor then preemption occurs immediately, so, this system has good responde times and less jitter than loop-based solutions.
Normally actor model does not require explicit synchronization like semaphores and mutexes. For example, mutual exclusion may be represented as multiple actors posting their requests to single queue and one actor associated with that queue will do all the work sequentially. Nevertheless, in practice it is often desirable to block preemption. This may be done using hardware features or interrupt locking.


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


Initialization of actor object. The actor will be called inside this function and the last argument is a message for that call.

        void mg_actor_init(struct mg_actor_t* a, struct mg_queue_t* (*f)(struct mg_actor_t*, struct mg_message_t*), unsigned int vect, struct mg_message_t* m);


Select next actor to run. Should be called insode vectors designated to actors.

        void mg_context_schedule(unsigned int vect);


Message management:

        void* mg_message_alloc(struct mg_message_pool_t* pool);
        void mg_message_free(struct mg_message_t* msg);


Sending message to queue:

        void mg_queue_push(struct mg_queue_t* q, struct mg_message_t* m);


Warning! Actor is automatically subscribed to queue it returns, don't call mg_queue_pop manually!
Warning! It is expected that interrupts are enabled on call of these functions.


Tips & tricks: if message pool returned NULL it may be typecasted to queue. If an actor returns sucn a queue it will be activated when someone free message. This is a way to deal with limited memory pools.


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

Because magnesium is the 'key component of strong lightweight metal alloys'. Also, magnesium is one of just seven 'simple' metals contaning only s- and p- electron orbitals.

