#include "async_io.h"
#include "uthread.h"
#include <aio.h>
#include <errno.h>

// TODO
ssize_t async_read(int fd, void *buf, size_t count, int offset){
    // initialize aio control block
    struct aiocb *aiocbp = new aiocb();
    aiocbp->aio_fildes = fd;
    aiocbp->aio_buf = buf;
    aiocbp->aio_nbytes = count;
    aiocbp->aio_offset = offset;
    aiocbp->aio_reqprio = 0;
    aiocbp->aio_sigevent.sigev_notify = SIGEV_NONE;

    // start async read
    if (aio_read(aiocbp) < 0){
        return -1;
    }
        
    // yield to other thread;
    while (aio_error(aiocbp) == EINPROGRESS){
        uthread_decrease_priority(uthread_self());
        uthread_yield();
    }

    // return to this thread and return result
    int ret = aio_return(aiocbp);
    delete aiocbp;
    return ret;
}

ssize_t async_write(int fd, void *buf, size_t count, int offset){
    // initialize aio control block
    struct aiocb* aiocbp = new aiocb();
    aiocbp->aio_fildes = fd;
    aiocbp->aio_buf = buf;
    aiocbp->aio_nbytes = count;
    aiocbp->aio_offset = offset;
    aiocbp->aio_reqprio = 0;
    aiocbp->aio_sigevent.sigev_notify = SIGEV_NONE;

    // start async write
    if (aio_write(aiocbp) < 0){
        return -1;
    }
        
    // yield to other thread;
    while (aio_error(aiocbp) == EINPROGRESS){
        uthread_yield();
    }

    // return to this thread and return result
    int ret = aio_return(aiocbp);
    delete aiocbp;
    return ret;
}