#include "uthread.h"
#include "TCB.h"
#include <cassert>
#include <deque>
#include <unordered_map>

using namespace std;

// Finished queue entry type
typedef struct finished_queue_entry {
  int tid;             // Pointer to TCB
  void *result;         // Pointer to thread result (output)
  finished_queue_entry(){}
  finished_queue_entry(int _tid, void* _result) :tid(_tid), result(_result){}
  bool operator == (const finished_queue_entry entry) const{
          return (this->tid==entry.tid && this->result==entry.result);
  }
} finished_queue_entry_t;

// Join queue entry type
typedef struct join_queue_entry {
  int tid;             // Pointer to TCB
  int waiting_for_tid;  // TID this thread is waiting on
  join_queue_entry(){}
  join_queue_entry(int _tid, int _waiting_for_tid) :tid(_tid), waiting_for_tid(_waiting_for_tid){}
  bool operator == (const join_queue_entry entry) const{
          return (this->tid==entry.tid && this->waiting_for_tid==waiting_for_tid);
  }
} join_queue_entry_t;

// You will need to maintain structures to track the state of threads
// - uthread library functions refer to threads by their TID so you will want
//   to be able to access a TCB given a thread ID
// - Threads move between different states in their lifetime (READY, BLOCK,
//   FINISH). You will want to maintain separate "queues" (doesn't have to
//   be that data structure) to move TCBs between different thread queues.
//   Starter code for a ready queue is provided to you
// - Separate join and finished "queues" can also help when supporting joining.
//   Example join and finished queue entry types are provided above

// Library Data Structure
// map
static unordered_map<int, TCB*> thread_map;
// Queues
static deque<int> ready_queue, block_queue;
static deque<finished_queue_entry_t> finish_queue;
static deque<join_queue_entry_t> join_queue; 
// Other attributes
static int thread_num, tid_num, total_quantums;
static TCB* running_thread;
static struct itimerval itv;
static struct sigaction act;
static bool interrupts_enabled;

// Interrupt Management --------------------------------------------------------
template<typename T>
void removeFromQueue(deque<T> &queue, T entry){
        for (int i = 0; i < queue.size(); ++i){
                if (queue[i]==entry){
                        queue.erase(queue.begin()+i);
                        break;
                }
        }
}

// Start a countdown timer to fire an interrupt
static void startInterruptTimer()
{
        // TODO
        setitimer(ITIMER_VIRTUAL, &itv, NULL);
}

// Block signals from firing timer interrupt
static void disableInterrupts()
{
        // TODO
        assert(interrupts_enabled);
        sigprocmask(SIG_BLOCK, &act.sa_mask, NULL);
        interrupts_enabled = false;
}

// Unblock signals to re-enable timer interrupt
static void enableInterrupts()
{
        // TODO
        assert(!interrupts_enabled);
        sigprocmask(SIG_UNBLOCK, &act.sa_mask, NULL);
        interrupts_enabled = true;
}

// Helper functions ------------------------------------------------------------

// Switch to the next ready thread
static void switchThreads(int dummy=0)
{
        volatile int flag = 0;
        if (running_thread && !ready_queue.empty()){
                // change state of current thread
                switch (dummy){
                        case 2:
                        running_thread->setState(BLOCKED);
                        block_queue.push_back(running_thread->getId());
                        break;
                        case 3:
                        running_thread->setState(FINISHED);
                        break;
                        default:
                        running_thread->setState(READY);
                        ready_queue.push_back(running_thread->getId());
                }  

                // change state of next thread
                int to_run_id = ready_queue.front();
                ready_queue.pop_front();
                thread_map[to_run_id]->setState(RUNNING);

                // switch
                getcontext(&running_thread->_context);
                if (flag == 1){
                        return;
                }
                running_thread = thread_map[to_run_id];
                flag = 1;
                setcontext(&running_thread->_context);
        }
}

// Library functions -----------------------------------------------------------

// The function comments provide an (incomplete) summary of what each library
// function must do

// Starting point for thread. Calls top-level thread function
void stub(void *(*start_routine)(void *), void *arg) // finished
{
        if (!interrupts_enabled)
                enableInterrupts();
        void* retval = (*start_routine)(arg);
        uthread_exit(retval);
}

static void alarm_action(int dummy=0){
        // increase quantums
        ++total_quantums;
        running_thread->increaseQuantum();
        // switch to next thread (current thread added to ready queue)
        switchThreads();
}

int uthread_init(int quantum_usecs)
{
        // Initialize any data structures (to be changed for future usage)
        thread_num = 1; // at least 1 initial thread
        tid_num = 1; // 0 preserved for main thread
        ready_queue.clear();
        block_queue.clear();
        finish_queue.clear();
        join_queue.clear();

        // Setup timer interrupt and handler
        // after quantum_usecs starting the first timer interrupt
        itv.it_value.tv_sec = 0;
        itv.it_value.tv_usec = quantum_usecs;
        // betwen quantum_usecs sending successive timer interrupts
        itv.it_interval.tv_sec = 0;
        itv.it_interval.tv_usec = quantum_usecs;
        // set handler
        act = {0};
        act.sa_handler = &alarm_action;
        sigemptyset(&act.sa_mask);
        sigaddset(&act.sa_mask, SIGVTALRM);
        sigaction(SIGVTALRM, &act, NULL);
        // set related values
        interrupts_enabled = true;
        total_quantums = 0;

        // Create a thread for the caller (main) thread
        TCB* tcb = new TCB(0, (void*(*)(void*))NULL, (void*)NULL, RUNNING);
        running_thread = tcb;
        getcontext(&running_thread->_context);
        thread_map[0] = running_thread;

        startInterruptTimer();
        return 0;
}

