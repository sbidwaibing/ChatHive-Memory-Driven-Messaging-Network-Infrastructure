#include "client.h"
#include "common.h"

#include <chat-cmd.h>
#include <errors.h>
#include <msgargs.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <semaphore.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>


/** send params for add cmd to server via shm,  as per protocol. */
static void
send_add_req(Shm *shm, const AddCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->user) + 1);
  nBytes += (strlen(cmd->room) + 1);
  nBytes += (strlen(cmd->message) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  ClientHdr clientHdr = {
    .cmd = ADD_CMD,
    .count = -1,
    .nTopics = cmd->nTopics,
    .reqSize = nBytes,
  };
  send_data(shm, sizeof(ClientHdr), &clientHdr);
  char buf[nBytes];
  char *p = buf;
  strcpy(p, cmd->user); p += strlen(cmd->user) + 1;
  strcpy(p, cmd->room); p += strlen(cmd->room) + 1;
  strcpy(p, cmd->message); p += strlen(cmd->message) + 1;
  for (int i = 0; i < cmd->nTopics; i++) {
    strcpy(p, cmd->topics[i]); p += strlen(cmd->topics[i]) + 1;
  }
  send_data(shm, nBytes, buf);
}

/** send params for query cmd to server via shm,  as per protocol. */
static void
send_query_req(Shm *shm, const QueryCmd *cmd)
{
  size_t nBytes = 0;
  nBytes += (strlen(cmd->room) + 1);
  for (int i = 0; i < cmd->nTopics; i++) {
    nBytes += strlen(cmd->topics[i]) + 1;
  }
  ClientHdr clientHdr = {
    .cmd = QUERY_CMD,
    .count = cmd->count,
    .nTopics = cmd->nTopics,
    .reqSize = nBytes,
  };
  send_data(shm, sizeof(ClientHdr), &clientHdr);
  char buf[nBytes];
  char *p = buf;
  strcpy(p, cmd->room); p += strlen(cmd->room) + 1;
  for (int i = 0; i < cmd->nTopics; i++) {
    strcpy(p, cmd->topics[i]); p += strlen(cmd->topics[i]) + 1;
  }
  send_data(shm, nBytes, buf);
}

/** receive response from remote server on shm as per protocol, copying
 *  response onto appropriate client stream: out if response was okay,
 *  err if response was in error.
 */
static void
receive_res(Shm *shm, FILE *out, FILE *err)
{
  bool didOk = false;  //set true once we output OKAY
  do { //loop until we get a error response or an empty ok response
    ServerHdr hdr;
    receive_data(shm, sizeof(ServerHdr), &hdr);
    size_t nBytes = hdr.resSize;
    TRACE("read hdr status = %d, nBytes = %zu", hdr.status, nBytes);
    if (hdr.status != 0) { //error response
      const char *errStatus =
        (const char *[]){ "", "SYS_ERR: ", "FATAL_ERR: " }[hdr.status - 1];
      fprintf(err, ERROR "%s%.*s\n", errStatus, (int)nBytes, shm->buf);
      fflush(err);
      break;  //end loop on error response
    }
    else { //success response
      fprintf(out, "%s", didOk ? "" : OKAY); fflush(out);
      didOk = true;
      if (nBytes == 0) break; //end loop on empty ok response
      char buf[nBytes];
      receive_data(shm, nBytes, buf);
      TRACE("read buf %.*s", (int)nBytes, buf);
      fprintf(out, "%.*s", (int)nBytes, buf);  fflush(out);
    }
  } while (true);
}


void
do_client(Shm *shm, FILE *in, FILE *out, FILE *err)
{
  bool isInteractive = isatty(fileno(in));
  const char *prompt = isInteractive ? "> " : "";
  MsgArgs *msgArgs = NULL;
  ErrNum errNum;
  ChatCmd cmd;
  fprintf(out, "%s", prompt); fflush(out);
  while ((msgArgs = read_msg_args(in, msgArgs, &errNum)) != NULL) {
    if (errNum != NO_ERR) { fatal("%s", errnum_to_string(errNum)); }
    int rc = parse_cmd(msgArgs, &cmd, err);
    if (rc == 0) do_client_cmd(shm, out, err, &cmd);
    fprintf(out, "%s", prompt); fflush(out);
  }

  // send END_CMD
  cmd.type = END_CMD;
  do_client_cmd(shm, out, err, &cmd);

}
