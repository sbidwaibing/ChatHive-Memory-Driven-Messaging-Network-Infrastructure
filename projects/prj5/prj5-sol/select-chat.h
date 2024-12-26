#ifndef SELECT_CHAT_H_
#define SELECT_CHAT_H_

#include <chat-cmd.h>

#include <stdio.h>

#include <unistd.h>

// The project *MUST* meet the implementation constraints detailed
// in the project description.

// standard ADT idiom.
typedef struct _Chat Chat;

typedef struct {
  const char *host;
  int port;
  const char *user;
  const char *room;
  FILE *in;
  FILE *out;
  FILE *err;
} ChatParams;

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
const char *do_chat(const ChatParams *params);

#endif //#ifndef SELECT_CHAT_H_
