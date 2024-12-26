#ifndef SERVER_LOOP_H_
#define SERVER_LOOP_H_

#include "common.h"

#include <chat-db.h>

void server_loop(ChatDb *chatDb, Shm *shm);

#endif //#ifndef SERVER_LOOP_H_
