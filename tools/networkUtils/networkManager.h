#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <string.h>
#include "log.h"

class NetworkManager{
	public:
		NetworkManager();
		int createClntConnection(char* ip, int port);
		int createHostConnection(int port);
		void send(int devID, char* data, int size);
		void recv(int devID, char* data, int size);
		void deleteConnection(int devID);
	private:
		std::map<int,int> socketMap; // deviceID, socket
		int deviceID;	
};


#endif
