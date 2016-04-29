/***
 *
 * RemoteCall.cpp
 *
 * This pass substitutes call instruction which has 'comp' metadata into
 * produce/consume function call. It also takes input argument device name
 * so that target device keeps original code.
 *
 * **/
#define DEBUG_TYPE "remotecall"

#include "llvm/Support/Debug.h"
#include "llvm/IR/LLVMContext.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/Casting.h"
//#include "corelab/Metadata/NamedMetadata.h"
//#include "corelab/Metadata/Metadata.h"
//#include "corelab/Metadata/LoadNamer.h"
#include "corelab/Esperanto/EspUtils.h"
#include "corelab/Esperanto/RemoteCall.h"
#include "corelab/Esperanto/EspInit.h"
#include "corelab/Esperanto/InstMarker.h"

#include <vector>

using namespace corelab;
using namespace std;

char RemoteCall::ID = 0;
static RegisterPass<RemoteCall> X("remote-call-test", "substitute remote call instructions.", false, false);

void RemoteCall::getAnalysisUsage(AnalysisUsage &AU) const {
	//AU.addRequired< LoadNamer >();
	//AU.addRequired< EsperantoNamer >();
	AU.addRequired< EspInitializer >();
	AU.addRequired< InstMarker >();
	AU.setPreservesAll();
}

// command line argument
static cl::opt<string> DeviceName("device_name", cl::desc("Specify the device name to be split"), cl::value_desc("device name"));
//static cl::opt<string> DeviceType("device_type", cl::desc("Specify the device type to be split"), cl::value_desc("device type"));

bool RemoteCall::runOnModule(Module& M) {
	//getClassPointer(M);	
	deviceName = std::string(DeviceName);	
	setFunctions(M);					// Insert runtime function into Module
	substituteRemoteCall(M);	// RemoteCall substitution
	generateFunctionTableProfile();
	return true;
}



