/***
 *
 * Server.cpp : Code partitioner for Server.
 *
 *
 * **/

#include "llvm/IR/LLVMContext.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Utilities/Casting.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/UVA/Server.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

//#define GLOBAL_DEBUG
//#define OFFLOADED_FUNCTION_ONLY

using namespace corelab;
using namespace std;

static const char* arrOffLibFns[] = {
	#define OFFLIB(x) #x,
	#include "offlib_fn.dat"
	#undef OFFLIB
};

static const char* arrLocalExtFns[] = {
	#define LOCAL(x) #x,
	#include "local_ext_fn.dat"
	#undef LOCAL
};

char OffloadServer::ID = 0;
static RegisterPass<OffloadServer> X("offload-server", "offload server-side(x86).", false, false);

void OffloadServer::getAnalysisUsage(AnalysisUsage &AU) const {
//	AU.addRequired< DataLayoutPass >();
	AU.addRequired< LoadNamer >();
	AU.setPreservesAll();
}

bool OffloadServer::runOnModule(Module& M) {
	initialize (M);

#if 0
	// FIXME: only for encoder_demo
	if (Function *fnH264ref = pM->getFunction ("h264ref_main"))
		fnH264ref->setName (string ("main"));
#endif

	// set function pointer translatation.
	fcnptr = new FunctionPointerTranslater (*pM);
	fcnptr->installTranslator (*pM, *loadNamer, *dataLayout);
	fcnptr->installBackTranslator (*pM, *loadNamer, *dataLayout);

	// Filter out global variables using pointer and move up to global variable.
	hoist = new HoistVariables (*pM);
	hoist->getGlobalVariableList (*pM);
	//hoist->distinguishGlobalVariables ();
	//hoist->createSubGlobalVariables (*pM, *dataLayout);
	hoist->createGlobalVariableFunctions (*dataLayout);
	
	// Insert operation function to original codes.
	set<Function *> setExtern = getNonOffLibAndLocalExtFns ();
	buildYieldFunctions (setExtern);
	
	// FIXME: insert pseudo offloadClientYMain.
	// 	The server, although it doesn't need this, also should have the one
	// 	because the linker complains undefined reference to it.
	// 	It would be better to separate entire offloading library
	// 	into two of the each server and client's one,
	// 	so that each other need not to care about the other's configuration.
	FunctionType *tyVoidVoidFn = FunctionType::get (Type::getVoidTy (*pC), false);
	Function *fnYMain = Function::Create (tyVoidVoidFn, GlobalValue::ExternalLinkage, "offloadClientYMain", pM);
	BasicBlock *blkYMain = BasicBlock::Create (*pC, "empty", fnYMain);
	new UnreachableInst (*pC, blkYMain);

	installInitFinal ();
	createExecFunction ();

	delete hoist;
	delete fcnptr;
	return false;
}

