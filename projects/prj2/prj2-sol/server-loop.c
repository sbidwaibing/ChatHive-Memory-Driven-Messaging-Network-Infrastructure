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

typedef struct {
  ChatDb *chatDb;
  FILE *in;
  FILE *out;
} Server;

static void
end_server_response(ChatDb *chatDb, ServerStatus status,
                    const char *errMsg, FILE *out)
{
  Hdr serverHdr = { .hdrType = SERVER_HDR, .status = status, };
  if (status == OK_STATUS) {
    serverHdr.nBytes = 0;
    write_header(&serverHdr, out);
  }
  else {
    serverHdr.nBytes = strlen(errMsg);
    write_header(&serverHdr, out);
    TRACE("error %s", errMsg);
    fprintf(out, "%s", errMsg);
  }
  fflush(out);
}

static int
query_iterator(const ChatInfo *result, void *ctx)
{
  TRACE("entry");
  const Server *server = ctx;
  const size_t nTopics = result->nTopics;
  size_t nBytes = strlen(ISO_8601_FORMAT) + 1;
  nBytes += strlen(result->user) + 1 +
    strlen(result->room) + (nTopics > 0); //' ' if nTopics > 0
  for (int i = 0; i < nTopics; i++) {
    nBytes += strlen(result->topics[i]) + (i < nTopics - 1);
  }
  nBytes += 1; //for '\n'
  nBytes += strlen(result->message);
  char buf[nBytes];
  char *p = buf;
  p += timestamp_to_iso8601(result->timestamp, nBytes, p);
  p += sprintf(p, "\n%s %s%s", result->user, result->room,
               (nTopics > 0) ? " " : "");
  for (int i = 0; i < nTopics; i++) {
    p += sprintf(p, "%s%s", result->topics[i], (i < nTopics - 1) ? " " : "");
  }
  p += sprintf(p, "\n");
  p += sprintf(p, "%s", result->message);
  Hdr hdr = { .hdrType = SERVER_HDR, .status = OK_STATUS, .nBytes = nBytes };
  write_header(&hdr, server->out);
  fwrite(buf, 1, nBytes, server->out);
  return 0;
}


static void
do_query_cmd(const Server *server, const Hdr *clientHdr)
{
  ChatDb *chatDb = server->chatDb;
  FILE *in = server->in;
  FILE *out = server->out;

  size_t nBytes = clientHdr->nBytes;
  char buf[nBytes];
  fread(buf, 1, nBytes, in);
  TRACE("nBytes = %zu; buf = %.*s", nBytes, (int) nBytes, buf);
  const char *room = buf;
  size_t count;
  int errCode = count_room_chat_db(chatDb, room, &count);
  if (errCode != 0) {
    end_server_response(chatDb, SYS_ERR_STATUS, error_chat_db(chatDb), out);
    return;
  }
  else if (count == 0) {
    end_server_response(chatDb, USER_ERR_STATUS, "BAD_ROOM: unknown room", out);
    return;
  }
  const size_t nTopics = clientHdr->nTopics;
  const char *topics[nTopics];
  const char *p = buf + strlen(room) + 1;
  TRACE("room = %s, topics[0] = %s",
        room, nTopics > 0 ? p : "");
  for (int i = 0; i < nTopics; i++) {
    topics[i] = p;
    p += strlen(p) + 1;
    errCode = count_topic_chat_db(chatDb, topics[i], &count);
    if (errCode != 0) {
      end_server_response(chatDb, SYS_ERR_STATUS, error_chat_db(chatDb), out);
      return;
    }
    else if (count == 0) {
      end_server_response(chatDb, USER_ERR_STATUS,
                          "BAD_TOPIC: unknown topic", out);
      return;
    }
  }
  const char **topicsP = (nTopics == 0) ? NULL : topics;
  errCode = query_chat_db(chatDb, room, nTopics, topicsP, clientHdr->count,
                              query_iterator, (void *)server);
  TRACE("query_chat_db(%p, %s, %zu, %p, %d, %p, %p) = %d",
        chatDb, room, nTopics, topicsP, clientHdr->count,
        query_iterator, server, errCode);
  ServerStatus status = (errCode == 0) ? OK_STATUS : SYS_ERR_STATUS;
  const char *errMsg = (errCode == 0) ? NULL : error_chat_db(chatDb);
  end_server_response(chatDb, status, errMsg, out);
}

static void
do_add_cmd(const Server *server, const Hdr *clientHdr)
{
  ChatDb *chatDb = server->chatDb;
  FILE *in = server->in;
  FILE *out = server->out;

  char buf[clientHdr->nBytes];
  fread(buf, 1, clientHdr->nBytes, in);
  TRACE("nBytes = %zu; buf = %s", clientHdr->nBytes, buf);
  const char *user = buf;
  const char *room = user + strlen(user) + 1;
  const char *message = room + strlen(room) + 1;
  const char *p = message + strlen(message) + 1;
  const size_t nTopics = clientHdr->nTopics;
  const char *topics[nTopics];
  TRACE("user = %s, room = %s, message = %s, topics[0] = %s",
        user, room, message, nTopics > 0 ? p : "");
  for (int i = 0; i < nTopics; i++) {
    topics[i] = p;
    p += strlen(p) + 1;
  }
  const char **topicsP = (nTopics == 0) ? NULL : topics;
  int errCode = add_chat_db(chatDb, user, room, nTopics, topicsP, message);
  TRACE("add_chat_db(%p, %s, %s, %zu, %p, %s) = %d",
        chatDb, user, room, nTopics, topicsP, message, errCode);
  ServerStatus status = (errCode == 0) ? OK_STATUS : SYS_ERR_STATUS;
  const char *errMsg = (errCode == 0) ? NULL : error_chat_db(chatDb);
  end_server_response(chatDb, status, errMsg, out);
}

void
server_loop(ChatDb *chatDb, FILE *in, FILE *out)
{
  const Server server = { .chatDb = chatDb, .in = in, .out = out };
  bool isDone = false;
  while (!isDone) {
    Hdr hdr = { .hdrType = CLIENT_HDR };
    read_header(&hdr, in);
    switch (hdr.cmdType) {
    case ADD_CMD:
      do_add_cmd(&server, &hdr);
      break;
    case QUERY_CMD:
      TRACE("query");
      do_query_cmd(&server, &hdr);
      break;
    case END_CMD:
      isDone = true;
      break;
    default:
      fprintf(stderr, "serve(): impossible cmdType = %d\n", hdr.cmdType);
      assert(false);
    }
  };
}
