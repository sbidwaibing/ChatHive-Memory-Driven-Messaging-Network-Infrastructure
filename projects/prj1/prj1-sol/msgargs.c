#include "msgargs.h"

#include "errnum.h"

#include <errors.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  MsgArgs msgArgs;
  size_t argsSize;
  size_t bufSize;
  char *buf;
} XMsgArgs;


static size_t
get_args(char *line, XMsgArgs *msgArgs, ErrNum *err)
{
  enum { INIT_ARGS_SIZE = 2, };
  bool lastIsSpace = true;
  size_t nArgs = 0;
  for (char *p = line; *p != '\0'; p++) {
    char c = *p;
    if (isspace(c)) {
      if (!lastIsSpace) *p = '\0';
      lastIsSpace = true;
    }
    else {
      if (lastIsSpace) { //start of an arg
        if (nArgs == msgArgs->argsSize) {
          size_t newSize =
            msgArgs->argsSize == 0 ? INIT_ARGS_SIZE : 2 * msgArgs->argsSize;
          char **args = realloc(msgArgs->msgArgs.args, newSize*sizeof(char *));
          if (args == NULL) {
            *err = MEM_ERR;
          }
          else {
            msgArgs->msgArgs.args = args; msgArgs->argsSize = newSize;
          }
        }
        assert(nArgs < msgArgs->argsSize);
        msgArgs->msgArgs.args[nArgs++] = p;
      }
      lastIsSpace = false;
    }
  }
  msgArgs->msgArgs.nArgs = nArgs;
  return nArgs;
}

static void
ensure_space(size_t nc, XMsgArgs *msgArgs, ErrNum *err)
{
  enum { INIT_BUF_SIZE = 8 };
  if (nc == msgArgs->bufSize) {
    size_t newSize =
      msgArgs->bufSize == 0 ? INIT_BUF_SIZE : 2 * msgArgs->bufSize;
    char *buf = realloc(msgArgs->buf, newSize*sizeof(char));
    if (buf == NULL) {
      *err = MEM_ERR;
    }
    else {
      msgArgs->buf = buf; msgArgs->bufSize = newSize;
    }
  }
}


static size_t
read_lines(FILE *in, XMsgArgs *msgArgs, ErrNum *err)
{
  enum { TERM_CHAR = '.' };
  int c;
  char lastC = '\n';
  size_t nc = 0;
  while ((c = fgetc(in)) != EOF) {
    ensure_space(nc, msgArgs, err);
    if (*err != NO_ERR) return 0;
    assert(nc < msgArgs->bufSize);
    if (c == '\n') {
      if (lastC == TERM_CHAR) {
        msgArgs->buf[nc - 1] = '\0'; //replace TERM_CHAR with str terminator
        return nc - 1;
      }
      else {
        lastC = '\n';
      }
    }
    else if (c == TERM_CHAR && lastC == '\n') {
      lastC = TERM_CHAR; //only set to TERM_CHAR immediately after '\n'
    }
    else {
      lastC = '\0';
    }
    msgArgs->buf[nc++] = c;
  }
  if (ferror(in)) *err = IO_ERR;
  return nc;
}

/** Read lines from `in` until a line containg only a single period
 *  '.' is read.  Set args[nArgs] to whitespace delimited strings from
 *  the first line and msg to the text (if any) of the subsequent
 *  lines.  The terminating line containing the '.' is not included.
 *  Each string in args[] as well as msg are NUL-terminated.
 *
 *  All memory used by the returned MsgArgs is dynamically allocated.
 *  That memory must be explicitly freed using free_msg_args() or
 *  recycled by being passed as the lastMsgArgs parameter in a
 *  subsequent call to read_msg_args().
 *
 *  Returns NULL on EOF.
 *
 *  Will set *err to:
 *    MEM_ERR on a memory error
 *    IO_ERR on an I/O error
 *
 *  Frees up all memory (including that passed in via lastMsgArgs)
 *  on EOF or error.
 *
 *  Typical usage for reading from FILE *in:
 *
 *  MsgArgs *msgArgs = NULL;
 *  ErrNum err;
 *  while ((msgArgs = read_msg_args(in, msgArgs, &err)) != NULL) {
 *    //process msgArgs
 *  }
 *  // no need to call freeMsgArgs() since memory freed on EOF.
 */
MsgArgs *
read_msg_args(FILE *in, MsgArgs *lastMsgArgs, ErrNum *err)
{
  *err = NO_ERR;
  XMsgArgs *msgArgs = (XMsgArgs *)lastMsgArgs;
  if (msgArgs == NULL) {
    msgArgs = calloc(1, sizeof(XMsgArgs));
    if (msgArgs == NULL) {
      *err = MEM_ERR;
      return NULL;
    }
  }
  size_t nc = read_lines(in, msgArgs, err);
  if (nc == 0 || *err != NO_ERR) {
    free_msg_args((MsgArgs *)msgArgs);
    return NULL;
  }
  const char *nlP = strchr(msgArgs->buf, '\n');
  assert(nlP != NULL);
  size_t line1Len = nlP - msgArgs->buf;
  msgArgs->buf[line1Len] = '\0';  //replace first newline
  get_args(msgArgs->buf, msgArgs, err);
  if (*err != NO_ERR) {
    free_msg_args((MsgArgs *)msgArgs);
    return NULL;
  }
  msgArgs->msgArgs.msg =
    (nc > line1Len + 1) ? msgArgs->buf + line1Len + 1 : NULL;
  return (MsgArgs *)msgArgs;
}

/** Free all memory used by msgArgs */
void
free_msg_args(MsgArgs *msgArgs0)
{
  XMsgArgs *msgArgs = (XMsgArgs *)msgArgs0;
  free(msgArgs->buf);
  free(msgArgs->msgArgs.args);
  free(msgArgs);
}


#ifdef TEST_MSG_ARGS

char *testLines =
  "\n"
  ".\n"
  "cmd0\n"
  ".\n"
  "cmd1 arg1_0 arg1_1 arg1_2 \t\n"
  ".\n"
  "\t cmd2  \t arg2_0  arg2_1  \n"
  ".\n"
  " x   \t\v 0 1 2  \t3\n"
  "some msg line 1\n"
  "some msg line 2\n"
  "some msg line 3\n"
  ".\n";

static void
print_msg_args(FILE *out, const MsgArgs *msgArgs) {
  for (int i = 0; i < msgArgs->nArgs; i++) {
    fprintf(out, "%s\n", msgArgs->args[i]);
  }
  fprintf(out, "%s", msgArgs->msg == NULL ? "NO_MSG\n" : msgArgs->msg);
}

int
main(int argc, const char *argv[])
{
  FILE *in = (argc > 1) ? fmemopen(testLines, strlen(testLines), "r") : stdin;
  FILE *out = stdout;
  MsgArgs *msgArgs = NULL;
  ErrNum err;
  while ((msgArgs = read_msg_args(in, msgArgs, &err)) != NULL) {
    if (err != NO_ERR) { fatal("%s", errnum_to_string(err)); }
    print_msg_args(out, msgArgs);
  }
  if (argc > 1) fclose(in);
}

#endif //#ifdef TEST_MSG_ARGS
