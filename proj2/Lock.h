#ifndef LOCK_H
#define LOCK_H

#include "TCB.h"
#include <queue>

enum LockState {FREE, BUSY};

// Synchronization lock
class Lock {
public:
  Lock();

  // Attempt to acquire lock. Grab lock if available, otherwise thread is
  // blocked until the lock becomes available
  void lock();

  // Unlock the lock. Wake up a blocked thread if any is waiting to acquire the
  // lock and hand off the lock
  void unlock();

private:
  // TODO - Add members as needed
  LockState _value;  // current lock state
  std::queue<TCB*> _waiting;  // list of threads waiting to acquire this lock
  std::queue<TCB*> _signaling;  // list of threads once called signal（）ing waiting to be returned

  // Unlock the lock while interrupts have already been disabled
  // NOTE: Assumes interrupts are disabled
  void _unlock();

  // simply lock the lock while interrupts have already been disabled
  void _lock();

  // Let the lock know that it should switch to this thread after the lock has
  // been released (following Hoare semantics)
  // NOTE: Assumes interrupts are disabled
  void _signal(TCB *tcb);

  // Allow condition variable class access to Lock private members
  // NOTE: CondVar should only use _unlock() and _signal() private functions
  //       (should not access private variables directly)
  friend class CondVar;
};

#endif // LOCK_H
