#include "utils.h"

//uncomment next line to turn on tracing; use TRACE() with printf-style args
#define DO_TRACE
#include <trace.h>


#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

enum { MAX_NAME_LEN = 80, N_PRIVATE_FIFOS = 2, };

#define WELL_KNOWN_FIFO_NAME "REQUESTS"

/** Open well-known FIFO.  If !isClient, create FIFO if necessary */
int
open_well_known_fifo(bool isClient, FILE **fifo)
{
  FILE *f = NULL;
  const char *const name = WELL_KNOWN_FIFO_NAME;
  if (isClient) {
    f = fopen(name, "w");
  }
  else {
    int err = mkfifo(name, 0666);
    if (err < 0 && errno != EEXIST) {
      return 1;
    }
    int fd = open(name, O_RDWR);
    if (fd < 0) return 1;
    f = fdopen(fd, "rw");
  }
  *fifo = f;
  return (f == NULL);
}


static int
make_client_fifo_name(pid_t clientPid, int zeroOne, char name[MAX_NAME_LEN])
{
  size_t n = snprintf(name, MAX_NAME_LEN, "%lld.%d",
                      (long long)clientPid, zeroOne);
  return (n >= MAX_NAME_LEN);
}

/** create client-specific fifos in current directory
 *  (called by client).  Terminates program on error.
 */
int
make_client_fifos(void)
{
  pid_t clientPid = getpid();
  for (int i = 0; i < N_PRIVATE_FIFOS; i++) {
    char name[MAX_NAME_LEN];
    if (make_client_fifo_name(clientPid, i, name) != 0) return 1;
    if (mkfifo(name, 0666) != 0) return 1;
  }
  return 0;
}

/** remove client-specific fifos from current directory (called by client) */
int
remove_client_fifos(void)
{
  pid_t clientPid = getpid();
  for (int i = 0; i < N_PRIVATE_FIFOS; i++) {
    char name[MAX_NAME_LEN];
    if (make_client_fifo_name(clientPid, i, name) != 0) return 1;
    if (unlink(name) != 0) return 1;
  }
  return 0;
}

/** Maintain a pair of fifos[2] FIFO FILE stream. fifo[0] is
 *  for reading by the client and written by the worker while
 *  fifo[1] for reading by the worker and written by the client.
 *
 *  To prevent deadlock, client and worker open FIFOs in the
 *  same order:
 *
 *    + Client will first open fifo[0] for reading and then fifo[1]
 *      for writing.
 *
 *    + Worker will first open fifo[0] for writing and then fifo[1]
 *      for reading.
 */
int
open_client_fifos(pid_t clientPid, bool isClient, FILE *fifos[2])
{
  const char *mode0 = (isClient) ? "r" : "w";
  const char *mode1 = (isClient) ? "w" : "r";
  const char *modes[] = { mode0, mode1 };
  assert(sizeof(modes)/sizeof(modes[0]) == N_PRIVATE_FIFOS);
  for (int i = 0; i < N_PRIVATE_FIFOS; i++) {
    char name[MAX_NAME_LEN];
    if (make_client_fifo_name(clientPid, i, name) != 0) return 1;
    FILE *f = fopen(name, modes[i]);
    if (!f) {
      return 1;
    }
    fifos[i] = f;
  }
  return 0;
}
