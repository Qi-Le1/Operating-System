#include "uthread.h"
#include "Lock.h"
#include "SpinLock.h"
#include "CondVar.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sys/time.h>
#include <string>

using namespace std;

#define UTHREAD_TIME_QUANTUM 10000
#define LOOP_TASK_COUNT 10000
#define DEBUG 0

// task counter
static int task_count = 0;

// synchronization objects
static Lock lock;
static SpinLock spinlock;

// worker functions: execute some tasks using Lock(*arg=true) or SpinLock(*arg=false)

// the worker will finish a simple task
void* worker_simple(void* arg) {
  // acquire lock
  if (*(bool*)(arg)){
    lock.lock();
  }
  else{
    spinlock.lock();
  }

  // finish a task
  task_count += 1;

  // release lock
  if (*(bool*)(arg)){
    lock.unlock();
  }
  else{
    spinlock.unlock();
  }
  return nullptr;
}

// the worker will finish a simple task and yield
void* worker_simple_yield(void* arg) {
  // acquire lock
  if (*(bool*)(arg)){
    lock.lock();
  }
  else{
    spinlock.lock();
  }

  // finish a task
  task_count += 1;

  // yield
  uthread_yield();

  // release lock
  if (*(bool*)(arg)){
    lock.unlock();
  }
  else{
    spinlock.unlock();
  }
  return nullptr;
}

// the worker will finish a long task
void* worker_long(void* arg) {
  // acquire lock
  if (*(bool*)(arg)){
    lock.lock();
  }
  else{
    spinlock.lock();
  }

  // finish tasks
  int tmp = 0;
  for (int i = 0; i < LOOP_TASK_COUNT; ++i){
    tmp += 1;
  }
  task_count += 1;

  // release lock
  if (*(bool*)(arg)){
    lock.unlock();
  }
  else{
    spinlock.unlock();
  }
  return nullptr;
}

// the test function for each kind of worker
void test(int* worker_threads, int worker_count, void*(func)(void*), void* arg, string test_name){
  // Create threads
  for (int i = 0; i < worker_count; i++) {
    worker_threads[i] = uthread_create(func, arg);
    if (worker_threads[i] < 0) {
      cerr << "Error: uthread_create lock_worker" << endl;
    }
  }

  // wait for all threads to complete
  struct timeval start, end;
  gettimeofday(&start, NULL);
  for (int i = 0; i < worker_count; i++) {
    int result = uthread_join(worker_threads[i], nullptr);
    if (result < 0) {
      cerr << "Error: uthread_join lock_worker" << endl;
    }
  }
  gettimeofday(&end, NULL);
  unsigned long timer = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
  cout << test_name << " completed: task_count = " << task_count << ", time = " << timer << "us" << endl;

}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./lock-vs-spinlock <num_thread_for_each_test>" << endl;
    cerr << "Example: ./lock-vs-spinlock 20" << endl;
    exit(1);
  }

  int worker_count = atoi(argv[1]);
  int *worker_threads = new int[worker_count];

  // Init user thread library
  int ret = uthread_init(UTHREAD_TIME_QUANTUM);
  if (ret != 0) {
    cerr << "Error: uthread_init" << endl;
    exit(1);
  }

  // Create lock worker threads
  void* arg = (void*)malloc(sizeof(bool));

  // tests for lock
  *(bool*)(arg) = true;
  test(worker_threads, worker_count, worker_simple, arg, "Simple test (Lock)");
  test(worker_threads, worker_count, worker_simple_yield, arg, "Simple test with yield (Lock)");
  test(worker_threads, worker_count, worker_long, arg, "Complex test (Lock)");

  // tests for spinlock
  *(bool*)(arg) = false;
  test(worker_threads, worker_count, worker_simple, arg, "Simple test (SpinLock)");
  test(worker_threads, worker_count, worker_simple_yield, arg, "Simple test with yield (SpinLock)");
  test(worker_threads, worker_count, worker_long, arg, "Complex test (SpinLock)");

  delete[] worker_threads;
  free(arg);

  return 0;
}
