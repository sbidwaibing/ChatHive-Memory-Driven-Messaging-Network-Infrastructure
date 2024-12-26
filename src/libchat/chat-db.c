#include "chat-db.h"
#include "schema.sql.cpp"

#include <errors.h>
#include <str-space.h>
#include <vector.h>

//#define DO_TRACE
#include <trace.h>

#include <sqlite3.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// *Important Note*: sqlite3 uses 1-based indexes for binding
// prepared statements but 0-based indexes for accessing result
// columns.

// *KNOWN BUG*: dbPath does not work with symlinks.  According to my
// reading of the docs, symlinks should work: there is a
// SQLITE_OPEN_NOFOLLOW flag, but nothing which is the opposite, so it
// seems that symlinks-follow is on by default

/******************** Type and Constant Definitions ********************/

typedef int64_t RowId;

/** Error codes which are returned.  Error message is in
 *  chatDb->err except for make_chat_db() which returns
 *  a statically allocated error string.
 */
enum {
  NO_ERR,
  DB_ERR,
  MEM_ERR,
  IO_ERR
};

enum { MAX_CHATS_QUERY_N_TOPICS_PREP = 4 };

/** IDs for cached prepared statements */
enum {
  ROOM_COUNT_PREP,             //count chats for a room
  TOPIC_COUNT_PREP,            //count chats for a topic
  CHATS_ADD_PREP,              //insert chats row
  TOPICS_ADD_PREP,             //insert topics row
  TOPICS_QUERY_PREP,           //query all topics given chatId
  CHATS_QUERY_TOPICS_0_PREP,   //query chats, topics joined with 0 topics
  CHATS_QUERY_TOPICS_1_PREP,   //query chats, topics joined with 1 topic
  CHATS_QUERY_TOPICS_2_PREP,   //query chats, topics joined with 2 topics
  CHATS_QUERY_TOPICS_3_PREP,   //query chats, topics joined with 3 topics
  N_PREPS  //must be last
};

struct _ChatDb {
  const char *path;             //path for db file
  sqlite3 *db;                  //sqlite db handle
  StrSpace errSpace;            //used for errors and results
  const char *err;              //point to err msg, usually in err
  sqlite3_stmt *preps[N_PREPS]; //cache for lazily initialized prepare statements
};


//sqlite3 specification for an in-memory db
#define SQLITE3_MEMORY_DB ":memory:"

/************************** Error Utilities ****************************/


/** set chatDb->err to sqlite error message */
static int
sqlite3_error(ChatDb *chatDb)
{
  const char *err = sqlite3_errmsg(chatDb->db);
  clear_str_space(&chatDb->errSpace);
  if (add_str_space(&chatDb->errSpace, err) == 0) {
    chatDb->err = iter_str_space(&chatDb->errSpace, NULL);
    return DB_ERR;
  }
  else {
    chatDb->err = "error while reporting sqlite3 error";
    return MEM_ERR;
  }
}

static int
str_space_error(ChatDb *chatDb, const char *err)
{
  clear_str_space(&chatDb->errSpace);
  if (add_str_space(&chatDb->errSpace, err) == 0) {
    chatDb->err = iter_str_space(&chatDb->errSpace, NULL);
  }
  else {
    chatDb->err = "error while reporting str_space error";
  }
  return MEM_ERR;
}

/***************************** DB Utilities ****************************/

/** set statement stmt to one prepared with the provided sql.  If
 *  prepIndex >= 0, then use chatDb->preps as a cache.
 */
static int
prepare_stmt(ChatDb *chatDb, const char *sql, int prepIndex,
             sqlite3_stmt **stmt)
{
  if (prepIndex >= 0 && chatDb->preps[prepIndex]) {
    *stmt = chatDb->preps[prepIndex];
    return NO_ERR;
  }
  int rc = sqlite3_prepare_v2(chatDb->db, sql, -1, stmt, NULL);
  if (rc != SQLITE_OK) return sqlite3_error(chatDb);
  if (prepIndex >= 0) chatDb->preps[prepIndex] = *stmt;
  return NO_ERR;
}

/** fill in key into prepared count stmt and return matching count  */
static int
run_count_stmt(ChatDb *chatDb, sqlite3_stmt *stmt, const char *key,
               size_t *count)
{
  int rc = sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) return sqlite3_error(chatDb);
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) return sqlite3_error(chatDb);
  *count = sqlite3_column_int(stmt, 0);
  TRACE("got %zu count for expanded counts query: %p: %s",
        *count, stmt, sqlite3_expanded_sql(stmt));
  rc = sqlite3_reset(stmt);
  if (rc != SQLITE_OK) return sqlite3_error(chatDb);
  return NO_ERR;
}

