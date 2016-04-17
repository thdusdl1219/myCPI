#include "dataQElem.h"

DataQElem::DataQElem(){
}

void DataQElem::setIsFunctionCall(bool isFunctionCall_){
	isFunctionCall = isFunctionCall_;
}

void DataQElem::setArgs(void* args_, int argsSize_){
	args = args_;
	argsSize = argsSize_;
}

void DataQElem::setFunctionID(int id){
	functionID = id;
}

void DataQElem::setJobID(int id){
	jobID = id;
}

void DataQElem::setRetVal(void* retVal_, int retSize_){
	retVal = retVal_;
	retSize = retSize_;
}

int DataQElem::getArgsSize(){
	return argsSize;
}

int DataQElem::getRetSize(){
	return retSize;
}

int DataQElem::getJobID(){
	return (int)jobID;
}

int DataQElem::getFunctionID(){
	return functionID;
}

bool DataQElem::getIsFunctionCall(){
	return isFunctionCall;
}

void* DataQElem::getArgs(){
	return args;
}

void* DataQElem::getRetVal(){
	return retVal;
}

/*
void DataQElem::operator=(DataQElem& dest){
	dest.setIsFunctionCall(isFunctionCall());
	dest.setArgs(args,argsSize);
	dest.setJobID(jobID);
	dest.setFunctionID(functionID);
	dest.setRetVal(retVal,retSize);	
	//argsSize = source.getArgsSize();
	//retSize = source.getRetSize();
}*/
