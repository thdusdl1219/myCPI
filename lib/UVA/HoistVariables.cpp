/***
 *
 * HoistVariables.cpp
 *
 * Class HoistVariables provides global variable hoisting function.
 * It checks all the global variables and determine which global variables
 * should be hoisted. And provides the api to move the global variable into
 * heap to module pass.
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
#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/UVA/HoistVariables.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
#include <set>
#include <algorithm>
#define GLOBAL_DEBUG

namespace corelab {

HoistVariables::HoistVariables(Module& M) {
	LLVMContext &Context = getGlobalContext();
/*	Malloc = M.getOrInsertFunction(
			"offload_malloc",
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	Free = M.getOrInsertFunction(
			"offload_free",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);

	Memcpy = M.getOrInsertFunction(
			"offloadUtilMemcpy",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	ServerProduceGlobalVar = M.getOrInsertFunction(
			"offloadServerProduceGlobalVariable",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	ServerConsumeGlobalVar = M.getOrInsertFunction(
			"offloadServerConsumeGlobalVariable",
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	ClientProduceGlobalVar = M.getOrInsertFunction(
			"offloadClientProduceGlobalVariable",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	ClientConsumeGlobalVar = M.getOrInsertFunction(
			"offloadClientConsumeGlobalVariable",
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);
	
	
	ClientInitiateGlobalVar = M.getOrInsertFunction(
			"offloadClientInitializeGlobalVariable",
			Type::getVoidTy(Context),
			(Type*)0);
	
	ServerInitiateGlobalVar = M.getOrInsertFunction(
			"offloadServerInitializeGlobalVariable",
			Type::getVoidTy(Context),
			(Type*)0);

	PrintAddress = M.getOrInsertFunction(
			"offloadPrintAddress",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);

	std::vector<Type*> formals(0);
	FunctionType* voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);
	ServerConsumeGlobalVariable = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__offloadServerConsumeGlobalVariable", &M);
	ServerProduceGlobalVariable = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__offloadServerProduceGlobalVariable", &M);
	ClientConsumeGlobalVariable = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__offloadClientConsumeGlobalVariable", &M);
	ClientProduceGlobalVariable = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__offloadClientProduceGlobalVariable", &M);
	ServerInitGlobalVariable = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__offloadServerInitGlobalVariable", &M);
	ClientInitGlobalVariable = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__offloadClientInitGlobalVariable", &M);
*/
	return;
}

/// Get all the instruction that contain targeting constant c.
/// It checks the user set of constant. If user is instruction,
/// add it to target instruction set. If user is constant, recursively
/// check it also. It also returns the global variables that uses 
/// constant c.
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
			fprintf(stderr, "Globalvariable is Operator????\n");
			exit(1);
		}
	}
	return;
}

/// Get all the instruction that contain targeting constant c.
/// It checks the user set of constant. If user is instruction,
/// add it to target instruction set. If user is constant, recursively
/// check it also.
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
			fprintf(stderr, "Globalvariable is Operator????\n");
			exit(1);
		}
	}
	return;
}


/// Get global variable list.
void HoistVariables::getGlobalVariableList(Module& M){
	typedef Module::global_iterator GI;
	for(GI g = M.global_begin(), e = M.global_end(); g!=e ; g++){
		GlobalVariable* gv = &*g;

		// FIXME: this function is not for filtering!
		if(gv->getName() == "stderr") continue;
		if(gv->getName() == "stdout") continue;
		if(gv->getName() == "stdin") continue;

		//if(hasFunctionType (gv->getType()) && gv->isConstant ())
		//	gv->setConstant (false);

		// FIXME: external linkage && no initializer --> assumed to be a external global variable.
		//if(gv->hasExternalLinkage () && !gv->hasInitializer ()) continue;

		globalVar.push_back(gv);
		
#ifdef GLOBAL_DEBUG
  fprintf(stderr, "[HOIST] getGlobalVariableList | gv_name : %s\n", gv->getName());
#endif
		if (!gv->isConstant ())
			NoConstant_NoUse.push_back (gv); 	// XXX gwangmu (see what happens.)
	}
}

