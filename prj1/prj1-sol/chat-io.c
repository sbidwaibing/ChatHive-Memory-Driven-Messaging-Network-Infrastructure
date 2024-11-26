#include "chat-io.h"

#include "chat.h"
#include "errnum.h"

#include <errors.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void
do_add_cmd(Chat *chat, MsgArgs *add, FILE *err)
{
  assert(strcmp(add->args[0], "+") == 0);
  if (add->nArgs < 2) {
    errorf(err, "BAD_USER: missing USER arg");
  }
  else if (add->args[1][0] != '@') {
    errorf(err, "BAD_USER: USER arg \"%s\" does not start with a '@'",
           add->args[1]);
  }
  else if (add->nArgs < 3) {
    errorf(err, "BAD_ROOM: missing ROOM arg");
  }
  else if (!isalpha(add->args[2][0])) {
    errorf(err, "BAD_ROOM: ROOM \"%s\" does not start with an "
           "alphabetical character", add->args[2]);

  }
  else {
    for (int i = 3; i < add->nArgs; i++) {
      if (add->args[i][0] != '#') {
        errorf(err, "BAD_TOPIC: topic \"%s\" does not start with a '#'",
               add->args[i]);
        return;
      }
    }
    if (add->msg == NULL) {
      errorf(err, "NO_MSG: missing message");
      return;
    }
    ErrNum errnum = NO_ERR;
    add_chat(chat, add, &errnum);
    if (errnum != NO_ERR) fatalf(err, "%s", errnum_to_string(errnum));
  }
}

static void
out_chat(MsgInfo *info, FILE *out) {
  fprintf(out, "%s %s", info->user, info->room);
  const char *p = info->topics;
  for (int i = 0; i < info->nTopics; i++) {
    fprintf(out, " %s", p);
    p += strlen(p) + 1;
  }
  fprintf(out, "\n%s", info->msg);
}

static void
do_query_cmd(const Chat *chat, const MsgArgs *query,
             FILE *out, FILE *err)
{
  long count = 1;
  int topicsIndex = 2;
  assert(strcmp(query->args[0], "?") == 0);
  if (query->nArgs < 2) {
    errorf(err, "BAD_ROOM: missing ROOM arg");
    return;
  }
  else if (!isalpha(query->args[1][0])) {
    errorf(err, "BAD_ROOM: ROOM arg \"%s\" "
           "does not start with a letter", query->args[1]);
    return;
  }
  else if (!is_known_room(chat, query->args[1])) {
    errorf(err, "BAD_ROOM: unknown ROOM arg \"%s\"", query->args[1]);
    return;
  }
  else if (query->nArgs > 2 && isdigit(query->args[2][0])) {
    char *p;
    count = strtol(query->args[2], &p, 10);
    topicsIndex++;
    if (*p != '\0') {
      errorf(err, "BAD_COUNT: bad COUNT arg \"%s\"", query->args[2]);
      return;
    }
  }
  for (int i = topicsIndex; i < query->nArgs; i++) {
    const char *topic = query->args[i];
    if (topic[0] != '#') {
      errorf(err, "BAD_TOPIC: \"%s\" does not start with '#'",
             topic);
      return;
    }
    else if (!is_known_topic(chat, topic)) {
      errorf(err, "BAD_TOPIC: unknown TOPIC arg \"%s\"", topic);
      return;
    }
  }
  MsgInfo *lastMatch = NULL;
  for (int i = 0; i < count; i++) { //external iterator requires us to iterate
    lastMatch = query_chat(chat, query, lastMatch);
    if (!lastMatch) return;
    out_chat(lastMatch, out);
  }
}

