#ifndef UDP_COMM_H
#define UDP_COMM_H

#include "baseComm.h"

class UDPCommHandler{
public:
	void createServerSocket(int port);
	void createClientSocket(char* ip, int port);
	void send(char* data, int size);
	void recv(char* data, int size);
private:
	struct sockaddr_in saddr;
  int sock_d;
};

#endif