/************************** DB Initialization **************************/

/** Set *exists to true iff tableName exists in db.  Note that since
 *  this function is run during construction, it does not report
 *  errors via chatDb->err.
 */
static int
table_exists(ChatDb *chatDb, const char *tableName, bool *exists)
{
  const char *sql =
    "SELECT name FROM sqlite_schema WHERE type='table' AND name=?;";
  int rc;

  // Prepare the SQL statement
  sqlite3_stmt *stmt;
  int errCode = prepare_stmt(chatDb, sql, -1, &stmt);
  if (errCode != NO_ERR) return errCode;

  // Bind the table name to the SQL statement
  sqlite3_bind_text(stmt, 1, tableName, -1, SQLITE_STATIC);

  // Execute the query and check if a result is returned
  rc = sqlite3_step(stmt);

  if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
    return sqlite3_error(chatDb);
  }
  *exists = (rc == SQLITE_ROW);

  // Clean up the prepared statement
  sqlite3_finalize(stmt);
  return NO_ERR;
}


/** If CHATS table does not exist, then assume db needs to be
 *  set up.  Use DDL statements from schema.sql.cpp to create tables.
 */
static int
init_db(ChatDb *chatDb)
{
  bool dbExists;
  if (table_exists(chatDb, CHATS_TABLE, &dbExists) != NO_ERR) {
    return DB_ERR;
  }
  if (dbExists) return NO_ERR;
  const char *sqls[] = { CREATE_CHATS_SQL_STR, CREATE_TOPICS_SQL_STR };
  for (int i = 0; i < sizeof(sqls)/sizeof(sqls[0]); i++) {
    const char *sql = sqls[i];
    char *err;
    int rc = sqlite3_exec(chatDb->db, sql, NULL, 0, &err);
    sqlite3_free(err);
    if (rc != SQLITE_OK) return DB_ERR;
  }
  return NO_ERR;
}

/*********************** Chat Message Addition *************************/

#define CHAT_INSERT_SQL \
  "INSERT INTO chats (user, room, message) VALUES(lower(?), lower(?), ?)"
#define TOPIC_INSERT_SQL \
  "INSERT INTO topics (chatId, topic) VALUES(?, lower(?))"

static int
add_chat(ChatDb *chatDb, const char *user, const char *room,
         size_t nTopics, const char *message, sqlite3_int64 *rowId)
{
  sqlite3_stmt *addChatStmt = NULL;
  int errCode =
    prepare_stmt(chatDb, CHAT_INSERT_SQL, CHATS_ADD_PREP, &addChatStmt);
  if (errCode != NO_ERR) return errCode;
  const char *texts[] = { user, room, message };
  const size_t nTexts = sizeof(texts)/sizeof(texts[0]);
  for (int i = 0; i < nTexts; i++) {
    const char *val = texts[i];
    errCode = sqlite3_bind_text(addChatStmt, i+1, val, -1, SQLITE_TRANSIENT);
    if (errCode != SQLITE_OK) {
      return sqlite3_error(chatDb);
    }
  }

  errCode = sqlite3_step(addChatStmt);
  sqlite3_reset(addChatStmt); //not checking for error here
  if (errCode != SQLITE_DONE) return sqlite3_error(chatDb);
  *rowId = sqlite3_last_insert_rowid(chatDb->db);
  return NO_ERR;
}

static int
add_topics(ChatDb *chatDb, sqlite3_int64 rowId,
           size_t nTopics, const char *topics[])
{
  sqlite3_stmt *addTopicStmt = NULL;
  int errCode =
    prepare_stmt(chatDb, TOPIC_INSERT_SQL, TOPICS_ADD_PREP, &addTopicStmt);
  if (errCode != NO_ERR) return errCode;
  for (int i = 0; i < nTopics; i++) {
    errCode = sqlite3_bind_int(addTopicStmt, 1, rowId);
    if (errCode != SQLITE_OK) {
      return sqlite3_error(chatDb);
    }
    errCode =
      sqlite3_bind_text(addTopicStmt, 2, topics[i], -1, SQLITE_TRANSIENT);
    if (errCode != SQLITE_OK) {
      return sqlite3_error(chatDb);
    }
    errCode = sqlite3_step(addTopicStmt);
    sqlite3_reset(addTopicStmt); //not checking for error here
    if (errCode != SQLITE_DONE) {
      if (sqlite3_extended_errcode(chatDb->db) == SQLITE_CONSTRAINT_UNIQUE) {
        continue;
      }
      return sqlite3_error(chatDb);
    }
  }
  return NO_ERR;
}

