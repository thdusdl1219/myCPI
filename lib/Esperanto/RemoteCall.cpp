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
#include <algorithm>

using namespace corelab;
using namespace std;

char RemoteCall::ID = 0;
static RegisterPass<RemoteCall> X("remote-call-test", "substitute remote call instructions.", false, false);

static bool hasFunction (Constant* cnst);
static void findGV(Module &M, vector<GlobalVariable*> &vecGvars);

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

  // void uva_sync();
  UvaSync = M.getOrInsertFunction(
      "uva_sync",
      Type::getVoidTy(Context),
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
				if(!isa<CallInst>(targetInstruction) && !isa<InvokeInst>(targetInstruction)) continue;
				//targetInstruction->dump();
				//DEBUG(errs() << "before check metadata namer\n");
	
				// check the metadata which is attached to attributes
				MDNode* md = targetInstruction->getMetadata("namer");
				if (md == NULL) continue;

        CallInst* ci;
        InvokeInst* ii;
        bool isCallInst = false;
        if(isa<CallInst>(targetInstruction)){
				  ci = (CallInst*)targetInstruction;
          isCallInst = true;
        }
        else if(isa<InvokeInst>(targetInstruction))
          ii = (InvokeInst*)targetInstruction;
					
				// check device name and type(architecture) are same with arguments 
				Value* deviceIDAsValue = cast<ValueAsMetadata>(md->getOperand(0).get())->getValue();
				Constant* deviceIDAsConstant = cast<Constant>(deviceIDAsValue);
				ConstantInt* deviceIDAsConstantInt = cast<ConstantInt>(deviceIDAsConstant);
				int deviceID = (int)deviceIDAsConstantInt->getZExtValue();
				//DEBUG(errs() << "this function has namer metadata : did = " << deviceID << "\n");
				//StringRef compType = cast<MDString>(md->getOperand(1).get())->getString();
        Function* calledFunction;
        if(isCallInst)
				  calledFunction = ci->getCalledFunction();
        else
          calledFunction = ii->getCalledFunction();
				if (calledFunction == NULL) continue;
				//DEBUG(errs() << "called function is not null"<< "\n");

				int calledFunctionId = esp.functionTable.getFunctionID(calledFunction);
				if(calledFunctionId < 0) continue;
									//DEBUG(errs() << "called function's id is not 0"<< "\n");
				
        /*  
        // BONG BONG BONG BONG BONG 
        // BEGIN: all function body has to include uva_sync call on both entry and exit(s).
        // entry
        std::vector<Value*> actuals(0);
        BasicBlock &entry = calledFunction->getEntryBlock();
        Instruction *nonphi = entry.getFirstNonPHI();
        InstInsertPt out = InstInsertPt::Before(nonphi);
        out << CallInst::Create(UvaSync, actuals, ""); 
  
        // exit: function may have mutiple exit blocks...
        // Loop over all of the blocks in a function, tracking all of the blocks that
        // return.
        BasicBlock *ReturnBlock;
        BasicBlock *UnreachableBlock;
        std::vector<BasicBlock*> ReturningBlocks;
        std::vector<BasicBlock*> UnreachableBlocks;
        for (BasicBlock &I : *calledFunction)
          if (isa<ReturnInst>(I.getTerminator()))
            ReturningBlocks.push_back(&I);
          else if (isa<UnreachableInst>(I.getTerminator()))
            UnreachableBlocks.push_back(&I);

        // Then unreachable blocks. XXX: Check
        if (UnreachableBlocks.empty()) {
          UnreachableBlock = nullptr;
        } else if (UnreachableBlocks.size() == 1) {
          UnreachableBlock = UnreachableBlocks.front();
        } else {
          UnreachableBlock = BasicBlock::Create(calledFunction->getContext(), 
              "UnifiedUnreachableBlock", calledFunction);
          new UnreachableInst(calledFunction->getContext(), UnreachableBlock);

          for (std::vector<BasicBlock*>::iterator I = UnreachableBlocks.begin(),
              E = UnreachableBlocks.end(); I != E; ++I) {
            BasicBlock *BB = *I;
            BB->getInstList().pop_back();  // Remove the unreachable inst.
            BranchInst::Create(UnreachableBlock, BB);
          }
        }

        // Now handle return blocks.
        if (ReturningBlocks.empty()) {
          ReturnBlock = nullptr;
        } else {
          for (BasicBlock *BB : ReturningBlocks) {
            ReturnBlock = BB;
            out = InstInsertPt::Before(ReturnBlock->getTerminator());
            out << CallInst::Create(UvaSync, actuals, "");
          }
        }
        // BONG BONG BONG BONG BONG END: //
*/
        //printf("function id : %d ==> %s\n",calledFunctionId,calledFunction->getName().data());
        //printf("function & deviceID = %s / %d\n",calledFunction->getName().data(),deviceID);
        StringRef devName = StringRef(deviceName);
        if(esp.DITable.getDeviceID(devName) == deviceID){
          DEBUG(errs() << "This function " << calledFunction->getName().data() << " is localFunction : " <<  calledFunctionId<<"\n");
          //printf("This function is in device %d\n",deviceID);	
          localFunctionTable[calledFunction] = true;	
          continue;
        }
        else if(esp.MDTable.getDeviceName(StringRef("region"),StringRef(calledFunction->getName().drop_front())).size() != 0){
          //printf("inside elseif\n");
          if(deviceID != esp.DITable.getDeviceID(DeviceName)){
            //printf("OMG :: it is not my local region\n");
            eraseList.push_back(ci);
            continue;
          }
        }
        localFunctionTable[calledFunction] = false;	

        bool isAsync = false;
        for(auto async_ci : im.async_fcn_list){
          if(ci->getCalledFunction() == async_ci)
            isAsync = true;
        }
        if(isAsync){
          createProduceAsyncFArgs(calledFunction, targetInstruction, targetInstruction);        
        }
        else{
          // BONG BEGIN: uva_sync have to be called before and after remote call 
          std::vector<Value*> actuals(0);
          InstInsertPt out = InstInsertPt::Before(targetInstruction);
          out << CallInst::Create(UvaSync, actuals, "");
          out = InstInsertPt::After(targetInstruction);
          out << CallInst::Create(UvaSync, actuals, "");
          // BONG END: //
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

  // BONG BEGIN
  std::vector<GlobalVariable*> vecGvars;
  findGV(M, vecGvars);
  InstInsertPt out;
  std::vector<Value*> actuals(2);
  //actuals[0] = 0x15000000;
  //actuals[1] = ;
  std::vector<std::string> *vecClasses = new std::vector<std::string>();
  NamedMDNode *classNamedMD = M.getNamedMetadata("esperanto.class");
  if (classNamedMD != NULL) {
    MDNode *classMD = classNamedMD->getOperand(0);

    for (auto it = classMD->op_begin(); it != classMD->op_end(); ++it) {
      std::string className = cast<MDString>(it->get())->getString().str();
      vecClasses->push_back(className);
    }
  }
  for (GlobalVariable *gv : vecGvars) {
    if (gv->getType()->isPointerTy() 
        && gv->getType()->getPointerElementType()->isPointerTy()
        && gv->getType()->getPointerElementType()->getPointerElementType()->isStructTy()) {
      Type *type = gv->getType()->getPointerElementType()->getPointerElementType();
      StringRef name = type->getStructName();
      if (name.startswith_lower(StringRef("class."))) {
        std::string name_ = name.str();
        std::string realName = name_.substr(6);
        if (std::find(vecClasses->begin(), vecClasses->end(), realName) != vecClasses->end()) {
          //printf("\n\n############################\n");
          //printf("%s\n", realName.c_str());
          //gv->dump();
        }
      }
    }
    for(auto U : gv->users()){  // U is of type User*
      if (Instruction *I = dyn_cast<Instruction>(U)){
        if(StoreInst *SI = dyn_cast<StoreInst>(I)) {
          // an instruction uses V
          out = InstInsertPt::After(I);
          //out << CallInst::Create(UvaSync, actuals, "");
          
          //AllocaInst *alloca = AllocaInst::Create(Type::getInt8Ptr(Context), ConstantInt::get(Type::getInt32Ty(Context), num), 8);
          //out << StoreInst::Create( , alloca);
          //out << CallInst::Create(Waiter, actuals, "");
        }
      }
    }
  }
  // BONG END
}

void RemoteCall::createProduceAsyncFArgs(Function* f, Instruction* I, Instruction *insertBefore) {

  //EsperantoNamer& esperantoNamer = getAnalysis< EsperantoNamer >();
  EspInitializer& esp = getAnalysis< EspInitializer >();
  InstMarker& iMarker = getAnalysis< InstMarker >();
  Module *M = I->getModule();
  LLVMContext &Context = M->getContext();
  const DataLayout &dataLayout = M->getDataLayout();
  std::vector<Value*> actuals(0);
  if(isa<CallInst>(I)){

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
  }
  else if(isa<InvokeInst>(I)){
    InvokeInst* ci = (InvokeInst*)I;
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
  }
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
  Module *M = I->getModule();
  LLVMContext &Context = M->getContext();
  const DataLayout &dataLayout = M->getDataLayout();
  std::vector<Value*> actuals(0);
  DEBUG(errs() << "function " << f->getName().data() << " is remote call function " << esp.functionTable.getFunctionID(f) << "\n");
  if(isa<CallInst>(I)){

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
  }
  else if(isa<InvokeInst>(I)){
    InvokeInst* ci = (InvokeInst*)I;
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
  }
  return;
}

Instruction* RemoteCall::createConsumeReturn(Function* f, Value* JobId, Instruction* I) {
  Module* M = I->getModule();
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
	//printf("device name : %s, id : %d\n",DeviceName.c_str(),ei.DITable.getDeviceID(DeviceName));
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
	//printf("funcNum : %d\n",funcNum);
	for(FI fi = localFunctionTable.begin(), fe = localFunctionTable.end(); fi != fe; ++fi){
		Function* func = (Function*)(fi->first);
		if(fi->second){
			int functionID = ei.functionTable.getFunctionID(func);
				//loadNamer.getFunctionId(*func);
			//printf("function ID : %d\n", functionID);
			fprintf(output,"%d ",functionID);
		}
	}	
	fclose(output);

}


static bool hasFunction (Constant* cnst) {
		if (dyn_cast<Function> (cnst)) 	return true;

		for (User::op_iterator iop = cnst->op_begin ();
				 iop != cnst->op_end (); ++iop) {
			Constant *cnstOper = dyn_cast<Constant> (iop->get ());
			
			if (hasFunction (cnstOper))		return true;
		}

		return false;
	}

static void findGV(Module &M, vector<GlobalVariable*> &vecGvars) {
  for (Module::global_iterator igvar = M.global_begin ();
      igvar != M.global_end (); ++igvar) {
    GlobalVariable *gvar = &*igvar;

    // FIXME assume external, if it doesn't have an initializer.
    if ((!gvar->hasExternalLinkage () || gvar->hasInitializer ()) &&
        (gvar->getName().str().length () < 5 ||
         gvar->getName().str().substr (0, 5) != string ("llvm."))) {
      if (!gvar->isConstant ()) {
        //vecGvars.insert (gvar);
        vecGvars.push_back(gvar);
      } else {
        // WORKAROUND: If the global initializer takes functions,
        // it breaks the rule where function pointers of the server
        // must have a function's address of the client, ending
        // up with a devestating result.

        // It would be more ideal to fix the function translator pass,
        // but now, we just add a small workaround code here,
        // so that the initializer can be effectivly overlapped 
        // to the client's one at runtime.
        if (hasFunction (gvar->getInitializer ())) {
          gvar->setConstant (false);
          vecGvars.push_back(gvar);
        }	
      }
    }
  }
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