void RemoteCall::setFunctions(Module &M) {
	LLVMContext &Context = M.getContext();

  // void pushArgument(int rc_id, void* buf, int size);
  PushArgument = M.getOrInsertFunction(
      "pushArgument",
      Type::getVoidTy(Context),
      Type::getInt32Ty(Context),
      Type::getInt8PtrTy(Context),
      Type::getInt32Ty(Context),
      (Type*)0);

	// int generateJobId(int);
	GenerateJobId = M.getOrInsertFunction(
			"generateJobId",
			Type::getInt32Ty(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

  // void produceAsyncFunctionArgs(int functionID, void* buf, int size);
  ProduceAsyncFunctionArgument = M.getOrInsertFunction(
      "produceAsyncFunctionArgs",
      Type::getVoidTy(Context),
      Type::getInt32Ty(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	// void produceFunctionArgument(int jobId, void* buf, int size);
	ProduceFunctionArgument = M.getOrInsertFunction(
			"produceFunctionArgs",
			Type::getVoidTy(Context),
			Type::getInt32Ty(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	// void* consumeReturn(int jobId);
	ConsumeReturn = M.getOrInsertFunction(
			"consumeReturn",
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	return;
}


void RemoteCall::substituteRemoteCall(Module& M) {
	//LoadNamer& loadNamer = getAnalysis< LoadNamer >();
	EspInitializer& esp = getAnalysis< EspInitializer >();
  InstMarker& im = getAnalysis< InstMarker >();
	typedef Module::iterator FF;
	typedef Function::iterator BB;
	typedef BasicBlock::iterator II;

	std::vector<CallInst*> eraseList;

	for(FF FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
		Function* F = (Function*) &*FI;
		if (F->isDeclaration()) continue;
		int functionId = esp.functionTable.getFunctionID(F);
		//if(functionId == 0) continue;

		for(BB BI = F->begin(), BE = F->end(); BI != BE; ++BI) {
			BasicBlock* B = (BasicBlock*) &*BI;

			for(II Ii = B->begin(), Ie = B->end(); Ii != Ie; ++Ii) {
				Instruction* targetInstruction = (Instruction*) &*Ii;
				if(!isa<CallInst>(targetInstruction)) continue;
				//targetInstruction->dump();
				DEBUG(errs() << "before check metadata namer\n");
	
				// check the metadata which is attached to attributes
				MDNode* md = targetInstruction->getMetadata("namer");
				if (md == NULL) continue;

				CallInst* ci = (CallInst*)targetInstruction;
					
				// check device name and type(architecture) are same with arguments 
				Value* deviceIDAsValue = cast<ValueAsMetadata>(md->getOperand(0).get())->getValue();
				Constant* deviceIDAsConstant = cast<Constant>(deviceIDAsValue);
				ConstantInt* deviceIDAsConstantInt = cast<ConstantInt>(deviceIDAsConstant);
				int deviceID = (int)deviceIDAsConstantInt->getZExtValue();
				DEBUG(errs() << "this function has namer metadata : did = " << deviceID << "\n");
				//StringRef compType = cast<MDString>(md->getOperand(1).get())->getString();
				Function* calledFunction = ci->getCalledFunction();
				if (calledFunction == NULL) continue;
				DEBUG(errs() << "called function is not null"<< "\n");

				int calledFunctionId = esp.functionTable.getFunctionID(calledFunction);
				if(calledFunctionId < 0) continue;
									DEBUG(errs() << "called function's id is not 0"<< "\n");
					
				printf("function id : %d ==> %s\n",calledFunctionId,calledFunction->getName().data());
				//printf("function & deviceID = %s / %d\n",calledFunction->getName().data(),deviceID);
				StringRef devName = StringRef(deviceName);
				if(esp.DITable.getDeviceID(devName) == deviceID){
					//printf("This function is in device %d\n",deviceID);	
					localFunctionTable[calledFunction] = true;	
					continue;
				}
				else if(esp.MDTable.getDeviceName(StringRef("region"),StringRef(calledFunction->getName().drop_front())).size() != 0){
					printf("inside elseif\n");
					if(deviceID != esp.DITable.getDeviceID(DeviceName)){
						printf("OMG :: it is not my local region\n");
						eraseList.push_back(ci);
						continue;
					}
				}
				localFunctionTable[calledFunction] = false;	

        bool isAsync = false;
        for(auto async_ci : im.async_fcn_list){
          if(ci == async_ci)
            isAsync = true;
        }
        if(isAsync){
          createProduceAsyncFArgs(calledFunction, targetInstruction, targetInstruction);        
        }
        else{
          Instruction* jobId = createJobId(calledFunction, targetInstruction);
          createProduceFArgs(calledFunction, targetInstruction, (Value*)jobId, targetInstruction);
          createConsumeReturn(calledFunction, (Value*)jobId, targetInstruction);
        }
			}
		}
	}
	
	removeOriginalCallInst();
	for(auto &i : eraseList) {
		i->eraseFromParent();
	}
}

void RemoteCall::createProduceAsyncFArgs(Function* f, Instruction* I, Instruction *insertBefore) {
	
	//EsperantoNamer& esperantoNamer = getAnalysis< EsperantoNamer >();
	EspInitializer& esp = getAnalysis< EspInitializer >();
	InstMarker& iMarker = getAnalysis< InstMarker >();
	Module *M = f->getParent();
	LLVMContext &Context = M->getContext();
	const DataLayout &dataLayout = M->getDataLayout();
	std::vector<Value*> actuals(0);

	CallInst* ci = (CallInst*)I;
	size_t argSize = f->arg_size();
	Value* pointer = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
	int sum = 0;
	bool isFirst = true;	
  int functionId = esp.functionTable.getFunctionID(f);

  
	// for each func args, add it arg list.
	for (size_t i = 0; i < argSize; ++i) {
		Value* argValue = ci->getArgOperand(i); // original argument
		
		Type* type = argValue->getType(); // original type
    if(!type->isPointerTy()){
      const size_t sizeInBits = dataLayout.getTypeAllocSizeInBits(type); // allocation type size
      sum += (int)sizeInBits;
      Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1); 
      AllocaInst* alloca = new AllocaInst(type, one, (sizeInBits/8),"", insertBefore); // allocate buffer on stack
      if(i == (argSize -1)) {
        pointer = (Value*) alloca;
        isFirst = false;
      }
      new StoreInst(argValue, alloca, insertBefore); // copy values to buffer
      actuals.resize(0);
      actuals.resize(3);

      Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
      InstInsertPt out = InstInsertPt::Before(insertBefore);
      actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), rc_id);
      actuals[1] = Casting::castTo((Value*)alloca, temp, out, &dataLayout);
      actuals[2] = ConstantInt::get(Type::getInt32Ty(Context), (sizeInBits/8));

      CallInst::Create(PushArgument,actuals,"",insertBefore);
    }
    else{
      sum += 32;
      Type* newType = Type::getInt32Ty(Context);
      Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1); 
      AllocaInst* alloca = new AllocaInst(newType, one, 4,"", insertBefore); // allocate buffer on stack
      InstInsertPt out = InstInsertPt::Before(insertBefore);
      Value* addrIn32 = Casting::castTo(argValue, ConstantInt::get(Type::getInt32Ty(Context),0),out,&dataLayout);
      new StoreInst(addrIn32,alloca,insertBefore);

      actuals.resize(0);
      actuals.resize(3);
      Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
      out = InstInsertPt::Before(insertBefore);
      actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), rc_id);
      actuals[1] = Casting::castTo((Value*)alloca, temp, out, &dataLayout);
      actuals[2] = ConstantInt::get(Type::getInt32Ty(Context), 4);

      CallInst::Create(PushArgument,actuals,"",insertBefore);

    }
	}
  /*for(size_t i=0; i<argSize; ++i){
    Value* argValue = ci->getArgOperand(i);
    Type* type = argValue->getType();
    const size_t sizeInBits = dataLayout.getTypeAllocSizeInBits(type);
    sum += (int)sizeInBits;
  }*/
	sum /= 8;
  //AllocaInst* alloca = new AllocaInst(
  actuals.resize(0);
	
	// XXX:'sum' can occur align problem.
	actuals.resize(2);
	actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), functionId);
	//actuals[1] = Casting::castTo(pointer, temp, out, &dataLayout);
	actuals[1] = ConstantInt::get(Type::getInt32Ty(Context), rc_id);
  
	CallInst* new_ci = CallInst::Create(ProduceAsyncFunctionArgument,  actuals, "", insertBefore);
  removedCallInst.push_back(I);
	substitutedCallInst.push_back(new_ci);
  rc_id++;

	return;
}

