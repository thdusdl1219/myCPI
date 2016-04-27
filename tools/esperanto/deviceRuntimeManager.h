#ifndef DEVICE_RUNTIME_MANAGER_H
#define DEVICE_RUNTIME_MANAGER_H

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <map>
#include <pthread.h>
#include <unistd.h>
#include <unordered_map>

using namespace std;

struct args_info{
  void* args;
  int size;
};

class DeviceRuntimeManager{
public:
	DeviceRuntimeManager();
	int getJobID();
	int getRecvSocket(){
		return recvSocket;
	}
	int getSendSocket(){
		return sendSocket;
	}
	void setSockets(int sendSocket, int recvSocket);

  void insertArgsInfo(int rc_id,void* buf, int size);
	void insertRunningJob(int jobID, int functionID);
	void insertConsumeWait(int jobID);
	void insertJobIDMapping(int localJobID, int sourceJobID);
	void insertReturnValue(int jobID, void* ret);	
	void insertArgs(int jobID, void* args);

	void onValueReturn(int jobID);

	void deleteRunningJob(int jobID);
	void deleteConsumeWait(int jobID);
	void deleteJobIDMapping(int localJobID);

	void* getReturnValue(int jobID);
	void* getArgs(int jobID);
	int getRunningJobFID(int jobID);
  int getSourceJobID(int localJobID);
  void* getArgsOfRC(int rc_id);
  int getArgsTotalSize(int rc_id);

	bool checkConsumeWait(int jobID);
		
	
	//unordered_map<int,void*> argsTable;
private:
	//pthread_mutex_t* jobIDLock;
		//PTHREAD_MUTEX_INITIALIZER;
	
		pthread_mutex_t* argsLock;
	pthread_mutex_t* consumeTableLock;
	pthread_mutex_t* runningJobsLock;
	pthread_mutex_t* jobIDMapLock;
	pthread_mutex_t* returnValueTableLock;
	pthread_mutex_t* jobIDLock;
  pthread_mutex_t* argsListLock;

  std::map<int,std::vector<struct args_info>> args_list;
	int sendSocket;
	int recvSocket;
	int jobID;
};
#endif
