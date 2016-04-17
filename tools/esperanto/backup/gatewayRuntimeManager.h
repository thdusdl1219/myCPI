#ifndef GATEWAY_RUNTIME_MANAGER_H
#define GATEWAY_RUNTIME_MANAGER_H

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>
#include <map>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

enum Ownership{
	Owner,
	Shared,
	Modified,
};

typedef struct JobElement{
	int outSocket;
	int sourceJobID;
	
}JobElement;

typedef struct SocketInfo{
	int inSocket;
	int outSocket;
}SocketInfo;

typedef struct PageInfo{
	int pageID;
	Ownership ownership;
	int localVersion;
	int currentVersion;	
}PageInfo;

class GatewayRuntimeManager{
public:
	GatewayRuntimeManager();
	
	void alertReady();


	bool isLocalFunction(int functionID);
	bool isLocalReturn(int jobID);
	
	void insertJobMapping(int localJID, int outSocket ,int sourceJID);
	void insertInSocket(int deviceID, int outSocket);
	void insertOutSocket(int deviceID, int outSocket);
	void insertRouting(int functionID, int sock_d);
	/* from device runtime manager */
	void insertRunningJob(int jobID, int functionID);
	void insertConsumeWait(int jobID);
	void insertReturnValue(int jobID, void* ret);	
	void insertArgs(int jobID, void* args);

	void onValueReturn(int jobID);

	void deleteRunningJob(int jobID);
	void deleteConsumeWait(int jobID);
	/*from device runtime manager */


	void deleteJobMapping(int localJID);
	//void insertFuncJobMatching(int jobID, int functionID);
	//void deleteFuncJobMatching(int jobID);
	/* from device runtime manager */
	void* getReturnValue(int jobID);
	void* getArgs(int jobID);
	int getRunningJobFID(int jobID);
	bool checkConsumeWait(int jobID);
	/* from device runtime manager */

	int getFuncDest(int functionID);
	int getInSocket(int deviceID);
	int getOutSocketByDeviceID(int deviceID);
	int getSourceJobID(int localJobID);
	int getOutSocket(int localJobID);

	int getJID();
	int getDID();

	void printRT(); // DEBUG
	void printST(int did); // DEBUG
private:
	pthread_mutex_t functionRTableLock;	
	pthread_mutex_t socketMapLock;
	pthread_mutex_t jobMapLock;
	
	pthread_mutex_t argsLock;
	pthread_mutex_t consumeTableLock;
	pthread_mutex_t runningJobsLock;
	pthread_mutex_t returnValueTableLock;
	//map<int,int> funcJobMatchingTable; // <jobID, functionID>
	//functions for local function (in gateway)
	map<int,void*> argsTable;
	map<int,void*> returnValueTable; //<jobID, returnValue>
	map<int,bool> consumeTable; // <jobID, isReturnValueArrived>
	map<int,int> runningJobs; // <jobID, functionID>
	// functions for remote function call
	map<int,int> functionRoutingTable; // <funcID, outSocket>
	map<int,SocketInfo> socketMap; // <deviceID, SocketInfo>
	map<int,JobElement> jobMap; // <localJobID, jobInfo>
	map<int,PageInfo> pageManager;
	int jobID;
	int deviceID = 1;	
};
#endif
