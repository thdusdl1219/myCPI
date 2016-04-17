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
#include "corelab/Utilities/IniFini.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/Esperanto/HoistVariables.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <set>
#include <algorithm>
#define GLOBAL_DEBUG

#ifdef GLOBAL_DEBUG
//#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release builds */
#endif


namespace corelab {

using namespace std;

char HoistVariables::ID = 0;
static RegisterPass<HoistVariables> X("hoist-glovar", "map global variable into heap.", false, false);

void HoistVariables::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired< LoadNamer >();
	AU.setPreservesAll();
}

bool HoistVariables::runOnModule(Module& M) {
	const DataLayout dataLayout = M.getDataLayout();
	getGlobalVariableList(M);
	distinguishGlobalVariables();
	createSubGlobalVariables(M, dataLayout);
	
	setFunctions(M);
	setIniFini(M);
	return false;
}


void HoistVariables::setFunctions(Module& M) {
	LLVMContext &Context = getGlobalContext();
	Malloc = M.getOrInsertFunction(
			"uva_malloc",
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	Free = M.getOrInsertFunction(
			"uva_free",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);

	Memcpy = M.getOrInsertFunction(
			"uva_memcpy",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	return;
}

void HoistVariables::setIniFini(Module& M) {
	const DataLayout dataLayout = M.getDataLayout();
	Function* init = getOrInsertConstructor(M);
	Function* fini = getOrInsertDestructor(M);

	Instruction* initInst = init->front().getFirstNonPHI();
	Instruction* finiInst = fini->front().getFirstNonPHI();

	//initBB
	deployGlobalVariable(M, initInst, dataLayout);
	hoistGlobalVariable(M, initInst, dataLayout);
	initializeGlobalVariable(M, initInst, dataLayout);
	
	//finalBB
	freeHoistedGlobalVariable(M, finiInst, dataLayout);
}

void HoistVariables::getOuterInstructions(Constant* c, set<Instruction*>& iset, set<GlobalVariable*>& gset) {
	// end condition
	if(isa<GlobalVariable>(c)) {
		gset.insert((GlobalVariable*)c);
		return;
	}

	typedef Value::user_iterator UI;
	for (UI ui = c->user_begin(), ue = c->user_end(); ui != ue; ++ui) {
		User* u = *ui;
		if (isa<Instruction>(u)) {
			Instruction* inst = (Instruction*) u;
			iset.insert(inst);
		} else if (isa<Constant>(u)) {
			Constant* con = (Constant*) u;
			getOuterInstructions(con, iset, gset);
		} else {
			assert(0 && "Globalvariable is Operator????");
			exit(1);
		}
	}
	return;
}

void HoistVariables::getOuterInstructions(Constant* c, std::set<Instruction*>& iset) {
	// exit condition
	if(isa<GlobalVariable>(c)) {
		return;
	}

	typedef Value::user_iterator UI;
	for (UI ui = c->user_begin(), ue = c->user_end(); ui != ue; ++ui) {
		User* u = *ui;
		if (isa<Instruction>(u)) {
			Instruction* inst = (Instruction*) u;
			iset.insert(inst);
		} else if (isa<Constant>(u)) {
			Constant* con = (Constant*) u;
			getOuterInstructions(con, iset);
		} else {
			assert(0 && "Globalvariable is Operator????");
			exit(1);
		}
	}
	return;
}


void HoistVariables::getGlobalVariableList(Module& M){
	// find all the global variables
	typedef Module::global_iterator GI;
	for(GI g = M.global_begin(), e = M.global_end(); g!=e ; g++){
		GlobalVariable* gv = &*g;

		if(gv->getName() == "stderr") continue;
		if(gv->getName() == "stdout") continue;
		if(gv->getName() == "stdin") continue;

		globalVar.push_back(gv);
	}
}

void HoistVariables::createSubGlobalVariables(Module& M, const DataLayout& dataLayout){
	// Create Substitution of orignal global variable which new one indicates
	typedef std::vector<GlobalVariable*>::iterator GVI;
	for(GVI gi = Hoist.begin(), ge = Hoist.end(); gi != ge; ++gi) {
		GlobalVariable* gv = *gi;
		Type* typeGv = gv->getType();

		Constant* init = ConstantPointerNull::get((PointerType*)typeGv);
		char name[100] = "sub_";
		strcat(name, gv->getName().data());
		GlobalVariable* newGv = new GlobalVariable(M, typeGv, false, GlobalValue::CommonLinkage, init, name);
		subGlobalVar.push_back(newGv);
	}
	return;
}

bool HoistVariables::findGlobalVariableInConstant(Constant* c, GlobalVariable* g) {
	if (isa<GlobalVariable>(c)) {
		return true;
	} else if (isa<ConstantInt>(c)) {
		return false;
	} else if (isa<ConstantFP>(c)) {
		return false;
	} else if (isa<ConstantExpr>(c)||isa<ConstantStruct>(c)||isa<ConstantArray>(c)||isa<ConstantVector>(c)) {
		for(size_t i = 0; i < c->getNumOperands(); ++i) {
			bool internalRes = findGlobalVariableInConstant((Constant*)c->getOperand(i), g);
			if(internalRes) 
				return true;
		}
		return false;
	} else if (isa<ConstantAggregateZero>(c)) {
		return false;
	} else if (isa<ConstantPointerNull>(c)) {
		return false;
	} else if (isa<ConstantDataSequential>(c)) {
		return false;
	} else if (isa<BlockAddress>(c)) {
		return false;
	} else if (isa<GlobalValue>(c)) {
		return false;
	} 
	return false;
}


void HoistVariables::checkHoistedInstCases(Instruction* inst, GlobalVariable* gv, bool& isUsed) {
	if(gv->isConstant()) {
		return;
	}
	if(isa<StoreInst>(inst)) {
		Value* firstOp = inst->getOperand(0);
		if(isa<Constant>(firstOp)) {
			if(findGlobalVariableInConstant((Constant*)firstOp, gv)) {
				DEBUG_PRINT("[Global][Dividing] \"%s\" stored by inst %s\n", gv->getName().data(), inst->getName().data());
				isUsed = true;
			}
		}
	} else if(isa<CallInst>(inst)) {
		DEBUG_PRINT("[Global][Dividing] \"%s\" called by inst %s\n", gv->getName().data(), inst->getName().data());
		isUsed = true;
	} else if(isa<BitCastInst>(inst)) {
		DEBUG_PRINT("[Global][Dividing] \"%s\" casted by inst %s\n", gv->getName().data(), inst->getName().data());
		isUsed = true;
	} else if(isa<LoadInst>(inst)) { 
		// do nothing
	} else if(isa<GetElementPtrInst>(inst)) { 
		DEBUG_PRINT("[Global][Dividing] \"%s\" casted by inst %s\n", gv->getName().data(), inst->getName().data());
		isUsed = true;
	} else {
		// do nothing cases
		DEBUG_PRINT("[Global][OtherInst] \"%s\", inst %s, opcode %s\n", gv->getName().data(), inst->getName().data(), inst->getOpcodeName());
	}
	return;
}

void HoistVariables::distinguishGlobalVariables() {
	typedef std::vector<GlobalVariable*>::iterator GVI;
	typedef Value::user_iterator UI;

	DEBUG_PRINT(">> Dividing Global Variable starts\n");
	set<GlobalVariable*> InitNeededSet;
	for(GVI gi = globalVar.begin(), ge = globalVar.end(); gi != ge; ++gi) {
		GlobalVariable* gv = *gi;
		bool isUsed = false;
		for(UI ui = gv->user_begin(), ue = gv->user_end(); ui != ue; ++ui) {
			User* user = *ui;
			if (isa<Instruction>(user)) {
				// User case 1: instruction
				Instruction* inst = (Instruction*) user;
				checkHoistedInstCases(inst, gv, isUsed);
			} else if (isa<Constant>(user)) {
				// User case 2: constant
				Constant* con = (Constant*) user;
				set<Instruction*> iset;
				set<GlobalVariable*> gset;
				getOuterInstructions(con, iset, gset);

				// check outer instruction
				typedef set<Instruction*>::iterator SI;
				for(SI si = iset.begin(), se = iset.end(); si != se; ++si) {
					Instruction* outerInst = *si;
					checkHoistedInstCases(outerInst, gv, isUsed);
				}
				
				// case included in global variable
				typedef set<GlobalVariable*>::iterator SG;
				for(SG sgi = gset.begin(), sge = gset.end(); sgi != sge; ++sgi) {
					GlobalVariable* outerGv = *sgi;
					InitNeededSet.insert(outerGv);
					DEBUG_PRINT("[Global][Dividing] %s initailized in %s\n", gv->getName().data(), outerGv->getName().data());
					isUsed = true;
				}

			} else {
				assert(0 && "Globalvariable is Operator????");
				exit(1);
			}
		}
		
		// distinguish the global variables according to their type
		// Case1 : Hoist
		if (isUsed) {
			Hoist.push_back(gv);
			InitNeededSet.insert(gv);
		} else {
			if (gv->isConstant()) {
				// Case2 : Constant_NoUse
				// treat it as hoisted
				Hoist.push_back(gv);
				InitNeededSet.insert(gv);
				// Constant_NoUse.push_back(gv);
			} else {
				// Case3 : NoConstant_NoUse
				// treat it as hoisted
				Hoist.push_back(gv);
				InitNeededSet.insert(gv);
			}
		}
	}

	// sorting
	vector<GlobalVariable*>InitTemp;
	InitTemp.assign(InitNeededSet.begin(), InitNeededSet.end());
	for(size_t i = 0; i < globalVar.size(); ++i) {
		for(size_t j = 0; j < InitTemp.size(); ++j) {
			if(globalVar[i] == InitTemp[j]) {
				InitNeeded.push_back(InitTemp[j]);
				break;
			}
		}
	}
	DEBUG_PRINT(">> Dividing Global Variable ends\n");
	return;
}

// deployGlobalVariable(): allocate the pointer of old global variable to heap and 
// make sub_global variable to address the old global variable on heap.
void HoistVariables::deployGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	DEBUG_PRINT(">> Deploying Global Variable starts\n");
	assert(Hoist.size() == subGlobalVar.size() && "global variable substitution vector size is not matching");
	LLVMContext& Context = getGlobalContext();

	// initialize new Gvs copying old ones
	for (size_t i = 0; i < Hoist.size(); ++i) {
		GlobalVariable* gv = Hoist[i];
		Type* typeGv = gv->getType();
		Type* typeElem = ((PointerType*)typeGv)->getPointerElementType();
		const size_t sizeInBytes = dataLayout.getTypeAllocSize(typeElem);
		
		DEBUG_PRINT("[Global][Deploy] %s, size %lu\n", gv->getName().data(), sizeInBytes);

		std::vector<Value*> actuals(1);
		Value* Size = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
		actuals.resize(1);
		actuals[0] = Size;

		Value* substitutedGv = CallInst::Create(Malloc, actuals, "", I);  
		Value* tempGv = Constant::getNullValue(typeGv);
		InstInsertPt out = InstInsertPt::Before(I);
		Value* storingSubstitutedGv = Casting::castTo(substitutedGv, tempGv, out, &dataLayout);

		// new global variable indicates allocated address
		new StoreInst (storingSubstitutedGv, subGlobalVar[i], I);
		subGvAlloc.push_back(substitutedGv);
	}
	DEBUG_PRINT(">> Deploying Global Variable ends\n");
	DEBUG_PRINT("Hoisted - %zu, Constant - %zu, NonConstant - %zu\n", Hoist.size(), Constant_NoUse.size(), NoConstant_NoUse.size());
	return;
}

void HoistVariables::initializeGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	LLVMContext& Context = getGlobalContext();
	DEBUG_PRINT(">> Initializing Global Variable starts\n");
	for(uint32_t i = 0; i < InitNeeded.size(); ++i) {
		GlobalVariable* gv = InitNeeded[i];
		DEBUG_PRINT("[Global] %s\n", gv->getName().data());
		
		if(gv->hasInitializer()) {
			Constant* c = gv->getInitializer();	
			int loadNeeded = 0;
			int written = 0;
			int alloc = 0;
			Value* subC = castConstant(c, I, loadNeeded, written, alloc, 1, dataLayout);
			
			// if casting a constant makes
			if (written) {
				DEBUG_PRINT("[Global][Initialize] %s from modified\n", gv->getName().data());
				Type* typeGv = gv->getType();
				Type* typeElem = ((PointerType*)typeGv)->getPointerElementType();
				const size_t sizeInBytes = dataLayout.getTypeAllocSize(typeElem);
				Value* Size = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
				const size_t subCSize = dataLayout.getTypeAllocSize(((PointerType*)subC->getType())->getPointerElementType());

				Value* tempCpy = Constant::getNullValue(Type::getInt8PtrTy(Context));
				InstInsertPt out = InstInsertPt::Before(I);
				Value* gvAllocedCpy = Casting::castTo(subC, tempCpy, out, &dataLayout);
				
				// initiailizing space is on heap or original global variable;
				Value* substitutedGv;
				if(findSubGlovar(gv) != NULL) { 
					substitutedGv = subGvAlloc[findSubGlovarIndex(gv)];
				}	else {
					Value* tempGv = Constant::getNullValue(Type::getInt8PtrTy(Context));
				  substitutedGv = Casting::castTo(gv, tempGv, out, &dataLayout);
				}
				std::vector<Value*> actuals(3);
				actuals.resize(3);
				actuals[0] = substitutedGv; 
				actuals[1] = gvAllocedCpy;
				actuals[2] = Size;
				CallInst::Create(Memcpy, actuals, "", I);
			} else {
				// initiailing space is on heap or original global variable;

				DEBUG_PRINT("[Global][Initialize] %s from original\n", gv->getName().data());
				Value* substitutedGv;
				if(findSubGlovar(gv)) { 
					substitutedGv = subGvAlloc[findSubGlovarIndex(gv)];
					Type* typeGv = gv->getType();
					Type* typeElem = ((PointerType*)typeGv)->getPointerElementType();
					const size_t sizeInBytes = dataLayout.getTypeAllocSize(typeElem);
					Value* Size = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
					Value* tempCpy = Constant::getNullValue(Type::getInt8PtrTy(Context));
					InstInsertPt out = InstInsertPt::Before(I);
					Value* gvCpy = Casting::castTo(gv, tempCpy, out, &dataLayout);

					std::vector<Value*> actuals(3);
					actuals.resize(3);
					actuals[0] = substitutedGv; 
					actuals[1] = gvCpy;
					actuals[2] = Size;
					CallInst::Create(Memcpy, actuals, "", I);
				}
			}

			if(alloc) {
				std::vector<Value*> actuals(1);
				actuals.resize(1);
				for(size_t ai = 0; ai < freeNeeded.size(); ++ai) {
					actuals[0] = freeNeeded[ai];
					CallInst::Create(Free, actuals, "", I);
				}
				freeNeeded.clear();
			}
		}
	}
	DEBUG_PRINT(">> Initializing Global Variable ends\n");
	return;
}

