# // this file is to be #included in C programs.  However, it is
# // also automatically edited into a sql program.
# //   - all lines starting with # (like this one) are removed
# //   - remaining // comments are converted to -- sql comments
# //   - trailing \ characters are removed
# //
# //  the following line only makes sense for the generated file
// This file was auto-generated from @{FILE}

// minimally normalized schema

#define STRINGIFY(s) #s
#define STR(s) STRINGIFY(s)


#define CHATS_TABLE "chats"
#define CREATE_CHATS_SQL \
  CREATE TABLE IF NOT EXISTS chats ( \
    id INTEGER PRIMARY KEY, \
    user TEXT, \
    room TEXT, \
    message TEXT, \
    creationTime INTEGER \
      DEFAULT (CAST(1000*(STRFTIME('%s', 'NOW') + \
		      MOD(STRFTIME('%f', 'NOW'), 1)) \
		    AS INTEGER))		  \
  ); \
  CREATE INDEX IF NOT EXISTS roomx ON chats(room); \


#define CREATE_CHATS_SQL_STR STR(CREATE_CHATS_SQL)

#define TOPICS_TABLE "topics"
#define CREATE_TOPICS_SQL \
  CREATE TABLE IF NOT EXISTS topics ( \
    chatId INTEGER, \
    topic TEXT, \
    FOREIGN KEY(chatId) REFERENCES chats(id) \
  ); \
  CREATE INDEX IF NOT EXISTS topicx ON topics(topic); \
  CREATE UNIQUE INDEX IF NOT EXISTS utopicx ON topics(chatId, topic);

#define CREATE_TOPICS_SQL_STR STR(CREATE_TOPICS_SQL)
