#include "uthread.h"
#include "async_io.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <unistd.h>

using namespace std;

#define UTHREAD_TIME_QUANTUM 10000
#define MAX_FILE_SIZE 40
#define DEBUG 0

// this worker will create a file and read from it
void* ioworker(void* arg){
    // initialization
    char *buf = (char*)"Hello world";
    int buffer_len = strlen(buf);
    char *fileName = (char*)".//async_test.txt";

    // open file
    int fd = open(fileName, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd < 0){
        perror("open(ioworker_sync):");
        exit(1);
    }
    ssize_t size;

    // write file
    size = async_write(fd, buf, buffer_len, 0);
    if (size < 0){
        perror("write(ioworker_sync):");
        exit(1);
    }

    // read file
    char buf_read[MAX_FILE_SIZE];
    size = async_read(fd, &buf_read, MAX_FILE_SIZE, 0);
    if (size<0){
        perror("read(ioworker_sync):");
        exit(1);
    }
    cout<<"read from "<<fileName<<": "<<buf_read<<endl;

    // close file
    if (close(fd)<0){
        perror("close(ioworker_sync):");
        exit(1);
    }
    return nullptr;
}

int main(int argc, char *argv[]) {
  int *threads = new int[1];

  // Init user thread library
  int ret = uthread_init(UTHREAD_TIME_QUANTUM);
  if (ret != 0) {
    cerr << "Error: uthread_init" << endl;
    exit(1);
  }

  // create IO thread
  threads[0] = uthread_create(ioworker, nullptr);
  if (threads[0] < 0){
      cerr << "Error: uthread_create" << endl;
  }

  // wait for IO to complete
  uthread_yield();
  cout<<"waiting for IO..."<<endl;
  int result = uthread_join(threads[0], nullptr);
  if (result < 0){
      cerr << "Error: uthread_join" << endl;
  }

  delete[] threads;
  return 0;
}
