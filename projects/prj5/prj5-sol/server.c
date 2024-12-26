#include "server.h"

#include "common.h"

#include <chat-cmd.h>
#include <errors.h>

#include <chat-db.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>

/**************************** Thread Info ******************************/

struct ThreadInfo_;  //forward reference

/** track information about all possible client threads */
typedef struct {
  pthread_rwlock_t rwlock;
  struct ThreadInfo_ *infoArray;
  int nInfoArray;
} AllThreadInfos;

/** information tracked for each client thread; not all the fields are
 *  necessary but can be useful when debugging.
 */
typedef struct ThreadInfo_ {
  bool isValid;
  pthread_t tid;
  AllThreadInfos *allThreadInfos;
  int nThreadInfos;
  ChatDb *chatDb;
  int clientSockFd;
  FILE *in;
  FILE *out;
  //filled in after initialization
  char *user;
  char *room;
} ThreadInfo;

typedef struct {
  AllThreadInfos *allThreadInfos;
  ChatDb *chatDb;
  int clientSockFd;
  ThreadInfo *threadInfo;
} ThreadArg;


/** fill in threadInfo; return non-zero on error */
static int
init_thread_info(ChatDb *chatDb, int clientSockFd,
                 AllThreadInfos *allThreadInfos, ThreadInfo *threadInfo)
{
  FILE *in = NULL;
  FILE *out = NULL;
  in = fdopen(clientSockFd, "r");
  if (!in) {
    error("fdopen(\"r\") on client socket %d:", clientSockFd);
    goto FAIL;
  }
  out = fdopen(clientSockFd, "w");
  if (!out) {
    error("fdopen(\"w\") on client socket %d:", clientSockFd);
    goto FAIL;
  }
  if (pthread_rwlock_wrlock(&allThreadInfos->rwlock) != 0) {
    error("cannot get write lock: %s", strerror(errno));
    goto FAIL;
  }
  *threadInfo = (ThreadInfo) {
    .tid = pthread_self(),
    .allThreadInfos = allThreadInfos,
    .chatDb = chatDb,
    .clientSockFd = clientSockFd,
    .in = in,
    .out = out,
    .user = NULL,
    .room = NULL,
    .isValid = true,
  };
  if (pthread_rwlock_unlock(&allThreadInfos->rwlock) != 0) {
    error("cannot unlock write lock: %s", strerror(errno));
    goto FAIL;
  }
  return 0;
 FAIL:
  //if we get here with the rwlock, then we must have already failed unlocking!
  if (in) fclose(in);
  if (out) fclose(out);
  return 1;
}

static void
cleanup_thread_info(ThreadInfo *threadInfo)
{
  pthread_rwlock_wrlock(&threadInfo->allThreadInfos->rwlock);
  threadInfo->isValid = false;
  free(threadInfo->room);
  free(threadInfo->user);
  fclose(threadInfo->out);
  fclose(threadInfo->in);
  pthread_rwlock_unlock(&threadInfo->allThreadInfos->rwlock);
}

/********************** Transmission Utilities *************************/

static void
end_server_response(ChatDb *chatDb, ServerStatus status,
                    const char *errMsg, FILE *out)
{
  Hdr serverHdr = { .hdrType = SERVER_HDR, .status = status, };
  if (status == OK_STATUS) {
    serverHdr.nBytes = 0;
    write_header(&serverHdr, out);
  }
  else {
    serverHdr.nBytes = strlen(errMsg);
    write_header(&serverHdr, out);
    TRACE("error %s", errMsg);
    fprintf(out, "%s", errMsg);
  }
  fflush(out);
}

static void
send_msg(const ThreadInfo *server, const char *msg, size_t msgLen)
{
  Hdr hdr = { .hdrType = SERVER_HDR, .status = OK_STATUS, .nBytes = msgLen };
  write_header(&hdr, server->out);
  fwrite(msg, 1, msgLen, server->out);
  end_server_response(server->chatDb, OK_STATUS, NULL, server->out);
}