GlobalVariable* HoistVariables::findSubGlovar(GlobalVariable* oldGv) {
	assert(Hoist.size() == subGlobalVar.size() && "global variable substitution vector size is not matching");
	for (size_t i = 0; i < Hoist.size(); ++i) {
		if (oldGv == Hoist[i])
			return subGlobalVar[i];
	}
	return NULL;
}

int HoistVariables::findSubGlovarIndex(GlobalVariable* oldGv) {
	assert(Hoist.size() == subGlobalVar.size() && "global variable substitution vector size is not matching");
	for (size_t i = 0; i < Hoist.size(); ++i) {
		if (oldGv == Hoist[i])
			return i;
	}
	return 0;
}



void HoistVariables::isCastNeeded(Constant* c, bool& castNeeded) {
	if (isa<GlobalVariable>(c)) {
		GlobalVariable* g = (GlobalVariable*) c;
		GlobalVariable* subG = findSubGlovar(g);
		if(subG != NULL) castNeeded = true;
	} else if (isa<ConstantInt>(c)) {
	} else if (isa<ConstantFP>(c)) {
	} else if (isa<ConstantExpr>(c) ||isa<ConstantArray>(c) || isa<ConstantStruct>(c) || isa<ConstantVector>(c)) {
		for(size_t i = 0; i < c->getNumOperands(); ++i) {
			Constant* internalC = (Constant*)c->getOperand(i);
			isCastNeeded(internalC, castNeeded);	
		}
	} else if (isa<ConstantAggregateZero>(c)) {
	} else if (isa<UndefValue>(c)) {
	} else if (isa<ConstantPointerNull>(c)) {
	} else if (isa<ConstantDataSequential>(c)) {
	} else if (isa<BlockAddress>(c)) {
	} else if (isa<GlobalValue>(c)) {
	}
	return;
}

