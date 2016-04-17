#ifndef ESPERANTO_UTILS_H
#define ESPERANTO_UTILS_H

#include "networkUtils/baseComm.h"
#include "networkUtils/udpComm.h"
#include "networkUtils/tcpComm.h"

void broadcastSend(char* data, int size, int port);
void getIP(char*);
int getPort();

#endif
