#include "msgargs.h"

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


static void
ensure_arg1_space(XMsgArgs *msgArgs, ErrNum *err)
{
  enum { INIT_ARGS_SIZE = 2, };
  if (msgArgs->msgArgs.nArgs == msgArgs->argsSize) {
    size_t newSize =
      msgArgs->argsSize == 0 ? INIT_ARGS_SIZE : 2 * msgArgs->argsSize;
    char **args = realloc(msgArgs->msgArgs.args, newSize*sizeof(char *));
    if (args == NULL) {
      *err = MEM_ERR;
    }
    else {
      msgArgs->msgArgs.args = (const char **)args;
      msgArgs->argsSize = newSize;
    }
  }
}

static char *
skip_space(char *p)
{
  while (isspace(*p)) p++;
  return p;
}

static char *
add_next_arg(char *p, XMsgArgs *msgArgs, ErrNum *err)
{
  p = skip_space(p);
  if (*p == '\0') return p;
  const char *arg = p;
  while (!isspace(*p) && *p != '\0') p++;
  if (*p != '\0') *p++ = '\0';
  ensure_arg1_space(msgArgs, err);
  if (*err != NO_ERR) return NULL;
  assert(msgArgs->msgArgs.nArgs < msgArgs->argsSize);
  msgArgs->msgArgs.args[msgArgs->msgArgs.nArgs++] = arg;
  return p;
}

static void
add_args(char *line, XMsgArgs *msgArgs, ErrNum *err)
{
  *err = NO_ERR;
  char *p = line;
  while (*p != '\0') {
    p = add_next_arg(p, msgArgs, err);
    if (*err != NO_ERR) return;
  }
}

static void
ensure_buf_space(size_t nc, XMsgArgs *msgArgs, ErrNum *err)
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
    ensure_buf_space(nc, msgArgs, err);
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

static XMsgArgs *
getXMsgArgs(MsgArgs *lastMsgArgs, ErrNum *err)
{
  XMsgArgs *msgArgs = (XMsgArgs *)lastMsgArgs;
  if (msgArgs == NULL) {
    msgArgs = calloc(1, sizeof(XMsgArgs));
    if (msgArgs == NULL) {
      *err = MEM_ERR;
      return NULL;
    }
  }
  msgArgs->msgArgs.nArgs = 0; msgArgs->msgArgs.msg = NULL;
  return msgArgs;
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
 *  Returns NULL on EOF or if the next read is simply a "." line.
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
  XMsgArgs *msgArgs = getXMsgArgs(lastMsgArgs, err);
  if (msgArgs == NULL) return NULL;
  size_t nc = read_lines(in, msgArgs, err);
  if (nc == 0 || *err != NO_ERR) {
    free_msg_args((MsgArgs *)msgArgs);
    return NULL;
  }
  const char *nlP = strchr(msgArgs->buf, '\n');
  assert(nlP != NULL);
  size_t line1Len = nlP - msgArgs->buf;
  msgArgs->buf[line1Len] = '\0';  //replace first newline
  add_args(msgArgs->buf, msgArgs, err);
  if (*err != NO_ERR) {
    free_msg_args((MsgArgs *)msgArgs);
    return NULL;
  }
  msgArgs->msgArgs.msg =
    (nc > line1Len + 1) ? msgArgs->buf + line1Len + 1 : NULL;
  return (MsgArgs *)msgArgs;
}

static size_t
read_line(FILE *in, XMsgArgs *msgArgs, ErrNum *err)
{
  int c;
  size_t nc = 0;
  bool seenNonSpace = false;
  while ((c = fgetc(in)) != EOF) {
    seenNonSpace = seenNonSpace || !isspace(c);
    ensure_buf_space(nc, msgArgs, err);
    if (*err != NO_ERR) return 0;
    assert(nc < msgArgs->bufSize);
    msgArgs->buf[nc++] = c;
    if (c == '\n') {
      if (seenNonSpace) break;
      nc = 0; //skip whitespace line
    }
  }
  if (ferror(in)) { *err = IO_ERR; return 0; }
  ensure_buf_space(nc, msgArgs, err);
  if (*err != NO_ERR) return 0;
   msgArgs->buf[nc] = '\0';
  return nc;
}

