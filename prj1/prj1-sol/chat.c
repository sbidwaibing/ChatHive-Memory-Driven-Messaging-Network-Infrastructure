#include "chat.h"

#include "errnum.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

//#define DO_TRACE
#include <trace.h>

//node in singly-linked list
struct _Chat {
  MsgInfo info;
  struct _Chat *next;
  char mem[]; //storage for strings in info: user, room, msg, topics
};

/** copy NUL-terminated string from src to dest, converting it to
 *  lower-case.  Adds terminating NUL character to dest and return
 *  pointer to it.  Assumes that dest has space for at least
 *  strlen(src) + 1 chars.
 */
static char *
stpcpy_lc(char *dest, char *src)
{
  char *destP = dest;
  for (char *srcP = src; *srcP != '\0'; srcP++) {
    *destP++ = tolower(*srcP);
  }
  *destP = '\0';
  return destP;
}

/** return a new empty chat instance */
Chat *
make_chat(ErrNum *err) {
  Chat *chat = calloc(1, sizeof(Chat));
  *err = chat ? NO_ERR :  MEM_ERR;
  return chat;
}

/** free previously created chat instance */
void
free_chat(Chat *chat) {
  Chat *p1;
  for (Chat *p = chat->next; p != NULL; p = p1) {
    p1 = p->next;
    free(p);
  }
  free(chat);
}

/** extract and add fields from add to chat */
// link in new Chat node at head of chat list
void
add_chat(Chat *chat, const MsgArgs *add, ErrNum *err)
{
  TRACE("adding chat msg %s", add->msg);
  *err = NO_ERR;
  size_t size_mem = 0;
  for (int i = 1; i < add->nArgs; i++) {
    size_mem += strlen(add->args[i]) + 1;
  }
  size_mem += strlen(add->msg) + 1;
  Chat *newChat = malloc(sizeof(Chat) + size_mem);
  if (!newChat) {
    *err = MEM_ERR;
    return;
  }
  newChat->next = chat->next; chat->next = newChat;
  char *p = newChat->mem;
  newChat->info.user = p;  p = stpcpy_lc(p, add->args[1]) + 1;
  newChat->info.room = p;  p = stpcpy_lc(p, add->args[2]) + 1;
  newChat->info.msg = p; p = stpcpy(p, add->msg) + 1;
  newChat->info.topics = p;
  size_t nTopics = 0;
  char *p0 = p;
  for (int i = 3; i < add->nArgs; i++) {
    char *topic = add->args[i];
    int j;
    char *p1 = p0;
    for (j = 0; j < nTopics; j++) {
      if (strcasecmp(topic, p1) == 0) break;
      p1 += strlen(p1) + 1;
    }
    if (j == nTopics) { //new topic
      p = stpcpy_lc(p, topic) + 1; nTopics++;
    }
  }
  newChat->info.nTopics = nTopics;
  TRACE("added chat with %zu topics", nTopics);
}


static bool
has_topic(const Chat *chat, const char *topic)
{
  const char *p = chat->info.topics;
  for (int i = 0; i < chat->info.nTopics; i++) {
    if (strcasecmp(topic, p) == 0) return true;
    p += strlen(p) + 1;
  }
  return false;
}

static bool
chat_match(const Chat *chat, const MsgArgs *query) {
  if (strcasecmp(chat->info.room, query->args[1]) != 0) return false;
  for (int i = (query->args[2][0] == '#') ? 2 : 3; i < query->nArgs; i++) {
    const char *topic = query->args[i];
    if (!has_topic(chat, topic)) return false;
  }
  return true;
}
/** return next match for query in chat, starting after lastMatch if
 *  not NULL.  Returns NULL if no match.
 */
MsgInfo *
query_chat(const Chat *chat, const MsgArgs *query, const MsgInfo *lastMatch)
{
  const Chat* lastMatch1 = (const Chat *)lastMatch;
  for (Chat *p = (lastMatch1) ? lastMatch1->next : chat->next;
       p != NULL;
       p = p->next) {
    if (chat_match(p, query)) return &p->info;
  }
  return NULL;
}

bool
is_known_room(const Chat *chat, const char* room)
{
  for (const Chat *p = chat->next; p != NULL; p = p->next) {
    if (strcasecmp(p->info.room, room) == 0) return true;
  }
  return false;
}

bool
is_known_topic(const Chat *chat, const char *topic)
{
  for (const Chat *p = chat->next; p != NULL; p = p->next) {
    if (has_topic(p, topic)) return true;
  }
  return false;
}