Instruction* RemoteCall::createJobId(Function* f, Instruction *insertBefore){
	LLVMContext& Context = f->getParent()->getContext();
	EspInitializer& esp = getAnalysis< EspInitializer >();
	int functionId = esp.functionTable.getFunctionID(f);

	std::vector<Value*> actuals(0);
	actuals.resize(1);
	actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), functionId);
	return CallInst::Create(GenerateJobId, actuals, "", insertBefore);
}

void RemoteCall::createProduceFArgs(Function* f, Instruction* I, Value* jobId, Instruction *insertBefore) {
	
	//EsperantoNamer& esperantoNamer = getAnalysis< EsperantoNamer >();
	EspInitializer& esp = getAnalysis< EspInitializer >();
	InstMarker& iMarker =getAnalysis< InstMarker >();
	Module *M = f->getParent();
	LLVMContext &Context = M->getContext();
	const DataLayout &dataLayout = M->getDataLayout();
	std::vector<Value*> actuals(0);

	CallInst* ci = (CallInst*)I;
	size_t argSize = f->arg_size();
	Value* pointer = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
	int sum = 0;
	bool isFirst = true;	
	// for each func args, add it arg list.
	for (size_t i = 0; i < argSize; ++i) {
		Value* argValue = ci->getArgOperand(i); // original argument

		Type* type = argValue->getType(); // original type
    if(!type->isPointerTy()){
		const size_t sizeInBits = dataLayout.getTypeAllocSizeInBits(type); // allocation type size
		sum += (int)sizeInBits;
		Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1); 
		AllocaInst* alloca = new AllocaInst(type, one, (sizeInBits/8),"", insertBefore); // allocate buffer on stack
		if(isFirst) {
			pointer = (Value*) alloca;
			isFirst = false;
		}
		new StoreInst(argValue, alloca, insertBefore); // copy values to buffer
    actuals.resize(0);
    actuals.resize(3);

	  Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
	  InstInsertPt out = InstInsertPt::Before(insertBefore);
    actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), rc_id);
    actuals[1] = Casting::castTo((Value*)alloca, temp, out, &dataLayout);
    actuals[2] = ConstantInt::get(Type::getInt32Ty(Context), (sizeInBits/8));

    CallInst::Create(PushArgument,actuals,"",insertBefore);
    }
    else{
      sum += 32;
      Type* newType = Type::getInt32Ty(Context);
      Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1); 
		  AllocaInst* alloca = new AllocaInst(newType, one, 4,"", insertBefore); // allocate buffer on stack
      InstInsertPt out = InstInsertPt::Before(insertBefore);
      Value* addrIn32 = Casting::castTo(argValue, ConstantInt::get(Type::getInt32Ty(Context),0),out,&dataLayout);
      new StoreInst(addrIn32,alloca,insertBefore);

      actuals.resize(0);
      actuals.resize(3);
      Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
      out = InstInsertPt::Before(insertBefore);
      actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), rc_id);
      actuals[1] = Casting::castTo((Value*)alloca, temp, out, &dataLayout);
      actuals[2] = ConstantInt::get(Type::getInt32Ty(Context), 4);

      CallInst::Create(PushArgument,actuals,"",insertBefore);

    }
	}
	sum /= 8;
	
	// XXX:'sum' can occur align problem.
  actuals.resize(0);
	actuals.resize(2);
	actuals[0] = jobId;
	//actuals[1] = Casting::castTo(pointer, temp, out, &dataLayout);
	actuals[1] = ConstantInt::get(Type::getInt32Ty(Context),rc_id);
	//actuals[2] = ConstantInt::get(Type::getInt32Ty(Context), sum);
	CallInst* new_ci = CallInst::Create(ProduceFunctionArgument,  actuals, "", insertBefore);

  rc_id++;
	return;
}

