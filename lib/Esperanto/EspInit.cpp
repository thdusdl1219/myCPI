/***
 * LoadCompareNamer.cpp
 *
 * Module pass to load the metadata from the file "metadata.profile", 
 * which is printed by "corelab/Metadata/Namer.h".
 * 
 * */

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/CallSite.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"

#include "corelab/Esperanto/EspInit.h"
#include "corelab/Esperanto/StringUtils.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <string.h>
using namespace corelab;

namespace corelab {
	
	char EspInitializer::ID = 0;
	static RegisterPass<EspInitializer> X("espInit", "initialize esperanto pass", false, false);

	//void EspInitializer::getAnalysisUsage(AnalysisUsage& AU) const{
	//	AU.setPreservesAll();
	//}

	bool EspInitializer::runOnModule(Module& M)
	{
		//LLVMContext &Context = getGlobalContext();
		buildMetadataTable();
		//buildDriverTable();
		buildProtocolTable();
		buildFunctionTable(M);
		return false;
	}

	void EspInitializer::buildFunctionTable(Module& M){
		typedef Module::iterator FF;
		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;
			functionTable.insertFunction(F);
		}
	}

	void EspInitializer::makePTableEntry(ProtocolTableEntry* entry,std::map<StringRef,struct RuntimeFunctions> tempProtocols,StringRef tempProtocol){
		bool isHost = tempProtocol.substr(0,4).equals(StringRef("HOST"));
		bool isLast = false;
		//if(tempProtocol.substr(0,4).equals(StringRef("HOST"))){
		StringRef protocols = StringUtils::Substring(tempProtocol,5,tempProtocol.size()-6);
		while(1){
			StringRef	*tempName = new StringRef();
			if(protocols.find(',') != std::string::npos){
				int pos = protocols.find(',');
				*tempName = StringUtils::Substring(protocols,0,pos);
				protocols = StringUtils::DropFront(protocols,pos+1);
			}
			else{
				isLast = true;
				*tempName = protocols;
			}
      std::string tempName_string = tempName->str();
      printf("BONGJUN: tempName_string %s\n", tempName_string.c_str());
			struct ProtocolInfo pInfo;
			if(isHost)
				(pInfo).targetInit = tempProtocols[tempName_string].initHost;
			else
				(pInfo).targetInit = tempProtocols[tempName_string].initClnt;
			/*if (pInfo.targetInit != NULL) { 
				printf("BONG: protocol targetInit ptr %p\n", pInfo.targetInit);
				printf("BONG: protocol targetInit => %s\n", (pInfo.targetInit)->str().c_str());
			}*/
			(pInfo).targetSend = tempProtocols[tempName_string].send;
			(pInfo).targetRecv = tempProtocols[tempName_string].recv;
			(pInfo).targetFini = tempProtocols[tempName_string].fini;
			if(isHost)
				entry->insertHostProtocol(tempName_string,pInfo);
			else
				entry->insertClntProtocol(tempName_string,pInfo);
			//printf("tempName address : %p, %p\n",tempName_,(tempName).data());
			if(isHost)
				printf("HOST :: ");
			else
				printf("CLNT :: ");
			printf("inserted protocol is %s\n",(*tempName).str().c_str());
      printf("BONGJUN inserted protocol is %s\n", tempName_string.c_str());
			if(isLast)
				break;
		}	
	}

	void EspInitializer::buildProtocolTableImpl(std::map<StringRef,struct RuntimeFunctions> tempProtocols){
		char protocolBuffer[100];
		//int count = 0;
		std::vector<struct MetadataInfo*> DevDecls = MDTable.getDevDeclList();	
		for(int i=0;i<(int)DevDecls.size();i++){
			ProtocolTableEntry* entry = new ProtocolTableEntry();
			if(DevDecls[i]->arg3->size() == 0 ) continue;
			strcpy(protocolBuffer,DevDecls[i]->arg3->data());
			StringRef tempProtocol = StringRef(protocolBuffer);

			if(tempProtocol.find(':') != std::string::npos){
				printf("%s - both protocol is exist\n",DevDecls[i]->arg1->data());
				std::pair<StringRef,StringRef> protocolPair = tempProtocol.split(':');
				StringRef first = protocolPair.first;
				StringRef second = protocolPair.second;
				printf("dev21 : %s\n",DevDecls[i]->arg1->data());
				makePTableEntry(entry,tempProtocols,first);
				printf("dev22 : %s\n",DevDecls[i]->arg1->data());
				makePTableEntry(entry,tempProtocols,second);
			}
			else{
				printf("dev1 : %s\n",DevDecls[i]->arg1->data());
				makePTableEntry(entry,tempProtocols,tempProtocol);	
			}
			PTable.insertProtocol(*(DevDecls[i]->arg1),*entry);
		}
	}

	void EspInitializer::buildProtocolTable(){
		FILE* protocolFile = fopen("EspProtocol.profile","r");
		if(protocolFile == nullptr)
			return;
		char readBuffer[100];
		char* temp;
		char c = 1;
		int type = -1;
		std::map<StringRef,struct RuntimeFunctions> tempProtocols;

		while(c != EOF){
			// protocol name
			c = fscanf(protocolFile,"%s",readBuffer);
			temp = (char*)malloc(100);
			strcpy(temp,readBuffer);
			StringRef* protocolName = new StringRef(temp);
			//printf("name : %s\n",readBuffer);
			// target function name
			c = fscanf(protocolFile,"%s",readBuffer);
			temp = (char*)malloc(100);
			strcpy(temp,readBuffer);
			StringRef* targetFunction = new StringRef(temp);
			//printf("tFunc : %s\n",readBuffer);
			// runtime function name
			c = fscanf(protocolFile,"%s",readBuffer);
			temp = (char*)malloc(100);
			strcpy(temp,readBuffer);
			StringRef* runtimeFunction = new StringRef(temp);	
			//printf("rFunc : %s\n",readBuffer);
			// type
			c = fscanf(protocolFile,"%s",readBuffer);
			temp = (char*)malloc(100);
			strcpy(temp,readBuffer);
			StringRef* protocolType = new StringRef(temp);

			if(tempProtocols.find(*protocolName) == tempProtocols.end()){
				struct RuntimeFunctions r;
				tempProtocols[*protocolName] = r;
			}
			if(strcmp(runtimeFunction->data(),"init")==0){
				if(strcmp(protocolType->data(),"HOST") == 0){
					tempProtocols[*protocolName].initHost = targetFunction;
				}
				else
					tempProtocols[*protocolName].initClnt = targetFunction;
			}
			else if(strcmp(runtimeFunction->data(),"send") == 0){
				tempProtocols[*protocolName].send = targetFunction;
			}
			else if(strcmp(runtimeFunction->data(),"recv") == 0){
				tempProtocols[*protocolName].recv = targetFunction;
			}
			else
				tempProtocols[*protocolName].fini = targetFunction;
		}
		buildProtocolTableImpl(tempProtocols);	
	
	}

	

	void EspInitializer::buildMetadataTable(){
		FILE* metadataFile = fopen("EspMetadata.profile","r");
		if(metadataFile == nullptr)
			return;
		char readBuffer[100];
		char* temp;
		char c = 1;
		while(c != EOF){
			c = fscanf(metadataFile,"%s",readBuffer);
			temp = (char*)malloc(100);
			strcpy(temp,readBuffer);
			StringRef* metadataType = new StringRef(temp);
			if(strcmp(readBuffer,"EspDevDecl") == 0){
				struct MetadataInfo mi;
				c = fscanf(metadataFile,"%s",readBuffer);
				printf("DeviceName : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg1 = new StringRef(temp);
				c = fscanf(metadataFile,"%s",readBuffer);
				printf("Constructor : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg2 = new StringRef(temp);
				c = fscanf(metadataFile,"%s",readBuffer);
				printf("Destructor : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg3 = new StringRef(temp);
				MDTable.insertEspDevDecl(mi);
			}
			else if(strcmp(readBuffer,"EspDevice")==0){
				struct MetadataInfo mi;
				c = fscanf(metadataFile,"%s",readBuffer);
				//printf("Type : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg1 = new StringRef(temp);
				c = fscanf(metadataFile,"%s",readBuffer);
				//printf("Identifier : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg2 = new StringRef(temp);
				c = fscanf(metadataFile,"%s",readBuffer);
				//printf("Devices : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg3 = new StringRef(temp);
				MDTable.insertEspDevice(mi);
				DITable.insertDevice(*mi.arg3);

				//metadataTable[metadataType] = mi;
			}
			else if(strcmp(readBuffer,"EspVarDecl")==0){
				struct MetadataInfo mi;
				c = fscanf(metadataFile,"%s",readBuffer);
				//printf("TypeName : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg1 = new StringRef(temp);
				c = fscanf(metadataFile,"%s",readBuffer);
				//printf("VarName : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				mi.arg2 = new StringRef(temp);
				//metadataTable[metadataType] = mi;
				MDTable.insertEspVarDecl(mi);
			}
		}
		//printf("metadata table size : %d\n",(int)metadataTable.size());
		fclose(metadataFile);
		std::vector<struct MetadataInfo*> te = MDTable.getDevDeclList();
		std::vector<struct MetadataInfo*>::iterator it;
		for(it = te.begin();it!=te.end();++it){
			struct MetadataInfo* t = *it;
			printf("1. %s\n2. %s\n3. %s\n",t->arg1->str().c_str(),t->arg2->str().c_str(),t->arg3->str().c_str());
		}
	}

	void EspInitializer::buildDriverTable(){
		FILE* driverFile = fopen("EspDriver.profile","r");
		if(driverFile == nullptr)
			return;
		char readBuffer[100];
		char c = 1;
		int functionCounter = 0;

		bool isClassExist = false;
		char* temp;
		StringRef* className;
		while(c != EOF){
			c = fscanf(driverFile,"%s",readBuffer);
			//StringRef type = StringRef(readBuffer);
			if(strcmp(readBuffer,"class") ==0){
				isClassExist = false;
				functionCounter = 0;
				struct DriverClassInfo di;
				c = fscanf(driverFile,"%s",readBuffer);
				temp = (char*)malloc(100);
				//printf("DriverName : %s\n",readBuffer);
				strcpy(temp,readBuffer);
				di.driverName = new StringRef(temp);
				c = fscanf(driverFile,"%s",readBuffer);
				//printf("AbstractClassName : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				di.abstractClassName = new StringRef(temp);
				c = fscanf(driverFile,"%s",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);	
				std::map<StringRef*,struct DriverClassInfo>::iterator it;
				for(it = driverTable.begin();it!=driverTable.end();++it){
					if(strcmp((it->first)->data(),temp) == 0){
						isClassExist = true;
						break;
					}
				}
				//strcpy(tempClassName,readBuffer,100);
				className = new StringRef(temp);
				c = fscanf(driverFile,"%s",readBuffer);
				//printf("Condition : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				di.classCondition = new StringRef(temp);
				di.functions = (struct DriverFunctionInfo*)malloc(sizeof(DriverFunctionInfo));
				if(!isClassExist){
					//printf("new class insert\n");
					//printf("class Name inserted : %s\n",className->data());
					driverTable[className] = di;
				}
							
				//printf("driver table size : %d\n",(int)driverTable.size());
			}
			else if(strcmp(readBuffer,"function")==0){
				if(!isClassExist){
				struct DriverFunctionInfo di;
				functionCounter++;
				driverTable[className].functions = (struct DriverFunctionInfo*)realloc(driverTable[className].functions,sizeof(DriverFunctionInfo)*functionCounter);
				c = fscanf(driverFile,"%s",readBuffer);
				//printf("DriverName : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				di.driverName = new StringRef(temp);
				c = fscanf(driverFile,"%s",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				//printf("FunctionName : %s\n",readBuffer);
				di.functionName = new StringRef(temp);
				c = fscanf(driverFile,"%s",readBuffer);
				//printf("Condition : %s\n",readBuffer);
				temp = (char*)malloc(100);
				strcpy(temp,readBuffer);
				di.condition = new StringRef(temp);
				(driverTable[className].functions)[functionCounter-1] = di;
				//driverTable[type] = di;
				}
			}
		}
		fclose(driverFile);	

	}	

	/*void EspInitializer::handleMetadata(char* readBuffer){

		}*/

	/*void EspInitializer::checkCallInst(Module& M){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;
		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;

			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
				BasicBlock* B = (BasicBlock*) &*BI;
				for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
					Instruction* inst = (Instruction*) &*Ii;
					if(!isa<CallInst>(inst)) continue;
					CallInst* callInst = (CallInst*)inst;
					int num = (int)(callInst->getNumArgOperands());
					for(int i=0;i<num;i++){
						callInst->getArgOperand(i)->dump();
					}
					
				}
			}
		}
	
	}

	Function* EspInitializer::setFunction(Module& M){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;
		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;
			printf("Function name : %s\n",F->getName().data());	
			if(strcmp(F->getName().data(),"_Z8fakeFuncv") == 0){
				printf("function is selected\n");
				return F;
			}
		}
	}

	void EspInitializer::replaceBitCast(Module& M, Function* fakeFunc){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;
		int count=0;
		printf("replace BitCast instruction\n"); fflush(stdout);
		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;
			printf("for every function\n"); fflush(stdout);

			if(strcmp(F->getName().data(),"main") != 0) continue;

			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
				BasicBlock* B = (BasicBlock*) &*BI;
				printf("for every basic block\n"); fflush(stdout);
				for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
					printf("for every instruction\n"); fflush(stdout);
					Instruction* inst = (Instruction*) &*Ii;
					if(!isa<BitCastInst>(inst)) continue;
					//////////////////////////////////////////////
					
					if(count == 0){
						count = 1;
						continue;
					}
//				select appropriate bitcast instruction using desc file
////////////////////////////////////////////////////////
					printf("Instruction is bitcast inst\n"); fflush(stdout);
					//Function* faceFunc = M.getFunction(StringRef("_Z8fackFuncv"));
					if(fakeFunc == nullptr)
						printf("function is not selected\n"); fflush(stdout);
					std::vector<Value*> actuals(0);
					printf("ready to replace\n"); fflush(stdout);
					
					Value* newVal = CallInst::Create(fakeFunc,actuals,"",inst); 

				//	typedef Value::user_iterator U;
				//	for(U ui = inst->user_begin(),ue = inst->user_end(); ui!= ue;++ui){
				//		User* u = (User*) *ui;
				//		u->replaceUsesOfWith((Value*)u,newVal);
				//	}
					//inst->removeFromParent();
					inst->replaceAllUsesWith(newVal);
					//inst->removeFromParent();
					printf("after replace\n"); fflush(stdout);
				}
			}
		}
	}*/
	
}