Constant* HoistVariables::unfoldZeroInitailizer(Constant* c) {
	Type* type = c->getType();

	if(isa<StructType>(type)) {
		StructType* stype = (StructType*)type;
		uint32_t num = stype->getNumElements();
		std::vector<Constant*> constants;
		for(uint32_t i = 0; i < num; ++i) {
			Type* elementType = stype->getElementType(i);
			Constant* tempC = Constant::getNullValue(elementType);
			constants.push_back(unfoldZeroInitailizer(tempC));
		}
		Constant* newC = ConstantStruct::get(stype, constants);
		return newC;
	} else if(isa<ArrayType>(type)) {
		ArrayType* atype = (ArrayType*)type;
		uint32_t num = atype->getNumElements();
		Type* elementType = atype->getElementType();
		std::vector<Constant*> constants;
		for(uint32_t i = 0; i < num; ++i) {
			constants.push_back(Constant::getNullValue(elementType));
		}
		Constant* newC = ConstantArray::get(atype, constants);
		return newC;
	} else if(isa<VectorType>(type)) {
		VectorType* vtype = (VectorType*)type;
		uint32_t num = vtype->getNumElements();
		Type* elementType = vtype->getElementType();
		std::vector<Constant*> constants;
		for(uint32_t i = 0; i < num; ++i) {
			constants.push_back(Constant::getNullValue(elementType));
		}
		Constant* newC = ConstantVector::get(constants);
		return newC;
	} else {
		Constant* newC = Constant::getNullValue(type);
		return newC;
	}

	// unreacherable
	return NULL;
}

