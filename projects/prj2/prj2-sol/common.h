#ifndef COMMON_H_
#define COMMON_H_

#include <chat-cmd.h>

#include <stdio.h>

// declarations common between server and client


enum { MAX_HDR_LEN = 80 };

typedef enum {
  OK_STATUS,
  USER_ERR_STATUS,
  SYS_ERR_STATUS,
  FATAL_ERR_STATUS,
} ServerStatus;

typedef struct {
  enum { CLIENT_HDR, SERVER_HDR } hdrType;
  union {
    struct {          // type == CLIENT_HDR
      CmdType cmdType;// ADD_CMD, QUERY_CMD or END_CMD
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

/** Read header line from in and fill in hdr */
void read_header(Hdr *hdr, FILE *in);

/** write header line for hdr to stream out and flush it. */
void write_header(const Hdr *hdr, FILE *out);

#endif //#ifndef COMMON_H_
