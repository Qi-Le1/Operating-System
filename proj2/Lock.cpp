#include "Lock.h"
#include "uthread_private.h"
#include <cassert>
#include <iostream>
#define DEBUG 0

// TODO
Lock::Lock() : _value(FREE) {}

void Lock::_unlock(){
    TCB *chosen_thread;
    while (!_signaling.empty()){
        // immediately switch back to the first thread that called signal() and is waiting to be returned
        // add running thread to the ready queue
        addToReady(running);
        running->setState(READY);
        
        // switch to the 'oldest' thread in the signaling queue
        TCB *chosen_thread = _signaling.front();
        _signaling.pop();
        switchToThread(chosen_thread);
    }
    
    if (!_waiting.empty()){
        // choosing the 'oldest' thread waiting for the lock
        chosen_thread = _waiting.front();
        _waiting.pop();

        // send the thread back to the ready queue, the chosen thread will resume at Lock.cpp:32
        // note that the lock should not be released to keep the order of waiting threads
        chosen_thread->setState(READY);
        addToReady(chosen_thread);
        // pass the lock
        running->decreaseLockCount();
        chosen_thread->increaseLockCount();
    }
    else{
        _value = FREE; // release the lock
        running->decreaseLockCount();
    }
}

void Lock::_lock(){
    _value = BUSY;
    running->increaseLockCount();
}

void Lock::_signal(TCB *tcb){
    if (tcb){
        if (DEBUG) std::cout<<"_signal gets a non-NULL tcb"<<std::endl;
        // change the state of running thread and push it into the signaling queue
        _signaling.push(running);
        running->setState(READY);

        // immediately switch to new thread
        switchToThread(tcb);
    }  
}

void Lock::lock(){
    disableInterrupts();

    if (_value == BUSY){
        // change the state of running thread and push it into the waiting queue
        _waiting.push(running);
        running->setState(BLOCK);

        // switch to next ready thread
        switchThreads();
    }
    else{
        // grab lock
        _lock();
    }

    enableInterrupts();
}

void Lock::unlock(){
    disableInterrupts();
    TCB *chosen_thread;
    while (!_signaling.empty()){
        // immediately switch back to the first thread that called signal() and is waiting to be returned
        // add running thread to the ready queue
        addToReady(running);
        running->setState(READY);
        
        // switch to the 'oldest' thread in the signaling queue
        TCB *chosen_thread = _signaling.front();
        _signaling.pop();
        switchToThread(chosen_thread);
    }
    
    if (!_waiting.empty()){
        // choosing the 'oldest' thread waiting for the lock
        chosen_thread = _waiting.front();
        _waiting.pop();

        // send the thread back to the ready queue, the chosen thread will resume at Lock.cpp:32
        // note that the lock should not be released to keep the order of waiting threads
        chosen_thread->setState(READY);
        addToReady(chosen_thread);
        // pass the lock
        running->decreaseLockCount();
        chosen_thread->increaseLockCount();
    }
    else{
        _unlock();
    }
    enableInterrupts();
}