Value* HoistVariables::castConstant(Constant* c, Instruction* I, int& loadNeeded, int& written, int& alloc, int count, const DataLayout& dataLayout) {
#ifdef GLOBAL_DEBUG
	for (int x = 0; x < count; ++x) {
		DEBUG_PRINT("  ");
	}
#endif
	LLVMContext& Context = getGlobalContext();
	if (isa<GlobalVariable>(c)) {
		DEBUG_PRINT("[ConstGlobal]\n");
		GlobalVariable* g = (GlobalVariable*) c;
		GlobalVariable* subG = findSubGlovar(g);
		if(subG == NULL) {
			loadNeeded = 0;
			return g;
		} else {
			written = 1;
			loadNeeded = 1;
			return subG;
		}
	} else if (isa<ConstantInt>(c)) {
		DEBUG_PRINT("[ConstInt]\n");
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantFP>(c)) {
		DEBUG_PRINT("[ConstFP]\n");
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantExpr>(c)) {
		DEBUG_PRINT("[ConstExpr]\n");
		bool castNeeded = false;
		isCastNeeded(c, castNeeded);
		if(castNeeded) {
			Instruction* inst = ((ConstantExpr*)c)->getAsInstruction();
			
			// Check all the operand of constant
			// If there is a replacement, then return replaced inst.
			// Othrewise just return original constant;
			for(size_t i = 0; i < inst->getNumOperands(); ++i) {
				int internalLoadNeeded = 0;
				Constant* internalC = (Constant*)inst->getOperand(i);
				Value* castedInternalC = castConstant(internalC, I, internalLoadNeeded, written, alloc, count+1, dataLayout);
				if(internalLoadNeeded) {
					castedInternalC = new LoadInst(castedInternalC, "", I);
				}
				inst->replaceUsesOfWith(internalC, castedInternalC);
			}
			inst->insertBefore(I);
			written = 1;
			loadNeeded = 0;
			return inst;
		}
		
		// return original value
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantStruct>(c) || isa<ConstantArray>(c) || isa<ConstantVector>(c)) {

#ifdef GLOBAL_DEBUG
		if (isa<ConstantStruct>(c))
			DEBUG_PRINT("[ConstStruct]\n");
		else if (isa<ConstantArray>(c))
			DEBUG_PRINT("[ConstArray]\n");
		else if (isa<ConstantVector>(c))
			DEBUG_PRINT("[ConstVector]\n");
#endif
		
		bool castNeeded = false;
		isCastNeeded(c, castNeeded);
		
		if(castNeeded) {
			Type* type = c->getType();
			const size_t sizeInBytes = dataLayout.getTypeStoreSize(type);
			std::vector<Value*> actuals(0);
			actuals.resize(1);
			Value* Size = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
			actuals[0] = Size;
			Instruction* init = CallInst::Create(Malloc, actuals, "", I);
			freeNeeded.push_back(init);
			Value* temp = Constant::getNullValue(PointerType::get(type, 0));
			InstInsertPt out = InstInsertPt::Before(I);
			Value* castedInit = Casting::castTo(init, temp, out, &dataLayout);

			for(size_t i = 0; i < c->getNumOperands(); ++i) {
				int internalLoadNeeded = 0;
				Constant* internalC = (Constant*)c->getOperand(i);
				Value* castedInternalC = castConstant(internalC, I, internalLoadNeeded, written, alloc, count+1, dataLayout);
				
				if(internalLoadNeeded)
					castedInternalC = new LoadInst(castedInternalC, "", I);
				std::vector<Value*> indice(2);
				indice[0] = ConstantInt::get(Type::getInt32Ty(Context), 0);
				indice[1] = ConstantInt::get(Type::getInt32Ty(Context), i);

				Instruction* ptr = GetElementPtrInst::CreateInBounds(castedInit, indice, "", I);
#if 0
				/* XXX:debug: print addr */
				Value* tempAddr = Constant::getNullValue(Type::getInt8PtrTy(Context));
				tempAddr = Casting::castTo(ptr, tempAddr, out, &dataLayout);
				actuals.resize(1);
				actuals[0] = tempAddr;
				CallInst::Create(PrintAddress, actuals, "", I);
#endif
				new StoreInst(castedInternalC, ptr, I);
			}
			written = 1;
			alloc = 1;
			loadNeeded = 1;
			return castedInit;
		} else {
			loadNeeded = 0;
			return c;
		}
	} else if (isa<ConstantPointerNull>(c)) {
		DEBUG_PRINT("[ConstPointerNull]\n");
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantDataSequential>(c)) {
		DEBUG_PRINT("[ConstDataSeq]\n");
		loadNeeded = 0;
		return c;
	} else if (isa<BlockAddress>(c)) {
		DEBUG_PRINT("[BlckAddress]\n");
		loadNeeded = 0;
		return c;
	} else if (isa<GlobalValue>(c)) {
		DEBUG_PRINT("[GlobalValue]\n");
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantAggregateZero>(c)){
		DEBUG_PRINT("[ConstantZeroInit]\n");
		loadNeeded = 0;
		return unfoldZeroInitailizer(c);
	} else if (isa<UndefValue>(c)){
		DEBUG_PRINT("[UndefValue]\n");
		loadNeeded = 0;
		return unfoldZeroInitailizer(c);
	} else {
		DEBUG_PRINT("[Global...else?]\n");
		loadNeeded = 0;
	} 
	return NULL;
}

