#ifndef DATA_Q_MANAGER_H
#define DATA_Q_MANAGER_H

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <map>
#include <pthread.h>
#include "dataQ.h"

using namespace std;

class DataQManager{
public:
	DataQManager();
	void initQ();
	void addRecvQ(int sock_d);	

	int getSendQSize();
	int getRecvQSize(int sock);
	int getLocalQSize();	
	
	DataQElem* getRecvQElement(int sock_d);
	DataQElem* getSendQElement();
	DataQElem* getLocalQElement();

	void insertElementToRecvQ(int sock_d,DataQElem*);
	void insertElementToSendQ(DataQElem*);
	void insertElementToLocalQ(DataQElem*);
private:
	pthread_mutex_t recvQAddLock;
	std::map<int,pthread_mutex_t>* recvQLocks;
	pthread_mutex_t sendQLock;
	pthread_mutex_t localQLock;
	std::map<int,DataQ*>* recvQMap;
	DataQ* sendQ;
	DataQ* localQ;
};
#endif
