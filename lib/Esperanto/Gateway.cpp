/***
 *
 * Gateway.cpp : Code partitioner for Server.
 *
 *
 * **/

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
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Utilities/Casting.h"
#include "corelab/Metadata/NamedMetadata.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/Esperanto/Gateway.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>


using namespace corelab;
using namespace std;

char EsperantoServer::ID = 0;
static RegisterPass<EsperantoServer> X("esperanto-server", "Esperanto server-side(x86).", false, false);

void EsperantoServer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired< LoadNamer >();
	AU.addRequired< EsperantoNamer >();
	AU.setPreservesAll();
}

bool EsperantoServer::runOnModule(Module& M) {
	// Insert operation function to original codes.
	setFunctions(M);
	setIniFini(M);
	//createExecFunction(M);
	setCalls(M);

	return false;
}


#if 0
/// Function* createExecFunction(Module& M)
/// To construct function "execFunction", which get the function id and function arg list(32bit int list) 
/// and executes function corressponding with args. 
void EsperantoServer::createExecFunction(Module& M) {
	LLVMContext& Context = getGlobalContext();
	LoadNamer& loadNamer = getAnalysis< LoadNamer >();

	// Set basic blocks
	BasicBlock* entry = BasicBlock::Create(Context, "entry", execFunction);
	BasicBlock* loopEntry = BasicBlock::Create(Context, "loop_entry", execFunction);
	BasicBlock* consumeGlobal = BasicBlock::Create(Context, "consume_global", execFunction);
	BasicBlock* execExitCase = BasicBlock::Create(Context, "exitCase", execFunction);
	BasicBlock* ret = BasicBlock::Create(Context, "ret", execFunction);
	
	std::vector<Value*> actuals(0);
	Value* fId = (Value*) CallInst::Create(ConsumeFId, actuals, "", loopEntry);

	// Create base of switch instruction
	SwitchInst* selectExec = SwitchInst::Create(fId, ret, 1, loopEntry);
	ConstantInt* zero = ConstantInt::get(Type::getInt32Ty(Context), 0); // case of exit
	selectExec->addCase(zero, execExitCase);
	selectExec->setDefaultDest(consumeGlobal);
	selectExec = SwitchInst::Create(fId, ret, 1, consumeGlobal);
	actuals.resize(0);
	CallInst::Create(ExitFunc, actuals, "", execExitCase);

	// Consume global variables for each function call
	createConsumeGlobalVariable(selectExec);
	BranchInst::Create(loopEntry, entry);
	BranchInst::Create(ret, execExitCase);
	ReturnInst::Create(Context, 0, ret);
	std::vector<Function*> saving;


	// for each functions, create the case to call itself with own id.
	int function_num = 0;
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration())
			continue;

		int functionId = loadNamer.getFunctionId(*F);
		if (functionId == 0) continue;

		function_num++;

		// XXX:if not Esperanto target, remove it from server code
		if (!isEsperantoTarget(F)){
			continue;
		}
		saving.push_back(F);

		// Branch instruction : add case and basic blocks.
		BasicBlock *execFunc = BasicBlock::Create(Context, "execCase", execFunction);
		selectExec->addCase(ConstantInt::get(Type::getInt32Ty(Context), functionId), execFunc);
		BranchInst::Create(loopEntry, execFunc);
		
		// Get each argument type of function.
		size_t argSize = F->arg_size();
		Instruction* baseInst = execFunc->getFirstNonPHI();
		std::vector<Value*> Args(argSize);
		FunctionType* fcnType = F->getFunctionType(); 

		// Get function args
		for (size_t i= 0; i < argSize; ++i) {
			actuals.resize(1);
			Type* type = fcnType->getParamType(i);
			const size_t sizeInBytes = dataLayout.getTypeAllocSize(type);
			actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
			Value* arg = (Value*) CallInst::Create(ConsumeFArg, actuals, "", baseInst);
			InstInsertPt out = InstInsertPt::Before(baseInst);
			Value* temp = ConstantPointerNull::get(type->getPointerTo(0));
			Value* castedValue = Casting::castTo(arg, temp, out, &dataLayout);
			Value* loadedValue = new LoadInst(castedValue, "", baseInst);
			Args[i] = loadedValue;
		}
		
		// After all, call original function
		actuals.resize(0);
		CallInst::Create(RunFunction, actuals, "", baseInst); 
		CallInst* callInst = CallInst::Create(F, Args, "", baseInst);
		Value* retVal = (Value*) callInst;

		// Produce the return value to client
		Args.resize(2);
		if( retVal->getType()->getTypeID() == Type::VoidTyID ) {
			Args[0] = ConstantPointerNull::get(Type::getInt8PtrTy(Context)); // case of void type
			Args[1] = ConstantInt::get(Type::getInt32Ty(Context), 0);
		}
		else {
			Type* type = retVal->getType();
			const size_t sizeInBytes = dataLayout.getTypeAllocSize(type);
			Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1);
			AllocaInst* alloca = new AllocaInst(type, one, "", execFunction->begin()->getFirstNonPHI());
			new StoreInst(retVal, alloca, baseInst);
			InstInsertPt outRet = InstInsertPt::Before(baseInst);
			Value* tempRet = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
			Args[0] = Casting::castTo(alloca, tempRet, outRet, &dataLayout);
			Args[1] = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
		}
		createProduceGlobalVariable(baseInst);
		CallInst::Create(ProduceRet, Args, "", baseInst);
	}

	
	// Create new main function for server. It calls the constructor of server program,
	// stack initialization and execution function for server.
	// ( gwangmu: If 'main' does not exists, create a new 'main' function. )
	Function *fnMain = M.getFunction ("main");
	if (fnMain) {
		fnMain->deleteBody();
	}
	else {
		FunctionType *tyVoidVoidTy = FunctionType::get (Type::getVoidTy (M.getContext ()), false);
		fnMain = Function::Create (tyVoidVoidTy, GlobalValue::ExternalLinkage, "main", &M);
	}
