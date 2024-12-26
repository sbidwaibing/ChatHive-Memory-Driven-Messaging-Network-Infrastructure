#include "server.h"
#include "common.h"
#include "server-loop.h"

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
do_server(Shm *shm, const char *dbPath)
{
  ChatDb *chatDb = NULL;
  MakeChatDbResult result;
  if (make_chat_db(dbPath, &result) != 0) {
    fatal("%s", result.err);
  }
  chatDb = result.chatDb;
  server_loop(chatDb, shm);
  free_chat_db(chatDb);
  exit(0);
}
