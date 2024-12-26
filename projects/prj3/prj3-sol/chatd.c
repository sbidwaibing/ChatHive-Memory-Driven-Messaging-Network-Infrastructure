#include "common.h"
#include "server-loop.h"
#include "utils.h"

#include <chat-cmd.h>
#include <chat-db.h>
#include <errors.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// ok to terminate since this is a worker process
static void
do_work(pid_t clientPid, const char *dbPath)
{
  MakeChatDbResult result;
  if (make_chat_db(dbPath, &result) != 0) {
    fatal("cannot open db at %s: %s", dbPath, result.err);
  }
  ChatDb *chatDb = result.chatDb;
  FILE *fifos[2];
  if (open_client_fifos(clientPid, false, fifos) != 0) {
    fatal("cannot open fifos[]:");
  }
  server_loop(chatDb, fifos[1], fifos[0]);
}


static void
service(FILE *fifo, const char *dbPath)
{
  while (true) {
    TRACE("service loop");
    long long reqId;
    if (fscanf(fifo, "%lld", &reqId) != 1) {
      error("cannot read well-known fifo:");
    }
    else {
      TRACE("req from %lld", reqId);
      pid_t clientPid = reqId;
      //use double-fork to create worker process without zombies
      pid_t pid1 = fork();
      if (pid1 < 0) {
        error("cannot fork:");
      }
      else if (pid1 == 0) {
        fclose(fifo);
        const pid_t workerPid = fork();
        if (workerPid < 0) {
          error("cannot fork:");
        }
        else if (workerPid > 0) { //intermediate parent
          exit(0);
        }
        else {
          do_work(clientPid,  dbPath);
          exit(0);
        }
      }
      else { //daemon process
        if (waitpid(pid1, NULL, 0) != pid1) error("wait error:");
      }
    }
  }

}


static pid_t
make_daemon(FILE *fifo, const char *dbPath)
{
  pid_t pid;
  if ((pid = fork()) < 0) {
    return -1;
  }
  else if (pid != 0) { /* parent */
    fclose(fifo);
    return pid;
  }
  else { /* child becomes daemon */
    setsid();
    service(fifo, dbPath); /** provide daemon service */
    assert(0); /** should never exit */
  }
}

/** Invoked with two arguments:
 *
 *    SERVER_DIR: the path to the directory in which the server should
 *    run and where all FIFOs will be created.
 *
 *    DBFILE_PATH: path to the sqlite file.  This must be relative to
 *    SERVER_DIR.
 *
 *  The server may use `stderr` for "logging", but all such logging
 *  *must* be turned off before submission.
 */
int
main(int argc, const char *argv[])
{
  if (argc != 3) fatal("usage: %s SERVER_DIR DBFILE_PATH", argv[0]);
  const char *serverDir = argv[1];
  const char *dbPath = argv[2];

  if (chdir(serverDir) < 0) fatal("cannot cd to \"%s\":", serverDir);

  // verify open of chat-db
  MakeChatDbResult result;
  if (make_chat_db(dbPath, &result) != 0) {
    fatal("cannot open db at %s: %s", dbPath, result.err);
  }
  free_chat_db(result.chatDb);

  FILE *fifo;
  if (open_well_known_fifo(false, &fifo) != 0) {
    fatal("cannot open well-known FIFO");
  }
  pid_t daemonPid = make_daemon(fifo, dbPath);
  if (daemonPid < 0) {
    fatal("cannot create daemon");
    fclose(fifo);
    return 1;
  }
  fprintf(stdout, "%lld\n", (long long)daemonPid);
  return 0;
}
