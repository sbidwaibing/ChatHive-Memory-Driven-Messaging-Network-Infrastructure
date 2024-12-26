#ifndef MSGARGS_H_
#define MSGARGS_H_

#include "errnum.h"

#include <stdio.h>

typedef struct {
  size_t nArgs;       /** # of arguments in args[] */
  char **args;        /** args[nArgs] */
  const char *msg;    /** msg if any (NULL if none) */
} MsgArgs;

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
MsgArgs *read_msg_args(FILE *in, MsgArgs *lastMsgArgs, ErrNum *err);

/** Free all memory used by msgArgs */
void free_msg_args(MsgArgs *msgArgs);

#endif // #ifndef MSGARGS_H_
