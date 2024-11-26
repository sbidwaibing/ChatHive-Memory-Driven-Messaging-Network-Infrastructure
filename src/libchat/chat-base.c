#include "chat-db.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/** set buf to ISO-8601 representation of timestamp in localtime.
 *  Should have bufSize > strlen(ISO_8601_FMT).
 *
 *  Will not exceed bufSize.  Return value like snprintf():
 *  # of characters which would have been output (not counting the NUL).
 */
size_t
timestamp_to_iso8601(TimeMillis timestamp, size_t bufSize, char buf[bufSize])
{
  size_t iso8601Len = strlen(ISO_8601_FORMAT);
  if (iso8601Len + 1 > bufSize) return iso8601Len;
  time_t t = timestamp/1000;
  int millis = timestamp%1000;
  struct tm brokenDownTime;
  localtime_r(&t, &brokenDownTime);
  // return value is 0 if buf not big enough, hence use precomputed ret value
  size_t n = strftime(buf, bufSize, "%Y-%m-%dT%H:%M:%S", &brokenDownTime);
  snprintf(&buf[n], bufSize - n, ".%03d", millis);
  return iso8601Len;
}

/** For debugging: write chatInfo on out.  Always returns 0. */
int
out_chat_info(const ChatInfo *chatInfo, FILE *out)
{
  size_t ISO_8601_LEN = strlen(ISO_8601_FORMAT);
  char iso8601[ISO_8601_LEN + 1];
  timestamp_to_iso8601(chatInfo->timestamp, ISO_8601_LEN+1, iso8601);
  fprintf(out, "%s\n", iso8601);
  fprintf(out, "%s %s ", chatInfo->user, chatInfo->room);
  for (int i = 0; i < chatInfo->nTopics; i++) {
    fprintf(out, "%s ", chatInfo->topics[i]);
  }
  fprintf(out, "\n%s\n", chatInfo->message);
  return 0;
}