static void
broadcast_to_room(const ThreadInfo *server, const char *msg, size_t msgLen)
{
  pthread_rwlock_rdlock(&server->allThreadInfos->rwlock);
  const char *room = server->room;
  for (int i = 0; i < server->allThreadInfos->nInfoArray; i++) {
    const ThreadInfo *p = &server->allThreadInfos->infoArray[i];
    if (p != server && p->isValid && strcmp(p->room, room) == 0) {
      send_msg(p, msg, msgLen);
    }
  }
  pthread_rwlock_unlock(&server->allThreadInfos->rwlock);
}


/************************** Message Broadcasts *************************/

static void
broadcast_enter_msg(const ThreadInfo *server)
{
  const char *user = server->user;
  const char *msgPrefix = "user ";
  const char *msgSuffix = " has entered the room\n";
  const int msgSize = strlen(msgPrefix) + strlen(user) + strlen(msgSuffix) + 1;
  char msg[msgSize];
  int n = snprintf(msg, msgSize, "%s%s%s", msgPrefix, user, msgSuffix);
  assert(n == msgSize - 1);
  TRACE("broadcast msg = %s", msg);
  broadcast_to_room(server, msg, n);
}

static void
broadcast_leave_msg(const ThreadInfo *server)
{
  TRACE("broadcast_leave() entry");
  const char *user = server->user;
  const char *msgPrefix = "user ";
  const char *msgSuffix = " has left the room\n";
  const int msgSize = strlen(msgPrefix) + strlen(user) + strlen(msgSuffix) + 1;
  char msg[msgSize];
  int n = snprintf(msg, msgSize, "%s%s%s", msgPrefix, user, msgSuffix);
  assert(n == msgSize - 1);
  broadcast_to_room(server, msg, n);
  TRACE("broadcast_leave() exit");
}

static void
broadcast_add_msg(const ThreadInfo *server, const char *user,
                  size_t nTopics, const char *topics[nTopics],
                  const char *message)
{
  const char *msgPrefix = "message from ";
  int msgSize = strlen(msgPrefix) + strlen(user) + 1; //+1 for newline
  for (int i = 0; i < nTopics; i++) { msgSize += strlen(topics[i]) + 1; }
  msgSize += strlen(message) + 1; //+ 1 for snprintf '\0'
  char msg[msgSize];
  int n = 0;
  n += snprintf(&msg[n], msgSize - n, "%s%s\n", msgPrefix, user);
  for (int i = 0; i < nTopics; i++) {
    n += snprintf(&msg[n], msgSize - n, "%s ", topics[i]);
  }
  n += snprintf(&msg[n], msgSize - n, "%s", message);
  assert(n == msgSize - 1);
  broadcast_to_room(server, msg, n);
}

/******************** Client Thread Initialization *********************/

static void
do_init_cmd(ThreadInfo *server, const Hdr *clientHdr)
{
  TRACE("do_init_cmd()");
  FILE *in = server->in;

  char buf[clientHdr->nBytes];
  fread(buf, 1, clientHdr->nBytes, in);
  TRACE("nBytes = %zu; buf = %s", clientHdr->nBytes, buf);
  const char *user = buf;
  const char *room = user + strlen(user) + 1;
  server->user = malloc(strlen(user) + 1);
  if (server->user == NULL) {
    error("init_cmd(): cannot malloc for user \"%s\"", user);
    goto FAIL;
  }
  strcpy(server->user, user);
  server->room = malloc(strlen(room) + 1);
  if (server->room == NULL) {
    error("init_cmd(): cannot malloc for user \"%s\"", user);
    goto FAIL;
  }
  strcpy(server->room, room);
  TRACE("server->user = %s; server->room = %s", server->room, server->user);
  broadcast_enter_msg(server);
  return;
 FAIL:
  cleanup_thread_info(server);

}

/**************************** Query Command ****************************/