/** Add chat message with specified params to chatDb */
int
add_chat_db(ChatDb *chatDb0, const char *user, const char *room,
            size_t nTopics, const char *topics[nTopics], const char *message)
{
  ChatDb *chatDb = (ChatDb *)chatDb0;
  sqlite3_exec(chatDb->db, "BEGIN TRANSACTION", 0, 0, 0);
  sqlite3_int64 rowId;
  int errCode = add_chat(chatDb, user, room, nTopics, message, &rowId);
  if (errCode != NO_ERR) {
    sqlite3_exec(chatDb->db, "ROLLBACK TRANSACTION", 0, 0, 0);
    return errCode;
  }
  errCode = add_topics(chatDb, rowId, nTopics, topics);
  if (errCode != NO_ERR) {
    sqlite3_exec(chatDb->db, "ROLLBACK TRANSACTION", 0, 0, 0);
    return errCode;
  }
  sqlite3_exec(chatDb->db, "COMMIT TRANSACTION", 0, 0, 0);
  return NO_ERR;
}

/*************************** CHAT_DB Query *****************************/

// Run ChatsQuery to iterate through all chats and topics rows which
// match query params.  Within each iteration, use ID of matching chat
// to run topicsQuery which will return all topics for that chat.

// Build up prepared stmt chatsQuery for variable # of topics which
// looks like (for two topics):
//
// SELECT user, room, message, creationTime, id
//   FROM topics T0, topics T1, chats
//   WHERE T0.topic = lower(?) AND T1.topic = lower(?) AND room = lower(?)
//   ORDER BY id DESC;
//
// Need to tediously build this up manually because of the variable #
// of topics.

#define CHATS_QUERY_PREFIX \
  "SELECT user, room, message, creationTime, id FROM "

static int
prepare_chats_query(ChatDb *chatDb, size_t nTopics, sqlite3_stmt **chatsQuery)
{
  bool isCacheableStmt = nTopics < MAX_CHATS_QUERY_N_TOPICS_PREP;
  int prepIndex = isCacheableStmt ? CHATS_QUERY_TOPICS_0_PREP + nTopics : -1;
  if (isCacheableStmt && chatDb->preps[prepIndex]) {
    //cached
    *chatsQuery = chatDb->preps[CHATS_QUERY_TOPICS_0_PREP + nTopics];
    return NO_ERR;
  }
  StrSpace sqlSpace;
  init_str_space(&sqlSpace);
  const char *err;
  if (add_str_space(&sqlSpace, CHATS_QUERY_PREFIX) != 0) {
    err = "cannot add query chats prefix to sqlSpace";
    goto STR_SPACE_ERROR;
  }
  for (int i = 0; i < nTopics; i++) {
    if (append_sprintf_str_space(&sqlSpace, "topics T%d, ", i) != 0) {
      err = "cannot add topics table spec to sqlSpace";
      goto STR_SPACE_ERROR;
    }
  }
  if (append_str_space(&sqlSpace, "chats WHERE ") != 0) {
    err = "cannot add query chats WHERE to sqlSpace";
    goto STR_SPACE_ERROR;
  }
  for (int i = 0; i < nTopics; i++) {
    if (append_sprintf_str_space(&sqlSpace, "T%d.chatId = id AND "
                                 "T%d.topic = lower(?) AND ", i, i)  != 0) {
      err = "cannot add topic constraint to sqlSpace";
      goto STR_SPACE_ERROR;
    }
  }
  if (append_str_space(&sqlSpace, "room = lower(?) ORDER BY id DESC;") != 0) {
    err = "cannot add room constraint to sqlSpace";
    goto STR_SPACE_ERROR;
  }
  const char *sql = iter_str_space(&sqlSpace, NULL);
  sqlite3_stmt *stmt;
  int errCode = prepare_stmt(chatDb, sql, prepIndex, &stmt);
  TRACE("prepared chatsQuery for nTopics = %zu: %p %s", nTopics, stmt, sql);
  free_str_space(&sqlSpace);
  if (errCode != NO_ERR) return errCode;
  *chatsQuery = stmt;
  return NO_ERR;
 STR_SPACE_ERROR:
    free_str_space(&sqlSpace);
    return str_space_error(chatDb, err);
}

