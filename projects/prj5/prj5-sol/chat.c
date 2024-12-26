#include "chat.h"
#include "common.h"

#include <chat-cmd.h>
#include <errors.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pthread.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>

struct _Chat {
  FILE *out;          //for writing success output
  FILE *err;          //for writing error messages
  FILE *serverIn;     //for reading from server
  FILE *serverOut;    //for writing to server
  const char *user;
  const char *room;
  pthread_t tid;      //thread used to receive data from server
};

// prefix for all error messages
#define ERROR "err "

// line to indicate a successful response
#define OKAY "ok\n"

static void
send_init_req(Chat *chat)
{
  size_t nBytes = 0;
  nBytes += (strlen(chat->user) + 1);
  nBytes += (strlen(chat->room) + 1);
  Hdr hdr = {
    .hdrType = CLIENT_HDR,
    .cmdType = INIT_CMD,
    .count = -1,
    .nTopics = 0,
    .nBytes = nBytes,
  };
  FILE *out = chat->serverOut;
  if (write_header(&hdr, out) != 0) fatal("send_init_req(): write header:");
  fwrite(chat->user, 1, strlen(chat->user)+1, out);
  fwrite(chat->room, 1, strlen(chat->room)+1, out);
  fflush(out);
}

/** send params for add cmd to remote server specified by client,
 *  as per protocol.
 */
static void
send_add_req(Chat *chat, const AddCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->user) + 1);
  nBytes += (strlen(cmd->room) + 1);
  nBytes += (strlen(cmd->message) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  Hdr hdr = {
    .hdrType = CLIENT_HDR,
    .cmdType = ADD_CMD,
    .count = -1,
    .nTopics = cmd->nTopics,
    .nBytes = nBytes,
  };
  FILE *out = chat->serverOut;
  if (write_header(&hdr, out) != 0) fatal("send_add_req(): write header:");
  fwrite(cmd->user, 1, strlen(cmd->user)+1, out);
  fwrite(cmd->room, 1, strlen(cmd->room)+1, out);
  fwrite(cmd->message, 1, strlen(cmd->message)+1, out);
  for (int i = 0; i < cmd->nTopics; i++) {
    fwrite(cmd->topics[i], 1, strlen(cmd->topics[i])+1, out);
  }
  fflush(out);
}

/** send params for query cmd to remote server specified by chat,
 *  as per protocol.
 */
static void
send_query_req(Chat *chat, const QueryCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->room) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  Hdr hdr = {
    .hdrType = CLIENT_HDR,
    .cmdType = QUERY_CMD,
    .count = cmd->count,
    .nTopics = cmd->nTopics,
    .nBytes = nBytes,
  };
  FILE *out = chat->serverOut;
  if (write_header(&hdr, out) != 0) fatal("send_query_req(): write header:");
  fwrite(cmd->room, 1, strlen(cmd->room)+1, out);
  for (int i = 0; i < cmd->nTopics; i++) {
    fwrite(cmd->topics[i], 1, strlen(cmd->topics[i])+1, out);
  }
  fflush(out);
}

/** receive response from remote server as per protocol, copying
 *  response onto appropriate chat stream: out if response was okay,
 *  err if response was in error.
 */
static bool
receive_res(Chat *chat)
{
  FILE *in = chat->serverIn;
  FILE *out = chat->out;
  FILE *err = chat->err;
  bool didOk = false;  //set true once we output OKAY
  do { //loop until we get a error response or an empty ok response
    Hdr hdr = { .hdrType = SERVER_HDR };
    if (read_header(&hdr, in) != 0) fatal("cannot read header:");
    size_t nBytes = hdr.nBytes;
    TRACE("read hdr status = %d, nBytes = %zu", hdr.status, nBytes);
    if (hdr.status == END_STATUS) return true;
    if (hdr.status != 0) { //error response
      char buf[nBytes];
      if (fread(buf, 1, nBytes, in) != nBytes) {
        fatal("error reading error response:");
      }
      const char *errStatus =
        (const char *[]){ "", "SYS_ERR: ", "FATAL_ERR: " }[hdr.status - 1];
      fprintf(err, ERROR "%s%.*s\n", errStatus, (int)nBytes, buf);
      fflush(err);
      break;  //end loop on error response
    }
    else { //success response
      fprintf(out, "%s", didOk ? "" : OKAY); fflush(out);
      didOk = true;
      if (nBytes == 0) break; //end loop on empty ok response
      char buf[nBytes];
      if (fread(buf, 1, nBytes, in) != nBytes) {
        fatal("error reading success response:");
      }
      TRACE("read buf %.*s", (int)nBytes, buf);
      fprintf(out, "%.*s", (int)nBytes, buf);  fflush(out);
    }
  } while (true);
  return false;
}

