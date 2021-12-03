#include "TCB.h"
using namespace std;

void f1(int* a){
    while (true){
        cout << "test function f1 is running..." <<endl;
        cout << "test function f1 start yielding..."<<endl;
        uthread_yield();
    }
}

int main(){
    int* args = new int[1];
    args[0] = 1;

    // Init user thread library
    int ret = uthread_init(1000);
    if (ret != 0) {
        cerr << "uthread_init FAIL!\n" << endl;
        exit(1);
    }

    // create thread
    int test_thread = uthread_create((void*(*)(void*))&f1, args);

    cout<<"main thread start yielding..."<<endl;
    uthread_yield();
    cout<<"yield successfully tested."<<endl;

    delete [] args;
    return 0;
}