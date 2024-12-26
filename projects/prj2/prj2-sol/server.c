#include "server.h"
#include "server-loop.h"


#include "common.h"

#include <chat-cmd.h>
#include <errors.h>

#include <chat-db.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

void
do_server(const char *dbPath, int inPipe[2], int outPipe[2])
{
  FILE *clientIn = NULL;
  FILE *clientOut = NULL;
  ChatDb *chatDb = NULL;
  const char *errMsg = NULL;
  MakeChatDbResult result;
  if (make_chat_db(dbPath, &result) != 0) {
    errMsg = result.err;
    goto CLEANUP;
  }
  chatDb = result.chatDb;
  if (close(outPipe[0]) < 0) {
    errMsg = "SYS_ERR: server cannot close outPipe[0]";
    goto CLEANUP;
  }
  if (close(inPipe[1]) < 0) {
    errMsg = "SYS_ERR: server cannot close inPipe[1]";
    goto CLEANUP;
  }
  clientIn = fdopen(inPipe[0], "r");
  if (!clientIn) {
    errMsg = "SYS_ERR: server cannot fdopen(outPipe[0])";
    goto CLEANUP;
  }
  clientOut = fdopen(outPipe[1], "w");
  if (!clientOut) {
    errMsg = "SYS_ERR: server cannot fdopen(inPipe[1])";
    goto CLEANUP;
  }
  server_loop(chatDb, clientIn, clientOut);
 CLEANUP:
  if (chatDb) free_chat_db(chatDb);
  if (clientIn) fclose(clientIn);
  if (clientOut) fclose(clientOut);
  if (errMsg) fatal("%s:", errMsg);
  exit(0);
}
