#include "gatewayRuntimeManager.h"

GatewayRuntimeManager::GatewayRuntimeManager(){
	jobID = 0;
}

void GatewayRuntimeManager::alertReady(){
	map<int,SocketInfo>::iterator it;
	for(it = socketMap.begin();it != socketMap.end(); it++){
		char ack =1;
		write(it->second.outSocket,&ack,1);
		printf("DEBUG :: send ack to socket %d\n",it->second.outSocket);
	}  
}

bool GatewayRuntimeManager::isLocalFunction(int functionID){
	bool ret = true;
	pthread_mutex_lock(&functionRTableLock);
	if(functionRoutingTable.find(functionID) != functionRoutingTable.end())
		ret = false;
	pthread_mutex_unlock(&functionRTableLock);
	return ret;
}

bool GatewayRuntimeManager::isLocalReturn(int jobID){
	bool ret = true;
	pthread_mutex_lock(&jobMapLock);
	if(jobMap.find(jobID) != jobMap.end())
		ret = false;
	pthread_mutex_unlock(&jobMapLock);
	return ret;
}

void GatewayRuntimeManager::insertJobMapping(int localJID, int outSocket, int sourceJID){
	pthread_mutex_lock(&jobMapLock);
	jobMap[localJID].outSocket = outSocket;
	jobMap[localJID].sourceJobID = sourceJID;
	printf("\nDEBUG ::ljid = %d, os = %d, sjid = %d\n\n",localJID, outSocket,sourceJID);
	pthread_mutex_unlock(&jobMapLock);
}

void GatewayRuntimeManager::insertInSocket(int deviceID, int inSocket){
	pthread_mutex_lock(&socketMapLock);
	socketMap[deviceID].inSocket = inSocket;
	pthread_mutex_unlock(&socketMapLock);	
}

void GatewayRuntimeManager::insertOutSocket(int deviceID, int outSocket){
	pthread_mutex_lock(&socketMapLock);
	socketMap[deviceID].outSocket = outSocket;
	pthread_mutex_unlock(&socketMapLock);	
}

void GatewayRuntimeManager::insertRouting(int functionID, int sock_d){
	pthread_mutex_lock(&functionRTableLock);
	functionRoutingTable[functionID] = sock_d;
	pthread_mutex_unlock(&functionRTableLock);
}

void GatewayRuntimeManager::insertRunningJob(int jobID, int functionID){
	pthread_mutex_lock(&runningJobsLock);
	runningJobs[jobID] = functionID;
	pthread_mutex_unlock(&runningJobsLock);
}

void GatewayRuntimeManager::insertConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable[jobID] = false;
	pthread_mutex_unlock(&consumeTableLock);
}

void GatewayRuntimeManager::insertReturnValue(int jobID, void* ret){
	pthread_mutex_lock(&returnValueTableLock);
	returnValueTable[jobID] = ret;
	pthread_mutex_unlock(&returnValueTableLock);	
}

void GatewayRuntimeManager::insertArgs(int jobID, void* args){
	printf("DEBUG :: insert args is started\n");
	//pthread_mutex_lock(&argsLock);
	argsTable[jobID] = args;
	//pthread_mutex_unlock(&argsLock);
	printf("DEBUG :: insert args is ended\n");
}

void GatewayRuntimeManager::onValueReturn(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable[jobID] = true;
	pthread_mutex_unlock(&consumeTableLock);
}

void GatewayRuntimeManager::deleteRunningJob(int jobID){
	pthread_mutex_lock(&runningJobsLock);	
	runningJobs.erase(jobID);
	pthread_mutex_unlock(&runningJobsLock);
}

void GatewayRuntimeManager::deleteConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable.erase(jobID);
	pthread_mutex_unlock(&consumeTableLock);
	pthread_mutex_lock(&returnValueTableLock);
	returnValueTable.erase(jobID);
	pthread_mutex_unlock(&returnValueTableLock);	

}

void GatewayRuntimeManager::deleteJobMapping(int localJID){
	pthread_mutex_lock(&jobMapLock);
	jobMap.erase(localJID);
	pthread_mutex_unlock(&jobMapLock);
}

/*void GatewayRuntimeManager::insertFuncJobMatching(int jobID, int functionID){
	funcJobMatchingTable[jobID] = functionID;
}

void GatewayRuntimeManager::deleteFuncJobMatching(int jobID){
	funcJobMatchingTable.erase(jobID);
}*/

void* GatewayRuntimeManager::getArgs(int jobID){
	printf("DEBUG :: get args is started\n");
	//pthread_mutex_lock(&argsLock);
	void* ret = argsTable[jobID];
	argsTable.erase(jobID);
	//pthread_mutex_unlock(&argsLock);
	printf("DEBUG :: get args is ended\n");
	return ret;
}

void* GatewayRuntimeManager::getReturnValue(int jobID){
	pthread_mutex_lock(&returnValueTableLock);
	void* ret = returnValueTable[jobID];
	returnValueTable.erase(jobID);
	pthread_mutex_unlock(&returnValueTableLock);	
	//pthread_mutex_lock(&consumeTableLock);
	//consumeTable.erase(jobID);
	//pthread_mutex_unlock(&consumeTableLock);
	return ret;
}

int GatewayRuntimeManager::getRunningJobFID(int jobID){
	pthread_mutex_lock(&runningJobsLock);	
	int ret = runningJobs[jobID];
	pthread_mutex_unlock(&runningJobsLock);
	return ret;
}

bool GatewayRuntimeManager::checkConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	bool ret = consumeTable[jobID];
	pthread_mutex_unlock(&consumeTableLock);
	return ret;
}


int GatewayRuntimeManager::getFuncDest(int functionID){
	pthread_mutex_lock(&functionRTableLock);
	int outSocket = functionRoutingTable[functionID];	
	pthread_mutex_unlock(&functionRTableLock);
	return outSocket;
}

int GatewayRuntimeManager::getInSocket(int DID){
	pthread_mutex_lock(&socketMapLock);
	int ret = socketMap[DID].inSocket;
	pthread_mutex_unlock(&socketMapLock);
	return ret;	
}

int GatewayRuntimeManager::getOutSocketByDeviceID(int DID){
	pthread_mutex_lock(&socketMapLock);
	int ret = socketMap[DID].outSocket;
	pthread_mutex_unlock(&socketMapLock);
	return ret;	
}

int GatewayRuntimeManager::getSourceJobID(int localJobID){
	pthread_mutex_lock(&jobMapLock);
	int ret = jobMap[localJobID].sourceJobID;
	pthread_mutex_unlock(&jobMapLock);
	return ret;
}

int GatewayRuntimeManager::getOutSocket(int localJobID){
	pthread_mutex_lock(&jobMapLock);
	int ret = jobMap[localJobID].outSocket;
	pthread_mutex_unlock(&jobMapLock);
	return ret;
}

int GatewayRuntimeManager::getJID(){
	return jobID++;
}

int GatewayRuntimeManager::getDID(){
	return deviceID++;
}

void GatewayRuntimeManager::printRT(){
	printf("----RoutingTable----\n");
	map<int,int>::iterator it;
	for(it = functionRoutingTable.begin(); it!=functionRoutingTable.end();it++){
		printf("functionID : %d ==> outSocket : %d\n",it->first,it->second);
	}
}

void GatewayRuntimeManager::printST(int deviceID){
	printf("----SocketMap----\n");
	printf("device %d ==> inSocket : %d, outSocket : %d\n",deviceID,socketMap[deviceID].inSocket,socketMap[deviceID].outSocket);	
}
