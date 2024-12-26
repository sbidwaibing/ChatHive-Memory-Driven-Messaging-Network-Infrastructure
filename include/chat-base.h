#ifndef CHAT_BASE_H_
#define CHAT_BASE_H_

#include <stdint.h>
#include <stdio.h>

typedef int64_t TimeMillis;

/** complete information for a chat message */
typedef struct {
  const char *user;
  const char *room;
  size_t nTopics;
  const char **topics; //const char *topics[nTopics]
  const char *message;
  TimeMillis timestamp;
} ChatInfo;

/** For debugging: write chatInfo on out.  Always returns 0. */
int out_chat_info(const ChatInfo *chatInfo, FILE *out);

/** exact format produced by timestamp_to_iso8601() */
#define ISO_8601_FORMAT "YYYY-MM-DDThh:mm:ss.ttt"

/** set buf to ISO-8601 representation of timestamp in localtime.
 *  Should have bufSize > strlen(ISO_8601_FMT).
 *
 *  Will not exceed bufSize.  Return value like snprintf():
 *  # of characters which would have been output (not counting the NUL).
 */
size_t timestamp_to_iso8601(TimeMillis timestamp,
                            size_t bufSize, char buf[bufSize]);

#endif //#ifndef CHAT_BASE_H_