static int
query_iterator(const ChatInfo *result, void *ctx)
{
  TRACE("entry");
  const ThreadInfo *server = ctx;
  const size_t nTopics = result->nTopics;
  size_t nBytes = strlen(ISO_8601_FORMAT) + 1;
  nBytes += strlen(result->user) + 1 +
    strlen(result->room) + (nTopics > 0); //' ' if nTopics > 0
  for (int i = 0; i < nTopics; i++) {
    nBytes += strlen(result->topics[i]) + (i < nTopics - 1);
  }
  nBytes += 1; //for '\n'
  nBytes += strlen(result->message);
  char buf[nBytes];
  char *p = buf;
  p += timestamp_to_iso8601(result->timestamp, nBytes, p);
  p += sprintf(p, "\n%s %s%s", result->user, result->room,
               (nTopics > 0) ? " " : "");
  for (int i = 0; i < nTopics; i++) {
    p += sprintf(p, "%s%s", result->topics[i], (i < nTopics - 1) ? " " : "");
  }
  p += sprintf(p, "\n");
  p += sprintf(p, "%s", result->message);
  Hdr hdr = { .hdrType = SERVER_HDR, .status = OK_STATUS, .nBytes = nBytes };
  write_header(&hdr, server->out);
  fwrite(buf, 1, nBytes, server->out);
  return 0;
}


static void
do_query_cmd(const ThreadInfo *server, const Hdr *clientHdr)
{
  ChatDb *chatDb = server->chatDb;
  FILE *in = server->in;
  FILE *out = server->out;

  size_t nBytes = clientHdr->nBytes;
  char buf[nBytes];
  fread(buf, 1, nBytes, in);
  TRACE("nBytes = %zu; buf = %.*s", nBytes, (int) nBytes, buf);
  const char *room = buf;
  size_t count;
  int errCode = count_room_chat_db(chatDb, room, &count);
  if (errCode != 0) {
    end_server_response(chatDb, SYS_ERR_STATUS, error_chat_db(chatDb), out);
    return;
  }
  else if (count == 0) {
    end_server_response(chatDb, USER_ERR_STATUS, "BAD_ROOM: unknown room", out);
    return;
  }
  const size_t nTopics = clientHdr->nTopics;
  const char *topics[nTopics];
  const char *p = buf + strlen(room) + 1;
  TRACE("room = %s, topics[0] = %s",
        room, nTopics > 0 ? p : "");
  for (int i = 0; i < nTopics; i++) {
    topics[i] = p;
    p += strlen(p) + 1;
    errCode = count_topic_chat_db(chatDb, topics[i], &count);
    if (errCode != 0) {
      end_server_response(chatDb, SYS_ERR_STATUS, error_chat_db(chatDb), out);
      return;
    }
    else if (count == 0) {
      end_server_response(chatDb, USER_ERR_STATUS,
                          "BAD_TOPIC: unknown topic", out);
      return;
    }
  }
  const char **topicsP = (nTopics == 0) ? NULL : topics;
  errCode = query_chat_db(chatDb, room, nTopics, topicsP, clientHdr->count,
                              query_iterator, (void *)server);
  TRACE("query_chat_db(%p, %s, %zu, %p, %d, %p, %p) = %d",
        chatDb, room, nTopics, topicsP, clientHdr->count,
        query_iterator, server, errCode);
  ServerStatus status = (errCode == 0) ? OK_STATUS : SYS_ERR_STATUS;
  const char *errMsg = (errCode == 0) ? NULL : error_chat_db(chatDb);
  end_server_response(chatDb, status, errMsg, out);
}

/***************************** Add Command *****************************/

static void
do_add_cmd(const ThreadInfo *server, const Hdr *clientHdr)
{
  ChatDb *chatDb = server->chatDb;
  FILE *in = server->in;
  FILE *out = server->out;

  char buf[clientHdr->nBytes];
  fread(buf, 1, clientHdr->nBytes, in);
  TRACE("nBytes = %zu; buf = %s", clientHdr->nBytes, buf);
  const char *user = buf;
  const char *room = user + strlen(user) + 1;
  const char *message = room + strlen(room) + 1;
  const char *p = message + strlen(message) + 1;
  const size_t nTopics = clientHdr->nTopics;
  const char *topics[nTopics];
  TRACE("user = %s, room = %s, message = %s, topics[0] = %s",
        user, room, message, nTopics > 0 ? p : "");
  for (int i = 0; i < nTopics; i++) {
    topics[i] = p;
    p += strlen(p) + 1;
  }
  const char **topicsP = (nTopics == 0) ? NULL : topics;
  int errCode = add_chat_db(chatDb, user, room, nTopics, topicsP, message);
  TRACE("add_chat_db(%p, %s, %s, %zu, %p, %s) = %d",
        chatDb, user, room, nTopics, topicsP, message, errCode);
  ServerStatus status = (errCode == 0) ? OK_STATUS : SYS_ERR_STATUS;
  const char *errMsg = (errCode == 0) ? NULL : error_chat_db(chatDb);
  end_server_response(chatDb, status, errMsg, out);
  broadcast_add_msg(server, user, nTopics, topics, message);
}

