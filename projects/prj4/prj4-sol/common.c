#include "common.h"

#include <errors.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <semaphore.h>
#include <unistd.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>


void
send_data(Shm *shm, bool isServer, size_t nData, const void *dataP)
{
  const Sem dataSem = isServer ? SERVER_DATA_SEM : CLIENT_DATA_SEM;
  const char *data = dataP;
  TRACE("%lld entry: nData=%zu, data=%.*s", (long long) getpid(),
        nData, (int)nData, data);
  size_t n = 0;
  while (n < nData) {
    if (sem_wait(&shm->sems[MEMORY_SEM]) < 0) {
      fatal("send_data(): cannot wait on MEMORY_SEM:");
    }
    const size_t nLeft = nData - n;
    const size_t nSend = (nLeft < shm->bufSize) ? nLeft : shm->bufSize;
    memset(shm->buf, 0, shm->bufSize);  //to keep valgrind happy
    memcpy(shm->buf, &data[n], nSend);
    n += nSend;
    TRACE("%lld sent: %zu",  (long long) getpid(), nSend);
    if (sem_post(&shm->sems[dataSem]) < 0) {
      fatal("send_data(): cannot post on dataSem %d:", dataSem);
    }
  }
}

void
receive_data(Shm *shm, bool isServer, size_t nData, void *dataP)
{
  char *data = dataP;
  const Sem dataSem = isServer ? CLIENT_DATA_SEM : SERVER_DATA_SEM;
  TRACE("%lld entry: %zu", (long long) getpid(), nData);
  size_t n = 0;
  while (n < nData) {
    if (sem_wait(&shm->sems[dataSem]) < 0) {
      fatal("receive_data(): cannot wait on dataSem %d:", dataSem);
    }
    const size_t nLeft = nData - n;
    const size_t nReceive = (nLeft < shm->bufSize) ? nLeft : shm->bufSize;
    memcpy(&data[n], shm->buf, nReceive);
    n += nReceive;
    TRACE("%lld received: %zu", (long long) getpid(), nReceive);
    if (sem_post(&shm->sems[MEMORY_SEM]) < 0) {
      fatal("receive_data(): cannot post on MEMORY_SEM:");
    }
  }
  TRACE("%lld received data: %.*s", (long long) getpid(), (int)nData, data);
}