static void
free_chats_query(ChatDb *chatDb, size_t nTopics, sqlite3_stmt *chatsQuery)
{
  if (nTopics >= MAX_CHATS_QUERY_N_TOPICS_PREP) sqlite3_finalize(chatsQuery);
}

static int
fill_chats_query(ChatDb *chatDb, const char *room,
                 size_t nTopics, const char *topics[nTopics],
                 sqlite3_stmt *chatQuery)
{
  for (int i = 0; i < nTopics + 1; i++) {
    const char *val = (i == nTopics) ? room : topics[i];
    int rc = sqlite3_bind_text(chatQuery, i + 1, val, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) return sqlite3_error(chatDb);
  }
  return NO_ERR;
}

#define TOPICS_QUERY \
  "SELECT topic FROM chats, topics \
     WHERE id = ? AND chatId = id \
     ORDER BY topic;"

static int
prepare_topics_query(ChatDb *chatDb, sqlite3_stmt **topicsQuery)
{
  return prepare_stmt(chatDb, TOPICS_QUERY, TOPICS_QUERY_PREP, topicsQuery);
}


/** Query chat-db using an internal iterator.  Specifically, call
 *  iterFn() for each chat message from chatDb which matches room and
 *  all topics, passing the matching chat-info and the provided
 *  context ctx as arguments.  The function is called at most count
 *  times. If iterFn() returns non-zero, then the iterator is
 *  terminated; otherwise the iterator is continued.
 *
 *  Note that the messages are iterated most recent first.
 */
int
query_chat_db(ChatDb *chatDb, const char *room,
              size_t nTopics, const char *topics[], size_t count,
              IterFn *iterFn, void *ctx)
{
  int errCode = NO_ERR;
  StrSpace results;
  init_str_space(&results);
  Vector topicsResult;
  init_vector(&topicsResult, sizeof(char *));

  sqlite3_stmt *chatsQuery = NULL;
  sqlite3_stmt *topicsQuery = NULL;
  errCode = prepare_chats_query(chatDb, nTopics, &chatsQuery);
  if (errCode != NO_ERR) goto CLEANUP;
  TRACE("prepared chatsQuery: %p", chatsQuery);
  errCode = fill_chats_query(chatDb, room, nTopics, topics, chatsQuery);
  if (errCode != NO_ERR) goto CLEANUP;
  TRACE("expanded chats query: %p: %s", chatsQuery,
        sqlite3_expanded_sql(chatsQuery));
  errCode = prepare_topics_query(chatDb, &topicsQuery);
  if (errCode != NO_ERR) goto CLEANUP;
  TRACE("prepared topicsQuery = %p", topicsQuery);
  for (int i = 0; i < count; i++) {
    clear_str_space(&results);
    clear_vector(&topicsResult);
    int rc = sqlite3_step(chatsQuery);
    if (rc == SQLITE_DONE) break;
    if (rc != SQLITE_ROW) { errCode = sqlite3_error(chatDb); break; }
    for (int colN = 0; colN < 3; colN++) {
      const char *text = (const char *)sqlite3_column_text(chatsQuery, colN);
      add_str_space(&results, text);
      TRACE("retrieved colN %d: %s", colN, text);
    }
    int64_t ints[] = { /*creationTime*/ 0, /*id*/ 0, };
    for (int colN = 3; colN < 5; colN++) {
      ints[colN - 3] = sqlite3_column_int64(chatsQuery, colN);
      TRACE("retrieved colN %d: %ld", colN, ints[colN - 3]);
    }
    TimeMillis creationTime = ints[0];
    RowId id = ints[1];
    rc = sqlite3_bind_int64(topicsQuery, 1, id);
    if (rc != SQLITE_OK) { errCode = sqlite3_error(chatDb); break; }
    TRACE("expanded topics query: %p: %s", topicsQuery, sqlite3_expanded_sql(topicsQuery));
    int retNTopics = 0;
    while ((rc = sqlite3_step(topicsQuery)) == SQLITE_ROW) {
      const char *topic = (const char *)sqlite3_column_text(topicsQuery, 0);
      add_str_space(&results, topic);
      TRACE("topic = %s", topic);
      retNTopics++;
    }
    rc = sqlite3_reset(topicsQuery);
    if (rc != SQLITE_OK) { errCode = sqlite3_error(chatDb); break; }
    int iterN = 0;
    ChatInfo chatInfo = { .timestamp = creationTime, .nTopics = retNTopics };
    for (const char *str = iter_str_space(&results, NULL);
         str != NULL;
         str = iter_str_space(&results, str)) {
      TRACE("iter-str = %s", str);
      switch (iterN) {
      case 0:
        chatInfo.user = str;
        break;
      case 1:
        chatInfo.room = str;
        break;
      case 2:
        chatInfo.message = str;
        break;
      default:
        add_vector(&topicsResult, (void *)&str);
        break;
      }
      iterN++;
    }
    assert(retNTopics == n_elements_vector(&topicsResult));
    chatInfo.topics = get_base_vector(&topicsResult);
    if (iterFn(&chatInfo, ctx) != 0) break;
  } //for int i = 0; i < count; i++)
 CLEANUP:
  TRACE("cleanup: errCode = %d, chatsQuery = %p, topicsQuery = %p",
        errCode, chatsQuery, topicsQuery);
  free_str_space(&results);
  free_vector(&topicsResult);
  if (chatsQuery != NULL) {
    if (sqlite3_reset(chatsQuery) != SQLITE_OK) errCode = DB_ERR;
    free_chats_query(chatDb, nTopics, chatsQuery);
  }
  if (topicsQuery != NULL) {
    if (sqlite3_reset(topicsQuery) != SQLITE_OK) errCode = DB_ERR;
  }
  return errCode;
}


