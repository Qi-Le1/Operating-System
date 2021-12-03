#include "TCB.h"

TCB::TCB(int tid, void *(*start_routine)(void* arg), void *arg, State state)
{
	// set initial attributes
	_tid = tid;
	_quantum = 0;
	_state = state;
	// allocate stack
	_stack = (char*)malloc(STACK_SIZE);
	getcontext(&_context);
	_context.uc_stack.ss_sp = _stack;
	_context.uc_stack.ss_size = STACK_SIZE;
	_context.uc_link = NULL;
	if (tid != 0)
	    makecontext(&_context, (void(*)())&stub, 2, start_routine, arg); // stub and put func&args on stack
}

TCB::~TCB()
{
	if (_stack != nullptr)
		free(_stack);
}

void TCB::setState(State state)
{
	_state = state;
}

State TCB::getState() const
{
	return _state;
}

int TCB::getId() const
{
	return _tid;
}

void TCB::increaseQuantum()
{
	// to be changed for future usage
	_quantum++;
}

int TCB::getQuantum() const
{
	return _quantum;
}
