#include "common.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

//uncomment next line to turn on tracing; use TRACE() with printf-style args
//#define DO_TRACE
#include <trace.h>

/** Read header line from in and fill in hdr; return non-zero on error */
int
read_header(Hdr *hdr, FILE *in)
{
  char buf[MAX_HDR_LEN];
  if (fgets(buf, MAX_HDR_LEN, in) != buf) return 1;
  assert(buf[strlen(buf)] == '\0');
  TRACE("pid = %ld, buf = %s", (long)getpid(), buf);
  if (hdr->hdrType == CLIENT_HDR) {
    int cmdType;
    if (sscanf(buf, "%d %d %zu %zu", &cmdType, &hdr->count, &hdr->nTopics,
               &hdr->nBytes) != 4) {
      return 1;
    }
    hdr->cmdType = cmdType;
  }
  else {
    int serverStatus;
    if (sscanf(buf, "%d %zu", &serverStatus, &hdr->nBytes) != 2) return 1;
    hdr->status = serverStatus;
  }
  return 0;
}

/** write header line for hdr to stream out and flush it; return
 *  non-zero on error
 */
int
write_header(const Hdr *hdr, FILE *out)
{
  char buf[MAX_HDR_LEN];
  int nBytes =
    hdr->hdrType == CLIENT_HDR
    ? snprintf(buf, MAX_HDR_LEN, "%d %d %zu %zu\n",
               hdr->cmdType, hdr->count, hdr->nTopics, hdr->nBytes)
    : snprintf(buf, MAX_HDR_LEN, "%d %zu\n", hdr->status, hdr->nBytes);
  assert(nBytes < MAX_HDR_LEN);
  TRACE("pid = %ld, nBytes = %d, buf = %s", (long)getpid(), nBytes, buf);
  if (fwrite(buf, 1, nBytes, out) != nBytes) return 1;
  if (fflush(out) != 0) return 1;
  return 0;
}