/******************** CHAT_DB Creation/Destruction *********************/

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
int
make_chat_db(const char *path, MakeChatDbResult *resultP)
{
  // resources to be cleaned up on error
  // note for all these types clean up when resource pointer is NULL is a NOP
  char *path1 = NULL;
  sqlite3 *db = NULL;
  ChatDb *chatDb = NULL;

  StrSpace *errSpace = NULL;

  int errCode = NO_ERR;

  const bool isInMemory = path == NULL || strcmp(path, SQLITE3_MEMORY_DB) == 0;
  if (isInMemory) path = SQLITE3_MEMORY_DB;
  const char *prefix = isInMemory ? "" : "./";
  path1 = malloc(strlen(prefix) + strlen(path) + 1);
  if (!path1) {
    resultP->err = "path memory allocation failure";
    errCode = MEM_ERR;
    goto CLEANUP;
  }
  sprintf(path1, "%s%s", prefix, path);

  // does not seem to follow symlinks;
  // sqlite_open_v2() with the two extra args seems to have the same issue
  if (sqlite3_open(path1, &db) != SQLITE_OK) {
    resultP->err = "db open error";
    errCode = DB_ERR;
    goto CLEANUP;
  }

  chatDb = calloc(1, sizeof(ChatDb));
  if (!chatDb) {
    resultP-> err = "ChatDb memory allocation failure";
    errCode = MEM_ERR;
    goto CLEANUP;
  }

  //looking good, initialize *chatDb
  chatDb->path = path1;
  chatDb->db = db;
  resultP->chatDb = chatDb;
  init_str_space(&chatDb->errSpace); errSpace = &chatDb->errSpace;

  if (init_db(chatDb) != NO_ERR) {
    resultP->err = "db initialization error";
    errCode = DB_ERR;
    goto CLEANUP;
  }
  assert(errCode == NO_ERR);
  return errCode;
 CLEANUP:
  sqlite3_close(db);
  if (errSpace) free_str_space(errSpace);
  free((void*)path1);
  free(chatDb);
  return errCode;
}

/** Free all resources used by chatDb. */
int
free_chat_db(ChatDb *chatDb)
{
  for (int i = 0; i < N_PREPS; i++) {   // clean up cached prepared statements
    sqlite3_finalize(chatDb->preps[i]); //calling on NULL is a NOP
  }
  if (sqlite3_close(chatDb->db) != SQLITE_OK) {
    return sqlite3_error((ChatDb *)chatDb);
  }
  free((void *)chatDb->path);
  free_str_space(&chatDb->errSpace);
  free((void *)chatDb);
  return NO_ERR;
}


/************************** Misc API Functions *************************/

/** return error message for last error on chatDb. */
const char *
error_chat_db(const ChatDb *chatDb)
{
  return iter_str_space(&chatDb->errSpace, NULL);
}

#define COUNT_ROOM_CHATS_SQL "SELECT COUNT(*) FROM chats WHERE room = lower(?);"

/** set count to # of messages for room */
int
count_room_chat_db(ChatDb *chatDb, const char *room, size_t *count)
{
  sqlite3_stmt *countStmt;
  int errCode = prepare_stmt(chatDb, COUNT_ROOM_CHATS_SQL, ROOM_COUNT_PREP,
                             &countStmt);
  if (errCode != NO_ERR) return errCode;
  return run_count_stmt(chatDb, countStmt, room, count);
}

