#ifndef CHAT_H_
#define CHAT_H_

#include "errnum.h"
#include "msgargs.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  const char *user;   //lowercased
  const char *room;   //lowercased
  size_t nTopics;     //# of topics
  const char *topics; //pointer to nTopics consecutive unique lowercased
                      //topics in insertion order
  const char *msg;    //actual chat message
} MsgInfo;


typedef struct _Chat Chat;

/** return a new empty chat instance */
Chat *make_chat(ErrNum *err);

/** free previously created chat instance */
void free_chat(Chat *chat);

/** extract and add fields from add to chat */
void add_chat(Chat *chat, const MsgArgs *add, ErrNum *err);


/** external iterator to return next match for query in chat, starting
 *  after lastMatch if not NULL
 */
MsgInfo *query_chat(const Chat *chat, const MsgArgs *query,
                    const MsgInfo *lastMatch);

bool is_known_room(const Chat *chat, const char* room);
bool is_known_topic(const Chat *chat, const char *topic);

#endif //#ifndef CHAT_H_
