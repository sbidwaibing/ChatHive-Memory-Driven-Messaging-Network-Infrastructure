#include <chat-db.h>

#include <stdio.h>


/** set buf to ISO-8601 representation of timestamp in localtime.
 *  Should have bufSize > strlen(ISO_8601_FMT).
 *
 *  Will not exceed bufSize.  Return value like snprintf():
 *  # of characters which would have been output (not counting the NUL).
 */
// mock for testing, will get linked in as opposed to library function
size_t
timestamp_to_iso8601(TimeMillis timestamp,
                     size_t bufSize, char buf[bufSize])
{
  return snprintf(buf, bufSize, "%s", ISO_8601_FORMAT);
}
