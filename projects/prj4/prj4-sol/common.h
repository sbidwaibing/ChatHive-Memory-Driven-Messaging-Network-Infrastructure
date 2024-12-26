#ifndef COMMON_H_
#define COMMON_H_

#include <chat-cmd.h>

#include <semaphore.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// declarations common between server and client

#define ERROR "err "
#define OKAY "ok\n"

enum { MIN_SHM_SIZE = 1024 };


typedef struct {
  CmdType cmd;
  size_t nTopics;  //used by ADD_CMD and QUERY_CMD
  int count;       //used only by QUERY_CMD
  size_t reqSize;  //total size of request which follows
} ClientHdr;

typedef enum {
  OK_STATUS, USER_ERR_STATUS, SYSTEM_ERR_STATUS, FATAL_ERR_STATUS
} ServerStatus;

typedef struct {
  ServerStatus status;
  size_t resSize;
} ServerHdr;

typedef enum {
  MEMORY_SEM,        //1 means shm is empty
  SERVER_DATA_SEM,   //1 means shm contains data from server
  CLIENT_DATA_SEM,   //1 means shm contains data from client
  N_SEM
} Sem;

typedef struct {
  size_t shmSize;
  size_t bufSize;
  sem_t sems[N_SEM];
  char buf[];
} Shm;


void send_data(Shm *shm, bool isServer, size_t nData, const void *data);
void receive_data(Shm *shm, bool isServer, size_t nData, void *data);

#ifndef SEM_TRACE
#define SEM_TRACE 1
#endif

#if SEM_TRACE
#define SEM_VALUE(prg, state, sem, posixName) \
  do { \
    int sval; \
    if (sem_getvalue(sem, &sval) < 0) { \
      fatal("cannot get value for semaphore %s:", posixName); \
    } \
    fprintf(stderr, "%s: %s value of semaphore %s is %d\n", \
            prg, state, posixName, sval);                   \
  } while (0)
#else
#define SEM_VALUE(prg, state, sem, posixName) do { } while (0)
#endif



#endif //#ifndef COMMON_H_
