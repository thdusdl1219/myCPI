#include "dataQ.h"

DataQ::DataQ(){
	dataQ = new std::vector<DataQElem*>();
	//printf("dataQ is created\n");
	//printf("dataQsize is initialized : %d\n",dataQ->size());
}

int DataQ::getSize(){
	pthread_mutex_lock(&lock);
	//printf("dataQ address : %p / %d\n",dataQ,dataQ->size());
	int temp = (int)dataQ->size();
	pthread_mutex_unlock(&lock);
	return temp;
}

void DataQ::produce(DataQElem* data) {
	printf("address of elem : %p\n",data);	
	pthread_mutex_lock(&lock);
	printf("address of elem : %p\n",data);	
	//printf("address of dataQ = %p\n",dataQ);
	dataQ->push_back(data);
	pthread_mutex_unlock(&lock);
	printf("address of elem : %p\n",data);	
}

DataQElem* DataQ::consume() {
	bool notNull = false;
	DataQElem* data;
	//DataQElem* data = new DataQElem();
	pthread_mutex_lock(&lock);
	if(dataQ->size() != 0){
		notNull = true;
		/*data->setIsFunctionCall(dataQ.front()->getIsFunctionCall());
		data->setArgs(dataQ.front()->getArgs(),dataQ.front()->getArgsSize());
		data->setJobID(dataQ.front()->getJobID());
		data->setFunctionID(dataQ.front()->getFunctionID());
		data->setRetVal(dataQ.front()->getRetVal(),dataQ.front()->getRetSize());	
*/
		data = dataQ->front();
		//printf("dataQ size 1 is : %d\n",dataQ->size());
		dataQ->erase(dataQ->begin());
		//printf("dataQ size 2 is : %d\n",dataQ->size());
	}
	pthread_mutex_unlock(&lock);
	if(notNull)	
		return data;
	return NULL;
}

/*
template <typename T>
T DataQ<T>::find(int jobID) {
	
	
}*/
