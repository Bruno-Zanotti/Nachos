/// Routines for synchronizing threads.
///
/// Three kinds of synchronization routines are defined here: semaphores,
/// locks and condition variables (the implementation of the last two are
/// left to the reader).
///
/// Any implementation of a synchronization routine needs some primitive
/// atomic operation.  We assume Nachos is running on a uniprocessor, and
/// thus atomicity can be provided by turning off interrupts.  While
/// interrupts are disabled, no context switch can occur, and thus the
/// current thread is guaranteed to hold the CPU throughout, until interrupts
/// are reenabled.
///
/// Because some of these routines might be called with interrupts already
/// disabled (`Semaphore::V` for one), instead of turning on interrupts at
/// the end of the atomic operation, we always simply re-set the interrupt
/// state back to its original value (whether that be disabled or enabled).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2020 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "synch.hh"
#include "system.hh"


/// Initialize a semaphore, so that it can be used for synchronization.
///
/// * `debugName` is an arbitrary name, useful for debugging.
/// * `initialValue` is the initial value of the semaphore.
Semaphore::Semaphore(const char *debugName, int initialValue)
{
    name  = debugName;
    value = initialValue;
    queue = new List<Thread *>;
}

/// De-allocate semaphore, when no longer needed.
///
/// Assume no one is still waiting on the semaphore!
Semaphore::~Semaphore()
{
    delete queue;
}

const char *
Semaphore::GetName() const
{
    return name;
}

/// Wait until semaphore `value > 0`, then decrement.
///
/// Checking the value and decrementing must be done atomically, so we need
/// to disable interrupts before checking the value.
///
/// Note that `Thread::Sleep` assumes that interrupts are disabled when it is
/// called.
void
Semaphore::P()
{
    DEBUG('s', "Semaphore: %s wants to decrease counter\n", this -> GetName());    
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);
      // Disable interrupts.

    while (value == 0) {  // Semaphore not available.
        DEBUG('s', "Semaphore: %s starts waiting\n", this -> GetName());    
        queue->Append(currentThread);  // So go to sleep.
        currentThread->Sleep();
    }
    value--;  // Semaphore available, consume its value.

    interrupt->SetLevel(oldLevel);  // Re-enable interrupts.
}

/// Increment semaphore value, waking up a waiter if necessary.
///
/// As with `P`, this operation must be atomic, so we need to disable
/// interrupts.  `Scheduler::ReadyToRun` assumes that threads are disabled
/// when it is called.
void
Semaphore::V()
{
    DEBUG('s', "Semaphore: %s wants to increment counter\n", this -> GetName());    
    IntStatus oldLevel = interrupt->SetLevel(INT_OFF);

    Thread *thread = queue->Pop();
    if (thread != nullptr)
        // Make thread ready, consuming the `V` immediately.
        scheduler->ReadyToRun(thread);
    value++;

    interrupt->SetLevel(oldLevel);
}

/// Dummy functions -- so we can compile our later assignments.
///
/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

/// Plancha 2 - Ejercicio 1
Lock::Lock(const char *debugName)
{
    name  = debugName;
    thread = nullptr;
    semaphore = new Semaphore(name, 1);
    threadPriority = 0;
}

/// Plancha 2 - Ejercicio 1
Lock::~Lock()
{
    delete semaphore;
}

const char *
Lock::GetName() const
{
    return name;
}

/// Plancha 2 - Ejercicio 1
void
Lock::Acquire()
{
    ASSERT (!IsHeldByCurrentThread());
    
    // Plancha 2 - Ejercicio 4
    int p = currentThread -> GetPriority();
    if (thread != nullptr && p > threadPriority)  // if someone has the lock, compare:
        thread -> SetPriority(p);                 //  - priority of the 'new' thread
                                                  //  - priority of the thread which has the lock 
                                                  // if the first is bigger than the second
                                                  // assign the same priority to the second one
    semaphore->P();
    thread = currentThread;
    threadPriority = p;
    
    DEBUG('s', "Thread: %s acquires %s\n", currentThread -> GetName(), name);
}

/// Plancha 2 - Ejercicio 1
void
Lock::Release()
{
    ASSERT (IsHeldByCurrentThread());
    
    // Plancha 2 - Ejercicio 4
    if (threadPriority != thread -> GetPriority())  // restore the original priority
        thread -> SetPriority(threadPriority);
    
    thread = nullptr;
    semaphore->V();
    DEBUG('s', "Thread: %s releases %s\n", currentThread -> GetName(), name);
}

/// Plancha 2 - Ejercicio 1
bool
Lock::IsHeldByCurrentThread() const
{
    return currentThread == thread;
}

/// Plancha 2 - Ejercicio 1
Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name = debugName;
    lock = conditionLock;
    sem_queue = new List<Semaphore *>;
}

/// Plancha 2 - Ejercicio 1
Condition::~Condition()
{
    while(! sem_queue -> IsEmpty())
        delete sem_queue -> Pop();
    delete sem_queue;
}

const char *
Condition::GetName() const
{
    return name;
}

/// Plancha 2 - Ejercicio 1
void
Condition::Wait()
{
    DEBUG('s', "Thread: %s wait\n", currentThread -> GetName());
    
    Semaphore *s = new Semaphore("condition_sem",0);
    sem_queue -> Append(s);
    
    lock -> Release();
    s -> P();
    lock -> Acquire();
}

/// Plancha 2 - Ejercicio 1
void
Condition::Signal()
{
    DEBUG('s', "Thread: %s make a signal\n", currentThread -> GetName());
    Semaphore *s = sem_queue -> Pop();
    // Si no hay receivers esperando, se rompe (caso joineable nadie hace join)
    ASSERT(s != nullptr);
    s -> V();
}

/// Plancha 2 - Ejercicio 1
void
Condition::Broadcast()
{
    DEBUG('s', "Thread: %s make a broadcast\n", currentThread -> GetName());
    while(! sem_queue -> IsEmpty())
        sem_queue -> Pop() -> V();
}

/// Plancha 2 - Ejercicio 2  
Channel::Channel(const char *debugName)
{
    name  = debugName;
    buffer = 0;
    bufferEmpty = true;
    lock = new Lock("Channel-lock");
    senders = new Condition ("senders-list",lock);
    receivers = new Condition ("receivers-list",lock);
}

/// Plancha 2 - Ejercicio 2  
Channel::~Channel()
{
    delete senders;
    delete receivers;
    delete lock;
}

/// Plancha 2 - Ejercicio 2  
const char *
Channel::GetName() const
{
    return name;
}

/// Plancha 2 - Ejercicio 2  
void
Channel::Send(int message)
{
    lock -> Acquire();
    
    while(!bufferEmpty)     // if the buffer isn't empty
        senders -> Wait();  // the senders wait

    DEBUG('s', "Thread: %s sends %d\n", currentThread -> GetName(), message);

    buffer = message;      // "send" the message
    bufferEmpty = false;    // now the buffer isn't empty
    receivers -> Signal();  // send a signal to a receiver
    
    while(! bufferEmpty)
        senders -> Wait(); // Esperar que alguien tome algun mensaje
    
    lock -> Release();     
}

/// Plancha 2 - Ejercicio 2  
void
Channel::Receive(int *message)
{
    lock -> Acquire();
    
    while(bufferEmpty)          // if the buffer is empty
        receivers -> Wait();    // the receivers wait
    
    *message = buffer;          // "receive" the message
    DEBUG('s', "Thread: %s receives %d\n", currentThread -> GetName(), *message);
    bufferEmpty = true;         // now the buffer is empty
    senders -> Signal();        // comunication finished, call a new sender.
    
    lock -> Release();    
}