/** Read a *single* line from `in`, skipping empty lines.  If line
 *  starts with a ?, then read rest of line as for read_msg_args().
 *  Otherwise, set up args as for a `+` command, with initial `#words`
 *  added to args.
 *
 *  Same memory allocation strategy, error returns and usage
 *  as read_msg_args().
 */
MsgArgs *
read_line_args(FILE *in, MsgArgs *lastMsgArgs, ErrNum *err)
{
  *err = NO_ERR;
  XMsgArgs *msgArgs = getXMsgArgs(lastMsgArgs, err);
  if (msgArgs == NULL) return NULL;
  size_t nc = read_line(in, msgArgs, err);
  if (nc == 0 || *err != NO_ERR) goto FAIL;
  char *p = skip_space(msgArgs->buf);
  if (p[0] == '?' && isspace(p[1])) {
    add_args(msgArgs->buf, msgArgs, err);
    if (*err != NO_ERR) goto FAIL;
  }
  else {
    const char* fixedArgs[] = { "+", NULL, NULL };
    for (int i = 0; i < sizeof(fixedArgs)/sizeof(fixedArgs[0]); i++) {
      ensure_arg1_space(msgArgs, err);
      if (*err != NO_ERR) goto FAIL;
      msgArgs->msgArgs.args[msgArgs->msgArgs.nArgs++] = fixedArgs[i];
    }
    while (*p == '#') {
      p = add_next_arg(p, msgArgs, err);
      if (*err != NO_ERR) goto FAIL;
      p = skip_space(p);
    }
    msgArgs->msgArgs.msg = p;
  }
  return (MsgArgs*)msgArgs;
 FAIL:
  if (msgArgs != NULL) free_msg_args((MsgArgs *)msgArgs);
  return NULL;
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

// must be in same order as ErrNum enum
// (this order dependency can be removed using macros)
static const char *errMsgs[] = {
  "no error",
  "memory allocation error",
  "I/O error",
};

/** return a static string explaining err */
const char *
errnum_to_string(ErrNum err)
{
  const int nErrNums = sizeof(errMsgs)/sizeof(errMsgs[0]);
  assert(err < nErrNums);
  return errMsgs[err];
}


#ifdef TEST_MSGARGS

char *testLines[] = {
  //test lines for read_msg_args()
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
  ".\n",
  //test lines for read_line_args()
  "\n"
  "\n"
  "some text\n"
  " \t some more text\t \n"
  " ? arg1_0 arg1_1 arg1_2 \t\n"
  "  #topic1 #topic2 here is some text\n",
};


static void
print_msg_args(FILE *out, const MsgArgs *msgArgs) {
  for (int i = 0; i < msgArgs->nArgs; i++) {
    fprintf(out, "%s\n", msgArgs->args[i]);
  }
  fprintf(out, "%s", msgArgs->msg == NULL ? "NO_MSG\n" : msgArgs->msg);
}

typedef MsgArgs *MsgArgsFn(FILE *in, MsgArgs *lastMsgArgs, ErrNum *err);

static MsgArgsFn *msgArgFns[] = { read_msg_args, read_line_args };
int
main(int argc, const char *argv[])
{
  if (argc < 2 || argc > 3) fatal("usage: %s msg|line [ANYTHING]", argv[0]);
  int fnIndex = strcmp(argv[1], "line") == 0;
  bool useStdin = argc > 2;
  char *lines = testLines[fnIndex];
  FILE *in = (useStdin) ? stdin : fmemopen(lines, strlen(lines), "r");
  MsgArgsFn *fn = msgArgFns[fnIndex];
  FILE *out = stdout;
  MsgArgs *msgArgs = NULL;
  ErrNum err;
  while ((msgArgs = fn(in, msgArgs, &err)) != NULL) {
    if (err != NO_ERR) { fatal("%s", errnum_to_string(err)); }
    print_msg_args(out, msgArgs);
  }
  if (argc == 2) fclose(in);
}

#endif //#ifdef TEST_MSGARGS