#if 0
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration())
			continue;
		
		if(F->getName() == "main") {
#endif
			BasicBlock* retInMain = BasicBlock::Create(Context, "ret", fnMain);
			Type* retType = fnMain->getReturnType();

			Instruction *retInst = NULL;
			if (retType != Type::getVoidTy (M.getContext ())) {
				Value* retVal = Constant::getNullValue(retType);
				retInst = ReturnInst::Create(Context, retVal, retInMain);
			}
			else {
				retInst = ReturnInst::Create (Context, retInMain);
			}

			actuals.resize(0);

			CallInst::Create(initForCtr, "", retInst);		//XXX Constructor before main

			// XXX TEMPORARY!!! XXX
			//	- insert global initzer call
			// 	- change llvm.global_ctors' name (!removing this causes error!)
//			for (Module::iterator ifn = M.begin (); ifn != M.end (); ++ifn) {
//				if (ifn->getName().str().substr (0, 7) == "_GLOBAL") {
//					Function *fnGInitzer = &*ifn;
//					CallInst::Create (fnGInitzer, actuals, "", retInst);
//					fnGInitzer->setAttributes (AttributeSet ());
//				}
//			}
			if (Value *gctor = M.getGlobalVariable("llvm.global_ctors"))
				gctor->setName ("global_ctors_deprecated");
			// XXX TEMPORARY!!! XXX

			CallInst::Create(StackInit, actuals, "", retInst);
			CallInst::Create(execFunction, actuals, "", retInst);
#if 0
		} 
	}
#endif

/*	
	// FIXME:It has a possbility to skip the global variable uses of erased global variable
	for(unsigned int i = 0; i < removed.size(); ++i) {
		Function* f = removed[i];
		for(Value::use_iterator ui = f->use_begin(), ue = f->use_end(); ui != ue; ++ui) {
			User* u = *ui;
			if(isa<GlobalVariable>(u)) {
				GlobalVariable* gv = (GlobalVariable*) u;
				if(std::find(globalRemove.begin(), globalRemove.end(), gv)!=globalRemove.end()) continue;
				//fprintf(stderr, "Find gv %s\n", gv->getName().data());
				globalRemove.push_back(gv);
			}
		}
	}

	for(unsigned int i = 0; i < globalRemove.size(); ++i) {
		globalRemove[i]->eraseFromParent();
	}

	for(unsigned int i = 0; i < removed.size(); ++i) {
		// fprintf(stderr, " [Server] Remove %s\n", removed[i]->getName().data());
		if(removed[i]->getName() == "main") continue;
		removed[i]->eraseFromParent();
	}
	*/

	return;
}
#endif 