#define COUNT_TOPIC_CHATS_SQL \
  "SELECT COUNT(*) FROM topics WHERE topic = lower(?);"

/** set count to # of messages for topic */
int
count_topic_chat_db(ChatDb *chatDb, const char *topic, size_t *count)
{
  sqlite3_stmt *countStmt;
  int errCode = prepare_stmt(chatDb, COUNT_TOPIC_CHATS_SQL, TOPIC_COUNT_PREP,
                             &countStmt);
  if (errCode != NO_ERR) return errCode;
  return run_count_stmt(chatDb, countStmt, topic, count);
}


/******************************* Testing *******************************/


#ifdef TEST_CHAT_DB

// both automated and manual tests (output of manual tests must be manually
// expected).  Manual tests generated on if MANUAL_TEST_CHAT_DB defined,
// otherwise automated tests.  The automated tests are more comprehensive.

// Test data used by both automated and manual tests
static ChatInfo data0 = {
  .user = "@ZDU",
  .room = "Sysprog",
  .nTopics = 3,
  .topics = (const char *[]) { "#db", "#sqlitE", "#db", },
  .message = "sqlite is pretty cool",
};

static ChatInfo data1 = {
  .user = "@ZDU",
  .room = "Sysprog",
  .nTopics = 6,
  .topics =  (const char *[]) { "#db", "#mongoDB", "#json",
                                "#js", "#python", "#ruby", },
  .message = "mongodb supports storing structured documents (essentially "
               "JSON) and can be accessed from many programming languages "
               "include popular scripting languges like JavaScript, Python "
               "and Ruby",
};

static ChatInfo data2 = {
  .user = "@Tom",
  .room = "Sysprog",
  .nTopics = 4,
  .topics = (const char *[]) { "#fork", "#syscall", "#unix", "#stdio"  },
  .message = "Worth noting: If stdio buffers are not flushed before a fork(), "
              "then the child will also get a copy of that buffer."
};

static ChatInfo data3 = {
  .user = "@Jane",
  .room = "Sysprog",
  .nTopics = 4,
  .topics =
  (const char *[]) { "#pipe", "#syscall", "#unix", "#file-descriptor"  },
  .message = "The pipe() syscall returns two file descriptors fd[2] to a "
               "pipe such that fd[0] refers to the read end of the pipe "
               "and fd[1] refers to the write end of the pipe. This pipe "
               "is typically used for inter-process communication."
};

static ChatInfo *data[] = {
  &data0, &data1, &data2, &data3,
};


static void
add_test_data(ChatDb *chatDb)
{
  for (int i = 0; i < sizeof(data)/sizeof(data[0]); i++) {
    const ChatInfo *d = data[i];
    int err = add_chat_db(chatDb, d->user, d->room,
                          d->nTopics, d->topics, d->message);
    if (err != NO_ERR) {
      error("%s", error_chat_db(chatDb));
      break;
    }
  }
}


#ifdef MANUAL_TEST_CHAT_DB

// used as query iteration function
static int
out_test_result(const ChatInfo *chatInfo, FILE *out)
{
  out_chat_info(chatInfo, out);
  fprintf(out, "-------\n");
  return 0;
}

/** produce output to be examined manually */
static int
do_tests(ChatDb *chatDb)
{
  FILE *out = stdout;
  IterFn *fn = (int (*)(const ChatInfo *, void *))&out_test_result;
  int err;
  const int count = 10;

  fprintf(out, "*** query sysprog ***\n");
  err = query_chat_db(chatDb, "Sysprog", 0, NULL, count, fn, out);
  if (err != NO_ERR) error("query no topics error: %s", error_chat_db(chatDb));

  fprintf(out, "*** query sysprog #ruby ***\n");
  err = query_chat_db(chatDb, "Sysprog", 1, (const char *[]){ "#ruby" },
                      count, fn, out);
  if (err != NO_ERR) error("query #ruby topic error: %s", error_chat_db(chatDb));

  fprintf(out, "*** query sysprog #pipe #unix ***\n");
  err = query_chat_db(chatDb, "Sysprog", 2, (const char *[]){ "#pipe", "#unix" },
                      count, fn, out);
  if (err != NO_ERR) error("query #ruby topic error: %s", error_chat_db(chatDb));

  fprintf(out, "*** query sysprog "
          "#db #mongoDB #json #js #python #ruby #js #db ***\n");
  const char *topics8[] =
    { "#db", "#mongoDB", "#json", "#js", "#python", "#ruby", "#js", "#db", };
  err = query_chat_db(chatDb, "Sysprog", 8, topics8, count, fn, out);
  if (err != NO_ERR) error("query no topics error: %s", error_chat_db(chatDb));

  fprintf(out, "*** query sysprog #pipe #db (expect no results) ***\n");
  err = query_chat_db(chatDb, "Sysprog", 2, (const char *[]){ "#pipe",  "#db" },
                      count, fn, out);
  if (err != NO_ERR) error("query #ruby topic error: %s", error_chat_db(chatDb));

  return 0; //results must be checked manually
}