/************************** Top-Level Routines *************************/

/** thread function */
static void *
server_loop(void *threadArg)
{
  ThreadArg *argP = (ThreadArg *)threadArg;
  ThreadInfo *threadInfo = argP->threadInfo;
  int err =
    init_thread_info(argP->chatDb, argP->clientSockFd, argP->allThreadInfos,
                     threadInfo);
  free(argP);
  if (err != 0) return NULL;
  bool isDone = false;
  while (!isDone) {
    Hdr hdr = { .hdrType = CLIENT_HDR };
    if (read_header(&hdr, threadInfo->in) != 0) goto CLEANUP;
    switch (hdr.cmdType) {
    case ADD_CMD:
      do_add_cmd(threadInfo, &hdr);
      break;
    case QUERY_CMD:
      TRACE("query");
      do_query_cmd(threadInfo, &hdr);
      break;
    case END_CMD: {
      const Hdr serverHdr = {
        .hdrType = SERVER_HDR, .status = END_STATUS, .nBytes = 0,
      };
      write_header(&serverHdr, threadInfo->out);
      isDone = true;
      break;
    }
    case INIT_CMD:
      do_init_cmd(threadInfo, &hdr);
      break;
    default:
      error("serve(): impossible cmdType = %d\n", hdr.cmdType);
      isDone = true;
      break;
    }
  }
  TRACE("exit server loop");
  broadcast_leave_msg(threadInfo);
 CLEANUP:
  cleanup_thread_info(threadInfo);
  return NULL;
}


//accept loop, start a new thread for each client connection */
void
do_serve(int serverSockFd, const char *dbPath)
{
  MakeChatDbResult result;
  if (make_chat_db(dbPath, &result) != 0) {
    //should not happen, since we already checked, but if it does,
    //there isn't much we can really do, so crash.
    fatal("cannot open db at %s: %s", dbPath, result.err);
  }
  ChatDb *chatDb = result.chatDb;
  enum { MAX_FDS = 20 };
  ThreadInfo threadInfos[MAX_FDS];  //indexed by accepted descriptor
  memset(threadInfos, 0, sizeof(ThreadInfo)*MAX_FDS);
  AllThreadInfos allInfos = {
    .infoArray = threadInfos,
    .nInfoArray = MAX_FDS,
  };
  if (pthread_rwlock_init(&allInfos.rwlock, NULL) != 0) {
    fatal("cannot init rwlock:");
  }
  while (true) {
    TRACE("service loop");
    struct sockaddr_in rsin;
    socklen_t rlen = sizeof(rsin);
    int clientSockFd = -1;
    ThreadArg *argP = NULL;
    clientSockFd = accept(serverSockFd, (struct sockaddr*)&rsin, &rlen);
    if (clientSockFd < 0) {
      error("accept:"); goto FAIL;
    }
    if (clientSockFd >= MAX_FDS) {
      fatal("clientSockFd %d >= MAX_FDS %d", clientSockFd, MAX_FDS);
    }
    argP = malloc(sizeof(ThreadArg));
    if (argP == NULL) {
      error("cannot malloc ThreadArg:"); goto FAIL;
    };
    *argP = (ThreadArg){
      .allThreadInfos = &allInfos,
      .clientSockFd = clientSockFd,
      .chatDb = chatDb,
      .threadInfo = &threadInfos[clientSockFd],
    };
    pthread_t tid;
    if (pthread_create(&tid, NULL, server_loop, argP) != 0) {
      error("cannot create thread:"); goto FAIL;
    }
    if (pthread_detach(tid) != 0) {
      error("cannot detach thread:"); goto FAIL;
    }
    continue;
  FAIL:
    if (argP != NULL) free(argP);
    if (clientSockFd >= 0) close(clientSockFd);
    // would like to cancel thread, but not covered in class
    continue;
  }

}
