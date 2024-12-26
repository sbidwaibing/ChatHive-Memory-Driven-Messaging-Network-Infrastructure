#ifndef CHAT_H_
#define CHAT_H_

#include <chat-cmd.h>

#include <stdio.h>

#include <unistd.h>

// The project *MUST* meet the implementation constraints detailed
// in the project description.

// standard ADT idiom.
typedef struct _Chat Chat;

/** Return a new Chat object which creates a server process which uses
 *  the sqlite database located at dbPath.  All commands must be sent
 *  by this client process to the server and handled by the server
 *  using the database. All IPC must use anonymous pipes.  The
 *  returned object should encapsulate all the state needed to
 *  implement the following API.
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
 *  will have to be done by the server which will then return an error
 *  response to the client for output.]
 *
 *  The server should not use the `in` or `out` streams.  It may use
 *  `stderr` for "logging", but all such logging *must* be turned off
 *  before submission.
 *
 *  If errors are encountered, then this function should return NULL.
 */
Chat *make_chat(const char *dbPath, FILE *out, FILE *err);

/** free all resources like memory, FILE's and descriptors used by chat
 *  All resources must be freed even after user errors have been detected.
 *  It is okay if resources are not freed after system errors.
 */
void free_chat(Chat *chat);

/** perform cmd using chat, with the client writing response to chat's
 *  out/err streams.  It can be assumed that cmd is free of user
 *  errors except for unknown room/topic for QUERY commands.
 *
 *  If the command is an END_CMD command, then ensure that the server
 *  process is shut down cleanly.
 */
void do_chat_cmd(Chat *chat, const ChatCmd *cmd);

/** return server's PID */
pid_t chat_server_pid(const Chat *chat);

#endif //#ifndef CHAT_H_
