/***
 * LoadCompareNamer.cpp
 *
 * Module pass to load the metadata from the file "metadata.profile", 
 * which is printed by "corelab/Metadata/Namer.h".
 * 
 * */

#define DEBUG_TYPE "main_creator"

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

#include "corelab/Esperanto/MainFcnCreator.h"
#include "corelab/Utilities/GlobalCtors.h"
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

	char MainCreator::ID = 0;
	static RegisterPass<MainCreator> X("mainCreator", "create main fcn if there is no main fcn", false, false);

	static cl::opt<string> DeviceName("device", cl::desc("device name for functions"),cl::value_desc("device name"));

	void MainCreator::getAnalysisUsage(AnalysisUsage& AU) const {
		AU.addRequired< EspInitializer >();
		AU.setPreservesAll();
	}

	bool MainCreator::runOnModule(Module& M)
	{
		//LLVMContext &Context = getGlobalContext();
		setFunctions(M);
		setIniFini(M);	
		return false;
	}


	void MainCreator::setIniFini(Module& M){
		LLVMContext &Context = M.getContext();
		std::vector<Type*> formals(0);
		std::vector<Value*> actuals(0);

		FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false); 

		Function *initForCtr = Function::Create(
				voidFcnVoidType, GlobalValue::ExternalLinkage, "__constructor__", &M);
		BasicBlock *entry = BasicBlock::Create(Context, "entry", initForCtr);
		BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr); 
		//actuals.resize(0);
		//CallInst::Create(constructor, actuals, "", entry); 
		BranchInst::Create(initBB, entry); 
		ReturnInst::Create(Context, 0, initBB);
		callBeforeMain(initForCtr);

		/* finalize */
		Function *finiForDtr = Function::Create(voidFcnVoidType, GlobalValue::ExternalLinkage, "__destructor__",&M);
		BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
		BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
		actuals.resize(0);
		CallInst::Create(destructor, actuals, "", fini);
		BranchInst::Create(fini, finiBB);
		ReturnInst::Create(Context, 0, fini);
		callAfterMain(finiForDtr);

	}

	StringRef MainCreator::getRealNameofFunction(StringRef original){
		std::string fName = original.data();
		int size=  original.size();
		char nameSize[3];
		sprintf(nameSize,"%d",size);
		std::string nameLength = std::string(nameSize);
		std::string pre = "_Z" + nameLength;
		std::string finalName = pre + fName;
		return StringRef(finalName);
		
	}

  Function* MainCreator::getSimilarFunction(Module& M, StringRef name){
    typedef Module::iterator FF;

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if(F->isDeclaration()) continue;
			DEBUG(errs() << "function name is " << F->getName().data() << "\n");
			if((F->getName().startswith_lower(name))){
				DEBUG(errs() << "linkage type number is " << F->getLinkage() << "\n");
				return F;
			}
		}
  }

  void MainCreator::setFunctions(Module& M){

    EspInitializer& espInit = getAnalysis<EspInitializer>();
    std::vector<Value*> actuals(0);
    bool mainArgsExist = false;
    //set constructor
    StringRef constructorName = espInit.MDTable.getConstructorName(DeviceName);

    DEBUG(errs() << "devicename is " << DeviceName.data() << "\n");
    DEBUG(errs() << "origin constructor name is " << constructorName.data() << "\n");
    DEBUG(errs() << "constructor name is " << getRealNameofFunction(constructorName) << "\n");
    StringRef realname_constructor = getRealNameofFunction(constructorName); 
    constructor = getSimilarFunction(M, realname_constructor);
      //M.getFunction(getRealNameofFunction(constructorName));
    
    //set destructor
    StringRef destructorName = espInit.MDTable.getDestructorName(DeviceName);
    DEBUG(errs() << "destructor name is " << getRealNameofFunction(destructorName) << "\n");
    StringRef realname_destructor = getRealNameofFunction(destructorName); 
    destructor = getSimilarFunction(M, realname_destructor);
      //M.getFunction(getRealNameofFunction(destructorName));

    //set main function
    mainFcn = getMainFcn(M);
    if(mainFcn == NULL){
      if(constructor->arg_empty()){
        DEBUG(errs() << "There is no main function, so create main with void args!!\n");
        FunctionType* intFcnVoidType = FunctionType::get(Type::getInt32Ty(M.getContext()),false);
        mainFcn = Function::Create(intFcnVoidType,GlobalValue::ExternalLinkage,"main",&M);
	      BasicBlock *entry = BasicBlock::Create(M.getContext(), "entry", mainFcn);
        BasicBlock* finalBasicBlock = BasicBlock::Create(M.getContext(),"",mainFcn);
        //BasicBlock& finalBasicBlock = mainFcn->back();
        actuals.resize(0);
		    CallInst::Create(constructor, actuals, "", entry); 

		    BranchInst::Create(finalBasicBlock,entry);
        IntegerType* int32Ty = IntegerType::get(M.getContext(),32);
        ConstantInt* zeroVal = ConstantInt::get(int32Ty,0,true);
        ReturnInst::Create(M.getContext(),(Value*)zeroVal,finalBasicBlock);

        DEBUG(errs() << "There is no main function, so create main with void args!! - end\n");
      }
      else{
        std::vector<Type*> main_args_type;
        main_args_type.resize(2);
        main_args_type[0] = IntegerType::get(M.getContext(),32);
        main_args_type[1] = (Type::getInt8PtrTy(M.getContext()))->getPointerTo();
        FunctionType* mainFcnType = FunctionType::get(Type::getInt32Ty(M.getContext()),main_args_type,false);
        mainFcn = Function::Create(mainFcnType,GlobalValue::ExternalLinkage,"main",&M);
        BasicBlock *entry = BasicBlock::Create(M.getContext(), "entry", mainFcn);
        BasicBlock* finalBasicBlock = BasicBlock::Create(M.getContext(),"",mainFcn);

        //BasicBlock& finalBasicBlock = mainFcn->back();
        actuals.resize(2);
        Function::arg_iterator ai = mainFcn->arg_begin();
        actuals[0] = (Value*)&*ai;
        actuals[1] = (Value*)&*(++ai);
        CallInst::Create(constructor, actuals, "", entry); 

		    BranchInst::Create(finalBasicBlock, entry);
        IntegerType* int32Ty = IntegerType::get(M.getContext(),32);
        ConstantInt* zeroVal = ConstantInt::get(int32Ty,0,true);
        ReturnInst::Create(M.getContext(),(Value*)zeroVal,finalBasicBlock);
      }
    }
    else{
      if(mainFcn->arg_empty()){
        BasicBlock* firstBB = (BasicBlock*)(&*mainFcn->begin());    
        Instruction* firstNonPHI = firstBB->getFirstNonPHI();
        CallInst::Create(constructor,"",firstNonPHI);
      }
      else{
        if(constructor->arg_empty()){
          BasicBlock* firstBB = (BasicBlock*)(&*mainFcn->begin());    
          Instruction* firstNonPHI = firstBB->getFirstNonPHI();
          CallInst::Create(constructor,"",firstNonPHI);
        }
        else{
          actuals.resize(2);
          Function::arg_iterator ai = mainFcn->arg_begin();
          actuals[0] = (Value*)&*ai;
          actuals[1] = (Value*)&*(++ai);
          BasicBlock* firstBB = (BasicBlock*)(&*mainFcn->begin());    
          Instruction* firstNonPHI = firstBB->getFirstNonPHI();
          CallInst::Create(constructor,actuals,"",firstNonPHI);
        }
      }
    }
  }

  Function* MainCreator::getMainFcn(Module& M){
    typedef Module::iterator FF;

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if(F->isDeclaration()) continue;
			DEBUG(errs() << "function name is " << F->getName().data() << "\n");
			if((F->getName().equals(StringRef("main")))){
				DEBUG(errs() << "linkage type number is " << F->getLinkage() << "\n");
				return F;
			}
		}
		return NULL;
	}

/*
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

	*/	
}
