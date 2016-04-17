#include "deviceRuntimeManager.h"
#include <stdio.h>
map<int,bool>* consumeTable = new map<int,bool>(); // <jobID, isReturnValueArrived>
char dumm1[1000];
map<int,int>* runningJobs = new map<int,int>(); // <jobID, functionID>

char dumm5[1000];
map<int,int>* jobIDMap = new map<int,int>(); // <local job id, source job id>

char dumm4[1000];
map<int,void*>* argsTable = new map<int,void*>();

char dumm3[1000];
map<int,void*>* returnValueTable = new map<int,void*>(); //<jobID, returnValue>

char dumm2[1000];


DeviceRuntimeManager::DeviceRuntimeManager(){
	jobID = 0;
	jobIDLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	consumeTableLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	runningJobsLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	returnValueTableLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	argsLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	jobIDMapLock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(jobIDLock,NULL);
	pthread_mutex_init(jobIDMapLock,NULL);
	pthread_mutex_init(consumeTableLock,NULL);
	pthread_mutex_init(argsLock,NULL);
	pthread_mutex_init(runningJobsLock,NULL);
	pthread_mutex_init(returnValueTableLock,NULL);
}

int DeviceRuntimeManager::getJobID(){
	int tempJobID;
	pthread_mutex_lock(jobIDLock);
	tempJobID = jobID++;
	pthread_mutex_unlock(jobIDLock);
	return tempJobID;
}

void DeviceRuntimeManager::setSockets(int sSocket, int rSocket){
	sendSocket = sSocket;
	recvSocket = rSocket;
}

void DeviceRuntimeManager::insertRunningJob(int jobID, int functionID){
	//printf("inserted job id : %d, %d\n",jobID, functionID);
	/*if(pthread_mutex_trylock(runningJobsLock) != 0){
		printf("already locked!!!!!!!!!!!!!!!!!!! : %p\n",runningJobsLock);
		fflush(stdout);
	}*/
	pthread_mutex_lock(runningJobsLock);
	//printf("insert running job before\n");
	fflush(stdout);
	(*runningJobs)[jobID] = functionID;
	//printf("insert running job after\n");
	fflush(stdout);
	pthread_mutex_unlock(runningJobsLock);
}

void DeviceRuntimeManager::insertConsumeWait(int jobID){
	pthread_mutex_lock(consumeTableLock);
	(*consumeTable)[jobID] = false;
	pthread_mutex_unlock(consumeTableLock);
}

void DeviceRuntimeManager::insertJobIDMapping(int localJobID, int sourceJobID){
	pthread_mutex_lock(jobIDMapLock);
	(*jobIDMap)[localJobID] = sourceJobID;
	pthread_mutex_unlock(jobIDMapLock);
}

void DeviceRuntimeManager::insertReturnValue(int jobID, void* ret){
	pthread_mutex_lock(returnValueTableLock);
	(*returnValueTable)[jobID] = ret;
	pthread_mutex_unlock(returnValueTableLock);	
}

void DeviceRuntimeManager::insertArgs(int jobID, void* args){
	pthread_mutex_lock(argsLock);
	//argsTable[-3] = nullptr;
	//printf("DRM :: insert args  : job id = %d, size = %d\n\n",jobID,(*argsTable).size());
	//std::pair<int,void*>* temp = new std::pair<int,void*>();
	//*temp = std::pair<int,void*>(jobID,args);
	//hexdump2("argsTable",&argsTable,100);
	//if(argsTable.find(jobID) != argsTable.end())
	//	argsTable.insert(*temp);
	//else
	//	printf("??\n");
//hexdump2("argsTable2",&argsTable,100);
	//argsTable[jobID] = args;
	(*argsTable)[jobID] = args;
	//printf("DRM :: insert args  : job id = %d, size = %d\n\n",jobID,(*argsTable).size());
	pthread_mutex_unlock(argsLock);
}

void DeviceRuntimeManager::onValueReturn(int jobID){
	pthread_mutex_lock(consumeTableLock);
	(*consumeTable)[jobID] = true;
	pthread_mutex_unlock(consumeTableLock);
}

void DeviceRuntimeManager::deleteRunningJob(int jobID){
	//printf("DEBUG :: deleteRunningJob\n");
	pthread_mutex_lock(runningJobsLock);	
	//printf("DEBUG :: delete running job inside\n");
	(*runningJobs).erase(jobID);
	pthread_mutex_unlock(runningJobsLock);
}

void DeviceRuntimeManager::deleteConsumeWait(int jobID){
	pthread_mutex_lock(consumeTableLock);
	(*consumeTable).erase(jobID);
	pthread_mutex_unlock(consumeTableLock);
}

void DeviceRuntimeManager::deleteJobIDMapping(int localJobID){
	pthread_mutex_lock(jobIDMapLock);
	(*jobIDMap).erase(localJobID);
	pthread_mutex_unlock(jobIDMapLock);
}

void* DeviceRuntimeManager::getArgs(int jobID){
	//pthread_mutex_lock(argsLock);
	
	//printf("DRM :: getArgs, job id = %d, table size = %d\n\n",jobID,(*argsTable).size());
	void* ret = (*argsTable)[jobID];
	//printf("DRM :: getArgs, job id = %d, value = %p, table size = %d\n\n",jobID,(argsTable)[jobID],argsTable.size());

	if((*argsTable).find(jobID) != (*argsTable).end())
		(*argsTable).erase((*argsTable).find(jobID));
	else
		printf("DRM :: getArgs error!! - no argument\n");
	//pthread_mutex_unlock(argsLock);
	return ret;
}

void* DeviceRuntimeManager::getReturnValue(int jobID){
	pthread_mutex_lock(returnValueTableLock);
	void* ret = (*returnValueTable)[jobID];
	(*returnValueTable).erase(jobID);
	pthread_mutex_unlock(returnValueTableLock);	
	
	return ret;
}

int DeviceRuntimeManager::getRunningJobFID(int jobID){
	//printf("DEBUG :: getRunningJobFID\n");
	pthread_mutex_lock(runningJobsLock);	
	//printf("DEBUG :: get running job fid inside\n");
	int ret = (*runningJobs)[jobID];
	pthread_mutex_unlock(runningJobsLock);
	return ret;
}

int DeviceRuntimeManager::getSourceJobID(int localJobID){
	pthread_mutex_lock(jobIDMapLock);
	int sourceJID = (*jobIDMap)[localJobID];
	pthread_mutex_unlock(jobIDMapLock);
	return sourceJID;
}

bool DeviceRuntimeManager::checkConsumeWait(int jobID){
	pthread_mutex_lock(consumeTableLock);
	bool ret = (*consumeTable)[jobID];
	pthread_mutex_unlock(consumeTableLock);
	return ret;
}
