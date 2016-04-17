#include "tcpComm.h"

int TCPCommHandler::getSockDesc(){
	return sock_d;
}

void TCPCommHandler::createServerSocket(int port){
    int sock_d = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    if(sock_d < 0)
    {
        printf("\nDEBUG :: Could not create socket\n");
        return;
    }
    printf("\nDEBUG :: TCP Socket has been created : %d\n",sock_d);
    this->sock_d = sock_d;

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_d, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nDEBUG :: Could not bind tcp socket\n");
        return;
    }

    if(listen(sock_d, SOMAXCONN) == -1)
	printf("DEBUG :: listen error\n");
	 // NOTE :: backlog is set to the MAXIMUM value
    printf("DEBUG :: Socket create is completed\n");
}

void TCPCommHandler::createClientSocket(char* ip, int port){
    int sock_d = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    if (sock_d < 0)
    {
        printf("\nDEBUG :: Could not create socket\n");	
	return;
    }
    printf("\nDEBUG :: TCP Socket has been created\n");
    this->sock_d = sock_d;
    
    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); 

    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)
    {   
        printf("\nDEBUG :: inet_pton error occured\n");
        return;
    }   

    if(connect(sock_d, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {   
       printf("\nDEBUG :: Connection Failed\n");
       return;
    }  
/*	struct timeval t;    
 t.tv_sec = 0;
 setsockopt(
      sock_d,     // Socket descriptor
      SOL_SOCKET, // To manipulate options at the sockets API level
      SO_RCVTIMEO,// Specify the receiving or sending timeouts 
      (void*)(&t), // option values
      sizeof(t) 
  ); */  

    printf("DEBUG :: createClientSocket is completed\n"); 
}

void TCPCommHandler::send(char* data, int size){
	printf("DEBUG :: sendData is started\n");
	int sendedSize = 0;
	int tempSize = 0;
	while(sendedSize != size){
		tempSize = (int)write(sock_d,data,size);
		sendedSize += tempSize;
		data += tempSize;
		size -= tempSize;
	}
	printf("DEBUG :: sendData is complete\n");

}

void TCPCommHandler::recv(char* data, int size){
	printf("DEBUG :: recvData is started\n");
	int readedSize = 0;
	int tempSize = 0;
	while(readedSize != size){
		tempSize = (int)read(sock_d,data,size);
		readedSize += tempSize;
		data += tempSize;
		size -= tempSize;
	}
	printf("DEBUG :: recvData is complete\n");

}