Instruction* RemoteCall::createConsumeReturn(Function* f, Value* JobId, Instruction* I) {
	Module* M = f->getParent();
	const DataLayout &dataLayout = M->getDataLayout();
	std::vector<Value*> actuals(0);

	actuals.resize(1);
	Type* type = f->getReturnType();
	if (type->getTypeID() == Type::VoidTyID) {
		// case: return type is void
		actuals[0] = JobId;
		CallInst* ci = CallInst::Create(ConsumeReturn, actuals, "", I);
		
		// add substituted call inst info.
		removedCallInst.push_back(I);
		substitutedCallInst.push_back(ci);
		return NULL;
	} else {
		// case: return type is not void
		actuals[0] = JobId;
		CallInst* ci = CallInst::Create(ConsumeReturn, actuals, "", I);
		//InstInsertPt out = InstInsertPt::Before(I);
		Value* temp = ConstantPointerNull::get(f->getReturnType()->getPointerTo(0));
		
		// cast to void* to pointer type of target
		Instruction* castedCI = (Instruction*)EspUtils::insertCastingBefore(ci,temp,&dataLayout,I);
			//Casting::castTo(ci, temp, out, &dataLayout);
		LoadInst* loadedCI = new LoadInst(castedCI, "", I);
		
		// add substituted call inst info.
		removedCallInst.push_back(I);
		substitutedCallInst.push_back(loadedCI);
		return loadedCI;
	}
	return NULL;
}

void RemoteCall::removeOriginalCallInst() {
	assert(removedCallInst.size() == substitutedCallInst.size() && "callinst substitution is crashed");

	for(size_t i = 0; i < removedCallInst.size(); ++i) {
		Instruction* old_ci = removedCallInst[i];
		Instruction* new_ci = substitutedCallInst[i];
		
		std::vector<User*> targets;
		for(Value::user_iterator ui = old_ci->user_begin(), ue = old_ci->user_end(); ui != ue; ++ui) {
			User* target = *ui; // instruction that uses callinst value

			targets.push_back(target);
		}

		for(std::vector<User*>::iterator ui = targets.begin(), ue = targets.end(); ui != ue; ++ui) {
			User* target = *ui;
			target->replaceUsesOfWith(old_ci, new_ci);
		}
	}

	for(size_t i = 0; i < removedCallInst.size(); ++i) {
		Instruction* old_ci = removedCallInst[i];
		old_ci->eraseFromParent();
	}
	return;
}