/// Create Substitution of orignal global variable which new one indicates.
void HoistVariables::createSubGlobalVariables(Module& M, const DataLayout& dataLayout){
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
	
	//createGlobalVariableFunctions(dataLayout);
	return;
}

/// Check if global variable g is used in constant c.
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

/// Check the global variable is hoisting target or not according to instruction that uses targetting global variable. 
/// XXX: In which cases global variable addresses are used?
void HoistVariables::checkHoistedInstCases(Instruction* inst, GlobalVariable* gv, bool& isUsed) {
	if(gv->isConstant()) {
		return;
	}

	// If instruction is store and source is gloabl variable, it means the address of global variable
	// is stored somewhere. So it uses global variable address.
	if(isa<StoreInst>(inst)) {
		Value* firstOp = inst->getOperand(0);
		if(isa<Constant>(firstOp)) {
			if(findGlobalVariableInConstant((Constant*)firstOp, gv)) {
				fprintf(stderr, "[Global][Dividing] \"%s\" stored by inst %s\n", gv->getName().data(), inst->getName().data());
				isUsed = true;
			}
		}
	
	} else if(isa<LoadInst>(inst)) { 
	
	// The address of global address is used in call instruction. 
	} else if(isa<CallInst>(inst)) {
		fprintf(stderr, "[Global][Dividing] \"%s\" called by inst %s\n", gv->getName().data(), inst->getName().data());
		isUsed = true;
	
	// XXX: BitCast is used for instruction changes.
	} else if(isa<BitCastInst>(inst)) {
		fprintf(stderr, "[Global][Dividing] \"%s\" casted by inst %s\n", gv->getName().data(), inst->getName().data());
		isUsed = true;

	// XXX: If global variable is used by GEP, it means global variable is accessed by pointer.
	} else if(isa<GetElementPtrInst>(inst)) { 
		fprintf(stderr, "[Global][Dividing] \"%s\" casted by inst %s\n", gv->getName().data(), inst->getName().data());
		isUsed = true;
/*
	} else if(isa<SelectInst>(inst)) { 
		fprintf(stderr, "[Global][Dividing] \"%s\" casted by inst %s\n", gv->getName().data(), inst->getName().data());
		isUsed = true;
*/
	} else {
		// do nothing cases
		fprintf(stderr, "[Global][OtherInst] \"%s\", inst %s, opcode %s\n", gv->getName().data(), inst->getName().data(), inst->getOpcodeName());
	}
	return;
}