void HoistVariables::hoistLocalVariable(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	LLVMContext& Context = getGlobalContext();
	DEBUG_PRINT(">> Hoisting Local Variable starts\n");
	typedef Module::iterator FI;
	typedef Function::iterator BI;
	typedef BasicBlock::iterator II;
	typedef vector<Instruction*>::iterator VI;
	typedef Value::user_iterator UI;
	typedef vector<User*>::iterator VUI;

	for (FI fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		
		int functionId = loadNamer.getFunctionId(*F);
		if(functionId == 0) continue;

		// XXX: Is header real first bb? Is end bb real retruning bb?
		BasicBlock* headerBB = &F->front();
		BasicBlock* retBB = &F->back();
		Instruction* baseInst = headerBB->getFirstNonPHI();
		Instruction* freeInst = (Instruction*) retBB->getTerminator();
		vector<Instruction*> allocas;

		for (BI bi = F->begin(), be = F->end(); bi != be; ++bi) {
			BasicBlock* B = &*bi;
		
			for (II ii = B->begin(), ie = B->end(); ii != ie; ++ii) {
				Instruction* I = &*ii;
				if(I->getOpcode() == Instruction::Alloca) {
					allocas.push_back(I);
				}
			}
		}

		for (VI vi = allocas.begin(), ve = allocas.end(); vi != ve; ++vi) {
			Instruction* I = *vi;
			assert(isa<AllocaInst>(I) && "local variable is not alloca inst");
			AllocaInst* al = (AllocaInst*) I;

			// Create malloc function call to substitute original alloca
			Type* type = al->getType();
			Type* allocatedType = al->getAllocatedType();
			const size_t sizeInBytes = dataLayout.getTypeStoreSize(allocatedType);
			std::vector<Value*> actuals(1);
			Value* Size = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
			actuals.resize(1);
			actuals[0] = Size;
			Value* heapAlloc = CallInst::Create(Malloc, actuals, "", baseInst);  
			Value* temp = Constant::getNullValue(type);
			InstInsertPt out = InstInsertPt::Before(baseInst);
			Value* castedHeapAlloc = Casting::castTo(heapAlloc, temp, out, &dataLayout);

			// Free the heap allocation before function is finished
			actuals.resize(1);
			actuals[0] = heapAlloc;
			CallInst::Create(Free, actuals, "", freeInst);
			
			// Substitute previous uses of alloca with heap malloc
			vector<User*> subTargets;
			for (UI ui = al->user_begin(), ue = al->user_end(); ui != ue; ++ui) {
				subTargets.push_back(*ui);
			}
			for (VUI ui = subTargets.begin(), ue = subTargets.end(); ui != ue; ++ui) {
				User* target = *ui;
				target->replaceUsesOfWith(al, castedHeapAlloc);
			}
		}

		// After all, delete original alloca
		for (VI vi = allocas.begin(), ve = allocas.end(); vi != ve; ++vi) {
			Instruction* I = *vi;
			I->eraseFromParent();
		}
	}
	DEBUG_PRINT(">> Hoisting Local Variable ends\n");
	return;
}