//thread function
static void *
do_receive(void *threadArg)
{
  Chat *chat = threadArg;
  bool isDone = false;
  while (!isDone) {
    isDone = receive_res(chat);
  }
  return NULL;
}

/** perform cmd using chat, writing response to chat's out/err
 *  streams.  It can be assumed that cmd is free of user errors except
 *  for unknown room/topic for QUERY commands.
 *
 *  If the command is an END_CMD command, then ensure that the server
 *  process is shut down cleanly.
 */
void
do_chat_cmd(Chat *chat, const ChatCmd *cmd)
{
  switch (cmd->type) {
  case ADD_CMD:
    send_add_req(chat, &cmd->add);
    break;
  case QUERY_CMD:
    send_query_req(chat, &cmd->query);
    break;
  case END_CMD: {
    Hdr hdr = { .hdrType = CLIENT_HDR, .cmdType = END_CMD };
    if (write_header(&hdr, chat->serverOut) != 0) {
      fatal("do_end_cmd() write header:");
    }
    break;
  }
  default:
    assert(0);
  }
}

static int
make_client_sock(const char *host, int port)
{
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  if (inet_pton(AF_INET, host, &sin.sin_addr.s_addr) <= 0) {
    error("cannot convert host %s:", host); return -1;
  }
  sin.sin_port = htons((unsigned short)port);

  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) { error("socket:"); return -1; }

  if (connect(s, (const struct sockaddr*)&sin, sizeof(sin)) < 0) {
    error("cannot connect:"); close(s); return -1;
  }
  return s;
}

/** Return a new Chat object which contacts the chatd server runnning
 *  on params->host and params->port on behalf of user params->user in
 *  room params->room.  All commands must be sent by this client
 *  process to the server and handled using its database. All IPC must
 *  use the network.  The returned object should encapsulate all the
 *  state needed to implement the following API.
 *
 *  The client process must use params->out for writing success output for
 *  commands where each output must start with a line containing "ok".
 *
 *  The client process must use params->err for writing error message
 *  lines. Each line must start with "err ERR_CODE: " where ERR_CODE is
 *  as in your previous project for user errors or SYS_ERR for non-user
 *  errors.

 *  [Note that since a `ChatCmd` is guaranteed to be syntactically
 *  valid, the only user errors which the program will need to detect
 *  will be `BAD_ROOM`/`BAD_TOPIC` for unknown room or topic.  This
 *  will have to be done by the worker process which will then return
 *  an error response to the client for output.]
 *
 *  If errors are encountered, then this function should return NULL.
 */
Chat *
make_chat(const ChatParams *params)
{
  int clientSockFd = make_client_sock(params->host, params->port);
  if (clientSockFd < 0) return NULL;
  FILE *serverIn = NULL;
  FILE *serverOut = NULL;
  Chat *chat = NULL;
  serverIn = fdopen(clientSockFd, "r");
  if (!serverIn) goto FAIL;
  serverOut = fdopen(clientSockFd, "w");
  if (!serverOut) goto FAIL;
  chat = malloc(sizeof(Chat));
  if (!chat) goto FAIL;
  chat->out = params->out; chat->err = params->err;
  chat->serverIn = serverIn; chat->serverOut = serverOut;
  chat->user = params->user; chat->room = params->room;
  pthread_t tid;
  if (pthread_create(&tid, NULL, do_receive, chat) != 0) goto FAIL;
  chat->tid = tid;
  send_init_req(chat);
  return chat;
 FAIL:
  if (chat) free(chat);
  if (serverOut) fclose(serverOut);
  if (serverIn) fclose(serverIn);
  return NULL;
}

/** free all resources like memory, FILE's and descriptors used by chat
 *  All resources must be freed even after user errors have been detected.
 *  It is okay if resources are not freed after system errors.
 */
void
free_chat(Chat *chat)
{
  pthread_join(chat->tid, NULL);
  fclose(chat->serverIn);
  fclose(chat->serverOut);
  free(chat);
}