void EsperantoServer::setFunctions(Module &M) {
	LLVMContext &Context = getGlobalContext();

	Initialize = M.getOrInsertFunction(
			"GdeviceInitialize",
			Type::getVoidTy(Context),
			(Type*)0);
	
	Finalize = M.getOrInsertFunction(
			"GdeviceFinalize",
			Type::getVoidTy(Context),
			(Type*)0);

	IsSelfTarget = M.getOrInsertFunction(
			"GisSelfTarget",
			Type::getInt1Ty(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	ProduceFunctionTarget = M.getOrInsertFunction(
			"GproduceFunctionTarget",
			Type::getVoidTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	ProduceFunctionId = M.getOrInsertFunction(
			"GproduceFunctionId",
			Type::getVoidTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	ProduceFunctionArg = M.getOrInsertFunction(
			"GproduceFunctionArg",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
#if 0
	Initialize = M.getOrInsertFunction(
			"EsperantoServerInitialize",
			Type::getVoidTy(Context),
			(Type*)0);
	
	Open = M.getOrInsertFunction(
			"EsperantoServerOpen",
			Type::getVoidTy(Context),
			(Type*)0);

	Finalize = M.getOrInsertFunction(
			"EsperantoServerFinalize",
			Type::getVoidTy(Context),
			(Type*)0);
	
	StackInit = M.getOrInsertFunction(
			"EsperantoServerStackInit",
			Type::getVoidTy(Context),
			(Type*)0);

	ConsumeFId = M.getOrInsertFunction(
			"EsperantoServerConsumeFunctionId",
			Type::getInt32Ty(Context),
			(Type*)0);

	ConsumeFArg = M.getOrInsertFunction(
			"EsperantoServerConsumeFunctionArg",
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	RunFunction = M.getOrInsertFunction(
			"EsperantoServerRunFunction",
			Type::getVoidTy(Context),
			(Type*)0);
	
	ReturnFunction = M.getOrInsertFunction(
			"EsperantoServerReturnFunction",
			Type::getVoidTy(Context),
			(Type*)0);
	
	ProduceRet = M.getOrInsertFunction(
			"EsperantoServerProduceRet",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	ExitFunc = M.getOrInsertFunction(
			"EsperantoServerExit",
			Type::getVoidTy(Context),
			(Type*)0);
	
	Malloc = M.getOrInsertFunction(
			"Esperanto_malloc",
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	Free = M.getOrInsertFunction(
			"Esperanto_free",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);
#endif
	return;
}

void EsperantoServer::setIniFini(Module& M) {
	LLVMContext& Context = getGlobalContext();
	// exec function declarartion
	std::vector<Type*> formals(0);
	FunctionType* voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);
	//execFunction = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "EsperantoServerExecFunction", &M); // create funciton

	std::vector<Value*> actuals(0);

	/* initialize */
	initForCtr = Function::Create( 
			voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M); 
	BasicBlock* entry = BasicBlock::Create(Context,"entry", initForCtr); 
	BasicBlock* initBB = BasicBlock::Create(Context, "init", initForCtr); 
	actuals.resize(0);
	CallInst::Create(Initialize, actuals, "", entry); 
	BranchInst::Create(initBB, entry); 
	ReturnInst::Create(Context, 0, initBB);
	callBeforeMain(initForCtr, 0);					

	/* finalize */
	Function* finiForDtr = Function::Create(
			voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
	BasicBlock* finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
	BasicBlock* fini = BasicBlock::Create(Context, "fini", finiForDtr);
	actuals.resize(0);
	CallInst::Create(Finalize, actuals, "", fini);
	BranchInst::Create(fini, finiBB);
	ReturnInst::Create(Context, 0, fini);
	callAfterMain(finiForDtr);
	return;
}

void EsperantoServer::setCalls(Module& M) {
	typedef Module::iterator FF;
	typedef Function::iterator BB;
	typedef BasicBlock::iterator II;
	LoadNamer& loadNamer = getAnalysis< LoadNamer >();
	for(FF FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
		Function* F = (Function*) &*FI;
		if (F->isDeclaration()) continue;
		int functionId = loadNamer.getFunctionId(*F);
		if(functionId == 0) continue;

		for(BB BI = F->begin(), BE = F->end(); BI != BE; ++BI) {
			BasicBlock* B = (BasicBlock*) &*BI;

			for(II Ii = B->begin(), Ie = B->end(); Ii != Ie; ++Ii) {
				Instruction* I = (Instruction*) &*Ii;
				if(!isa<CallInst>(I)) continue;

				MDNode* md = I->getMetadata("comp");
				if (md == NULL) continue;
				callLists.push_back((CallInst*)I);
			}
		}
	}

	for(std::vector<CallInst*>::iterator ci = callLists.begin(), ce = callLists.end();
	ci != ce; ++ci) {
		CallInst* c = *ci;
		createCall(c);
	}
}

void EsperantoServer::createCall(CallInst* C) {
	LLVMContext& Context = getGlobalContext();
	EsperantoNamer& eNamer = getAnalysis< EsperantoNamer >();
	BasicBlock* basicB = C->getParent();
	Function* F = basicB->getParent();
	
	std::vector<Value*> actuals(0);
	actuals.resize(1);
	uint32_t id = eNamer.getIdCount(C);
	actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), id);
	CallInst* Target = CallInst::Create(IsSelfTarget, actuals, "", C);

	BasicBlock::iterator it = basicB->begin();
	while((Instruction*)(&*it) != (Instruction*)(Target)) {
		assert(it != basicB->end() && "cannot find target callinstruction");
		++it;
	}
	++it;

	BasicBlock* afterCallBlock = basicB->splitBasicBlock(it, "afterCall");
	
	BasicBlock* localCallBlock = BasicBlock::Create(Context, "localCall", F, basicB);

	BasicBlock* remoteCallBlock = BasicBlock::Create(Context, "remoteCall", F, basicB);
	BranchInst::Create(afterCallBlock, localCallBlock);
	BranchInst::Create(afterCallBlock, remoteCallBlock);
	TerminatorInst* basicTerminator = basicB->getTerminator();
	basicTerminator->eraseFromParent();
	BranchInst::Create(localCallBlock, remoteCallBlock, Target, basicB);
	//C->moveBefore(localReturn);

	actuals.resize(1);
//	actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), id);
//	CallInst::Create(ProduceFunctionTarget, actuals, "", remoteReturn);
}

void EsperantoServer::createProduceFId(Function* f, Instruction* I) {
	LLVMContext &Context = getGlobalContext();
	LoadNamer &loadNamer = getAnalysis< LoadNamer >();
	std::vector<Value*> actuals(1);
	int calledFunctionId = loadNamer.getFunctionId(*f); 
	Value* FId = ConstantInt::get(Type::getInt32Ty(Context), calledFunctionId);
	actuals[0] = FId;
	CallInst::Create(ProduceFunctionId, actuals, "", I);
	return;
}

void EsperantoServer::createProduceFArgs(Function* f, Instruction* I, Instruction *insertBefore) {
	LLVMContext &Context = getGlobalContext();
	Module *M = f->getParent();
	const DataLayout &dataLayout = M->getDataLayout();
	std::vector<Value*> actuals(0);

	CallInst* ci = (CallInst*)I;
	size_t argSize = f->arg_size();

	// for each func args, add it arg list.
	for (size_t i = 0; i < argSize; ++i) {
		actuals.resize(2);
		Value* argValue = ci->getArgOperand(i);
		Type* type = argValue->getType();
		const size_t sizeInBytes = dataLayout.getTypeAllocSize(type);
		Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1);
		AllocaInst* alloca = new AllocaInst(type, one, "", insertBefore);
		new StoreInst(argValue, alloca, insertBefore);
		InstInsertPt out = InstInsertPt::Before(insertBefore);
		Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
		actuals[0] = Casting::castTo(alloca, temp, out, &dataLayout);
		actuals[1] = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
		CallInst::Create(ProduceFunctionArg, actuals, "", insertBefore);
	}

	return;
}