void HoistVariables::hoistGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	DEBUG_PRINT(">> Hoisting Global Variable starts\n");
	for(size_t i = 0; i < Hoist.size(); ++i) {
		GlobalVariable* gv = Hoist[i];
		GlobalVariable* newGv = subGlobalVar[i];
		std::vector<User*> targets;
		typedef Value::user_iterator UI;
		for(UI ui = gv->user_begin(), ue = gv->user_end(); ui != ue; ++ui) {
			targets.push_back(*ui);
		}

		// substituted previous global variable use to new one
		std::set<Instruction*> instIncludingConst;
		typedef std::vector<User*>::iterator VUI;
		for(VUI ui = targets.begin(), ue = targets.end(); ui != ue; ++ui) {
			User* target = *ui;

			if (isa<Instruction>(target)) {
				if(isa<PHINode>(target)) {
					// PHI node is not replaced directly. (For following the rule: phi-node included in top group of basic block)
					instIncludingConst.insert((Instruction*)target);
				} else {
					LoadInst* load = new LoadInst (newGv, "", (Instruction*) target);
					target->replaceUsesOfWith(gv, load);
				}
			}
			else if (isa<GlobalVariable>(target)){
				// do nothing
			}
			else if (isa<Constant>(target)) {
				getOuterInstructions((Constant*)target, instIncludingConst);
			}
			else {
				assert(0 && "Error: Global Variable substitution: Other case of global variable use");
				exit(0);
			}
		}
		DEBUG_PRINT("[Global][Hoist] %s, used in # of inst - %lu\n", gv->getName().data(), instIncludingConst.size());

		typedef std::set<Instruction*>::iterator SI;
		for(SI si = instIncludingConst.begin(), se = instIncludingConst.end(); si != se; ++si) {
			Instruction* target = *si;
			for(size_t ii = 0; ii < target->getNumOperands(); ++ii) {
				Value* v = target->getOperand(ii);
				if(isa<Constant>(v)) {
					int loadNeeded = 0;
					int written = 0;
					int alloc = 0;

					// case Instruction* target is phinode
					if(isa<PHINode>(target)) {
						LLVMContext& Context = getGlobalContext();
						BasicBlock* targetBlock = target->getParent();
						Function* targetFunction = targetBlock->getParent();
						BasicBlock* edgeBlock = BasicBlock::Create(Context, "edge", targetFunction);
						Instruction* replaced = BranchInst::Create(targetBlock, edgeBlock);

						PHINode* phi = (PHINode*)target;
						BasicBlock* enteringBlock = phi->getIncomingBlock(ii);
						phi->setIncomingBlock(ii, edgeBlock);
						TerminatorInst *term = enteringBlock->getTerminator();

						// translate all the phi node included in same block
						for(BasicBlock::iterator insti = targetBlock->begin(), inste = targetBlock->end(); insti != inste; ++insti) {
							Instruction* changing = &*insti;
							if(isa<PHINode>(changing)) {
								PHINode* otherPhi = (PHINode*)changing;

								for(size_t iii = 0; iii < otherPhi->getNumOperands(); ++iii) {
									BasicBlock* remainingBlock = otherPhi->getIncomingBlock(iii);
									if(remainingBlock == enteringBlock) {
										otherPhi->setIncomingBlock(iii, edgeBlock);
									}
								}
							}
						}

						for(size_t i = 0; i < term->getNumSuccessors(); ++i) {
							if(targetBlock == term->getSuccessor(i))
								term->setSuccessor(i, edgeBlock);
						}

						target = replaced;
					}

					Value* subV = castConstant((Constant*)v, target, loadNeeded, written, alloc, 1, dataLayout);
					if(loadNeeded)
						subV = new LoadInst(subV, "", target);
					target->replaceUsesOfWith(v, subV);
					
					if(alloc) {
						std::vector<Value*> actuals(1);
						actuals.resize(1);
						for(size_t ai = 0; ai < freeNeeded.size(); ++ai) {
							actuals[0] = freeNeeded[ai];
							CallInst::Create(Free, actuals, "", target);
						}
						freeNeeded.clear();
					}
				}
			}
		}
	}
	DEBUG_PRINT(">> Hoisting Global Variable ends\n");
	return;
}

void HoistVariables::freeHoistedGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	DEBUG_PRINT(">> Freeing Global Variable starts\n");
	LLVMContext& Context = getGlobalContext();
	
	for(size_t i = 0; i < Hoist.size(); ++i) {
		GlobalVariable* target = subGlobalVar[i];
		Value* addr = new LoadInst(target, "", I);
		Value* temp = Constant::getNullValue(Type::getInt8PtrTy(Context));
		InstInsertPt out = InstInsertPt::Before(I);
		addr = Casting::castTo(addr, temp, out, &dataLayout);

		std::vector<Value*> actuals(1);
		actuals.resize(1);
		actuals[0] = addr;
		CallInst::Create(Free, actuals, "", I);
	}
	DEBUG_PRINT(">> Freeing Global Variable ends\n");
	return;
}

}