/// distinguishGlobalVariables()
/// Devide global variables into three category.
/// 1. Hoist
/// 2. Constant_NoUse
/// 3. NoConstant_NoUse
/// If a global variable is used, that should be hoisted.
/// Otherwise, just the value of global variable is copied into server.
void HoistVariables::distinguishGlobalVariables() {
	typedef std::vector<GlobalVariable*>::iterator GVI;
	typedef Value::user_iterator UI;

	//XXX:for debug
	int const_i = 0;

	fprintf(stderr, ">> Dividing Global Variable starts\n");
	set<GlobalVariable*> InitNeededSet;
	for(GVI gi = globalVar.begin(), ge = globalVar.end(); gi != ge; ++gi) {
		GlobalVariable* gv = *gi;
		bool isUsed = false;

		// XXX: llvm reserved global variable should be excluded
		if (gv->getName().str().substr (0, 5) == "llvm.")
			continue;
		// XXX: std structure other than stl containers should be excluded
		// XXX: RTTI info should be excluded
		if (gv->getName().str().substr (0, 4) == "_ZTI"
				|| gv->getName().str().substr (0, 4) == "_ZTS"
				|| gv->getName().str().substr (0, 7) == "_ZTVN10")
		//		|| gv->getName().str().substr (0, 4) == "_ZTV")
			continue;

		for(UI ui = gv->user_begin(), ue = gv->user_end(); ui != ue; ++ui) {
			User* user = *ui;
#ifdef GLOBAL_DEBUG
      fprintf(stderr, "[HOIST] distinguishGlobalVariables | user : %s\n", (*ui)->getName().data());
      user->dump();
#endif
			if (isa<Instruction>(user)) {
				// User case 1: instruction
				Instruction* inst = (Instruction*) user;
				checkHoistedInstCases(inst, gv, isUsed);
#ifdef GLOBAL_DEBUG
      fprintf(stderr, "[HOIST] distinguishGlobalVariables | user isInst : %s\n", inst->getName().data());
      inst->dump();
#endif

			} else if (isa<Constant>(user)) {
				// User case 2: constant
				Constant* con = (Constant*) user;
				set<Instruction*> iset;
				set<GlobalVariable*> gset;
				getOuterInstructions(con, iset, gset);
#ifdef GLOBAL_DEBUG
      fprintf(stderr, "[HOIST] distinguishGlobalVariables | user isConst : %s\n", con->getName().data());
      con->dump();
#endif

				// check outer instruction
				typedef set<Instruction*>::iterator SI;
				for(SI si = iset.begin(), se = iset.end(); si != se; ++si) {
					Instruction* outerInst = *si;
					checkHoistedInstCases(outerInst, gv, isUsed);
				}
				
				if (gv->getName().str().substr (0, 4) == "_ZTV") {
					InitNeededSet.insert(gv);
					isUsed = true;
				}
				else {

					// case included in global variable
					// If global variable which address is used in other global variables,
					// using global variables initialized newly to point hoisted global variable.
					typedef set<GlobalVariable*>::iterator SG;
					for(SG sgi = gset.begin(), sge = gset.end(); sgi != sge; ++sgi) {
						GlobalVariable* outerGv = *sgi;
						InitNeededSet.insert(outerGv);
						fprintf(stderr, "[Global][Dividing] %s initailized in %s\n", gv->getName().data(), outerGv->getName().data());
						isUsed = true;
					}

				}

			} else {
				fprintf(stderr, "Globalvariable is Operator????\n");
				exit(1);
			}
		}
		
		// distinguish the global variables according to their type
		// Case1 : Hoist
		if (isUsed) {
			Hoist.push_back(gv);
			InitNeededSet.insert(gv);
#ifdef GLOBAL_DEBUG
      fprintf(stderr, "[HOIST] distinguishGlobalVariables | isUsed : %s\n", gv->getName());
#endif
		} else {
			if (gv->isConstant()) {
				// Case2 : Constant_NoUse
				// XXX:Temperal solution for h264ref
				if ((380 <= const_i && const_i < 390) || (682 <= const_i && const_i < 686)) {
					Hoist.push_back(gv);
					InitNeededSet.insert(gv);
				} else {
#ifdef GLOBAL_DEBUG
          fprintf(stderr, "[HOIST] distinguishGlobalVariables | !isUsed & const : %s\n", gv->getName());
#endif
					Constant_NoUse.push_back(gv);
				}
				++const_i;
			} else {
#ifdef GLOBAL_DEBUG
        fprintf(stderr, "[HOIST] distinguishGlobalVariables | !isUsed & !const : %s\n", gv->getName());
#endif
				// Case3 : NoConstant_NoUse 
				NoConstant_NoUse.push_back(gv);
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
	fprintf(stderr, ">> Dividing Global Variable ends\n");
	return;
}

/// deployGlobalVariable(): allocate the pointer of old global variable to heap and 
/// make sub_global variable to address the old global variable on heap.
void HoistVariables::deployGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	fprintf(stderr, ">> Deploying Global Variable starts\n");
	assert(Hoist.size() == subGlobalVar.size() && "global variable substitution vector size is not matching");
	LLVMContext& Context = getGlobalContext();

	// initialize new Gvs copying old ones
	for (size_t i = 0; i < Hoist.size(); ++i) {
		GlobalVariable* gv = Hoist[i];
		Type* typeGv = gv->getType();
		Type* typeElem = ((PointerType*)typeGv)->getPointerElementType();
		const size_t sizeInBytes = dataLayout.getTypeAllocSize(typeElem);
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[Global][Deploy] %s, size %lu\n", gv->getName().data(), sizeInBytes);
#endif
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
	fprintf(stderr, ">> Deploying Global Variable ends\n");
	fprintf(stderr, "Hoisted - %u, Constant - %u, NonConstant - %u\n", Hoist.size(), Constant_NoUse.size(), NoConstant_NoUse.size());
	return;
}

void HoistVariables::initializeGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	LLVMContext& Context = getGlobalContext();
	fprintf(stderr, ">> Initializing Global Variable starts\n");
	for(uint32_t i = 0; i < InitNeeded.size(); ++i) {
		GlobalVariable* gv = InitNeeded[i];
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[Global] %s\n", gv->getName().data());
#endif
		if(gv->hasInitializer()) {
			Constant* c = gv->getInitializer();	
			int loadNeeded = 0;
			int written = 0;
			int alloc = 0;
			Value* subC = castConstant(c, I, loadNeeded, written, alloc, 1, dataLayout);

			// if casting a constant makes
			if (written) {
#ifdef GLOBAL_DEBUG
				fprintf(stderr, "[Global][Initialize] %s from modified\n", gv->getName().data());
#endif
				Type* typeGv = gv->getType();
				Type* typeElem = ((PointerType*)typeGv)->getPointerElementType();
				const size_t sizeInBytes = dataLayout.getTypeAllocSize(typeElem);
				Value* Size = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
				const size_t subCSize = dataLayout.getTypeAllocSize(((PointerType*)subC->getType())->getPointerElementType());

				/*
				fprintf(stderr, "gv name %s sizeInBytes %u, subCSize %u\n", gv->getName().data(), sizeInBytes, subCSize);
				assert(sizeInBytes == subCSize && "original global variable and substitution global variables differs in alloc size");
				*/
 
				if (GlobalVariable *gvarSub = dyn_cast<GlobalVariable> (subC)) 
					if (gvarSub->getName().str().substr (0, 4) == "sub_")
						subC = new LoadInst (gvarSub, "", I);

				Value* tempCpy = Constant::getNullValue(Type::getInt8PtrTy(Context));
				InstInsertPt out = InstInsertPt::Before(I);
				Value* gvAllocedCpy = Casting::castTo(subC, tempCpy, out, &dataLayout);
				
				// initiailizing space is on heap or original global variable;
				Value* substitutedGv = gv;
				if(findSubGlovar(gv) != NULL) {
					substitutedGv = subGvAlloc[findSubGlovarIndex(gv)];
				}

				if (alloc) {
					Value* tempGv = Constant::getNullValue(Type::getInt8PtrTy(Context));
				  substitutedGv = Casting::castTo(substitutedGv, tempGv, out, &dataLayout);

					std::vector<Value*> actuals(3);
					actuals.resize(3);
					actuals[0] = substitutedGv; 
					actuals[1] = gvAllocedCpy;
					actuals[2] = Size;
					CallInst::Create(Memcpy, actuals, "", I);
				}	else {
					Value* tempGv = Constant::getNullValue(PointerType::get (Type::getInt8PtrTy(Context), 0));
				  substitutedGv = Casting::castTo(substitutedGv, tempGv, out, &dataLayout);
					new StoreInst (gvAllocedCpy, substitutedGv, I);
				}
			} else {
#ifdef GLOBAL_DEBUG
				fprintf(stderr, "[Global][Initialize] %s from original\n", gv->getName().data());
#endif
				// initiailing space is on heap or original global variable;
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
	fprintf(stderr, ">> Initializing Global Variable ends\n");
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
		//if(subG != NULL) fprintf(stderr, "isCastNeeded g - %s, subG - %s\n", g->getName().data(), subG->getName().data());
		//else fprintf(stderr, "isCastNeeded g - %s, subG - NULL\n", g->getName().data());
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
		for(int i = 0; i < num; ++i) {
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
		for(int i = 0; i < num; ++i) {
			constants.push_back(Constant::getNullValue(elementType));
		}
		Constant* newC = ConstantArray::get(atype, constants);
		return newC;
	} else if(isa<VectorType>(type)) {
		VectorType* vtype = (VectorType*)type;
		uint32_t num = vtype->getNumElements();
		Type* elementType = vtype->getElementType();
		std::vector<Constant*> constants;
		for(int i = 0; i < num; ++i) {
			constants.push_back(Constant::getNullValue(elementType));
		}
		Constant* newC = ConstantVector::get(constants);
		return newC;
	} else {
		Constant* newC = Constant::getNullValue(type);
		return newC;
	}

	//Unreacherable
	return NULL;
}

Value* HoistVariables::castConstant(Constant* c, Instruction* I, int& loadNeeded, int& written, int& alloc, int count, const DataLayout& dataLayout) {
#ifdef GLOBAL_DEBUG
	for (int x = 0; x < count; ++x) {
		fprintf(stderr, "  ");
	}
#endif
	LLVMContext& Context = getGlobalContext();
	if (isa<GlobalVariable>(c)) {
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[ConstGlobal]\n");
#endif
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
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[ConstInt]\n");
#endif
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantFP>(c)) {
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[ConstFP]\n");
#endif
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantExpr>(c)) {
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[ConstExpr]\n");
#endif
		bool castNeeded = false;
		isCastNeeded(c, castNeeded);
		if(castNeeded) {
			Instruction* inst = ((ConstantExpr*)c)->getAsInstruction();
		for(size_t i = 0; i < inst->getNumOperands(); ++i) {
			int internalLoadNeeded = 0;
			// Check all the operand of constant
			// If there is a replacement, then return replaced inst.
			// Othrewise just return original constant;
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
			fprintf(stderr, "[ConstStruct]\n");
		else if (isa<ConstantArray>(c))
			fprintf(stderr, "[ConstArray]\n");
		else if (isa<ConstantVector>(c))
			fprintf(stderr, "[ConstVector]\n");
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
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[ConstPointerNull]\n");
#endif
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantDataSequential>(c)) {
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[ConstDataSeq]\n");
#endif
		loadNeeded = 0;
		return c;
	} else if (isa<BlockAddress>(c)) {
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[BlckAddress]\n");
#endif
		loadNeeded = 0;
		return c;
	} else if (isa<GlobalValue>(c)) {
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[GlobalValue]\n");
#endif
		loadNeeded = 0;
		return c;
	} else if (isa<ConstantAggregateZero>(c)){
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[ConstantZeroInit]\n");
		loadNeeded = 0;
		return unfoldZeroInitailizer(c);
#endif
	} else if (isa<UndefValue>(c)){
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[UndefValue]\n");
#endif
		loadNeeded = 0;
		return unfoldZeroInitailizer(c);
	} else {
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[Global...else?]\n");
#endif
		loadNeeded = 0;
	} 
	return NULL;
}

void HoistVariables::hoistLocalVariable(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	LLVMContext& Context = getGlobalContext();
	fprintf(stderr, ">> Hoisting Local Variable starts\n");
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
	fprintf(stderr, ">> Hoisting Local Variable ends\n");
	return;
}

void HoistVariables::hoistGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	fprintf(stderr, ">> Hoisting Global Variable starts\n");
	for(size_t i = 0; i < Hoist.size(); ++i) {
		GlobalVariable* gv = Hoist[i];
		GlobalVariable* newGv = subGlobalVar[i];
		std::vector<User*> targets;
		typedef Value::user_iterator UI;
		for(UI ui = gv->user_begin(), ue = gv->user_end(); ui != ue; ++ui) {
			targets.push_back((User*)*ui);
		}

		// substituted previous global variable use to new one
		std::set<Instruction*> instIncludingConst;
		typedef std::vector<User*>::iterator VUI;
		for(VUI ui = targets.begin(), ue = targets.end(); ui != ue; ++ui) {
			User* target = *ui;

			if (isa<Instruction>(target)) {
				Instruction *instTarget = dyn_cast<Instruction> (target);
				Function *fnParent = instTarget->getParent()->getParent ();
				string pname = fnParent->getName().str ();

				// XXX: C++ global initializer should be excluded
				//if (pname.substr (0, 21) == "__cxx_global_var_init")
				//	continue;
				//if (pname.substr (0, 9) == "_GLOBAL__")
				//	continue;

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
				fprintf(stderr, "Error: Global Variable substitution: Other case of global variable use\n");
				exit(0);
			}
		}
#ifdef GLOBAL_DEBUG
		fprintf(stderr, "[Global][Hoist] %s, used in # of inst - %lu\n", gv->getName().data(), instIncludingConst.size());
#endif

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
	fprintf(stderr, ">> Hoisting Global Variable ends\n");
	return;
}

void HoistVariables::freeHoistedGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout) {
	fprintf(stderr, ">> Freeing Global Variable starts\n");
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
	fprintf(stderr, ">> Freeing Global Variable ends\n");
	return;
}

//XXX: for client only
void HoistVariables::createClientInitializeGlobalVariable(Instruction* I) {
	std::vector<Value*> actuals(0);
	actuals.resize(0);
	CallInst::Create(ClientInitGlobalVariable, actuals, "", I);
	actuals.resize(0);
	CallInst::Create(ClientInitiateGlobalVar, actuals, "", I);
	return;
}

//XXX: for server only
void HoistVariables::createServerInitializeGlobalVariable(Instruction* I) {
	std::vector<Value*> actuals(0);
	actuals.resize(0);
	CallInst::Create(ServerInitiateGlobalVar, actuals, "", I);
	actuals.resize(0);
	CallInst::Create(ServerInitGlobalVariable, actuals, "", I);
	return;
}

void HoistVariables::createServerProduceGlobalVariable(Instruction* I) {
	std::vector<Value*> actuals(0);
	actuals.resize(0);
	CallInst::Create(ServerProduceGlobalVariable, actuals, "", I);
	return;
}
void HoistVariables::createServerConsumeGlobalVariable(Instruction* I) {
	std::vector<Value*> actuals(0);
	actuals.resize(0);
	CallInst::Create(ServerConsumeGlobalVariable, actuals, "", I);
	return;
}
void HoistVariables::createClientProduceGlobalVariable(Instruction* I) {
	std::vector<Value*> actuals(0);
	actuals.resize(0);
	CallInst::Create(ClientProduceGlobalVariable, actuals, "", I);
	return;
}
void HoistVariables::createClientConsumeGlobalVariable(Instruction* I) {
	std::vector<Value*> actuals(0);
	actuals.resize(0);
	CallInst::Create(ClientConsumeGlobalVariable, actuals, "", I);
	return;
}

void HoistVariables::createGlobalVariableFunctions(const DataLayout& dataLayout) {
	createProduceFunction(ServerProduceGlobalVariable, NoConstant_NoUse, SERVER, dataLayout);
	createProduceFunction(ClientProduceGlobalVariable, NoConstant_NoUse, CLIENT, dataLayout);
	createConsumeFunction(ServerConsumeGlobalVariable, NoConstant_NoUse, SERVER, dataLayout);
	createConsumeFunction(ClientConsumeGlobalVariable, NoConstant_NoUse, CLIENT, dataLayout);
	createConsumeFunction(ServerInitGlobalVariable, subGlobalVar, SERVER, dataLayout);
	createProduceFunction(ClientInitGlobalVariable, subGlobalVar, CLIENT, dataLayout);
	return;
}

void HoistVariables::createProduceFunction(Function* F, vector<GlobalVariable*>& gvVector, Mode mode, const DataLayout& dataLayout) {
	LLVMContext& Context = getGlobalContext();
	std::vector<Value*> actuals(0);
	
	BasicBlock* entry = BasicBlock::Create(Context, "entry", F);
	BasicBlock* ret = BasicBlock::Create(Context, "ret", F);
	Instruction* pivot = BranchInst::Create(ret, entry); 
	
	size_t gvNum = gvVector.size();
	for (size_t i = 0; i < gvNum; ++i) {
		actuals.resize(2);
		/*
		Value* argValue = new LoadInst (gvVector[i], "", pivot);
		Type* type = argValue->getType();
		const size_t sizeInBytes = dataLayout.getTypeAllocSize(type);
		Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1);
		AllocaInst* alloca = new AllocaInst(type, one, "", pivot);
		new StoreInst(argValue, alloca, pivot);
		*/
		/*
		InstInsertPt out = InstInsertPt::Before(pivot);
		Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
		actuals[0] = Casting::castTo(alloca, temp, out, &dataLayout);
		actuals[1] = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
		if(mode==SERVER)
			CallInst::Create(ServerProduceGlobalVar, actuals, "", pivot);
		else
			CallInst::Create(ClientProduceGlobalVar, actuals, "", pivot);
		*/
		
		Type* type = ((PointerType*)gvVector[i]->getType())->getPointerElementType();
		const size_t sizeInBytes = dataLayout.getTypeAllocSize(type);
		
		InstInsertPt out = InstInsertPt::Before(pivot);
		Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
		actuals[0] = Casting::castTo(gvVector[i], temp, out, &dataLayout);
		actuals[1] = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
		if(mode==SERVER)
			CallInst::Create(ServerProduceGlobalVar, actuals, "", pivot);
		else
			CallInst::Create(ClientProduceGlobalVar, actuals, "", pivot);
		
	}
	ReturnInst::Create(Context, 0, ret);
	return;
}

void HoistVariables::createConsumeFunction(Function* F, vector<GlobalVariable*>& gvVector, Mode mode, const DataLayout& dataLayout) {
	LLVMContext& Context = getGlobalContext();
	std::vector<Value*> actuals(0);
	
	BasicBlock* entry = BasicBlock::Create(Context, "entry", F);
	BasicBlock* ret = BasicBlock::Create(Context, "ret", F);
	Instruction* pivot = BranchInst::Create(ret, entry); 
	size_t gvNum = gvVector.size();
	
	for (size_t i = 0; i < gvNum; ++i) {
		actuals.resize(1);
		Type* typeGv = gvVector[i]->getType()->getPointerElementType();
		const size_t sizeInBytes = dataLayout.getTypeAllocSize(typeGv);
		actuals[0] = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
		InstInsertPt out = InstInsertPt::Before(pivot);
		Value* Gv;
		if (mode == SERVER)
			Gv = CallInst::Create(ServerConsumeGlobalVar, actuals, "", pivot);
		else 
			Gv = CallInst::Create(ClientConsumeGlobalVar, actuals, "", pivot);
		/*
		Value* tempGv = ConstantPointerNull::get(gvVector[i]->getType());
		Value* castedGv = Casting::castTo(Gv, tempGv, out, &dataLayout);
		*/
		Value* tempGvVector = Constant::getNullValue(Type::getInt8PtrTy(Context));
		Value* castedGvVector = Casting::castTo(gvVector[i], tempGvVector, out, &dataLayout);
		actuals.resize(3);
		actuals[0] = castedGvVector;
		actuals[1] = Gv;
		actuals[2] = ConstantInt::get(Type::getInt32Ty(Context), sizeInBytes);
		CallInst::Create(Memcpy, actuals, "", pivot);
		/*
		LoadInst* loadedGv = new LoadInst(castedGv, "", pivot);
		new StoreInst(loadedGv, gvVector[i], pivot);
		*/
	}
	ReturnInst::Create(Context, 0, ret);
	return;
}

bool HoistVariables::hasFunctionType (Type *type, set<Type *> setVisited) {
	if (StructType *tyStruct = dyn_cast<StructType> (type)) {
		if (setVisited.find (tyStruct) == setVisited.end ()) {
			setVisited.insert (tyStruct);

			for (unsigned i = 0; i < tyStruct->getNumElements (); i++) {
				if (hasFunctionType (tyStruct->getElementType (i), setVisited))
					return true;
			}
		}

		return false;
	}
	else if (SequentialType *tySeq = dyn_cast<SequentialType> (type)) {
		if (setVisited.find (tySeq) == setVisited.end ()) {
			setVisited.insert (tySeq);
			return hasFunctionType (tySeq->getElementType (), setVisited);
		}
		
		return false;
	}
	else if (FunctionType *tyFn = dyn_cast<FunctionType> (type)) {
		return true;
	}
	else {
		return false;
	}
}
}

