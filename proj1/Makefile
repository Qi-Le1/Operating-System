CC = g++
CFLAGS = -lrt -g
DEPS = TCB.h uthread.h
TEST_OBJ = TCB.o uthread.o test.o
MAIN_OBJ = TCB.o uthread.o main.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

uthread-test: $(TEST_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

uthread-main: $(MAIN_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f uthread-test *.o
