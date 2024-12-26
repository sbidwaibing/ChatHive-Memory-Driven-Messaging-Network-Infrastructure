#include "chat.h"
#include "server.h"

#include <chat-cmd.h>
#include <errors.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>


//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>


struct _Chat {
  Shm *shm;
  FILE *out;
  FILE *err;
  pid_t serverPid;
};

/** send params for add cmd to server via shm,  as per protocol. */
static void
send_add_req(Shm *shm, const AddCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->user) + 1);
  nBytes += (strlen(cmd->room) + 1);
  nBytes += (strlen(cmd->message) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  ClientHdr clientHdr = {
    .cmd = ADD_CMD,
    .count = -1,
    .nTopics = cmd->nTopics,
    .reqSize = nBytes,
  };
  send_data(shm, false, sizeof(ClientHdr), &clientHdr);
  char buf[nBytes];
  char *p = buf;
  strcpy(p, cmd->user); p += strlen(cmd->user) + 1;
  strcpy(p, cmd->room); p += strlen(cmd->room) + 1;
  strcpy(p, cmd->message); p += strlen(cmd->message) + 1;
  for (int i = 0; i < cmd->nTopics; i++) {
    strcpy(p, cmd->topics[i]); p += strlen(cmd->topics[i]) + 1;
  }
  send_data(shm, false, nBytes, buf);
}

/** send params for query cmd to server via shm,  as per protocol. */
static void
send_query_req(Shm *shm, const QueryCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->room) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  ClientHdr clientHdr = {
    .cmd = QUERY_CMD,
    .count = cmd->count,
    .nTopics = cmd->nTopics,
    .reqSize = nBytes,
  };
  send_data(shm, false, sizeof(ClientHdr), &clientHdr);
  char buf[nBytes];
  char *p = buf;
  strcpy(p, cmd->room); p += strlen(cmd->room) + 1;
  for (int i = 0; i < cmd->nTopics; i++) {
    strcpy(p, cmd->topics[i]); p += strlen(cmd->topics[i]) + 1;
  }
  send_data(shm, false, nBytes, buf);
}

/** receive response from remote server on shm as per protocol, copying
 *  response onto appropriate client stream: out if response was okay,
 *  err if response was in error.
 */
static void
receive_res(Shm *shm, FILE *out, FILE *err)
{
  bool didOk = false;  //set true once we output OKAY
  do { //loop until we get a error response or an empty ok response
    ServerHdr hdr;
    receive_data(shm, false, sizeof(ServerHdr), &hdr);
    size_t nBytes = hdr.resSize;
    TRACE("read hdr status = %d, nBytes = %zu", hdr.status, nBytes);
    if (hdr.status != 0) { //error response
      const char *errStatus =
        (const char *[]){ "", "SYS_ERR: ", "FATAL_ERR: " }[hdr.status - 1];
      char msg[nBytes];
      receive_data(shm, false, nBytes, msg);
      fprintf(err, ERROR "%s%.*s\n", errStatus, (int)nBytes, msg);
      fflush(err);
      break;  //end loop on error response
    }
    else { //success response
      fprintf(out, "%s", didOk ? "" : OKAY); fflush(out);
      didOk = true;
      if (nBytes == 0) break; //end loop on empty ok response
      char buf[nBytes];
      receive_data(shm, false, nBytes, buf);
      TRACE("read buf %.*s", (int)nBytes, buf);
      fprintf(out, "%.*s", (int)nBytes, buf);  fflush(out);
      fflush(out);
    }
  } while (true);
}



/** Return a new Chat object which creates a server process which uses
 *  the sqlite database located at dbPath.  All commands must be sent
 *  by this client process to the server and handled by the server
 *  using the database. All IPC must use shared memory of size shmSize
 *  with POSIX semaphores used for synchronization.  The returned
 *  object should encapsulate all the state needed to implement the
 *  following API.
 *
 *  The client process must use `out` for writing success output for
 *  commands where each output must start with a line containing "ok".
 *
 *  The client process must use `err` for writing error message
 *  lines. Each line must start with "err ERR_CODE: " where ERR_CODE is
 *  as in your previous project for user errors.  System errors
 *  can result in unclean program termination.
 *
 *  [Note that since a `ChatCmd` is guaranteed to be syntactically
 *  valid, the only user errors which the program will need to detect
 *  will be `BAD_ROOM`/`BAD_TOPIC` for unknown room or topic.  This
 *  will have to be done by the server which will then return an error
 *  response to the client for output.]
 *
 *  The server should not use the `in` or `out` streams.  It may use
 *  `stderr` for "logging", but all such logging *must* be turned off
 *  before submission.
 *
 *  If errors are encountered, then this function should return NULL.
 */
