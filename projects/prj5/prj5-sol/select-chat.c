#include "select-chat.h"
#include "common.h"

#include <chat-cmd.h>
#include <errors.h>
#include <msgargs.h>

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
#include <sys/select.h>
#include <unistd.h>


//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>

struct _Chat {
  FILE *in;           //for reading commands
  FILE *out;          //for writing success output
  FILE *err;          //for writing error messages
  FILE *serverIn;     //for reading from server
  FILE *serverOut;    //for writing to server
  const char *user;
  const char *room;
  const char *prompt;
  MsgArgs *msgArgs;
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

static bool
do_read(Chat *chat)
{
  ErrNum errNum;
  ChatCmd cmd;
  chat->msgArgs = read_line_args(chat->in, chat->msgArgs, &errNum);
  if (chat->msgArgs != NULL) {
    if (errNum != NO_ERR) { fatal("%s", errnum_to_string(errNum)); }
    int rc = parse_loggedin_cmd(chat->msgArgs, chat->user, chat->room, &cmd,
                                chat->err);
    if (rc == 0) do_chat_cmd(chat, &cmd);
    fprintf(chat->out, "%s", chat->prompt); fflush(chat->out);
    return false;
  }
  else {
    // send END_CMD
    cmd.type = END_CMD;
    do_chat_cmd(chat, &cmd);
    return true;
  }
}

#define PROMPT ">> "

/** Do a chat with the chatd server runnning on params->host and
 *  params->port on behalf of user params->user in room params->room.
 *  All commands read from params->in must be sent by this client
 *  process to the server and handled using its database. All IPC must
 *  use the network.
 *
 *  The client process must use params->out for writing success output for
 *  commands where each output must start with a line containing "ok".
 *
 *  The client process must use params->err for writing error message
 *  lines. Each line must start with "err ERR_CODE: " where ERR_CODE is
 *  as in your previous project for user errors or SYS_ERR for non-user
 *  errors.
 *
 *  If errors are encountered during initialization, then this function
 *  returns a const error string.  Otherwise it will return NULL when
 *  EOF is encountered on params->in.
 */
const char *
do_chat(const ChatParams *params)
{
  TRACE("entry");
  int clientSockFd = make_client_sock(params->host, params->port);
  if (clientSockFd < 0) return "cannot create client socket";
  FILE *serverIn = NULL;
  FILE *serverOut = NULL;
  serverIn = fdopen(clientSockFd, "r");
  if (!serverIn) return "cannot create r-FILE on client socket";
  serverOut = fdopen(clientSockFd, "w");
  if (!serverOut) {
    fclose(serverIn);
    return "cannot create w-FILE on client socket";
  }
  TRACE("created socket and FILEs");
  Chat chatStruct = { .in = params->in, .out = params->out, .err = params->err,
    .serverIn = serverIn, .serverOut = serverOut,
    .user = params->user, .room = params->room, .msgArgs = NULL,
    .prompt = PROMPT,
  };
  Chat *chat = &chatStruct;
  send_init_req(chat);
  TRACE("sent init-req");
  int inFd = fileno(params->in);
  int nFds = ((inFd > clientSockFd) ? inFd : clientSockFd)  + 1;
  fd_set fds;
  bool isDone = false;
  fprintf(chat->out, "%s", chat->prompt); fflush(chat->out);  //initial prompt
  while (!isDone) {
    TRACE("select loop");
    FD_ZERO(&fds);
    FD_SET(clientSockFd, &fds);
    FD_SET(inFd, &fds);
    int nReady = select(nFds, &fds, NULL, NULL, NULL);
    if (nReady < 0) {
      error("select:");
    }
    else {
      for (int i = 0; i < nReady; i++) {
        TRACE("select inner loop: i=%d, nReady=%d", i, nReady);
        if (FD_ISSET(inFd, &fds)) {
          FD_CLR(inFd, &fds);
          isDone = do_read(chat);
          continue;
        }
        else if (FD_ISSET(clientSockFd, &fds)) {
          FD_CLR(clientSockFd, &fds);
          receive_res(chat);
          continue;
        }
        else {
          error("unknown fd is ready!");
        }
      } //for
    } //else nReady >= 0
  }  //while (!isDone)
  return NULL;
}
