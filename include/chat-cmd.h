#ifndef CHAT_CMD_H_
#define CHAT_CMD_H_

#include "msgargs.h"

#include <stdio.h>

typedef enum { ADD_CMD, QUERY_CMD, END_CMD, N_CMDS } CmdType;

typedef struct {
  const char *user;
  const char *room;
  const char *message;
  size_t nTopics;
  const char **topics;   // topics[nTopics]
} AddCmd;

typedef struct {
  const char *room;
  size_t count;
  size_t nTopics;
  const char **topics;   // topics[nTopics]
} QueryCmd;

typedef struct {
  CmdType type;
  union {
    AddCmd add;
    QueryCmd query;
  };
} ChatCmd;

/** fill in cmd by parsing input.  Note that all strings in cmd share
 *  storage with strings in input.  Print error message on err if
 *  an error is detected and return non-zero; return 0 otherwise.
 */
int parse_cmd(const MsgArgs *input, ChatCmd *cmd, FILE *err);

/** fill in cmd by parsing input for user logged into chat-room room.
 *  Note that all strings in cmd share storage with strings in input
 *  as well as user and room.  Print error message on err if an error
 *  is detected and return non-zero; return 0 otherwise.
 */
int parse_loggedin_cmd(const MsgArgs *input, const char *user, const char *room,
                       ChatCmd *cmd, FILE *err);


/** utility routine to print cmd on out; use for debugging. */
void print_cmd(FILE *out, const ChatCmd *cmd);

#endif //#ifndef CHAT_CMD_H_