int uthread_create(void* (*start_routine)(void*), void* arg) // finished
{
        TCB* self = thread_map[uthread_self()];
        if (self->getState() != RUNNING || self != running_thread){
                cout<<"uthread_create: Thread Create Failure (current thread is not running)"<<endl;
                return -1;
        }
        if (thread_num >= MAX_THREAD_NUM){
                cout<<"uthread_create: Thread Create Failure (MAX_THREAD_NUM reached)"<<endl;
                return -1;
        }

        disableInterrupts();
        // create a new thread
        int tid = tid_num++;
        TCB * tcb = new TCB(tid, start_routine, arg, READY);
        // add to the ready queue and map
        ready_queue.push_back(tid);
        thread_map[tid] = tcb;
        enableInterrupts();
        return tid;
}

int uthread_join(int tid, void **retval)
{
        TCB * self = thread_map[uthread_self()];
        if (self->getState() != RUNNING || self != running_thread){
                cout<<"uthread_join: Thread Join Failure (current thread is not running)"<<endl;
                return -1;
        }
        // If the thread specified by tid is already terminated, just return
        // no TERMINATED state for now
        // If the thread specified by tid is still running, block until it terminates
        while (thread_map[tid]->getState() == READY){
                join_queue.push_back(join_queue_entry(self->getId(), tid));
                uthread_suspend(uthread_self());
        }
        // Set *retval to be the result of thread if retval != nullptr
        for (deque<finished_queue_entry_t>::iterator it = finish_queue.begin(); it != finish_queue.end();){
                if (it->tid == tid){
                        *retval = it->result;
                        it = finish_queue.erase(it);
                }
                else{
                        ++it;
                }
        }
        return 0;
}

int uthread_yield(void)
{
        TCB* self = thread_map[uthread_self()];
        if (self->getState() != RUNNING || self != running_thread){
                cout<<"uthread_yield: Thread Yield Failure (current thread is not running)"<<endl;
                return -1;
        }
        switchThreads(0);
        return 0;
}

void uthread_exit(void *retval)
{
        TCB* self = thread_map[uthread_self()];
        if (self->getState() != RUNNING || self != running_thread){
                cout<<"uthread_exit: Thread Exit Failure (current thread is not running)"<<endl;
                return;
        }
        // If this is the main thread, exit the program
        if (self->getId() == 0){
                exit(0);
        }
        // Move any threads joined on this thread back to the ready queue
        for (deque<join_queue_entry_t>::iterator it = join_queue.begin(); it != join_queue.end();){
                if (it->waiting_for_tid == self->getId()){
                        uthread_resume(it->tid);
                        it = join_queue.erase(it);
                }
                else{
                        ++it;
                }
        }
        disableInterrupts();
        // Move this thread to the finished queue
        self->setState(FINISHED);
        finished_queue_entry_t self_finished = {self->getId(), retval};
        finish_queue.push_back(self_finished);
        thread_num--;
        enableInterrupts();
        // switch to another thread
        switchThreads(3);
}

int uthread_suspend(int tid)
{
        // Move the thread specified by tid from whatever state it is
        // in to the block queue
        TCB * self = thread_map[uthread_self()];
        if (self->getId() == tid){
                switchThreads(2);
                return 0;
        }
        assert(thread_map[tid]->getState()!=RUNNING);
        disableInterrupts();
        switch (thread_map[tid]->getState())
        {
        case READY:
                removeFromQueue(ready_queue, tid);
                block_queue.push_back(tid);
                break;
        case FINISHED:
                cout<<"error(uthread_suspend): target thread is already finished."<<endl;
                enableInterrupts();
                return -1;
        case BLOCKED:
                cout<<"warning(uthread_suspend): target thread is already blocked."<<endl;
                break;
        }
        enableInterrupts();
        return 0;
}

int uthread_resume(int tid)
{
        // Move the thread specified by tid back to the ready queue
        disableInterrupts();
        if (thread_map[tid]->getState()!=BLOCKED){
                cout<<"warning(uthread_resume): target thread is NOT blocked."<<endl;
                enableInterrupts();
                return 0;
        }
        removeFromQueue(block_queue, tid);
        ready_queue.push_back(tid);
        enableInterrupts();
        return 0;
}

int uthread_self()
{
        return running_thread->getId();
}

int uthread_get_total_quantums()
{
        return total_quantums;
}

int uthread_get_quantums(int tid)
{
        return thread_map[tid]->getQuantum();
}
