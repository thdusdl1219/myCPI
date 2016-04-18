#include "networkManager.h"

NetworkManager::NetworkManager(){
	deviceID = 0;
}

int NetworkManager::createClntConnection(char* ip, int port){
	int sock;
	int returnID = 0;
	struct sockaddr_in server_addr;
	
	sock = socket(PF_INET,SOCK_STREAM,0);
	if(sock == -1)
		LOG("socket creation error\n");
	
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(ip);

	if(connect(sock,(struct sockaddr*)&server_addr,sizeof(server_addr)))
		LOG("socket connect error\n");
	
	socketMap[deviceID] = sock;
	returnID = deviceID;
	deviceID++;
	return returnID;
}

int NetworkManager::createHostConnection(int port){
	int sock;
	int acceptedSock;
	int addr_size;
	int returnID;

	struct sockaddr_in server_addr;
	struct sockaddr_in accepted_addr;

	sock = socket(PF_INET,SOCK_STREAM,0);
	if(sock == -1)
		LOG("socket creation error\n");
	
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sock,(struct sockaddr*)&server_addr,sizeof(server_addr)) == -1)
		LOG("socket bind error\n");

	if(listen(sock,5) == -1)
		LOG("socket listen error\n");

	addr_size = sizeof(accepted_addr);
	acceptedSock = accept(sock,(struct sockaddr*)&accepted_addr,(socklen_t*)&addr_size);

	if(acceptedSock == -1)
		LOG("socket accept error\n");

	socketMap[deviceID] = acceptedSock;
	returnID = deviceID;
	deviceID;
	return returnID;
		
}

void NetworkManager::send(int devID, char* data, int size){
	int tempSize = 0;
	int sock = socketMap[devID];

	while(size != 0){
		tempSize = write(sock,data,size);
		data += tempSize;
		size -= tempSize;
	}
}


void NetworkManager::recv(int devID, char* data, int size){
	int tempSize = 0;
	int sock = socketMap[devID];

	while(size != 0){
		tempSize = read(sock,data,size);
		data += tempSize;
		size -= tempSize;
	}
}

void NetworkManager::deleteConnection(int devID){
	int sock = socketMap[devID];
	close(sock);
	socketMap.erase(devID);
}
