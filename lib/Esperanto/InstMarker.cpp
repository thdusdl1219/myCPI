/***
 * LoadCompareNamer.cpp
 *
 * Module pass to load the metadata from the file "metadata.profile", 
 * which is printed by "corelab/Metadata/Namer.h".
 * 
 * */
#define DEBUG_TYPE "inst_marker"

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

#include "corelab/Esperanto/InstMarker.h"
//#include "corelab/Esperanto/EspInit.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <string.h>
using namespace corelab;

namespace corelab {
	
	char InstMarker::ID = 0;
	static RegisterPass<InstMarker> X("instMarker", "mark instructions for code split", false, false);

	void InstMarker::getAnalysisUsage(AnalysisUsage& AU) const {
		AU.addRequired< EspInitializer >();
		AU.setPreservesAll();
	}

	bool InstMarker::runOnModule(Module& M)
	{
		//LLVMContext &Context = getGlobalContext();
    handleAsyncFcn(M);
		// markClassInst(M);
		// markFunctionInst(M);
		// removeMarkedRegion(M);

    //for(auto ci : async_fcn_list)
    //  DEBUG(errs() << "async function is " << ci->getCalledFunction()->getName().data() << "\n");
		//buildMetadataTable();
		//buildDriverTable();
		return false;
	}

