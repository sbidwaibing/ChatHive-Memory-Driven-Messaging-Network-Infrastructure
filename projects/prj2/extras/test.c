#include "chat.h"

#include <chat-cmd.h>
#include <chat-db.h>
#include <errors.h>
#include <msgargs.h>

#include <stdbool.h>

#include <signal.h>
#include <unistd.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>


// #define TEST_MAIN to compile this code.
#ifdef TEST_MAIN

/** set buf to ISO-8601 representation of timestamp in localtime.
 *  Should have bufSize > strlen(ISO_8601_FMT).
 *
 *  Will not exceed bufSize.  Return value like snprintf():
 *  # of characters which would have been output (not counting the NUL).
 */
// mock for testing
size_t
timestamp_to_iso8601(TimeMillis timestamp,
                     size_t bufSize, char buf[bufSize])
{
  return snprintf(buf, bufSize, "%s", ISO_8601_FORMAT);
}


typedef struct {
  size_t nRead;
  size_t nWrite;
} IOStats;

/** return IO stats for process pid using /proc fs */
static IOStats
pid_io_stats(pid_t pid)
{
  enum { MAX_PATH = 256 };
  char path[MAX_PATH];
  snprintf(path, sizeof(path), "/proc/%lld/io", (long long) pid);
  FILE *pidIO = fopen(path, "r");
  if (!pidIO) fatal("cannot read %s:", path);

  enum { MAX_LINE = 256 };
  char line[MAX_LINE];
  IOStats ioStats;

  while (fgets(line, sizeof(line), pidIO) != NULL) {
    if (sscanf(line, "rchar: %zu", &ioStats.nRead) == 1) continue;
    if (sscanf(line, "wchar: %zu", &ioStats.nWrite) == 1) continue;
  }

  fclose(pidIO);
  return ioStats;
}


int
main(int argc, const char *argv[]) {
  if (argc != 2) {
    fatal("usage: %s SQLITE_DB_PATH", argv[0]);
  }
  FILE *in = stdin;
  FILE *out = stdout;
  FILE *err = stderr;
  MsgArgs *msgArgs = NULL;
  ErrNum errNum;
  ChatCmd cmd;
  Chat *chat = make_chat(argv[1], out, err);
  if (chat == NULL) fatalf(err, "unable to create Chat:");

  // verify server exists
  pid_t pid = chat_server_pid(chat);
  if (kill(pid, 0) != 0) fatal("no server %lld at start", (long long)pid);

  // get starting IO stats
  IOStats startIO = pid_io_stats(pid);
  TRACE("start bytes: nRead = %zu, nWrite = %zu", startIO.nRead, startIO.nWrite);

  // interact with chat client
  while ((msgArgs = read_msg_args(in, msgArgs, &errNum)) != NULL) {
    if (errNum != NO_ERR) { fatal("%s", errnum_to_string(errNum)); }
    int rc = parse_cmd(msgArgs, &cmd, err);
    if (rc == 0) do_chat_cmd(chat, &cmd);
  }

  // verify server still exists
  if (kill(pid, 0) != 0) fatal("no server %lld before end", (long long)pid);

  // get ending IO stats
  IOStats endIO = pid_io_stats(pid);
  TRACE("end bytes: nRead = %zu, nWrite = %zu", endIO.nRead, endIO.nWrite);

  // must have done some I/O
  if (endIO.nRead <= startIO.nRead) {
    fatal("no server reads: %zu/%zu", endIO.nRead, startIO.nRead);
  }
  if (endIO.nWrite <= startIO.nWrite) {
    fatal("no server reads: %zu/%zu", endIO.nWrite, startIO.nWrite);
  }

  // send END_CMD
  cmd.type = END_CMD;
  do_chat_cmd(chat, &cmd);

  // cleanup
  free_chat(chat);

}

#endif //#ifdef TEST_MAIN
