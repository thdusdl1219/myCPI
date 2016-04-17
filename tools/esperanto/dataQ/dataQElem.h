#ifndef DATA_Q_ELEM_H
#define DATA_Q_ELEM_H

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>

using namespace std;

class DataQElem{
public:
	DataQElem();
	void setIsFunctionCall(bool isFunctionCall);
	void setArgs(void* args, int argsSize);
	void setFunctionID(int id);
	void setJobID(int id);
	void setRetVal(void* retVal, int retSize);
	
	int getJobID();
	bool getIsFunctionCall();
	int getFunctionID();
	void* getArgs();
	void* getRetVal();
	int getArgsSize();
	int getRetSize();

	void operator=(DataQElem& source);
	
private:
	bool isFunctionCall;
	int functionID;
	int jobID;
	void* args;
	int argsSize;
	void* retVal;
	int retSize;
};

#endif
