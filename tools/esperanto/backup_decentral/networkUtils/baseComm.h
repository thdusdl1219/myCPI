#ifndef BASE_COMM_H
#define BASE_COMM_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

class CommHandler {
public:
	void disconnect(int sock_d);
	int getSocket();
private:
	int sock_d;
};

#endif
