#include "chat.h"
#include "common.h"

#include <chat-cmd.h>
#include <errors.h>
#include <msgargs.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

int
main(int argc, const char *argv[]) {
  if (argc != 5) {
    fatal("usage: %s HOST PORT USER ROOM", argv[0]);
  }
  FILE *in = stdin;
  FILE *out = stdout;
  FILE *err = stderr;
  const char *host = argv[1];
  const char *portStr = argv[2];
  const char *user = argv[3];
  const char *room = argv[4];
  char *p;
  int port = strtol(portStr, &p, 10);
  if (*p != '\0' || port < 1024 || port > 65535) {
    fatal("invalid port \"%s\"; must be int in [1024, 65535]", portStr);
  }
  if (user[0] != '@') {
    fatal("bad user \"%s\" must start with '@' character", user);
  }
  if (!isalpha(room[0])) {
    fatal("bad room \"%s\" must start with a letter", room);
  }

  ChatParams params = {
    .host = host, .port = port, .user = user, .room = room,
    .out = out, .err = err,
  };
  Chat *chat = make_chat(&params);
  if (!chat) fatal("cannot create chat client:");
  const char *prompt = "> ";
  MsgArgs *msgArgs = NULL;
  ErrNum errNum;
  ChatCmd cmd;

  fprintf(out, "%s", prompt); fflush(out);
  while ((msgArgs = read_line_args(in, msgArgs, &errNum)) != NULL) {
    if (errNum != NO_ERR) { fatal("%s", errnum_to_string(errNum)); }
    int rc = parse_loggedin_cmd(msgArgs, user, room, &cmd, err);
    if (rc == 0) do_chat_cmd(chat, &cmd);
    fprintf(out, "%s", prompt); fflush(out);
  }

  // send END_CMD
  cmd.type = END_CMD;
  do_chat_cmd(chat, &cmd);

  // cleanup
  free_chat(chat);

}