/// Function* createExecFunction(Module& M)
/// To construct function "fnExecFn", which get the function id and function arg list(32bit int list) 
/// and executes function corressponding with args. 
void OffloadServer::createExecFunction () {
	// exec function declarartion
	std::vector<Type*> formals(0);
	FunctionType* voidFcnVoidType = FunctionType::get(Type::getVoidTy(*pC), formals, false);
	Function *fnExecFn = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, 
			"offloadServerExecFunction", pM); // create funciton

	// Set basic blocks
	BasicBlock* entry = BasicBlock::Create(*pC, "entry", fnExecFn);
	BasicBlock* loopEntry = BasicBlock::Create(*pC, "loop_entry", fnExecFn);
	BasicBlock* consumeGlobal = BasicBlock::Create(*pC, "consume_global", fnExecFn);
	BasicBlock* execExitCase = BasicBlock::Create(*pC, "exitCase", fnExecFn);
	BasicBlock* ret = BasicBlock::Create(*pC, "ret", fnExecFn);
	
	std::vector<Value*> actuals(0);
	Value* fId = (Value*) CallInst::Create(cnstOffConsumeFID, actuals, "", loopEntry);

	// Create base of switch instruction
	SwitchInst* selectExec = SwitchInst::Create(fId, ret, 1, loopEntry);
	ConstantInt* zero = ConstantInt::get(Type::getInt32Ty(*pC), 0); // case of exit
	selectExec->addCase(zero, execExitCase);
	selectExec->setDefaultDest(consumeGlobal);
	selectExec = SwitchInst::Create(fId, ret, 1, consumeGlobal);
	actuals.resize(0);
	CallInst::Create(cnstOffExitFunc, actuals, "", execExitCase);

	// Consume global variables for each function call
	createConsumeGlobalVariable(selectExec);
	BranchInst::Create(loopEntry, entry);
	BranchInst::Create(ret, execExitCase);
	ReturnInst::Create(*pC, 0, ret);

	// for each functions, create the case to call itself with own id.
	int function_num = 0;
	for(Module::iterator fi = pM->begin(), fe = pM->end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration())
			continue;

		int functionId = loadNamer->getFunctionId(*F);
		if (functionId == 0) continue;

		function_num++;

		if (!isOffloadTarget (F))	continue;

		// Branch instruction : add case and basic blocks.
		BasicBlock *execFunc = BasicBlock::Create(*pC, "execCase", fnExecFn);
		selectExec->addCase(ConstantInt::get(Type::getInt32Ty(*pC), functionId), execFunc);
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
			const size_t sizeInBytes = dataLayout->getTypeAllocSize(type);
			actuals[0] = ConstantInt::get(Type::getInt32Ty(*pC), sizeInBytes);
			Value* arg = (Value*) CallInst::Create(cnstOffConsumeFArgs, actuals, "", baseInst);
			InstInsertPt out = InstInsertPt::Before(baseInst);
			Value* temp = ConstantPointerNull::get(type->getPointerTo(0));
			Value* castedValue = Casting::castTo(arg, temp, out, dataLayout);
			Value* loadedValue = new LoadInst(castedValue, "", baseInst);
			Args[i] = loadedValue;
		}
		
		// After all, call original function
		actuals.resize(0);
		CallInst::Create(cnstOffRunFn, actuals, "", baseInst); 
		CallInst* callInst = CallInst::Create(F, Args, "", baseInst);
		Value* retVal = (Value*) callInst;

		// Produce the return value to client
		Args.resize(2);
		if( retVal->getType()->getTypeID() == Type::VoidTyID ) {
			Args[0] = ConstantPointerNull::get(Type::getInt8PtrTy(*pC)); // case of void type
			Args[1] = ConstantInt::get(Type::getInt32Ty(*pC), 0);
		}
		else {
			Type* type = retVal->getType();
			const size_t sizeInBytes = dataLayout->getTypeAllocSize(type);
			Value* one = ConstantInt::get(Type::getInt32Ty(*pC), 1);
			AllocaInst* alloca = new AllocaInst(type, one, "", fnExecFn->begin()->getFirstNonPHI());
			new StoreInst(retVal, alloca, baseInst);
			InstInsertPt outRet = InstInsertPt::Before(baseInst);
			Value* tempRet = ConstantPointerNull::get(Type::getInt8PtrTy(*pC));
			Args[0] = Casting::castTo(alloca, tempRet, outRet, dataLayout);
			Args[1] = ConstantInt::get(Type::getInt32Ty(*pC), sizeInBytes);
		}
		CallInst::Create(cnstOffReturnFn, "", baseInst);
		createProduceGlobalVariable(baseInst);
		CallInst::Create(cnstOffProduceRet, Args, "", baseInst);
	}
	
	// Create new main function for server. It calls the constructor of server program,
	// stack initialization and execution function for server.
	// ( gwangmu: If 'main' does not exists, create a new 'main' function. )
	Function *fnMain = pM->getFunction ("main");
	if (fnMain) {
		fnMain->deleteBody();
	}
	else {
		FunctionType *tyVoidVoidTy = FunctionType::get (Type::getVoidTy (pM->getContext ()), false);
		fnMain = Function::Create (tyVoidVoidTy, GlobalValue::ExternalLinkage, "main", pM);
	}

	BasicBlock* retInMain = BasicBlock::Create(*pC, "ret", fnMain);
	Type* retType = fnMain->getReturnType();

	Instruction *retInst = NULL;
	if (retType != Type::getVoidTy (pM->getContext ())) {
		Value* retVal = Constant::getNullValue(retType);
		retInst = ReturnInst::Create(*pC, retVal, retInMain);
	}
	else {
		retInst = ReturnInst::Create (*pC, retInMain);
	}

	actuals.resize(0);

	CallInst::Create(cnstOffStackInit, actuals, "", retInst);
	CallInst::Create(fnExecFn, actuals, "", retInst);

	return;
}

