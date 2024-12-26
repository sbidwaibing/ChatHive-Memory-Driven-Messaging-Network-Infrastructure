#ifndef SERVER_H_
#define SERVER_H_

#include <stdio.h>

//server specific declarations

void do_server(const char *dbPath, int inPipe[2], int outPipe[2]);

#endif //#ifndef SERVER_H_
