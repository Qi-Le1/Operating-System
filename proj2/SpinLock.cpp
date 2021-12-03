#include "SpinLock.h"
#include <atomic>

SpinLock::SpinLock(){}

void SpinLock::lock(){
    while (atomic_value.test_and_set(std::memory_order_acquire)) {};
}

void SpinLock::unlock(){
    atomic_value.clear(std::memory_order_release);
}