void RemoteCall::generateFunctionTableProfile(){
	//EsperantoNamer& en = getAnalysis<EsperantoNamer>();
	EspInitializer& ei = getAnalysis<EspInitializer>();
	char filename[50];
	int funcNum = 0;
	//DeviceMapEntry* dme = new DeviceMapEntry();
	//dme->setName(DeviceName.c_str());
	printf("device name : %s, id : %d\n",DeviceName.c_str(),ei.DITable.getDeviceID(DeviceName));
	sprintf(filename,"functionTable-%d",ei.DITable.getDeviceID(DeviceName));
	FILE* output = fopen(filename,"w");
	typedef std::map<Function*, bool>::iterator FI;
	//LoadNamer& loadNamer = getAnalysis< LoadNamer >();
	for(FI fi = localFunctionTable.begin(), fe = localFunctionTable.end(); fi != fe; ++fi){
		//Function* func = (Function*)(fi->first);
		if(fi->second){
			funcNum++;
		}
	}
	fprintf(output,"%d\n",funcNum);	
	printf("funcNum : %d\n",funcNum);
	for(FI fi = localFunctionTable.begin(), fe = localFunctionTable.end(); fi != fe; ++fi){
		Function* func = (Function*)(fi->first);
		if(fi->second){
			int functionID = ei.functionTable.getFunctionID(func);
				//loadNamer.getFunctionId(*func);
			printf("function ID : %d\n", functionID);
			fprintf(output,"%d ",functionID);
		}
	}	
	fclose(output);

}



