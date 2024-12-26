#include "chat.h"

#include <chat-cmd.h>
#include <errors.h>
#include <msgargs.h>

#include <stdbool.h>

#include <unistd.h>

// #define NO_MAIN to not compile this function and provide another
// main() function.
#ifndef NO_MAIN

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    fatal("usage: %s SQLITE_DB_PATH", argv[0]);
  }
  FILE *in = stdin;
  FILE *out = stdout;
  FILE *err = stderr;
  bool isInteractive = isatty(fileno(in));
  const char *prompt = isInteractive ? "> " : "";
  MsgArgs *msgArgs = NULL;
  ErrNum errNum;
  ChatCmd cmd;
  Chat *chat = make_chat(argv[1], out, err);
  if (chat == NULL) fatalf(err, "unable to create Chat:");
  fprintf(out, "%lld\n", (long long)chat_server_pid(chat));
  fprintf(out, "%s", prompt); fflush(out);
  while ((msgArgs = read_msg_args(in, msgArgs, &errNum)) != NULL) {
    if (errNum != NO_ERR) { fatal("%s", errnum_to_string(errNum)); }
    int rc = parse_cmd(msgArgs, &cmd, err);
    if (rc == 0) do_chat_cmd(chat, &cmd);
    fprintf(out, "%s", prompt); fflush(out);
  }
  cmd.type = END_CMD;
  do_chat_cmd(chat, &cmd);
  free_chat(chat);
}

#endif //#ifndef NO_MAIN