void OffloadServer::initialize (Module &M) {
	this->pM = &M;
	this->pC = &pM->getContext ();
	this->loadNamer = &getAnalysis< LoadNamer >();
	//this->dataLayout = &getAnalysis< DataLayoutPass >().getDataLayout();
  this->dataLayout = &(M.getDataLayout());

	cnstOffInit = pM->getOrInsertFunction(
			"ServerInitialize",
			Type::getVoidTy(*pC),
			(Type*)0);
	
	// cnstOffOpen = pM->getOrInsertFunction(
	//		"offloadServerOpen",
	//		Type::getVoidTy(*pC),
	//		(Type*)0);

	cnstOffFinal = pM->getOrInsertFunction(
			"ServerFinalize",
			Type::getVoidTy(*pC),
			(Type*)0);
	
	cnstOffStackInit = pM->getOrInsertFunction(
			"offloadServerStackInit",
			Type::getVoidTy(*pC),
			(Type*)0);

	cnstOffConsumeFID = pM->getOrInsertFunction(
			"offloadServerConsumeFunctionId",
			Type::getInt32Ty(*pC),
			(Type*)0);

	cnstOffConsumeFArgs = pM->getOrInsertFunction(
			"offloadServerConsumeFunctionArg",
			Type::getInt8PtrTy(*pC),
			Type::getInt32Ty(*pC),
			(Type*)0);

	cnstOffRunFn = pM->getOrInsertFunction(
			"offloadServerRunFunction",
			Type::getVoidTy(*pC),
			(Type*)0);
	
	cnstOffReturnFn = pM->getOrInsertFunction(
			"offloadServerReturnFunction",
			Type::getVoidTy(*pC),
			(Type*)0);
	
	cnstOffProduceRet = pM->getOrInsertFunction(
			"offloadServerProduceRet",
			Type::getVoidTy(*pC),
			Type::getInt8PtrTy(*pC),
			Type::getInt32Ty(*pC),
			(Type*)0);

	cnstOffExitFunc = pM->getOrInsertFunction(
			"offloadServerExit",
			Type::getVoidTy(*pC),
			(Type*)0);
	
	cnstOffProduceYFID = pM->getOrInsertFunction (
			"offloadServerProduceYFunctionId",
			Type::getVoidTy(*pC),	
			Type::getInt32Ty(*pC), 
			(Type*)0);

	cnstOffProduceYFArgs = pM->getOrInsertFunction (
			"offloadServerProduceYFunctionArg",
			Type::getVoidTy(*pC), 
			Type::getInt8PtrTy(*pC), 
			Type::getInt32Ty(*pC), 
			(Type*)0);

	cnstOffRunYFn = pM->getOrInsertFunction(
			"offloadServerRunYFunction",
			Type::getVoidTy(*pC),
			(Type*)0);
	
	cnstOffConsumeYRet = pM->getOrInsertFunction (
			"offloadServerConsumeYRet",
			Type::getInt8PtrTy(*pC), 
			Type::getInt32Ty(*pC), 
			(Type*)0);
		
	for (unsigned i = 0; i < sizeof(arrOffLibFns) / sizeof(const char *); ++i) 
		setOffLibFns.insert (string (arrOffLibFns[i]));

	for (unsigned i = 0; i < sizeof(arrLocalExtFns) / sizeof(const char *); ++i) 
		setLocalExtFns.insert (string (arrLocalExtFns[i]));

	return;
}


/// bool isOffloadTarget(Function* F)
/// Read the offloadTarget from profiling results.
bool OffloadServer::isOffloadTarget(Function* F) {
	FILE* profile = fopen("offloadFunction.profile", "r");
	if(profile == NULL) {
		fprintf(stderr, "offloadFunction.profile doesn't exits\n");
		exit(1);
	}

	char strbuf[512];
	std::vector<int> funcIdList;
	while (fgets (strbuf, sizeof(strbuf), profile)) {
		int id = 0;
		int access = 0;
		sscanf(strbuf, "%d : %d\n", &id, &access);
		assert(id != 0 && "Reading offloadFunction.profile is failed");
		funcIdList.push_back(id);
	}

	int funcID = loadNamer->getFunctionId(*F);
	for (std::vector<int>::iterator ii = funcIdList.begin(),
			ie = funcIdList.end(); ii != ie; ++ii) {
		if (funcID == *ii) {
			fprintf(stderr, "[Server] target %d\n", *ii);
			fclose(profile);
			return true;
		}
	}
	fclose(profile);
	return false;
}

