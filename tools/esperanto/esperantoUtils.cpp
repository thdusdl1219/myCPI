#include "esperantoUtils.h"
#include <sys/types.h>
#include <ifaddrs.h>

const char* getRuntimeVariable(const char* VarName, const char* Condition){
  return "Hue";
}

void broadcastSend(char* data, int size,int port){
	int sockfd;
	int clilen;
	//int state;
	
	struct sockaddr_in serveraddr;

	clilen = sizeof(serveraddr);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	printf("DEBUG :: udp broadcast send socket is %d\n",sockfd);
	memset(&serveraddr,0,sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr("255.255.255.255");
	serveraddr.sin_port = htons(port);

	int sockopt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &sockopt, sizeof(sockopt)) == -1) {
		/* Handle error */
		printf("DEBUG :: setsockopt error\n");
	}
	printf("DEBUG :: broadcast send, size = %d, port = %d\n",size,port);
	sendto(sockfd, (void *)data, size, 0, (struct sockaddr *)&serveraddr, clilen);
	//recvfrom(sockfd, (void *)&add_data, sizeof(add_data), 0, NULL, NULL); 
	
	//printf("d + %d = %d\n", add_data.a, add_data.b, add_data.sum);
	close(sockfd);
} 

void getIP(char* addressBuffer){
	struct ifaddrs * ifAddrStruct=NULL;
	struct ifaddrs * ifa=NULL;
	void * tmpAddrPtr=NULL;

	getifaddrs(&ifAddrStruct);

	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr) {
			continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
			// is a valid IP4 Address
			tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			if(strcmp(addressBuffer,"127.0.0.1") !=0){
				return;
			}
		}     
	}
}

int getPort(){
	int port = 0;
	return port;
}
//
//
// network functions 
//
//