  void InstMarker::handleAsyncFcn(Module& M){
    typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;

		EspInitializer& database = getAnalysis< EspInitializer >();

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;

			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
				BasicBlock* B = (BasicBlock*) &*BI;
				for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
					Instruction* inst = (Instruction*) &*Ii;
					if(!isa<CallInst>(inst) && !isa<InvokeInst>(inst)) continue;
          Function* calledFunction;
          if(isa<CallInst>(inst)){
					CallInst* callInst = cast<CallInst>(inst);
					calledFunction = callInst->getCalledFunction();
          }
          else{
          InvokeInst* invokeInst = cast<InvokeInst>(inst);
					calledFunction = invokeInst->getCalledFunction();

          }
					if(calledFunction != nullptr){
						StringRef functionName = getFunctionNameInFunction(calledFunction->getName());
            StringRef className = getClassNameInFunction(calledFunction->getName());
            if(className.size() != 0)
            //DEBUG(errs() << "Names " << className.data() << " - " << functionName.data() << "\n");
						if(functionName.size() ==0) continue;
            if(className.size() == 0) continue;
            //DEBUG(errs() << "async function size is " << database.async_functions.size() << "\n");
            for(int i=0;i<database.async_functions.size();i++){

              if(strcmp(className.data(),(database.async_functions[i].className)->data()) != 0) continue;
              if(strcmp(functionName.data(),(database.async_functions[i].funcName)->data()) != 0) continue;
              async_fcn_list.push_back(calledFunction);
              //DEBUG(errs() << "matching -----------------------------------\n");

              //DEBUG(errs() << "classNames " << className.data() << " - " << (database.async_functions[i].className)->data() << "\n");

              //DEBUG(errs() << "function Names " << functionName.data() << " - " << (database.async_functions[i].funcName)->data() << "\n");
            }
            /*if(strcmp(functionName.data(),"EspAsyncFcn") != 0) continue;
            DEBUG(errs() << "async function name is " << functionName.data() << "\n");
            for(int i=0;i<callInst->getNumArgOperands();i++){

callInst->getArgOperand(i)->dump();
              DEBUG(errs() << "type of argument is " << callInst->getArgOperand(i)->getType()->getTypeID() << "\n");
            }*/
					}
					
				}
			}
		}

  }

	void InstMarker::makeMetadata(Instruction* instruction, int Id) {
		LLVMContext &context = instruction->getModule()->getContext();
		//XXX: Is it okay to cast Value* to Metadata* directly?
		Constant* IdV = ConstantInt::get(Type::getInt64Ty(context), Id);
		Metadata* IdM = (Metadata*)ConstantAsMetadata::get(IdV);
		Metadata* valuesArray[] = {IdM};
		ArrayRef<Metadata *> values(valuesArray, 1);
		MDNode* mdNode = MDNode::get(context, values);
		//NamedMDNode *namedMDNode = pM->getOrInsertNamedMetadata("corelab.namer");
		//namedMDNode->addOperand(mdNode);
		instruction->setMetadata("namer", mdNode);
		//instruction->dump();
		//errs() <<" has namer metadata\n";
		return;
	}

	StringRef InstMarker::getFunctionNameInFunction(StringRef functionName){
    //printf("\nfunction name is : %s\n",functionName.data());
		if(functionName.size() <= 2){
			return functionName;
		}

		if(functionName.data()[0] == '_'){
			if(functionName.size() >= 10){
				//printf("function name is longer than 10\n");
				char* temp = (char*)malloc(11);
				for(int i=0;i<10;i++){
					temp[i] = functionName.data()[i];
				}
				temp[10] = '\0';
				//printf("temp is %s\n",temp);
				if(strcmp(temp,"_espRegion") ==0){
					//printf("region function name : %s\n",functionName.substr(1,functionName.size()-1).data());
					return functionName.substr(1,functionName.size()-1);
				}
			}
		}

    if(functionName.data()[0] == '_' && functionName.data()[1] == 'Z'){	// cplusplus function
      int classRegion = 0;

      int count_ = 0;
      if(functionName.data()[2] == 'N'){
        int classNameLen = 0;
        for(int i=0;i<(int)(functionName.size()-3);i++){
          if(47 < functionName[i+3] && functionName[i+3] <58){
            //printf("%c : %d\n",functionName[i],functionName[i]);
            classNameLen *= 10;
            //printf("className length *10 = %d\n",classNameLen);
            classNameLen += (int)(functionName[i+3]-48);
            //printf("className length = %d\n",classNameLen);
            count_++;
          }
          else
            break;
        }
        classRegion = 3 + count_ + classNameLen;
        //printf("class Region length : %d\n",classRegion);
      }
			StringRef tempName = functionName.substr(2,functionName.size()-2);
      if(count_ > 0)
        tempName = functionName.substr(classRegion,functionName.size()-classRegion);
			//printf("temp name :%s\n",tempName.data());
			std::string ret = "";
			int classNameLength=0;
			int count = 0;
			bool isNum = true;
			for(int i=0;i<(int)tempName.size();i++){
				if(47 < tempName[i] && tempName[i] <58){
					//printf("%c : %d\n",tempName[i],tempName[i]);
					classNameLength *= 10;
					//printf("className length *10 = %d\n",classNameLength);
					classNameLength += (int)(tempName[i]-48);
					//printf("className length = %d\n",classNameLength);
					count++;
				}
				else{
					isNum = false;
					break;
				}
			}
			//printf("className length : %d\n",classNameLength);
			if(classNameLength !=0){
				for(int i=0;i<classNameLength;i++){
					//printf("className length : %d\n",i);
					ret.push_back(tempName[count+i]);
				}
        char* ret_temp = (char*)malloc(classNameLength);
        strcpy(ret_temp,ret.c_str());
        //printf("function name is %s\n",ret_temp);
				return StringRef(ret_temp);
			}
			else{
				if(!isNum){
					return StringRef("");
				}
			}
		}
		else{ // c function or extern "C"
			return functionName;
		}
	}

	std::string InstMarker::getClassNameInFunction(StringRef functionName){
    if(functionName.size() < 4) return "";
		StringRef fName = functionName.substr(3,functionName.size()-3);
		//printf("fName = %s\n",fName.data());
		std::string ret = "";
		int classNameLength=0;
		int count = 0;
    
    //if(!(fName[0] == '_' && fName[1] == 'Z' && fName[2] == 'N')) return "";
		for(int i=0;i<(int)fName.size();i++){
			if(47 < fName[i] && fName[i] <58){
				//printf("%c : %d\n",fName[i],fName[i]);
				classNameLength *= 10;
				//printf("className length *10 = %d\n",classNameLength);
				classNameLength += (int)(fName[i]-48);
				//printf("className length = %d\n",classNameLength);
				count++;
			}
			else
				break;
		}
		//printf("className length : %d\n",classNameLength);
		if(classNameLength !=0){
			int j=0;
			int k=0;
			for(k=0;k<classNameLength;k++){

				//printf("className length : %d\n",i);
				ret.push_back(fName[count+k]);
				j++;
			}
			if((fName[count+k] == 'C') && (fName[count+k+1] == '2'))
			 return "";	
			if(fName[count+j] <48 && fName[count+j] > 57)
				return "";
			return ret;
		}
		return ret;
	}

	void InstMarker::markFunctionInst(Module& M){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;
		
		EspInitializer& database = getAnalysis< EspInitializer >();

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;

			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
				BasicBlock* B = (BasicBlock*) &*BI;
				for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
					Instruction* inst = (Instruction*) &*Ii;
					if(!isa<CallInst>(inst)) continue;
					CallInst* callInst = cast<CallInst>(inst);
					Function* calledFunction = callInst->getCalledFunction();
					if(calledFunction != nullptr){
						if(callInst->hasMetadata()) continue;
						StringRef functionName = getFunctionNameInFunction(calledFunction->getName());
						//printf("functionName : %s\n",calledFunction->getName());
						if(functionName.size() ==0) continue;
						//printf("DEBUG :: function print -> %s\n",functionName.data());
						//printf("DEBUG :: class Name in function %s : %s\n",className.c_str(),calledFunction->getName().data());
						StringRef deviceName = database.MDTable.getDeviceName(StringRef("function"),StringRef(functionName));
						if(deviceName.size() == 0) continue;
						int deviceID = database.DITable.getDeviceID(deviceName);
						//printf("DEBUG :: deviceID = %d\n",deviceID);
						makeMetadata(inst,deviceID);	
					}
					
				}
			}
		}
	}

	void InstMarker::removeMarkedRegion(Module& M){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;
		
		EspInitializer& database = getAnalysis< EspInitializer >();

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;

			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
				BasicBlock* B = (BasicBlock*) &*BI;
				for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
					Instruction* inst = (Instruction*) &*Ii;
					if(!isa<CallInst>(inst)) continue;
					CallInst* callInst = cast<CallInst>(inst);
					Function* calledFunction = callInst->getCalledFunction();
					if(calledFunction != nullptr){
						if(callInst->hasMetadata()) continue;
						StringRef functionName = getFunctionNameInFunction(calledFunction->getName());
						if(functionName.size() == 0) continue;

						
						//printf("functionName : %s\n",calledFunction->getName());
						//printf("DEBUG :: function print -> %s\n",functionName.data());
						//printf("DEBUG :: class Name in function %s : %s\n",className.c_str(),calledFunction->getName().data());
						StringRef deviceName = database.MDTable.getDeviceName(StringRef("region"),StringRef(functionName));
						if(deviceName.size() == 0) continue;
						//printf("device name : %s\n", deviceName.data());
						int deviceID = database.DITable.getDeviceID(deviceName);
						//printf("DEBUG :: deviceID = %d\n",deviceID);
						makeMetadata(inst,deviceID);	
					}
					
				}
			}
		}
	}



	void InstMarker::markClassInst(Module& M){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;
		//LLVMContext& Context = getGlobalContext(); 
		EspInitializer& database = getAnalysis< EspInitializer >();

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;

			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
				BasicBlock* B = (BasicBlock*) &*BI;
				for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
					Instruction* inst = (Instruction*) &*Ii;
					if(!isa<CallInst>(inst)) continue;
          Function* calledFunction;
          Value* calledValue;
          bool isCallInst =false;
          CallInst* callInst = dyn_cast<CallInst>(inst);

          calledValue = callInst->getCalledValue();
          calledFunction = callInst->getCalledFunction();
          if(calledFunction != nullptr){
            DEBUG(errs() << "function name is " << calledFunction->getName().data() << "\n");
            std::string className = getClassNameInFunction(calledFunction->getName());
            if(className.size() !=0)
            DEBUG(errs() << "class name is " << className.c_str() << "\n");
            
            if(className.size() ==0) continue;
            StringRef deviceName = database.MDTable.getDeviceName(StringRef("class"),StringRef(className));
            
            if(deviceName.size() == 0) continue;
            int deviceID = database.DITable.getDeviceID(deviceName);
            DEBUG(errs() << "\nfunction class & devicename" << className.c_str() << " / " << deviceName.data() << "\n");
            /*if(strcmp("this",calledFunction->arg_begin()->getName().data()) == 0){
              if(!LFManager.isExist(calledFunction)){
                LFManager.insertLocalFunction(inst,false,NULL);
              }
            }
            else{
              LFManager.insertLocalFunction(inst,true,nullptr);
            }*/
            makeMetadata(inst,deviceID);	
          }
          else if(calledValue != nullptr){
            if(calledValue->getName().size() != 0){
            DEBUG(errs() << "called value " << calledValue->getName().data() << "\n");
            std::string className = getClassNameInFunction(calledValue->getName());

            if(className.size() ==0) continue;
            StringRef deviceName = database.MDTable.getDeviceName(StringRef("class"),StringRef(className));
            if(deviceName.size() == 0) continue;
            int deviceID = database.DITable.getDeviceID(deviceName);

            DEBUG(errs() << "\n value class & devicename" << className.c_str() << " / " << deviceName.data() << "\n");
            //LFManager.insertLocalFunction(inst,true,nullptr);
            makeMetadata(inst,deviceID);	
            }
          }

        }
      }
		}
	}
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
