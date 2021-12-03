CC = g++
CFLAGS = -g -lrt --std=c++14
DEPS = TCB.h uthread.h uthread_private.h Lock.h CondVar.h SpinLock.h async_io.h
OBJ = TCB.o uthread.o Lock.o CondVar.o SpinLock.o async_io.o
MAIN_OBJ = main.o
TEST1_OBJ = test_lock_vs_spinlock.cpp
TEST2_OBJ = test_sync_vs_async.cpp
TEST3_OBJ = test_priority_inversion.cpp
TEST4_OBJ = test_zoo.cpp
TEST5_OBJ = test_async_io.cpp

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

uthread-sync-demo: $(OBJ) $(MAIN_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

lock-vs-spinlock: $(OBJ) $(TEST1_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

sync-vs-async: $(OBJ) $(TEST2_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

priority-inversion: $(OBJ) $(TEST3_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

test-zoo: $(OBJ) $(TEST4_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

test-async-io: $(OBJ) $(TEST5_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o uthread-sync-demo lock-vs-spinlock sync-vs-async priority-inversion test-zoo test-async-io
