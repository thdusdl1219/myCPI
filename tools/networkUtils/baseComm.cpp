#include "baseComm.h"

void CommHandler::disconnect(int sock_d){
	close(sock_d);
	printf("DEBUG :: socket %d is disconnected\n",sock_d);
}
/*
void CommHandler::send(char* data, int size){
	printf("DEBUG :: sendData is started\n");
	int sendedSize = 0;
	int tempSize = 0;
	while(sendedSize != size){
		tempSize = (int)write(socket_d,data,size);
		sendedSize += tempSize;
		data += tempSize;
		size -= tempSize;
	}
	printf("DEBUG :: sendData is complete\n");
}

void CommHandler::recv(char* data, int size){
	printf("DEBUG :: recvData is started\n");
	int readedSize = 0;
	int tempSize = 0;
	data = (char*)malloc(size);
	while(readedSize != size){
		tempSize = (int)read(socket_d,data,size);
		readedSize += tempSize;
		data += tempSize;
		size -= tempSize;
	}
	printf("DEBUG :: recvData is complete\n");
}
*/
int CommHandler::getSocket(){
	return -1;
}
