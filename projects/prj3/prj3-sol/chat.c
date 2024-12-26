#include "chat.h"
#include "client.h"
#include "utils.h"

#include <chat-cmd.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

struct _Chat {
  Client client;
};

/** Return a new Chat object which contacts the chatd daemon runnning
 *  in serverDir.  All commands must be sent by this client process to
 *  a worker process and handled by that worker process using its
 *  database. All IPC must use named pipes.  The returned object
 *  should encapsulate all the state needed to implement the following
 *  API.
 *
 *  The client process must use `out` for writing success output for
 *  commands where each output must start with a line containing "ok".
 *
 *  The client process must use `err` for writing error message
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
make_chat(const char *serverDir, FILE *out, FILE *err)
{
  if (chdir(serverDir) < 0) return NULL;
  if (make_client_fifos() != 0) return NULL;
  FILE *wellKnownFifo = NULL;
  FILE *clientFifos[] = { NULL, NULL };
  if (open_well_known_fifo(true, &wellKnownFifo) != 0) goto CLEANUP;
  pid_t clientPid = getpid();
  fprintf(wellKnownFifo, "%lld\n", (long long)clientPid);
  fflush(wellKnownFifo);
  fclose(wellKnownFifo); wellKnownFifo = NULL;
  if (open_client_fifos(clientPid, true, clientFifos) != 0) goto CLEANUP;
  Chat *chat = malloc(sizeof(Chat));
  if (chat == NULL) goto CLEANUP;
  chat->client = (Client) {
    .out = out,
    .err = err,
    .serverIn = clientFifos[0],
    .serverOut = clientFifos[1],
  };
  return chat;
 CLEANUP:
  if (wellKnownFifo != NULL) fclose(wellKnownFifo);
  if (clientFifos[0] != NULL) fclose(clientFifos[0]);
  if (clientFifos[1] != NULL) fclose(clientFifos[1]);
  remove_client_fifos();
  if (chat != NULL) free(chat);
  return NULL;
}

/** free all resources like memory, FILE's and descriptors used by chat
 *  All resources must be freed even after user errors have been detected.
 *  It is okay if resources are not freed after system errors.
 */
void
free_chat(Chat *chat)
{
  fclose(chat->client.serverIn);
  fclose(chat->client.serverOut);
  remove_client_fifos();
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
