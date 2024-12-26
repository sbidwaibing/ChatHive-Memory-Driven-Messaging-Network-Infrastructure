#ifndef UTILS_H_
#define UTILS_H_

#include <stdbool.h>
#include <stdio.h>

#include <unistd.h>

/** Open well-known FIFO.  If !isClient, create FIFO if necessary */
int open_well_known_fifo(bool isClient, FILE **fifo);

/** create client-specific fifos in current directory (called by client) */
int make_client_fifos(void);

/** remove client-specific fifos from current directory (called by client) */
int remove_client_fifos(void);

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
int open_client_fifos(pid_t clientPid, bool isClient, FILE *fifos[2]);

#endif //#ifndef UTILS_H_
