#include "chat.h"
#include "client.h"
#include "common.h"
#include "server.h"

#include <chat-cmd.h>
#include <errors.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>

// define ADT
struct _Chat {
  Client client;
  pid_t serverPid;
};

/**************************** Private Routines *************************/

static Chat *
do_client(pid_t serverPid, FILE *out, FILE *err, int inPipe[2], int outPipe[2])
{
  FILE *serverIn = NULL;
  FILE *serverOut = NULL;
  Chat *chat = NULL;
  const char *errMsg;
  if (close(inPipe[1]) < 0) {
    errMsg = "SYS_ERR: client cannot close inPipe[1]";
    goto CLEANUP;
  }
  if (close(outPipe[0]) < 0) {
    errMsg = "SYS_ERR: client cannot close outPipe[0]";
    goto CLEANUP;
  }
  serverIn = fdopen(inPipe[0], "r");
  if (!serverIn) {
    errMsg = "SYS_ERR: client cannot fdopen(inPipe[0])";
    goto CLEANUP;
  }
  serverOut = fdopen(outPipe[1], "w");
  if (!serverOut) {
    errMsg = "SYS_ERR: server: cannot fdopen(inPipe[1])";
    goto CLEANUP;
  }
  chat = malloc(sizeof(Chat));
  if (!chat) {
    errMsg = "SYS_ERR: cannot malloc Chat";
    goto CLEANUP;
  }
  *chat = (Chat) {
    .client = (Client) {
      .out = out,
      .err = err,
      .serverIn = serverIn,
      .serverOut = serverOut,
    },
    .serverPid = serverPid,
  };
  return chat;
 CLEANUP:
  if (serverIn) fclose(serverIn);
  if (serverOut) fclose(serverOut);
  if (chat) free(chat);
  errorf(err, ERROR "%s:", errMsg);
  return NULL;
}

/******************************* Public API ****************************/

/** Return a new Chat object which creates a server process which uses
 *  the sqlite database located at dbPath.  All commands must be sent
 *  by this client process to the server and handled by the server
 *  using the database. All IPC must use anonymous pipes.  The
 *  returned object should encapsulate all the state needed to
 *  implement the following API.
 *
 *  The client process can use `out` for writing success output for
 *  commands where each output must start with a line containing "ok".
 *
 *  The client process should use `err` for writing error message
 *  lines (must start with "err ERR_CODE: " where ERR_CODE is
 *  BAD_ROOM/BAD_TOPIC for unknown room/topic, or SYS_ERR for non-user
 *  errors).
 *
 *  The server should not use the `in` or `out` streams.  It may use
 *  `stderr` for "logging", but all such logging *must* be turned off
 *  before submission.
 *
 *  If errors are encountered, then this function should return NULL.
 */
Chat *
make_chat(const char *dbPath, FILE *out, FILE *err)
{
  int inPipe[2];  //for reading by client
  if (pipe(inPipe) < 0) return NULL;

  int outPipe[2]; //for writing by client
  if (pipe(outPipe) < 0) {
    close(inPipe[0]); close(outPipe[1]);
    return NULL;
  }
  pid_t serverPid = fork();
  if (serverPid < 0) return NULL;
  if (serverPid == 0) {
    //server process
    do_server(dbPath, outPipe, inPipe);
  }
  else {
    //client process
    Chat *chat = do_client(serverPid, out, err, inPipe, outPipe);
    return chat;
  }
  return NULL;
}


/** free all resources like memory, FILE's and descriptors used by chat
 *  All resources must be freed even after user errors have been detected.
 *  It is okay if resources are not freed after system errors.
 */
void
free_chat(Chat *chat)
{
  fclose(chat->client.serverOut);
  fclose(chat->client.serverIn);
  free(chat);
}

/** perform cmd using chat, with the client writing response to chat's
 *  out/err streams.  It can be assumed that cmd is free of user
 *  errors except for unknown room/topic for QUERY commands.
 *
 *  If the command is an END_CMD command, then ensure that the server
 *  process is shut down cleanly.
 */
void
do_chat_cmd(Chat *chat, const ChatCmd *cmd)
{
  do_client_cmd(&chat->client, cmd);
}

/** return server's PID */
pid_t
chat_server_pid(const Chat *chat)
{
  return chat->serverPid;
}
