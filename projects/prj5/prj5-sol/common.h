#ifndef COMMON_H_
#define COMMON_H_

#include <chat-cmd.h>

#include <stdio.h>

// declarations common between server and client

//internal command used to send user and room.  It is sent right after
//a client first connects to server.
enum { INIT_CMD = N_CMDS };

enum { MAX_HDR_LEN = 80 };

typedef enum {
  OK_STATUS,
  USER_ERR_STATUS,
  SYS_ERR_STATUS,
  FATAL_ERR_STATUS,
  END_STATUS, //server ack for client END_CMD
} ServerStatus;

typedef struct {
  enum { CLIENT_HDR, SERVER_HDR } hdrType;
  union {
    struct {          // type == CLIENT_HDR
      CmdType cmdType;// ADD_CMD, QUERY_CMD, END_CMD or INIT_CMD
      int count;      // count for QUERY requests, not used for other requests
      size_t nTopics; // # of topics
    };
    struct {          // type == SERVER_HDR
      ServerStatus
        status;       // server status
    };
  };
  size_t nBytes;
} Hdr;

/** Read header line from in and fill in hdr; return non-zero on error */
int read_header(Hdr *hdr, FILE *in);

/** write header line for hdr to stream out and flush it; return
 *  non-zero on error
 */
int write_header(const Hdr *hdr, FILE *out);

#endif //#ifndef COMMON_H_
