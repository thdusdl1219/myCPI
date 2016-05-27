#ifndef ESPERANTO_UTILS_H
#define ESPERANTO_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

const char* getRuntimeVariable(const char* VarName, const char* Condition);
void broadcastSend(char* data, int size, int port);
void getIP(char*);
int getPort();

#endif
