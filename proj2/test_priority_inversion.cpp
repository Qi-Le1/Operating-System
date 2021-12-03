#include "uthread.h"
#include "Lock.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sys/time.h>
#include <string>

using namespace std;

#define UTHREAD_TIME_QUANTUM 10000
#define DEBUG 0

// synchronization objects
static Lock lock;

// shared variable
static bool isCreated = false;
static bool isSet = false;
static int threads[3]; // storing tid of threads C,B,A

// this function acquires the lock when it has the lowest priority
void* func_c(void* arg) {
    // spin until all threads are created
    while (!isCreated) {}

    // suspend A and B
    uthread_suspend(threads[1]);
    uthread_suspend(threads[2]);
    
    // set priorities
    // make sure C has lowest priority
    uthread_decrease_priority(threads[0]);
    uthread_decrease_priority(threads[0]);
    // make sure B has middle priority
    uthread_decrease_priority(threads[1]);
    uthread_decrease_priority(threads[1]);
    uthread_increase_priority(threads[1]);
    // make sure A has highest priority
    uthread_increase_priority(threads[2]);
    uthread_increase_priority(threads[2]);
    isSet = true;

    // acquire the lock
    lock.lock();

    // resume A and B
    uthread_resume(threads[1]);
    uthread_resume(threads[2]);

    // make sure that C runs longer than one time quantum (s.t. B has a chance to preempt C)
    usleep(UTHREAD_TIME_QUANTUM);

    cout<<"Function C finishes!"<<endl;
    lock.unlock();
    return nullptr;
}

// this function doesn't need a lock
void* func_b(void* arg) {
    // spin until its priority is properly set
    while (!isSet) {}
    cout<<"Function B finishes!"<<endl;
    return nullptr;
}

// this function acquires the lock with the lowest priority
void* func_a(void* arg) {
    // spin until its priority is properly set
    while (!isSet) {}

    // acquire the lock
    lock.lock();

    cout<<"Function A finishes!"<<endl;
    lock.unlock();
    return nullptr;
}

int main(int argc, char *argv[]) {
  // Init user thread library
  int ret = uthread_init(UTHREAD_TIME_QUANTUM);
  if (ret != 0) {
    cerr << "Error: uthread_init" << endl;
    exit(1);
  }

  // create three thread with priority C<B<A
  // for C:
  threads[0] = uthread_create(func_c, nullptr);
  // for B:
  threads[1] = uthread_create(func_b, nullptr);
  // for A:
  threads[2] = uthread_create(func_a, nullptr);
  // check if successfully created
  for (int i = 0; i < 3; i++) {
    if (threads[i] < 0) {
      cerr << "Error: uthread_create" << endl;
    }
  }
  isCreated = true;

  // wait for all threads to complete
  for (int i = 0; i < 3; i++) {
    int result = uthread_join(threads[i], nullptr);
    if (result < 0) {
      cerr << "Error: uthread_join lock_worker" << endl;
    }
  }

  return 0;
}
