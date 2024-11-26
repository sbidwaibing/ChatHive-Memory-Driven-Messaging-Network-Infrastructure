#include "chat-cmd.h"
#include "msgargs.h"

#include <errors.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define ERROR "err "

static int
parse_add_cmd(const MsgArgs *add, ChatCmd *cmd, FILE *err)
{
  assert(strcmp(add->args[0], "+") == 0);
  if (add->nArgs < 2) return errorf(err, ERROR "BAD_USER: missing USER arg");
  if (add->args[1][0] != '@') {
    return errorf(err, ERROR "BAD_USER: USER arg \"%s\" does not start with a '@'",
                  add->args[1]);
  }
  if (add->nArgs < 3) return errorf(err, ERROR "BAD_ROOM: missing ROOM arg");
  if (!isalpha(add->args[2][0])) {
    return errorf(err, ERROR "BAD_ROOM: ROOM \"%s\" does not start with an "
                  "alphabetical character", add->args[2]);
  }
  for (int i = 3; i < add->nArgs; i++) {
    if (add->args[i][0] != '#') {
       return  errorf(err, ERROR "BAD_TOPIC: topic \"%s\" does not start with a '#'",
                      add->args[i]);
    }
  }
  if (add->msg == NULL) return errorf(err, ERROR "NO_MSG: missing message");

  //all okay, fill out *cmd
  cmd->type = ADD_CMD;
  cmd->add.user = add->args[1];
  cmd->add.room = add->args[2];
  cmd->add.nTopics = add->nArgs - 3;
  cmd->add.topics = &add->args[3];
  cmd->add.message = add->msg;
  return 0;
}


static int
parse_query_cmd(const MsgArgs *query, ChatCmd *cmd, FILE *err)
{
  long count = 1;
  int topicsIndex = 2;
  assert(strcmp(query->args[0], "?") == 0);
  if (query->nArgs < 2) return errorf(err, ERROR "BAD_ROOM: missing ROOM arg");
  if (!isalpha(query->args[1][0])) {
    return errorf(err, ERROR "BAD_ROOM: ROOM arg \"%s\" "
                  "does not start with a letter", query->args[1]);
  }
  if (query->nArgs > 2 && isdigit(query->args[2][0])) {
    char *p;
    count = strtol(query->args[2], &p, 10);
    topicsIndex++;
    if (*p != '\0') {
      return errorf(err, ERROR "BAD_COUNT: bad COUNT arg \"%s\"", query->args[2]);
    }
  }
  if (query->msg != NULL) {
    return errorf(err, ERROR "BAD_MESSAGE: query command cannot have a message");
  }
  for (int i = topicsIndex; i < query->nArgs; i++) {
    const char *topic = query->args[i];
    if (topic[0] != '#') {
      return errorf(err, ERROR "BAD_TOPIC: \"%s\" does not start with '#'", topic);
    }
  }

  //all okay, fill out *cmd
  cmd->type = QUERY_CMD;
  cmd->query.room = query->args[1];
  cmd->query.count = count;
  cmd->query.nTopics = query->nArgs - topicsIndex;
  cmd->query.topics = &query->args[topicsIndex];
  return 0;
}

// input should specify an ADD or QUERY command:
// ADD: should have input->args[] "+" USER ROOM TOPIC*  and input->msg.
// QUERY: should have input->args[] "?" ROOM COUNT? TOPIC*, no input->msg.
// USER must start @, ROOM with letter, COUNT with digit, TOPIC with #.

/** fill in cmd by parsing input.  Note that all strings in cmd share
 *  storage with strings in input.  Print error message on err if
 *  an error is detected and return non-zero; return 0 otherwise.
 */
int
parse_cmd(const MsgArgs *input, ChatCmd *cmd, FILE *err)
{
  const char *cmdSpec = (input->nArgs == 0) ? "" : input->args[0];
  if (strcmp(cmdSpec, "+") == 0) {
    return parse_add_cmd(input, cmd, err);
  }
  else if (strcmp(cmdSpec, "?") == 0) {
    return parse_query_cmd(input, cmd, err);
  }
  else if (strlen(cmdSpec) == 0) {
    errorf(err, ERROR "BAD_COMMAND: missing command");
    return 1;
  }
  else {
    errorf(err, ERROR "BAD_COMMAND: unknown command \"%s\"", cmdSpec);
    return 1;
  }
}

/** fill in cmd by parsing input for user logged into chat-room room.
 *  Note that all strings in cmd share storage with strings in input
 *  as well as user and room.  Print error message on err if an error
 *  is detected and return non-zero; return 0 otherwise.
 */
int
parse_loggedin_cmd(const MsgArgs *input, const char *user, const char *room,
                   ChatCmd *cmd, FILE *err)
{
  if (strcmp(input->args[0], "+") == 0) {
    input->args[1] = user;
    input->args[2] = room;
  }
  return parse_cmd(input, cmd, err);
}

/** utility routine to print cmd on out; use for debugging. */
void
print_cmd(FILE *out, const ChatCmd *cmd) {
  switch (cmd->type) {
  case ADD_CMD:
    fprintf(out, "ADD %s %s ", cmd->add.user, cmd->add.room);
    for (int i = 0; i < cmd->add.nTopics; i++) {
      fprintf(out, "%s ", cmd->add.topics[i]);
    }
    fprintf(out, "\n");
    fprintf(out, "%s", cmd->add.message);
    break;
  case QUERY_CMD:
    fprintf(out, "QUERY %s %zu ", cmd->query.room, cmd->query.count);
    for (int i = 0; i < cmd->query.nTopics; i++) {
      fprintf(out, "%s ", cmd->query.topics[i]);
    }
    fprintf(out, "\n");
    break;
  case END_CMD:
    fprintf(out, "END\n");
    break;
  default:
    assert(0);
  }
  fprintf(out, "-------------\n");
}

#ifdef TEST_CHAT_CMD

char *testLines =
  "+ @ZDU room #topic1 #topic2\n"   //ADD
  "This is a message\n"
  ".\n"
  "? room 22 #topic\n"              //QUERY
  ".\n"
  "- room 22 #topic\n"              //err BAD_CMD
  ".\n"
  "+ room 22 #topic\n"              //err BAD_USER
  ".\n"
  "+ @ZDU 22 #topic\n"              //err BAD_ROOM
  ".\n"
  "+ @ZDU room 22 #topic\n"         //err BAD_TOPIC
  ".\n";


int
main(int argc, const char *argv[])
{
  FILE *in = (argc > 1) ? fmemopen(testLines, strlen(testLines), "r") : stdin;
  FILE *out = stdout;
  FILE *err = stderr;
  MsgArgs *msgArgs = NULL;
  ErrNum errNum;
  ChatCmd cmd;
  while ((msgArgs = read_msg_args(in, msgArgs, &errNum)) != NULL) {
    if (errNum != NO_ERR) { fatal("%s", errnum_to_string(errNum)); }
    int rc = parse_cmd(msgArgs, &cmd, err);
    if (rc == 0) print_cmd(out, &cmd);
  }
  if (argc > 1) fclose(in);
  cmd.type = END_CMD;
  print_cmd(out, &cmd);
}

#endif //#ifdef TEST_CHAT_CMD