#else // automated tests

#include <unit-test.h> //for CHKF() macro

/** return index of first topics1[] element which is not in outTopics[];
 *  < 0 if none.
 */
static int
check_topics(size_t nTopics0, const char *topics0[nTopics0],
             size_t nTopics1, const char *topics1[nTopics1])
{
  //assume topics0[] sorted
  for (int i = 0; i < nTopics1; i++) {
    const char *in = topics1[i];
    bool isFound = false;
    for (int j = 0; j < nTopics0; j++) {
      int cmp = strcasecmp(in, topics0[j]);
      if (cmp >= 0) {
        isFound = (cmp == 0);
        break;
      }
    }
    if (!isFound) return i;
  }
  return -1;
}


typedef struct _Test {
  char *label;                //label for test
  char *room;                 //room param for query
  size_t count;               //count param for query
  size_t nTopics;             //nTopics param for query
  const char **topics;        //topics param for query
  int terminateCount;         //if >= 0, have iter fn return non-zero after
                              //this many results
  size_t nExpected;           //# of expected results
  const ChatInfo **expected;  //expected results
} Test;

typedef struct {
  size_t *resultIndex;
  int *nErrors;
  const Test *test;
} TestContext;

static Test tests[] = {
  { .label = "no topics",
    .room = "Sysprog",
    .count = 100,
    .nTopics = 0,
    .topics = NULL,
    .terminateCount = -1,
    .nExpected = 4,
    .expected = (const ChatInfo *[]) { &data3, &data2, &data1, &data0 },
  },
  { .label = "no topics with limiting count",
    .room = "Sysprog",
    .count = 2,
    .nTopics = 0,
    .topics = NULL,
    .terminateCount = -1,
    .nExpected = 2,
    .expected = (const ChatInfo *[]) { &data3, &data2, },
  },
  { .label = "no topics, with iter-fn return non-zero after 3 results",
    .room = "Sysprog",
    .count = 100,
    .nTopics = 0,
    .topics = NULL,
    .terminateCount = 3,
    .nExpected = 3,
    .expected = (const ChatInfo *[]) { &data3, &data2, &data1 },
  },
  { .label = "one topic",
    .room = "SysProg",
    .count = 100,
    .nTopics = 1,
    .topics = (const char*[]) { "#ruby" },
    .terminateCount = -1,
    .nExpected = 1,
    .expected = (const ChatInfo *[]) { &data1, },
  },
  { .label = "two topics",
    .room = "SysProg",
    .count = 100,
    .nTopics = 2,
    .topics = (const char*[]) { "#unix", "#pipe" },
    .terminateCount = -1,
    .nExpected = 1,
    .expected = (const ChatInfo *[]) { &data3, },
  },
  { .label = "eight topics",
    .room = "SysProg",
    .count = 100,
    .nTopics = 8,
    .topics = (const char*[])
      { "#db", "#mongoDB", "#json", "#js", "#python", "#ruby", "#js", "#db", },
    .terminateCount = -1,
    .nExpected = 1,
    .expected = (const ChatInfo *[]) { &data1 },
  },
  { .label = "no results for two topics",
    .room = "SysProg",
    .count = 100,
    .nTopics = 2,
    .topics = (const char*[]) { "#unix", "#db" },
    .terminateCount = -1,
    .nExpected = 0,
    .expected = NULL,
  },
};

/** Compares current iter result actual with expected result in
 *  ctx->expected[resultIndex];
 *  Returns non-zero to terminate iteration
 */
