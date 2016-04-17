#ifndef TCP_COMM_H
#define TCP_COMM_H

#include "baseComm.h"

class TCPCommHandler {
public:
	int getSockDesc();
	void send(char*, int);
	void recv(char*, int);
	void createServerSocket(int port);
	void createClientSocket(char* ip, int port);
private:
  int sock_d;
};

#endif
