#include "runtimeManager.h"

RuntimeManager::RuntimeManager(){
	jobID = 0;
}

void RuntimeManager::alertReady(){
	map<int,SocketInfo>::iterator it;
	for(it = socketMap.begin();it != socketMap.end(); it++){
		char ack =1;
		write(it->second.outSocket,&ack,1);
		printf("DEBUG :: send ack to socket %d\n",it->second.outSocket);
	}  
}

bool RuntimeManager::isLocalFunction(int functionID){
	bool ret = true;
	pthread_mutex_lock(&functionRTableLock);
	if(functionRoutingTable.find(functionID) != functionRoutingTable.end())
		ret = false;
	pthread_mutex_unlock(&functionRTableLock);
	return ret;
}

bool RuntimeManager::isLocalReturn(int jobID){
	bool ret = true;
	pthread_mutex_lock(&jobMapLock);
	if(jobMap.find(jobID) != jobMap.end())
		ret = false;
	pthread_mutex_unlock(&jobMapLock);
	return ret;
}

void RuntimeManager::insertJobMapping(int localJID, int outSocket, int sourceJID){
	pthread_mutex_lock(&jobMapLock);
	jobMap[localJID].outSocket = outSocket;
	jobMap[localJID].sourceJobID = sourceJID;
	printf("\nDEBUG ::ljid = %d, os = %d, sjid = %d\n\n",localJID, outSocket,sourceJID);
	pthread_mutex_unlock(&jobMapLock);
}

void RuntimeManager::insertInSocket(int deviceID, int inSocket){
	pthread_mutex_lock(&socketMapLock);
	socketMap[deviceID].inSocket = inSocket;
	pthread_mutex_unlock(&socketMapLock);	
}

void RuntimeManager::insertOutSocket(int deviceID, int outSocket){
	pthread_mutex_lock(&socketMapLock);
	socketMap[deviceID].outSocket = outSocket;
	pthread_mutex_unlock(&socketMapLock);	
}

void RuntimeManager::insertRouting(int functionID, int sock_d){
	pthread_mutex_lock(&functionRTableLock);
	functionRoutingTable[functionID] = sock_d;
	pthread_mutex_unlock(&functionRTableLock);
}

void RuntimeManager::insertRunningJob(int jobID, int functionID){
	pthread_mutex_lock(&runningJobsLock);
	runningJobs[jobID] = functionID;
	pthread_mutex_unlock(&runningJobsLock);
}

void RuntimeManager::insertConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable[jobID] = false;
	pthread_mutex_unlock(&consumeTableLock);
}

void RuntimeManager::insertReturnValue(int jobID, void* ret){
	pthread_mutex_lock(&returnValueTableLock);
	returnValueTable[jobID] = ret;
	pthread_mutex_unlock(&returnValueTableLock);	
}

void RuntimeManager::insertArgs(int jobID, void* args){
	printf("DEBUG :: insert args is started\n");
	//pthread_mutex_lock(&argsLock);
	argsTable[jobID] = args;
	//pthread_mutex_unlock(&argsLock);
	printf("DEBUG :: insert args is ended\n");
}

void RuntimeManager::onValueReturn(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable[jobID] = true;
	pthread_mutex_unlock(&consumeTableLock);
}

void RuntimeManager::deleteRunningJob(int jobID){
	pthread_mutex_lock(&runningJobsLock);	
	runningJobs.erase(jobID);
	pthread_mutex_unlock(&runningJobsLock);
}

void RuntimeManager::deleteConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable.erase(jobID);
	pthread_mutex_unlock(&consumeTableLock);
	pthread_mutex_lock(&returnValueTableLock);
	returnValueTable.erase(jobID);
	pthread_mutex_unlock(&returnValueTableLock);	

}

void RuntimeManager::deleteJobMapping(int localJID){
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

void* RuntimeManager::getArgs(int jobID){
	printf("DEBUG :: get args is started\n");
	//pthread_mutex_lock(&argsLock);
	void* ret = argsTable[jobID];
	argsTable.erase(jobID);
	//pthread_mutex_unlock(&argsLock);
	printf("DEBUG :: get args is ended\n");
	return ret;
}

void* RuntimeManager::getReturnValue(int jobID){
	pthread_mutex_lock(&returnValueTableLock);
	void* ret = returnValueTable[jobID];
	returnValueTable.erase(jobID);
	pthread_mutex_unlock(&returnValueTableLock);	
	//pthread_mutex_lock(&consumeTableLock);
	//consumeTable.erase(jobID);
	//pthread_mutex_unlock(&consumeTableLock);
	return ret;
}

int RuntimeManager::getRunningJobFID(int jobID){
	pthread_mutex_lock(&runningJobsLock);	
	int ret = runningJobs[jobID];
	pthread_mutex_unlock(&runningJobsLock);
	return ret;
}

bool RuntimeManager::checkConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	bool ret = consumeTable[jobID];
	pthread_mutex_unlock(&consumeTableLock);
	return ret;
}


int RuntimeManager::getFuncDest(int functionID){
	pthread_mutex_lock(&functionRTableLock);
	int outSocket = functionRoutingTable[functionID];	
	pthread_mutex_unlock(&functionRTableLock);
	return outSocket;
}

int RuntimeManager::getInSocket(int DID){
	pthread_mutex_lock(&socketMapLock);
	int ret = socketMap[DID].inSocket;
	pthread_mutex_unlock(&socketMapLock);
	return ret;	
}

int RuntimeManager::getOutSocketByDeviceID(int DID){
	pthread_mutex_lock(&socketMapLock);
	int ret = socketMap[DID].outSocket;
	pthread_mutex_unlock(&socketMapLock);
	return ret;	
}

int RuntimeManager::getSourceJobID(int localJobID){
	pthread_mutex_lock(&jobMapLock);
	int ret = jobMap[localJobID].sourceJobID;
	pthread_mutex_unlock(&jobMapLock);
	return ret;
}

int RuntimeManager::getOutSocket(int localJobID){
	pthread_mutex_lock(&jobMapLock);
	int ret = jobMap[localJobID].outSocket;
	pthread_mutex_unlock(&jobMapLock);
	return ret;
}

int RuntimeManager::getJID(){
	return jobID++;
}

int RuntimeManager::getDID(){
	return deviceID++;
}

void RuntimeManager::printRT(){
	printf("----RoutingTable----\n");
	map<int,int>::iterator it;
	for(it = functionRoutingTable.begin(); it!=functionRoutingTable.end();it++){
		printf("functionID : %d ==> outSocket : %d\n",it->first,it->second);
	}
}

void RuntimeManager::printST(int deviceID){
	printf("----SocketMap----\n");
	printf("device %d ==> inSocket : %d, outSocket : %d\n",deviceID,socketMap[deviceID].inSocket,socketMap[deviceID].outSocket);	
}
