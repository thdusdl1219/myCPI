#include "udpComm.h"

void UDPCommHandler::createServerSocket(int port){
	printf("DEBUG :: serverSocket is created, %d\n",port);
    int sock_d = socket(AF_INET, SOCK_DGRAM, 0);

    if(sock_d < 0)
    {
        printf("\nDEBUG :: Could not create socket\n");
        return;
    }
    printf("\nDEBUG :: UDP Socket has been created :%d\n",sock_d);
    this->sock_d = sock_d;
    memset(&saddr, '0', sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(port);

    if (bind(sock_d, (struct sockaddr*) &saddr, sizeof(saddr)) < 0)
    {
        printf("\nDEBUG :: Could not bind socket\n");
        return;
    }
    int sockopt = 1;
    if(setsockopt(sock_d,SOL_SOCKET,SO_BROADCAST, &sockopt,sizeof(sockopt)) == -1)
	printf("DEBUG :: setsocketopt error\n");
}

void UDPCommHandler::createClientSocket(char* ip, int port){
    int sock_d = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock_d < 0)
    {
        printf("\nDEBUG :: Could not create socket\n");	
	    return;
    }
    printf("\nDEBUG :: UDP Socket has been created\n");
    this->sock_d = sock_d;
    
    memset(&saddr, '0', sizeof(saddr)); 
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port); 

    if(inet_pton(AF_INET, ip, &saddr.sin_addr)<=0)
    {   
        printf("\nDEBUG :: inet_pton error occured\n");
        return;
    }
}

void UDPCommHandler::send(char* data, int size){
	int sendedSize = 0;
	int tempSize = 0;
	while(sendedSize != size){
		tempSize = sendto(sock_d, (void *)data, size, 0, (struct sockaddr *)&saddr, sizeof(saddr));
		sendedSize += tempSize;
		data += tempSize;
		size -= tempSize;
	}
	printf("DEBUG :: udp send is complete\n");	
}

void UDPCommHandler::recv(char* data, int size){
	printf("DEBUG :: recv is started\n");
	printf("DEBUG :: size = %d\n",size);
	printf("DEBUG :: recv socket is %d\n",sock_d);
	int readedSize = 0;
	int tempSize = 0;
	struct sockaddr_in clientaddr;
	int clilen = sizeof(clientaddr);
	while(size>0){
		tempSize = recvfrom(sock_d, (void*)data,size,0,(struct sockaddr *)&clientaddr, (socklen_t *)&clilen);
		readedSize += tempSize;
		data += tempSize;
		size -= tempSize;
	}
	printf("DEBUG :: udp recv is complete\n");
}

