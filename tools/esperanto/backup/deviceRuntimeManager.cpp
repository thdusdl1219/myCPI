#include "deviceRuntimeManager.h"

DeviceRuntimeManager::DeviceRuntimeManager(){
	jobID = 0;
}

int DeviceRuntimeManager::getJobID(){
	pthread_mutex_lock(&jobIDLock);
	int tempJobID = jobID++;
	pthread_mutex_unlock(&jobIDLock);
	return tempJobID;
}

void DeviceRuntimeManager::setSockets(int sSocket, int rSocket){
	sendSocket = sSocket;
	recvSocket = rSocket;
}

void DeviceRuntimeManager::insertRunningJob(int jobID, int functionID){
	pthread_mutex_lock(&runningJobsLock);
	runningJobs[jobID] = functionID;
	pthread_mutex_unlock(&runningJobsLock);
}

void DeviceRuntimeManager::insertConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable[jobID] = false;
	pthread_mutex_unlock(&consumeTableLock);
}

void DeviceRuntimeManager::insertJobIDMapping(int localJobID, int sourceJobID){
	pthread_mutex_lock(&jobIDMapLock);
	jobIDMap[localJobID] = sourceJobID;
	pthread_mutex_unlock(&jobIDMapLock);
}

void DeviceRuntimeManager::insertReturnValue(int jobID, void* ret){
	pthread_mutex_lock(&returnValueTableLock);
	returnValueTable[jobID] = ret;
	pthread_mutex_unlock(&returnValueTableLock);	
}

void DeviceRuntimeManager::insertArgs(int jobID, void* args){
	pthread_mutex_lock(&argsLock);
	argsTable[jobID] = args;
	pthread_mutex_unlock(&argsLock);
}

void DeviceRuntimeManager::onValueReturn(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable[jobID] = true;
	pthread_mutex_unlock(&consumeTableLock);
}

void DeviceRuntimeManager::deleteRunningJob(int jobID){
	pthread_mutex_lock(&runningJobsLock);	
	runningJobs.erase(jobID);
	pthread_mutex_unlock(&runningJobsLock);
}

void DeviceRuntimeManager::deleteConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	consumeTable.erase(jobID);
	pthread_mutex_unlock(&consumeTableLock);
}

void DeviceRuntimeManager::deleteJobIDMapping(int localJobID){
	pthread_mutex_lock(&jobIDMapLock);
	jobIDMap.erase(localJobID);
	pthread_mutex_unlock(&jobIDMapLock);
}

void* DeviceRuntimeManager::getArgs(int jobID){
	pthread_mutex_lock(&argsLock);
	void* ret = argsTable[jobID];
	argsTable.erase(jobID);
	pthread_mutex_unlock(&argsLock);
		return ret;
}

void* DeviceRuntimeManager::getReturnValue(int jobID){
	pthread_mutex_lock(&returnValueTableLock);
	void* ret = returnValueTable[jobID];
	returnValueTable.erase(jobID);
	pthread_mutex_unlock(&returnValueTableLock);	
	
	return ret;
}

int DeviceRuntimeManager::getRunningJobFID(int jobID){
	pthread_mutex_lock(&runningJobsLock);	
	int ret = runningJobs[jobID];
	pthread_mutex_unlock(&runningJobsLock);
	return ret;
}

int DeviceRuntimeManager::getSourceJobID(int localJobID){
	pthread_mutex_lock(&jobIDMapLock);
	int sourceJID = jobIDMap[localJobID];
	pthread_mutex_unlock(&jobIDMapLock);
	return sourceJID;
}

bool DeviceRuntimeManager::checkConsumeWait(int jobID){
	pthread_mutex_lock(&consumeTableLock);
	bool ret = consumeTable[jobID];
	pthread_mutex_unlock(&consumeTableLock);
	return ret;
}