Chat *
make_chat(const char *dbPath, size_t shmSize, FILE *out, FILE *err)
{
  assert(sizeof(ClientHdr) <= shmSize);
  assert(sizeof(ServerHdr) <= shmSize);
  Chat *chat = NULL;
  Shm *shm = NULL;
  pid_t pid = -1;
  shm = mmap(NULL, shmSize, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,
             -1, 0);
  if (shm == MAP_FAILED) {
    error("cannot map memory:");
    return NULL;
  }
  TRACE("shm mapped at %p", shm);
  shm->shmSize = shmSize; shm->bufSize = shmSize - offsetof(Shm, buf);
  const int semInits[] = { 1, 0, 0 };
  int nSemInits = 0;
  assert(sizeof(semInits)/sizeof(semInits[0]) == N_SEM);
  for (int i = 0; i < N_SEM; i++) {
    if (sem_init(&shm->sems[i],  1, semInits[i]) < 0) {
      error("cannot init semaphore %d:", i);
      goto CLEANUP;
    }
    TRACE("init sem %d", i);
    nSemInits++;
  }
  pid = fork();
  if (pid < 0) {
    error("cannot create child:");
    goto CLEANUP;
  }
  else if (pid == 0) {
    TRACE("forked server %lld", (long long)pid);
    do_server(shm, dbPath);
  }
  else {
    chat = malloc(sizeof(Chat));
    if (chat == NULL) goto CLEANUP;
    *chat = (Chat){ .shm = shm, .serverPid = pid, .out = out, .err = err, };
    TRACE("created chat");
    return chat;
  }
 CLEANUP:
  if (pid > 0) {
    if (waitpid(pid, NULL, 0) != pid) {
      error("cannot wait for child termination:");
    }
  }
  if (shm != NULL) {
    for (int i = 0; i < nSemInits; i++) {
      if (sem_destroy(&shm->sems[i]) != 0) {
        error("cannot destroy sem %d:", i);      }
    }
    if (munmap(shm, shmSize) != 0) {
      error("cannot unmap memory at %p:", shm);
    }
  }
  if (chat) free(chat);
  return NULL;
}

/** free all resources like memory used by chat.  All resources must
 *  be freed even after user errors have been detected.  It is okay if
 *  resources are not freed after system errors.
 */
void
free_chat(Chat *chat)
{
  for (int i = 0; i < N_SEM; i++) {
    if (sem_destroy(&chat->shm->sems[i]) != 0) {
      error("cannot destroy sem %d:", i);      }
  }
  if (munmap(chat->shm, chat->shm->shmSize) != 0) {
    error("cannot unmap memory at %p:", chat->shm);
  }
  free(chat);
}


/** perform cmd using chat, with the client writing response to chat's
 *  out/err streams.  It can be assumed that cmd is free of user
 *  errors except for unknown room/topic for QUERY commands.
 *
 *  If the command is an END_CMD command, then ensure that the server
 *  process has terminated before returning.
 */
void
do_chat_cmd(Chat *chat, const ChatCmd *cmd)
{
  Shm *shm = chat->shm;
  FILE *out = chat->out;
  FILE *err = chat->err;
  switch (cmd->type) {
  case ADD_CMD:
    send_add_req(shm, &cmd->add);
    receive_res(shm, out, err);
    break;
  case QUERY_CMD:
    send_query_req(shm, &cmd->query);
    receive_res(shm, out, err);
    break;
    case END_CMD: {
      ClientHdr clientHdr = {
        .cmd = END_CMD,
        .count = -1,
        .nTopics = 0,
        .reqSize = 0,
      };
      send_data(shm, false, sizeof(ClientHdr), &clientHdr);
      if (waitpid(chat->serverPid, NULL, 0) != chat->serverPid) {
        fatal("cannot wait on server shutdown:");
      }
      break;
    }
  default:
    assert(0);
  }
}

/** return server's PID */
pid_t chat_server_pid(const Chat *chat) { return chat->serverPid; }
