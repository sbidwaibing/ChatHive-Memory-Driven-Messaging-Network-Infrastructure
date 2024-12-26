#ifndef CLIENT_H_
#define CLIENT_H_

#include "chat.h"

#include <stdio.h>

#include <unistd.h>

// information about streams used by a client
typedef struct {
  FILE *out;          //for writing success output
  FILE *err;          //for writing error messages
  FILE *serverIn;     //for reading from server
  FILE *serverOut;    //for writing to server
} Client;

// prefix for all error messages
#define ERROR "err "

// line to indicate a successful response
#define OKAY "ok\n"

/** perform cmd using chat, writing response to chat's out/err
 *  streams.  It can be assumed that cmd is free of user errors except
 *  for unknown room/topic for QUERY commands.
 *
 *  If the command is an END_CMD command, then ensure that the server
 *  process is shut down cleanly.
 */
void do_client_cmd(Client *client, const ChatCmd *cmd);

#endif //#ifndef CLIENT_H_
