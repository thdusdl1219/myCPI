/***
 * FixedGlobal.cpp : Global to address-fixed global
 *
 * Fix internal global variables' addresses.
 * XXX UNSTABLE TO USE LIBRARIES! Use it only for executable IRs. XXX
 * written by : gwangmu
 *
 * **/

#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/ADT/StringMap.h"

#include "corelab/UVA/FixedGlobalVariable.h"
#include "corelab/UVA/FixedGlobal.h"
#include "corelab/Utilities/GlobalCtors.h"

#include <set>
#include <list>
#include <cstdio>
#include <string>

#define FIXED_GLOBAL_BASE 			((void *)0x15000000)
#define FIXED_CONST_GLOBAL_BASE ((void *)0x16000000)

using namespace llvm;
using namespace std;

namespace corelab {
	static RegisterPass<FixedGlobal> X("fix-globals", "Fix global variables' addresses.", false, false);

	char FixedGlobal::ID = 0;

	void FixedGlobal::getAnalysisUsage (AnalysisUsage &AU) const {
		AU.setPreservesAll ();
	}

	bool FixedGlobal::runOnModule (Module& M) {
		this->pM = &M;

		/* Find all internal global variables. */
		set<GlobalVariable *> setGvars;
		set<GlobalVariable *> setConstGvars;
		for (Module::global_iterator igvar = M.global_begin ();
				 igvar != M.global_end (); ++igvar) {
			GlobalVariable *gvar = &*igvar;
			
			// FIXME assume external, if it doesn't have an initializer.
			if ((!gvar->hasExternalLinkage () || gvar->hasInitializer ()) &&
					(gvar->getName().str().length () < 5 ||
					 	gvar->getName().str().substr (0, 5) != string ("llvm."))) {
				if (!gvar->isConstant ())
					setGvars.insert (gvar);
				else {
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
						setGvars.insert (gvar);
					}
					else
						setConstGvars.insert (gvar);
				}
			}
		}

		/* Convert non-constant globals */
		convertToFixedGlobals (setGvars, FIXED_GLOBAL_BASE);

		/* Convert constant globals */
		size_t sizeConstGvars = convertToFixedGlobals (setConstGvars, FIXED_CONST_GLOBAL_BASE);
		uintptr_t uptConstGvarsBegin = (uintptr_t)FIXED_CONST_GLOBAL_BASE;
		uintptr_t uptConstGvarsEnd = (uintptr_t)FIXED_CONST_GLOBAL_BASE + sizeConstGvars;
	
		/* Set constant range */
		FunctionType *tyFnVoidVoid = FunctionType::get (
				Type::getVoidTy (pM->getContext ()), false);
		Function *fnDeclCRange = Function::Create (tyFnVoidVoid, GlobalValue::InternalLinkage, 
				"__decl_const_global_range__", pM);
		BasicBlock *blkDeclCRange = BasicBlock::Create (pM->getContext (), "initzer", fnDeclCRange);
		
		Type *tyVoid = Type::getVoidTy (M.getContext ());
		Type *tyInt8Pt = Type::getInt8PtrTy (M.getContext ());
		Type *tyUintPtr = Type::getIntNTy (M.getContext (),
				M.getDataLayout ().getPointerSizeInBits ());

		Constant *cnstOffSetCRange = M.getOrInsertFunction ("offloadUtilSetConstantRange",
				tyVoid, tyInt8Pt, tyInt8Pt, NULL);

		vector<Value *> vecSetCRangeArgs;
		vecSetCRangeArgs.push_back (ConstantExpr::getCast (Instruction::IntToPtr,
					ConstantInt::get (tyUintPtr, uptConstGvarsBegin), tyInt8Pt));
		vecSetCRangeArgs.push_back (ConstantExpr::getCast (Instruction::IntToPtr,
					ConstantInt::get (tyUintPtr, uptConstGvarsEnd), tyInt8Pt));
		CallInst::Create (cnstOffSetCRange, vecSetCRangeArgs, "", blkDeclCRange);

		ReturnInst::Create (M.getContext (), blkDeclCRange);

		callBeforeMain (fnDeclCRange, 0);

		/* Finalize */
		list<GlobalVariable *> lstDispGvars;
		lstDispGvars.insert (lstDispGvars.begin (), setGvars.begin (), setGvars.end ());
		lstDispGvars.insert (lstDispGvars.begin (), setConstGvars.begin (), setConstGvars.end ());

		while (!lstDispGvars.empty ()) {
			GlobalVariable *gvar = lstDispGvars.front ();
			lstDispGvars.pop_front ();

			if (!gvar->user_empty ()) {
				lstDispGvars.push_back (gvar);
				continue;
			}

			gvar->eraseFromParent ();
		}

		return false;
	}

	size_t FixedGlobal::convertToFixedGlobals (set<GlobalVariable *> setGvars, void *base) {
		typedef map<GlobalVariable *, FixedGlobalVariable *> GlobalToFixedMap;
		typedef pair<GlobalVariable *, FixedGlobalVariable *> GlobalToFixedPair;
		size_t sizeTotalGvars = 0;

		FixedGlobalFactory::begin (pM, base);

		/* Replace them to fixed globals */
		GlobalToFixedMap mapGlobalToFixed;
		for (set<GlobalVariable *>::iterator igvar = setGvars.begin ();
				 igvar != setGvars.end (); ++igvar) {
			GlobalVariable *gvar = *igvar;

			string gname = gvar->getName().str ();
			gvar->setName ("__disposed__");
			
			Constant *cnstInitzer = NULL;
			if (gvar->hasInitializer ())
				cnstInitzer = gvar->getInitializer ();

			FixedGlobalVariable *fgvar = FixedGlobalFactory::create (gvar->getType()->getElementType (),
					cnstInitzer, gname);
			mapGlobalToFixed.insert (GlobalToFixedPair (gvar, fgvar));
		}

		/* Correct references to globals */
		for (GlobalToFixedMap::iterator it = mapGlobalToFixed.begin ();
				 it != mapGlobalToFixed.end (); ++it) {
			GlobalVariable *gvar = it->first;
			FixedGlobalVariable *fgvar = it->second;
			
			gvar->replaceAllUsesWith (fgvar);
		}	

		sizeTotalGvars = FixedGlobalFactory::getTotalGlobalSize ();
		FixedGlobalFactory::end ();

		return sizeTotalGvars;
	}

	bool FixedGlobal::hasFunction (Constant* cnst) {
		if (dyn_cast<Function> (cnst)) 	return true;

		for (User::op_iterator iop = cnst->op_begin ();
				 iop != cnst->op_end (); ++iop) {
			Constant *cnstOper = dyn_cast<Constant> (iop->get ());
			
			if (hasFunction (cnstOper))		return true;
		}

		return false;
	}
}
