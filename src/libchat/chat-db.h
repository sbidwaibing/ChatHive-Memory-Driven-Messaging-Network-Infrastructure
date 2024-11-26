#ifndef CHAT_DB_H_
#define CHAT_DB_H_

#include <stdint.h>
#include <stdio.h>

/** Error Handling: Almost all routines which can fail return an int
 *  error-code which is returned as 0 if everything is okay.  Calling
 *  error_chat_db() on the chatDb object will return a string
 *  describing the last error which occurred.
 */

/** *IMPORTANT NOTE*: The sqlite docs state that replicating a a
 *  sqlite db handle across a fork() may be problematic.  Hence a
 *  ChatDb object should be used only within a single process.
 */

//usual ADT idiom
typedef struct _ChatDb ChatDb;

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

/** used for holding result of make_chat_db() */
typedef union {
  ChatDb *chatDb;       //success result: handle to ChatDb object
  const char *err;      //error result: statically allocated message
} MakeChatDbResult;

/** Create ChatDb structure for path and return a pointer to it via
 *  result. If path does not exist, then initialize a new database.
 *  If path is NULL, then return a transient in-memory database.
 *
 *  If the function succeeds, then resultP->chatDb will contain a
 *  ChatDb handle; if it fails (indicated by a non-zero return value)
 *  then resultP->err will contain a statically allocated error message string.
 *
 *  So a sample use of this function might look like:
 *
 *  const char *path = ...;
 *  MakeChatDbResult result;
 *  if (make_chat_db(path, &result) != 0) {
 *    print_error(result.err);
 *  }
 *  else {
 *    ChatDb *chatDb = result.chatDb;
 *    ...
 *  }
 */
int make_chat_db(const char *path, MakeChatDbResult *resultP);

/** Free all resources used by chatDb. */
int free_chat_db(ChatDb *chatDb);

/** Add chat message with specified params to chatDb */
int add_chat_db(ChatDb *chatDb, const char *user, const char *room,
                size_t nTopics, const char *topics[nTopics],
                const char *message);

/** Function Type used for iterating through query results: called for
 *  each result.  The ctx argument can be used by the caller to
 *  read/update arbitrary context.
 */
typedef int IterFn(const ChatInfo *result, void *ctx);


/** Query chat-db using an internal iterator.  Specifically, call
 *  iterFn() for each chat message from chatDb which matches room and
 *  all topics, passing the matching chat-info and the provided
 *  context ctx as arguments.  The function is called at most count
 *  times.  If iterFn() returns non-zero, then the iterator is
 *  terminated; otherwise the iterator is continued.
 *
 *  Note that the messages are iterated most recent first.
 */
int query_chat_db(ChatDb *chatDb, const char *room,
                  size_t nTopics, const char *topics[], size_t count,
                  IterFn *iterFn, void *ctx);

/** set count to # of messages for room */
int count_room_chat_db(ChatDb *chatDb, const char *room, size_t *count);

/** set count to # of messages for topic */
int count_topic_chat_db(ChatDb *chatDb, const char *topic, size_t *count);

/** return error message for last error on chatDb. */
const char *error_chat_db(const ChatDb *chatDb);

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

#endif //ifndef CHAT_DB_H_