/** This function should read commands from `in` and write successful
 *  responses to `out` and errors to `err` as per the following spec.
 *  It should flush prompt on `out` before each read from `in`.
 *
 *  A word is defined to be a maximal sequence of characters without
 *  whitespace.  We have the following types of words:
 *
 *    USER:  A word starting with a '@' character.
 *    TOPIC: A word starting with a '#' character.
 *    ROOM:  A word starting with a letter.
 *    COUNT: A non-negative integer.
 *
 *  Comparison of all the above words must be case-insensitive.
 *
 *  In the following description, we use regex-style notation:
 *
 *     X?:  Denotes an optional X.
 *     X*:  Denotes 0-or-more occurrences of X.
 *
 *  Errors are of two kinds:
 *
 *    *User Error* caused by incorrect user input.  This should result
 *    in a single line error message being printed on `err` starting
 *    with a specific error code (defined below) followed by a single
 *    ": " followed by an informational message; the informational
 *    message is not defined by this spec but can be any text giving
 *    the details of the error.  The function should continue reading
 *    the next input after outputting the user error.
 *
 *    *System Error* caused by a system error like a memory allocation
 *    failure, an I/O error or an internal error (like an assertion
 *    failure).  The program should terminate (with a non-zero status)
 *    after printing a suitable message on `err` (`stderr` if it is
 *    an assertion failure).
 *
 *  There are two kinds of commands:
 *
 *  1) An ADD command consists of the following sequence of words
 *  starting with the "+" word:
 *
 *    + USER ROOM TOPIC*
 *
 *  followed by *one*-or-more lines constituting a MESSAGE terminated
 *  by a line containing only a '.'.  Note that the terminating line
 *  is not included in the MESSAGE.
 *
 *  A sucessful ADD command will add MESSAGE to chat-room ROOM on
 *  behalf of USER associated with the specified TOPIC's (if any).  It
 *  will succeed silently and will not produce any output on out or
 *  err.
 *
 *  An incorrect ADD command should output a single line on `err`
 *  giving the first error detected from the following possiblities:
 *
 *    BAD_USER:  USER not specified or USER does not start with '@'
 *    BAD_ROOM:  ROOM not specified or ROOM does not start with a
 *               alphabetic character.
 *    BAD_TOPIC: TOPIC does not start with a '#'.
 *    NO_MSG:    message is missing
 *
 *  It is not an error for an ADD command to specify duplicate TOPIC's.
 *
 *  2) A QUERY command consists of a single line starting with the "?"
 *  word:
 *
 *    ? ROOM COUNT? TOPIC*
 *
 *   followed by a terminating line containing only a '.'.
 *
 *   If specified, COUNT must specify a positive integer.  If not
 *   specified, it should default to 1.
 *
 *   A successful QUERY must output (on `out`) up to the last COUNT
 *   messages from chat-room ROOM which match all of the specified
 *   TOPIC's in a LIFO order.
 *
 *   Each matching message must be preceded by a line containing
 *
 *      USER ROOM TOPIC*
 *
 *   where USER is the user who posted the message, ROOM is the
 *   queried ROOM and TOPIC's are all the *distinct* topics associated
 *   with the message in the same order as when added. In case there
 *   are duplicate topics, only the first occurrence of that topic is
 *   shown.  The USER, ROOM and TOPICs must be output in lower-case.  The
 *   message contents must be output with the same whitespace content
 *   as when input.
 *
 *   If there are no messages matching the specified QUERY, then no
 *   output should be produced. If there are fewer than COUNT matching
 *   messages, then those messages should be output without any error.
 *
 *  An incorrect QUERY command should output a single line on `err`
 *  giving the first error detected from the following possiblities:
 *
 *    BAD_ROOM:  ROOM not specified or ROOM does not start with a
 *               alphabetic character.
 *               or room has not been specified in any added message.
 *    BAD_COUNT: COUNT is not a positive integer.
 *    BAD_TOPIC: A TOPIC does not start with a '#" or it
 *               has not been specified in any added message.
 *
 *  If the command is not a ADD or QUERY command then an error with
 *  code BAD_COMMAND should be output on `err`.
 *
 *  A response from either kind of command must always be followed by
 *  a single empty line on `out`.
 *
 *  There should be no hard-coded limits: specifically no limits
 *  beyond available memory on the size of a message, the number of
 *  messages or the number of TOPIC's associated with a message.
 *
 *  Under normal circumstances, the function should return only when
 *  EOF is detected on `in`.  All allocated memory must have been
 *  deallocated before returning.
 *
 *  When a system error is detected, the function should terminate the
 *  program with a non-zero exit status after outputting a suitable
 *  message on err (or stderr for an assertion failure).  Memory need
 *  not be cleaned up.
 *
 */
void
chat_io(const char *prompt, FILE *in, FILE *out, FILE *err)
{
  ErrNum errnum = NO_ERR;
  Chat *chat = make_chat(&errnum);
  if (errnum != NO_ERR) {
    fatalf(err, "cannot create chat: %s", errnum_to_string(errnum));
  }
  fprintf(out, "%s", prompt); fflush(out);
  MsgArgs *msgArgs = NULL;
  while ((msgArgs = read_msg_args(in, msgArgs, &errnum)) != NULL) {
    if (errnum != NO_ERR) {
      fatalf(err, "cannot read input: %s", errnum_to_string(errnum));
    }
    if (msgArgs->nArgs == 0) {
      errorf(err, "BAD_COMMAND: missing command");;
    }
    else {
      const char *cmd = msgArgs->args[0];
      if (strcmp(cmd, "+") == 0) {
        do_add_cmd(chat, msgArgs, err);
      }
      else if (strcmp(cmd, "?") == 0) {
        do_query_cmd(chat, msgArgs, out, err);
      }
      else {
        errorf(err, "BAD_COMMAND: unknown command \"%s\"", msgArgs->args[0]);
      }
    }
    fprintf(out, "%s", prompt); fflush(out);
  }
  free_chat(chat);
}

//#define NO_CHAT_IO_MAIN to allow an alternate main()
#ifndef NO_CHAT_IO_MAIN

#include <unistd.h>

int main(int argc, const char *argv[]) {
  bool isInteractive = isatty(fileno(stdin));
  const char *prompt = isInteractive ? "> " : "";
  FILE *err = stderr;
  chat_io(prompt, stdin, stdout, err);
}

#endif //#ifndef NO_CHAT_IO_MAIN