static int
query_iter_fn(const ChatInfo *actual, void *ctx)
{
  TestContext *testCtx = ctx;
  const size_t resultIndex = *testCtx->resultIndex;
  const Test *test = testCtx->test;
  if (resultIndex == test->terminateCount) return 1;
  const char *label = test->label;
  bool chk;

  // should not have more than the expected # of results
  chk = resultIndex < test->nExpected;
  CHKF(chk, "%s %zu: more results than the %zu expected",
       label, resultIndex, test->nExpected);
  if (!chk) { (*testCtx->nErrors)++; return 0; }

  const ChatInfo *expected = test->expected[resultIndex];

  chk = strcasecmp(actual->user, expected->user) == 0;
  CHKF(chk, "%s %zu: user %s != %s",
       label, resultIndex, actual->user, expected->user);
  if (!chk) (*testCtx->nErrors)++;

  chk = strcasecmp(actual->room, expected->room) == 0;
  CHKF(chk, "%s %zu: room %s != %s",
       label, resultIndex, actual->room, expected->room);
  if (!chk) (*testCtx->nErrors)++;

  const int badIndex = check_topics(actual->nTopics, actual->topics,
                                    expected->nTopics, expected->topics);
  chk = badIndex >= 0;
  CHKF(chk, "%s %zu: missing topic %s",
       label, resultIndex, expected->topics[badIndex]);
  if (!chk) (*testCtx->nErrors)++;

  chk = strcmp(actual->message, expected->message) == 0;
  CHKF(chk, "%s %zu: message %s != %s",
       label, resultIndex, actual->message, expected->message);
  if (!chk) (*testCtx->nErrors)++;;

  (*testCtx->resultIndex)++;
  return 0;
}

/** returns # of errors */
static int
test_counts(ChatDb *chatDb)
{
  int nErrors = 0;
  int err;
  bool chk;
  size_t count;

  const char *room = "SysProg";
  err = count_room_chat_db(chatDb, room, &count);
  if (err != NO_ERR) {
    return error("count for room \"%s\": %s", room, error_chat_db(chatDb));
  }
  chk = count == 4;
  CHKF(chk, "count room %s messages: %zu != 4 (expected)", room, count);
  if (!chk) nErrors++;

  room = "SysProg1";
  err = count_room_chat_db(chatDb, room, &count);
  if (err != NO_ERR) {
    return error("count for room \"%s\": %s", room, error_chat_db(chatDb));
  }
  chk = count == 0;
  CHKF(chk, "count room %s messages: %zu != 0 (expected)", room, count);
  if (!chk) nErrors++;

  const char *topic = "#DB";
  err = count_topic_chat_db(chatDb, topic, &count);
  if (err != NO_ERR) {
    return error("count for topic \"%s\": %s", topic, error_chat_db(chatDb));
  }
  chk = (count == 2);
  CHKF(chk, "count topic %s messages: %zu != 2 (expected)", topic, count);
  if (!chk) nErrors++;

  topic = "#DB1";
  err = count_topic_chat_db(chatDb, topic, &count);
  if (err != NO_ERR) {
    return error("count for topic \"%s\": %s", topic, error_chat_db(chatDb));
  }
  chk = (count == 0);
  CHKF(chk, "count topic %s messages: %zu != 0 (expected)", topic, count);
  if (!chk) nErrors++;

  return nErrors;
}

/** returns # of errors */
static int
do_tests(ChatDb *chatDb)
{
  int nErrors = 0;
  for (int t = 0; t < sizeof(tests)/sizeof(tests[0]); t++) {
    size_t resultIndex = 0;
    TestContext ctx = {
      .resultIndex = &resultIndex,
      .nErrors = &nErrors,
      .test = &tests[t]
    };
    int err =
      query_chat_db(chatDb, tests[t].room, tests[t].nTopics, tests[t].topics,
                    tests[t].count, query_iter_fn, &ctx);
    if (err != NO_ERR) {
      error("%s: %s", tests[t].label, error_chat_db(chatDb));
      return 1;
    }
    bool chk = resultIndex == tests[t].nExpected;
    CHKF(chk, "%s: # of results %zu != # expected (%zu)",
         tests[t].label, resultIndex, tests[t].nExpected);
    if (!chk) nErrors++;
  }
  return nErrors + test_counts(chatDb);
}

#endif //ifndef MANUAL_TEST_CHAT_DB

//exits with error count
int
main(int argc, const char *argv[]) {
  const char *dbPath = argc > 1 ? argv[1] : NULL;
  MakeChatDbResult result;
  int err = make_chat_db(dbPath, &result);
  if (err != NO_ERR) {
    fatal("%s", result.err);
  }
  ChatDb *chatDb = result.chatDb;
  add_test_data(chatDb);
  int rc = do_tests(chatDb);
  free_chat_db(chatDb);
  return rc;
}

#endif //ifdef TEST_CHAT_DB