void OffloadServer::installInitFinal () {
	std::vector<Type*> formals(0);
	FunctionType* voidFcnVoidType = FunctionType::get(Type::getVoidTy(*pC), formals, false);

	/* initialize */
	Function *fnCtor = Function::Create (voidFcnVoidType, 
			GlobalValue::InternalLinkage, "__constructor__", pM); 
	BasicBlock* blkInit = BasicBlock::Create (*pC, "init", fnCtor); 
	ReturnInst *instInitTerm = ReturnInst::Create (*pC, 0, blkInit);

	// fill in the block
  // CallInst::Create (cnstOffOpen, "", instInitTerm); 
	CallInst::Create (cnstOffInit, "", instInitTerm);
	fcnptr->installToAddrRegisters (*pM, instInitTerm, *loadNamer, *dataLayout);
	fcnptr->consumeFunctionPointers (*pM, instInitTerm, *loadNamer, *dataLayout);
	//hoist->hoistGlobalVariable (*pM, instInitTerm, *dataLayout);
	hoist->createServerInitializeGlobalVariable (instInitTerm);

	// insert global initzer call
	for (Module::iterator ifn = pM->begin (); ifn != pM->end (); ++ifn) {
		if (ifn->getName().str().substr (0, 7) == "_GLOBAL") {
			Function *fnGInitzer = &*ifn;
			CallInst::Create (fnGInitzer, "", instInitTerm);
			fnGInitzer->setAttributes (AttributeSet ());
		}
	}

	// change llvm.global_ctors' name (!removing llvm.global_ctors may cause error!)
	if (GlobalVariable *gctor = pM->getGlobalVariable ("llvm.global_ctors")) {
		Constant *cnstArrCtor = gctor->getInitializer ();
		ArrayType *tyArrCtor = dyn_cast<ArrayType> (cnstArrCtor->getType ());

		for (unsigned i = 0; i < tyArrCtor->getNumElements (); i++) {
			Constant *cnstOCtor = cnstArrCtor->getAggregateElement(i)->getAggregateElement (1);
			CallInst::Create (cnstOCtor, "", blkInit->getTerminator ());
		}

		gctor->setName ("global_ctors_deprecated");
	}

	callBeforeMain (fnCtor, 65536);
	
	/* finalize */
	Function* fnDtor = Function::Create (voidFcnVoidType, 
			GlobalValue::InternalLinkage, "__destructor__", pM);
  BasicBlock* blkFinal = BasicBlock::Create (*pC, "entry", fnDtor);

	CallInst::Create(cnstOffFinal, "", blkFinal);
  ReturnInst::Create(*pC, 0, blkFinal);
	
	callAfterMain (fnDtor);

	return;
}

set<Function *> OffloadServer::getNonOffLibAndLocalExtFns () {
	set<Function *> setExtern;

	for (Module::iterator ifn = pM->begin (); ifn != pM->end (); ifn++) {
		Function *fn = &*ifn;
		string fnname = fn->getName().str ();

		if (fn->isDeclaration () && !fn->isIntrinsic () &&
				setOffLibFns.find (fnname) == setOffLibFns.end () &&
				setLocalExtFns.find (fnname) == setLocalExtFns.end () &&
				loadNamer->getFunctionId (*fn) != 0)
			setExtern.insert (fn);
	}

	return setExtern;
}

void OffloadServer::buildYieldFunctions (set<Function *> setTargets) {
	for (set<Function *>::iterator itar = setTargets.begin ();
			 itar != setTargets.end(); ++itar) {
		Function *fnLocal = *itar;
		string fnname = fnLocal->getName().str();

		Function *fnOffEntry = Function::Create (fnLocal->getFunctionType (), 
			GlobalValue::ExternalLinkage, "", pM);
		fnOffEntry->copyAttributesFrom (fnLocal);
		fnLocal->replaceAllUsesWith (fnOffEntry);

		// create remote call function
		string strRName = fnname + string ("_remote");
		Type *tyRet = fnLocal->getReturnType ();
		FunctionType *tyFnRemote = FunctionType::get (tyRet, false);
		Function *fnRemote = Function::Create (tyFnRemote, GlobalValue::InternalLinkage, 
			strRName, pM);
		BasicBlock *bbRemote = BasicBlock::Create (*pC, "remote", fnRemote);

		UnreachableInst *instTmpTerm = new UnreachableInst (*pC, bbRemote);

		Constant *cnstPad = ConstantInt::get (Type::getInt32Ty (*pC), 4096);
		new AllocaInst(Type::getInt8Ty(*pC), cnstPad, "", instTmpTerm);
		CallInst::Create(cnstOffRunYFn, "", instTmpTerm);

#ifndef SPECULATIVE_YIELDING
		createConsumeGlobalVariable(instTmpTerm);
		Instruction *instRet = createConsumeYRet(fnLocal, instTmpTerm);

		instTmpTerm->eraseFromParent ();
		if (tyRet == Type::getVoidTy (*pC))
			ReturnInst::Create (*pC, bbRemote);
		else
			ReturnInst::Create (*pC, instRet, bbRemote);
#else
		instTmpTerm->eraseFromParent ();
		if (tyRet == Type::getVoidTy (*pC))
			ReturnInst::Create (*pC, bbRemote);
		else
			ReturnInst::Create (*pC, Constant::getNullValue (tyRet), bbRemote);
#endif


		// create offloading entry function
		BasicBlock *blkRemoteCall = BasicBlock::Create (*pC, "call.remote", fnOffEntry);
		Instruction *instRCallTerm = new UnreachableInst (*pC, blkRemoteCall);

		vector<Value *> vecArgs;
		for (Function::arg_iterator iarg = fnOffEntry->arg_begin ();
				 iarg != fnOffEntry->arg_end (); iarg++)
			vecArgs.push_back (&*iarg);

		createProduceYFID(fnLocal, instRCallTerm);
		createProduceGlobalVariable(instRCallTerm);
		createProduceYFArgs(fnLocal, vecArgs, instRCallTerm);
		Instruction *instRemoteCall = CallInst::Create (fnRemote, "", instRCallTerm);

		instRCallTerm->eraseFromParent ();

		// (if not void return) create & install PHINode for BLK_JOIN
		if (fnLocal->getReturnType () != Type::getVoidTy (*pC)) 
			ReturnInst::Create (*pC, instRemoteCall, blkRemoteCall);
	 	else 
			ReturnInst::Create (*pC, blkRemoteCall);

		fnOffEntry->setName (fnLocal->getName().str () + string ("_yield"));
		//fnLocal->eraseFromParent ();
	}
}