/*
void RemoteCall::substituteRemoteCall(Module& M) {
	LoadNamer& loadNamer = getAnalysis< LoadNamer >();
	
	typedef Module::iterator FF;
	typedef Function::iterator BB;
	typedef BasicBlock::iterator II;

	for(FF FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
		Function* F = (Function*) &*FI;
		if (F->isDeclaration()) continue;
		int functionId = loadNamer.getFunctionId(*F);
		if(functionId == 0) continue;

		for(BB BI = F->begin(), BE = F->end(); BI != BE; ++BI) {
			BasicBlock* B = (BasicBlock*) &*BI;

			for(II Ii = B->begin(), Ie = B->end(); Ii != Ie; ++Ii) {
				Instruction* targetInstruction = (Instruction*) &*Ii;
				if(!isa<CallInst>(targetInstruction)) continue;

				// check the metadata which is attached to attributes
				MDNode* md = targetInstruction->getMetadata("comp");
				if (md == NULL) continue;

				CallInst* ci = (CallInst*)targetInstruction;
					
				// check device name and type(architecture) are same with arguments 
				StringRef compName = cast<MDString>(md->getOperand(0).get())->getString();
				StringRef compType = cast<MDString>(md->getOperand(1).get())->getString();
				Function* calledFunction = ci->getCalledFunction();
				if (calledFunction == NULL) continue;
				int calledFunctionId = loadNamer.getFunctionId(*calledFunction);
				if(calledFunctionId == 0) continue;

				if(compName.equals(DeviceName) && compType.equals(DeviceType)){ 
					localFunctionTable[calledFunction] = true;	
					continue;
				}
				localFunctionTable[calledFunction] = false;	
				
				Instruction* jobId = createJobId(calledFunction, targetInstruction);
				createProduceFArgs(calledFunction, targetInstruction, (Value*)jobId, targetInstruction);
				createConsumeReturn(calledFunction, (Value*)jobId, targetInstruction);
			}
		}
	}
	
	removeOriginalCallInst();
}

Instruction* RemoteCall::createJobId(Function* f, Instruction *insertBefore){
	LLVMContext& Context = getGlobalContext();
	LoadNamer& loadNamer = getAnalysis< LoadNamer >();
	int functionId = loadNamer.getFunctionId(*f);

	std::vector<Value*> actuals(0);
	actuals.resize(1);
	actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), functionId);
	return CallInst::Create(GenerateJobId, actuals, "", insertBefore);
}

void RemoteCall::createProduceFArgs(Function* f, Instruction* I, Value* jobId, Instruction *insertBefore) {
	
	LLVMContext &Context = getGlobalContext();
	EsperantoNamer& esperantoNamer = getAnalysis< EsperantoNamer >();
	Module *M = f->getParent();
	const DataLayout &dataLayout = M->getDataLayout();
	std::vector<Value*> actuals(0);

	CallInst* ci = (CallInst*)I;
	size_t argSize = f->arg_size();
	Value* pointer = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
	int sum = 0;
	bool isFirst = true;	
	// for each func args, add it arg list.
	for (size_t i = 0; i < argSize; ++i) {
		Value* argValue = ci->getArgOperand(i); // original argument
		//if(strcmp(argValue->getName().data(), "this") == 0) continue; // XXX: pass "this" argument
		
		bool isClassMember = false;
		std::map<StringRef,GlobalVariable*>::iterator it;
		for(it = classMatching.begin();it!=classMatching.end();it++){
			if(strcmp((esperantoNamer.getClassNameInFunction(f->getName())).c_str(),(it->first).data())==0)
				isClassMember = true;
		}
		if(isClassMember && i==0) continue;
		Type* type = argValue->getType(); // original type
		const size_t sizeInBits = dataLayout.getTypeAllocSizeInBits(type); // allocation type size
		sum += (int)sizeInBits;
		Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1); 
		AllocaInst* alloca = new AllocaInst(type, one, "", insertBefore); // allocate buffer on stack
		if(isFirst) {
			pointer = (Value*) alloca;
			isFirst = false;
		}
		new StoreInst(argValue, alloca, insertBefore); // copy values to buffer
	}
	sum /= 8;
	InstInsertPt out = InstInsertPt::Before(insertBefore);
	Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
	
	// XXX:'sum' can occur align problem.
	actuals.resize(3);
	actuals[0] = jobId;
	actuals[1] = Casting::castTo(pointer, temp, out, &dataLayout);
	actuals[2] = ConstantInt::get(Type::getInt32Ty(Context), sum);
	CallInst::Create(ProduceFunctionArgument,  actuals, "", insertBefore);

	return;
}

Instruction* RemoteCall::createConsumeReturn(Function* f, Value* JobId, Instruction* I) {
	Module* M = f->getParent();
	const DataLayout &dataLayout = M->getDataLayout();
	std::vector<Value*> actuals(0);

	actuals.resize(1);
	Type* type = f->getReturnType();
	if (type->getTypeID() == Type::VoidTyID) {
		// case: return type is void
		actuals[0] = JobId;
		CallInst* ci = CallInst::Create(ConsumeReturn, actuals, "", I);
		
		// add substituted call inst info.
		removedCallInst.push_back(I);
		substitutedCallInst.push_back(ci);
		return NULL;
	} else {
		// case: return type is not void
		actuals[0] = JobId;
		CallInst* ci = CallInst::Create(ConsumeReturn, actuals, "", I);
		InstInsertPt out = InstInsertPt::Before(I);
		Value* temp = ConstantPointerNull::get(f->getReturnType()->getPointerTo(0));
		
		// cast to void* to pointer type of target
		Instruction* castedCI = (Instruction*) Casting::castTo(ci, temp, out, &dataLayout);
		LoadInst* loadedCI = new LoadInst(castedCI, "", I);
		
		// add substituted call inst info.
		removedCallInst.push_back(I);
		substitutedCallInst.push_back(loadedCI);
		return loadedCI;
	}
	return NULL;
}

void RemoteCall::removeOriginalCallInst() {
	assert(removedCallInst.size() == substitutedCallInst.size() && "callinst substitution is crashed");

	for(size_t i = 0; i < removedCallInst.size(); ++i) {
		Instruction* old_ci = removedCallInst[i];
		Instruction* new_ci = substitutedCallInst[i];
		
		std::vector<User*> targets;
		for(Value::user_iterator ui = old_ci->user_begin(), ue = old_ci->user_end(); ui != ue; ++ui) {
			User* target = *ui; // instruction that uses callinst value

			targets.push_back(target);
		}

		for(std::vector<User*>::iterator ui = targets.begin(), ue = targets.end(); ui != ue; ++ui) {
			User* target = *ui;
			target->replaceUsesOfWith(old_ci, new_ci);
		}
	}

	for(size_t i = 0; i < removedCallInst.size(); ++i) {
		Instruction* old_ci = removedCallInst[i];
		old_ci->eraseFromParent();
	}
	return;
}*/
