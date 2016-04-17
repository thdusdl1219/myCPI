#include "esp_runtime.h"
#include "log.h"


extern "C" void debugAddress(void* d){
	LOG("DEBUG :: address = %p\n",d);
}

extern "C"
void start(ApiCallback fcn, int id){
	LOG("DEBUG :: start function - id = %d\n",id);
	dqm = new DataQManager();
	dqm->initQ();	
}

extern "C"
int generateJobId(int functionID){
	int jobID;
	if(isGateway){
		jobID = grm->getJID();
		grm->insertRunningJob(jobID, functionID);
		//grm->insertFuncJobMatching(jobID, functionID);		
	}
	else{
		jobID = drm->getJobID();
		drm->insertRunningJob(jobID, functionID);
	}

	LOG("DEBUG :: generate job ID = %d\n",jobID);
	return jobID;
}

extern "C"
void produceReturn(int jobID, void* buf, int size){
	DataQElem* elem = new DataQElem();
	void* ret = (void*)malloc(size);
	memcpy(ret,buf,size);
	if(isGateway){
		LOG("produceReturn gateway :%p\n",buf);
		LOG("return size : %d\n",size);
		hexdump("gateway return",buf,size);
		elem->setIsFunctionCall(false);
		elem->setArgs(NULL,0);
		elem->setFunctionID(grm->getRunningJobFID(jobID));
		elem->setJobID(jobID);
		elem->setRetVal(ret,size);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);
		pthread_mutex_unlock(&sendQLock);
		//do nothing
	}
	else{
		LOG("produceReturn device\n");
		elem->setIsFunctionCall(false);
		elem->setArgs(NULL,0);
		elem->setFunctionID(drm->getRunningJobFID(jobID));
		elem->setJobID(drm->getSourceJobID(jobID));
		elem->setRetVal(ret,size);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);
		pthread_mutex_unlock(&sendQLock);
	}
}

extern "C"
void produceFunctionArgs(int jobID, void* buf, int size){
	LOG("produce function Args / jobID = %d\n",jobID);
	DataQElem* elem = new DataQElem();
	if(isGateway){
		LOG("produceFArgs gateway\n");
		hexdump("produced args",buf,size);
		elem->setIsFunctionCall(true);
		elem->setArgs(buf,size);
		elem->setFunctionID(grm->getRunningJobFID(jobID));
		elem->setJobID(jobID);
		elem->setRetVal(NULL,0);
		grm->insertConsumeWait(jobID);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);	
		pthread_mutex_unlock(&sendQLock);
		LOG("produce function args to send q\n");
		// nothing
	}
	else{
		LOG("produceFArgs device\n");
		//LOG("args address : %p\n",buf);
		hexdump("produced args",buf,size);
		elem->setIsFunctionCall(true);
		elem->setArgs(buf,size);
		elem->setFunctionID(drm->getRunningJobFID(jobID));
		//LOG("function %d is called\n",drm->getRunningJobFID(jobID));
		elem->setJobID(jobID);
		elem->setRetVal(NULL,0);
		pthread_mutex_lock(&sendQLock);
		dqm->insertElementToSendQ(elem);	
		pthread_mutex_unlock(&sendQLock);
		LOG("produce function args to send q\n");

	}	
}

extern "C"
void* consumeFunctionArgs(int jobID){	
	
		LOG("consume function args\n");
	if(isGateway){
		//donothing
		pthread_mutex_lock(&handleArgsLock);
		//LOG("consume function args\n");
		void* ret = grm->getArgs(jobID);
		//LOG("args : %p\n",ret);
		pthread_mutex_unlock(&handleArgsLock);
		return ret;
	}
	else{
		pthread_mutex_lock(&handleArgsLock);
		void* ret = drm->getArgs(jobID);
		pthread_mutex_unlock(&handleArgsLock);
		hexdump("ConsumeFunctionArgs return value",ret,8);
		return ret;

	}
}

extern "C"
void* consumeReturn(int jobID){
	void* ret;
	LOG("Consume Return is started\n");
	if(isGateway){
		//nothing
		while(1){
			//pthread_mutex_lock(&consumeLock);
			if(grm->checkConsumeWait(jobID)){
				LOG("return is received, jobID = %d\n",jobID);
				ret = grm->getReturnValue(jobID);
				//LOG("return is received\n");
				break;
			}
			//pthread_mutex_lock(&consumeLock);
		}
	}
	else{
		while(1){
			if(drm->checkConsumeWait(jobID)){
				ret = drm->getReturnValue(jobID);
				LOG("return is received, jobID = %d\n",jobID);
				//drm->deleteConsumeWait(jobID);
				//hexdump("return",ret,4);
				LOG("return is received\n");
				break;
			}
		}
	}
	return ret;
}