Instruction* OffloadServer::createConsumeYRet (Function* fn, Instruction* instBefore) {
	vector<Value*> vecArgs(0);
	Instruction *instRet = NULL;

	Type* tyRet = fn->getReturnType();
	if (tyRet->getTypeID () == Type::VoidTyID) {
		vecArgs.push_back (ConstantInt::get (Type::getInt32Ty (*pC), 0));
		CallInst::Create (cnstOffConsumeYRet, vecArgs, "", instBefore);
	}
	else {
		const size_t tysize = dataLayout->getTypeAllocSize (tyRet);
		vecArgs.push_back (ConstantInt::get (Type::getInt32Ty (*pC), tysize));
		CallInst* instCall = CallInst::Create (cnstOffConsumeYRet, vecArgs, "", instBefore);

		InstInsertPt iOut = InstInsertPt::Before (instBefore);
		//Value *valTemp = ConstantPointerNull::get (fn->getReturnType()->getPointerTo (0));
		//Instruction *instCast = dyn_cast<Instruction> (Casting::castTo (
		//			instCall, valTemp, iOut, dataLayout));
		Instruction *instCast = CastInst::CreateBitOrPointerCast (instCall, fn->getReturnType()->getPointerTo (), "", instBefore);
		LoadInst *instLoad = new LoadInst (instCast, "", instBefore);

		instRet = instLoad;
	}

	return instRet;
}

void OffloadServer::createProduceYFID (Function* fn, Instruction* instBefore) {
	vector<Value*> vecArgs;

	Value* valFID = ConstantInt::get (Type::getInt32Ty(*pC), loadNamer->getFunctionId (*fn));
	vecArgs.push_back (valFID);

	CallInst::Create (cnstOffProduceYFID, vecArgs, "", instBefore);

	return;
}

void OffloadServer::createProduceYFArgs (Function* fn, vector<Value *> vecArgs, Instruction *instBefore) {
	size_t cntArgs = fn->arg_size();

	// for each func args, add it arg list.
	for (size_t i = 0; i < cntArgs; ++i) {
		Value* valArg = vecArgs[i];
		Type* tyArg = valArg->getType ();
		const size_t tysize = dataLayout->getTypeAllocSize (tyArg);

		AllocaInst* instAlloca = new AllocaInst (tyArg, "", instBefore);
		new StoreInst (valArg, instAlloca, instBefore);

		InstInsertPt iOut = InstInsertPt::Before (instBefore);
		Value *valTemp = ConstantPointerNull::get (Type::getInt8PtrTy (*pC));
		Value *valCast = Casting::castTo (instAlloca, valTemp, iOut, dataLayout);

		vector<Value *> vecArgs;
		vecArgs.push_back (valCast);
		vecArgs.push_back (ConstantInt::get(Type::getInt32Ty(*pC), tysize));
		CallInst::Create (cnstOffProduceYFArgs, vecArgs, "", instBefore);
	}

	return;
}
			

void OffloadServer::createProduceGlobalVariable(Instruction* I) {
	hoist->createServerProduceGlobalVariable(I);
	return;
}

void OffloadServer::createConsumeGlobalVariable(Instruction* I) {
	hoist->createServerConsumeGlobalVariable(I);
	return;
}

