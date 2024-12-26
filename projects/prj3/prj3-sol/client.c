#include "client.h"
#include "common.h"
#include "chat.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>


/** send params for add cmd to remote server specified by client,
 *  as per protocol.
 */
static void
send_add_req(Client *client, const AddCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->user) + 1);
  nBytes += (strlen(cmd->room) + 1);
  nBytes += (strlen(cmd->message) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  Hdr hdr = {
    .hdrType = CLIENT_HDR,
    .cmdType = ADD_CMD,
    .count = -1,
    .nTopics = cmd->nTopics,
    .nBytes = nBytes,
  };
  FILE *out = client->serverOut;
  write_header(&hdr, out);
  fwrite(cmd->user, 1, strlen(cmd->user)+1, out);
  fwrite(cmd->room, 1, strlen(cmd->room)+1, out);
  fwrite(cmd->message, 1, strlen(cmd->message)+1, out);
  for (int i = 0; i < cmd->nTopics; i++) {
    fwrite(cmd->topics[i], 1, strlen(cmd->topics[i])+1, out);
  }
  fflush(out);
}

/** send params for query cmd to remote server specified by client,
 *  as per protocol.
 */
static void
send_query_req(Client *client, const QueryCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->room) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  Hdr hdr = {
    .hdrType = CLIENT_HDR,
    .cmdType = QUERY_CMD,
    .count = cmd->count,
    .nTopics = cmd->nTopics,
    .nBytes = nBytes,
  };
  FILE *out = client->serverOut;
  write_header(&hdr, out);
  fwrite(cmd->room, 1, strlen(cmd->room)+1, out);
  for (int i = 0; i < cmd->nTopics; i++) {
    fwrite(cmd->topics[i], 1, strlen(cmd->topics[i])+1, out);
  }
  fflush(out);
}

/** receive response from remote server as per protocol, copying
 *  response onto appropriate client stream: out if response was okay,
 *  err if response was in error.
 */
static void
receive_res(Client *client)
{
  FILE *in = client->serverIn;
  FILE *out = client->out;
  FILE *err = client->err;
  bool didOk = false;  //set true once we output OKAY
  do { //loop until we get a error response or an empty ok response
    Hdr hdr = { .hdrType = SERVER_HDR };
    read_header(&hdr, in);
    size_t nBytes = hdr.nBytes;
    TRACE("read hdr status = %d, nBytes = %zu", hdr.status, nBytes);
    if (hdr.status != 0) { //error response
      char buf[nBytes];
      fread(buf, 1, nBytes, in);
      const char *errStatus =
        (const char *[]){ "", "SYS_ERR: ", "FATAL_ERR: " }[hdr.status - 1];
      fprintf(err, ERROR "%s%.*s\n", errStatus, (int)nBytes, buf);
      fflush(err);
      break;  //end loop on error response
    }
    else { //success response
      fprintf(out, "%s", didOk ? "" : OKAY); fflush(out);
      didOk = true;
      if (nBytes == 0) break; //end loop on empty ok response
      char buf[nBytes];
      fread(buf, 1, nBytes, in);
      TRACE("read buf %.*s", (int)nBytes, buf);
      fprintf(out, "%.*s", (int)nBytes, buf);  fflush(out);
    }
  } while (true);
}

/** perform cmd using chat, writing response to chat's out/err
 *  streams.  It can be assumed that cmd is free of user errors except
 *  for unknown room/topic for QUERY commands.
 *
 *  If the command is an END_CMD command, then ensure that the server
 *  process is shut down cleanly.
 */
void
do_client_cmd(Client *client, const ChatCmd *cmd)
{
  switch (cmd->type) {
  case ADD_CMD:
    send_add_req(client, &cmd->add);
    receive_res(client);
    break;
  case QUERY_CMD:
    send_query_req(client, &cmd->query);
    receive_res(client);
    break;
  case END_CMD: {
    Hdr hdr = { .hdrType = CLIENT_HDR, .cmdType = END_CMD };
    write_header(&hdr, client->serverOut);
    break;
  }
  default:
    assert(0);
  }
}
