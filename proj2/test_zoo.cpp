#include "uthread.h"
#include "Lock.h"
#include "CondVar.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace std;

#define UTHREAD_TIME_QUANTUM 10000
#define ANIMAL_ROOM 5
#define RANDOM_LEAVE_PERCENT 50
#define DEBUG 0

// shared variables
static int animal_room = ANIMAL_ROOM;
static int animal_accepted = 0; // total number of accepted animals
static int animal_leaving = 0; // total number of leaving animals
static bool isFinished = false;

// synchronization objects
static Lock lock;
static CondVar animalNeedRoom;
static CondVar zooNeedAnimalRoom;

// Test 1: A zoo is accepting animals
//         animals waiting to be accepted are broadcast when there is a room available
//         zoo is signaled when there is an animal leaving

void* animal(void* arg) {
  lock.lock();
  // wait for a room
  if (animal_room == 0){
    animalNeedRoom.wait(lock);
  }
  // occupy the room
  animal_room--;
  animal_accepted++;
  lock.unlock();
  // live a while
  if ((rand() % 100) < RANDOM_LEAVE_PERCENT){
    usleep(UTHREAD_TIME_QUANTUM);
  }
  else{
    usleep(UTHREAD_TIME_QUANTUM*2);
  }
  // leave the room
  lock.lock();
  animal_room++;
  animal_leaving++;
  zooNeedAnimalRoom.signal();
  lock.unlock();
  return nullptr;
}

void* zoo(void* arg) {
  while(!isFinished){
    lock.lock();
    if (animal_room==0){
      zooNeedAnimalRoom.wait(lock);
    }
    animalNeedRoom.broadcast();
    cout<<"current room: "<<animal_room
        <<", accepted animal num: "<<animal_accepted
        <<", leaving animal num: "<<animal_leaving<<endl;
    lock.unlock();
    uthread_yield();
  }
  return nullptr;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./test-zoo <animal_num>" << endl;
    cerr << "Example: ./test-zoo 20" << endl;
    exit(1);
  }

  int animal_count = atoi(argv[1]);
  int *threads = new int[animal_count+1];

  // Init user thread library
  int ret = uthread_init(UTHREAD_TIME_QUANTUM);
  if (ret != 0) {
    cerr << "Error: uthread_init" << endl;
    exit(1);
  }

  // create threads
  threads[0] = uthread_create(zoo, nullptr);
  for (int i = 1; i < animal_count + 1; i++){
    threads[i] = uthread_create(animal, nullptr);
  }
  // check if successfully created
  for (int i = 0; i < animal_count + 1; i++) {
    if (threads[i] < 0) {
      cerr << "Error: uthread_create producer" << endl;
    }
  }

  // Wait for all animal threads to complete
  for (int i = 1; i < animal_count + 1; i++) {
    int result = uthread_join(threads[i], nullptr);
    if (result < 0) {
      cerr << "Error: uthread_join producer" << endl;
    }
  }

  isFinished = true;  

  delete[] threads;

  return 0;
}
