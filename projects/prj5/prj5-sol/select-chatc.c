#include "select-chat.h"
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
    .in = in, .out = out, .err = err,
  };
  const char *chatErr = do_chat(&params);
  if (chatErr) {
    fatal("%s", chatErr);
  }

}
