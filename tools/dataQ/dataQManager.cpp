#include "dataQManager.h"

DataQManager::DataQManager(){
	recvQLocks = new std::map<int,pthread_mutex_t>();
	recvQMap = new std::map<int,DataQ*>();
}

void DataQManager::initQ(){
	sendQ = new DataQ();
	localQ = new DataQ();
}

int DataQManager::getSendQSize(){
	//pthread_mutex_lock(&sendQLock);
	//printf("sendQlock address : %p\n",&sendQLock);
	int size = sendQ->getSize();
	//pthread_mutex_unlock(&sendQLock);
	return size;
}

int DataQManager::getRecvQSize(int sock){
	//pthread_mutex_lock(&((*recvQLocks)[sock]));
	//printf("recvQLock address : %p\n",recvQLocks[sock]);
	int size = (*recvQMap)[sock]->getSize();
	//pthread_mutex_unlock(&((*recvQLocks)[sock]));
	return size; 
}

int DataQManager::getLocalQSize(){
	//pthread_mutex_lock(&localQLock);
	//printf("localQlock address : %p\n",&localQLock);
	int size = localQ->getSize();
	//pthread_mutex_unlock(&localQLock);
	return size;
}

DataQElem* DataQManager::getRecvQElement(int sock_d){
	//pthread_mutex_lock(&((*recvQLocks)[sock_d]));
	DataQ* recvQ = (*recvQMap)[sock_d];
	DataQElem* ret = recvQ->consume();
	//pthread_mutex_unlock(&((*recvQLocks)[sock_d]));
	return ret;
}

DataQElem* DataQManager::getSendQElement(){
	//pthread_mutex_lock(&sendQLock);
	DataQElem* ret = sendQ->consume();
	//pthread_mutex_unlock(&sendQLock);
	return ret;
}

DataQElem* DataQManager::getLocalQElement(){
	//pthread_mutex_lock(&localQLock);
	DataQElem* ret = localQ->consume();
	//pthread_mutex_unlock(&localQLock);
	return ret;
}

void DataQManager::insertElementToRecvQ(int sock_d,DataQElem* elem){
	//pthread_mutex_lock(&((*recvQLocks)[sock_d]));
	DataQ* recvQ = (*recvQMap)[sock_d];
	recvQ->produce(elem);
	//pthread_mutex_unlock(&((*recvQLocks)[sock_d]));
}

void DataQManager::insertElementToSendQ(DataQElem* elem){
	//pthread_mutex_lock(&sendQLock);
	//printf("address of elem : %p\n",elem);
	sendQ->produce(elem);
	//pthread_mutex_unlock(&sendQLock);
}

void DataQManager::insertElementToLocalQ(DataQElem* elem){
	//pthread_mutex_lock(&localQLock);
	//printf("insert localQ, address of elem : %p\n",elem);
	localQ->produce(elem);
	//pthread_mutex_unlock(&localQLock);
}

void DataQManager::addRecvQ(int sock_d){
	//pthread_mutex_lock(&recvQAddLock);
	//pthread_mutex_t* temp = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	//pthread_mutex_t newLock;
	//*temp = newLock;
	DataQ* newRecvQ = new DataQ();
	(*recvQMap)[sock_d] = newRecvQ;
	//recvQLocks[sock_d] = *temp;
	//pthread_mutex_unlock(&recvQAddLock);
}
