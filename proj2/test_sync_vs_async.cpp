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
#define MAX_FILE_SIZE 10000
#define DEBUG 0

// the ioworker will write and read files (sync/async)
void* ioworker(void* arg){
    // read parameters
    int *args = (int*) arg;
    int file_len = args[0];
    bool is_sync = false;
    if (args[1]<0){
        is_sync = true;
    }

    // initialization
    int buffer_len, buffer_num;
    if (file_len > MAX_FILE_SIZE){
        buffer_len = MAX_FILE_SIZE;
        buffer_num = buffer_len / MAX_FILE_SIZE + 1;
    }
    else{
        buffer_len = file_len;
        buffer_num = 1;
    }
    string content = string('a', buffer_len);
    char* buf = const_cast<char*>(content.c_str());
    string fileName = ".//dummy//" + to_string(uthread_self());

    // open file
    int fd = open(fileName.c_str(), O_CREAT | O_TRUNC | O_RDWR);
    if (fd < 0){
        perror("open(ioworker_sync):");
        exit(1);
    }
    ssize_t size;

    // write file
    for (int i = 0; i < buffer_num; ++i){
        if (is_sync){
            size = write(fd, buf, buffer_len);
        }
        else{
            size = async_write(fd, buf, buffer_len, 0);
        }
        if (size < 0){
            perror("write(ioworker_sync):");
            exit(1);
        }
    }


    // read file
    char buf_read[buffer_len];
    for (int i = 0; i < buffer_num; ++i){
        if (is_sync){
            size = read(fd, &buf_read, buffer_len);
        }
        else{
            size = async_read(fd, &buf_read, buffer_len, 0);
        }
        if (size<0){
            perror("read(ioworker_sync):");
            exit(1);
        }
        if (DEBUG) {cout<<"read from "<<fileName<<": length = "<<file_len;}
    }

    // close file
    if (close(fd)<0){
        perror("close(ioworker_sync):");
        exit(1);
    }
    return nullptr;
}

// the worker will compute the sum of several random numbers
void* compworker(void* arg) {
    int num = *(int*)(arg);
    double sum = 0;
    srand(time(0));
    for (int i = 0; i < num; ++i){
        sum += rand();
    }
    return nullptr;
}

// the test function for each kind of io
void test(int* threads, int io_count, int comp_count, void* io_arg, void* comp_arg, string test_name){
  // Create threads
  for (int i = 0; i < io_count + comp_count; i++) {
    if (i < io_count){
        threads[i] = uthread_create(ioworker, io_arg);
    }
    else{
        threads[i] = uthread_create(compworker, comp_arg);
    }
    if (threads[i] < 0) {
      cerr << "Error: uthread_create" << endl;
    }
  }

  // wait for all threads to complete
  struct timeval start, end;
  gettimeofday(&start, NULL);
  for (int i = 0; i < io_count + comp_count; i++) {
    int result = uthread_join(threads[i], nullptr);
    if (result < 0) {
      cerr << "Error: uthread_join" << endl;
    }
  }
  gettimeofday(&end, NULL);
  unsigned long timer = 1000000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
  int *io_args = (int*) io_arg;
  cout << test_name <<" test completed:" << endl 
    << "  io_operations = " << io_count << "x" << io_args[0] << endl
    << "  compute_operations = " << comp_count << "x" << *(int*)(comp_arg) << endl
    << "  time = " << timer << "us" << endl;
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    cerr << "Usage: ./sync-vs-async <io_thread> <io_filelen> <comp_thread> <comp_task_per_thread>" << endl;
    cerr << "Example: ./lock-vs-spinlock 20 1 20 10" << endl;
    exit(1);
  }

  int io_count = atoi(argv[1]);
  int comp_count = atoi(argv[3]);
  int *threads = new int[io_count+comp_count];

  // Init user thread library
  int ret = uthread_init(UTHREAD_TIME_QUANTUM);
  if (ret != 0) {
    cerr << "Error: uthread_init" << endl;
    exit(1);
  }

  // collect worker args
  int *io_arg = (int*)malloc(2*sizeof(int));
  io_arg[0] = atoi(argv[2]);
  int* comp_arg = (int*)malloc(sizeof(int));
  *comp_arg = atoi(argv[4]);

  // tests for synchronous IO
  io_arg[1] = -1;
  test(threads, io_count, comp_count, (void*)io_arg, (void*)comp_arg, "Synchronous");

  // tests for asynchronous IO
  io_arg[1] = 1;
  test(threads, io_count, comp_count, (void*)io_arg, (void*)comp_arg, "Asynchronous");

  delete[] threads;
  free(io_arg);
  free(comp_arg);

  return 0;
}
