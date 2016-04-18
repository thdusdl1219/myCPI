/***
 * Aligner.cpp : Stack aligner installer
 *
 * Install stack aligner
 * written by: gwangmu
 *
 * **/

#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"

#include "corelab/UVA/Aligner.h"
#include "corelab/Utilities/Debug.h"

#include <iostream>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <string>

using namespace llvm;
using namespace std;

namespace corelab {
	static RegisterPass<AlignerInstaller> X("inst-aligner", "install stack aligner", false, false);

	char AlignerInstaller::ID = 0;

	void AlignerInstaller::getAnalysisUsage (AnalysisUsage &AU) const {
		AU.setPreservesAll ();
	}

	bool AlignerInstaller::runOnModule (Module& M) {
		this->pM = &M;
		this->pC = &M.getContext ();

		// prepare mains
		FunctionType *tyVoidVoidFn = FunctionType::get (Type::getVoidTy (*pC), false);
		Function *fnNMain = Function::Create (tyVoidVoidFn, GlobalValue::PrivateLinkage,
			"_main", pM);
		Function *fnOMain = pM->getFunction ("main");

		if (!fnOMain) {
			FunctionType *tyVoidVoidTy = FunctionType::get (Type::getVoidTy (M.getContext ()), false);
			fnOMain = Function::Create (tyVoidVoidTy, GlobalValue::ExternalLinkage, "main", &M);
			BasicBlock *blkOMainEntry = BasicBlock::Create (M.getContext (), "entry", fnOMain);
			ReturnInst::Create (M.getContext (), blkOMainEntry);
		}

		assert (fnOMain && "function 'main' not found");

		// process
		moveBody (fnNMain, fnOMain);
		redirectToAligner (fnOMain, fnNMain);

		fnOMain->dump ();
		fnNMain->dump ();

		return false;
	}

	/* XXX DST must return void XXX */
	void AlignerInstaller::moveBody (Function *dst, Function *src) {
		vector<BasicBlock *> vecBlks;

		for (Function::iterator iblk = src->begin (); iblk != src->end (); ++iblk)
			vecBlks.push_back (&*iblk);

		// move DST's basic blocks to SRC, while taking out any return instruction.
		BasicBlock *blkEmpty = BasicBlock::Create (*pC, "tmp.empty", dst);		// temporary block for moving
		for (unsigned i = 0; i < vecBlks.size (); ++i) {
			vecBlks[i]->moveBefore (blkEmpty);

			if (ReturnInst *instTerm = dyn_cast<ReturnInst> (vecBlks[i]->getTerminator ())) {
				ReturnInst::Create (*pC, NULL, instTerm);
				instTerm->eraseFromParent ();
			}
		}
		blkEmpty->eraseFromParent ();

		// set DST's terminator

		// set SRC's terminator
		BasicBlock *blkTerm = BasicBlock::Create (*pC, "term", src);
		new UnreachableInst (*pC, blkTerm);
	}

	void AlignerInstaller::redirectToAligner (Function *target, Function *link) {
		FunctionType *tyVoidVoidFn = FunctionType::get (Type::getVoidTy (*pC), false);
		PointerType *tyPtVoidVoidFn = PointerType::get (tyVoidVoidFn, 0);
		Constant *cnstAligner = pM->getOrInsertFunction ("offloadSwitchStack",
			Type::getVoidTy (*pC), tyPtVoidVoidFn, NULL);

		vector<Value *> vecArgs;
		vecArgs.push_back (link);

		BasicBlock *blkFirst = &*target->begin ();
		Instruction *instFirst = &*blkFirst->begin ();
		Constant *padding = ConstantInt::get (Type::getInt32Ty (*pC), 4096);
		new AllocaInst (Type::getInt8Ty (*pC), padding, "padding", instFirst);
		CallInst::Create (cnstAligner, vecArgs, "", instFirst);
	}
}

