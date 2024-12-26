#ifndef SERVER_LOOP_H_
#define SERVER_LOOP_H_

#include <chat-db.h>

#include <stdio.h>

void server_loop(ChatDb *chatDb, FILE *in, FILE *out);

#endif //#ifndef SERVER_LOOP_H_
