#include "CondVar.h"
#include "uthread_private.h"
#include <iostream>

#define DEBUG 0

CondVar::CondVar(){}

void CondVar::wait(Lock &lock){
    // atomically operation
    disableInterrupts();
    
    // release lock
    lock._unlock();

    // block this thread
    _waiting_thread.push(running);
    _waiting_lock.push(&lock);
    running->setState(BLOCK);
    // switch to another thread
    switchThreads();

    if (DEBUG) std::cout<<"wait successfully finished"<<std::endl;

    // re-acquire lock
    lock._lock();
    enableInterrupts();
}

void CondVar::signal(){
    disableInterrupts();
    if(!_waiting_thread.empty()){
        if (DEBUG) std::cout<<"_waiting_thread length: "<<_waiting_thread.size()<<"="<<_waiting_lock.size()<<std::endl;

        // pop the 'oldest' waiting thread
        TCB* next_thread = _waiting_thread.front();
        _waiting_thread.pop();
        // pop the lock the 'oldest' waiting thread is waiting for
        Lock* waiting_lock = _waiting_lock.front();
        _waiting_lock.pop();
        if (DEBUG) std::cout<<"_waiting_thread length: "<<_waiting_thread.size()<<"="<<_waiting_lock.size()<<std::endl;
        if (DEBUG) std::cout<<"switch to id:"<<next_thread->getId()<<std::endl;
        // directly switch to the 'oldest' waiting thread
        waiting_lock->_signal(next_thread);
    }

    enableInterrupts();
}

void CondVar::broadcast(){
    disableInterrupts();

    while(!_waiting_thread.empty()){
        TCB* next_thread = _waiting_thread.front();
        _waiting_thread.pop();
        Lock* waiting_lock = _waiting_lock.front();
        _waiting_lock.pop();
        waiting_lock->_signal(next_thread);
        //lock._signal(next_thread);
    }

    enableInterrupts();
}
