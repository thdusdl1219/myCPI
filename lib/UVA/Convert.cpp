/***
 * Converter64 : target converter
 *
 * Port 32-bit ARM(v7) bitcode to 64-bit x86 bitcode
 * XXX doubling malloc/calloc/realloc size regardless of type XXX
 * written by : gwangmu 
 *
 * **/

#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"

#include "corelab/UVA/Convert.h"
#include "corelab/Utilities/Debug.h"

#include <inttypes.h>

using namespace corelab;
using namespace std;

static RegisterPass<Converter64> X("converter64", "convert to x86 bitcode", false, false);

char Converter64::ID = 0;

void Converter64::getAnalysisUsage (AnalysisUsage &AU) const {
	AU.setPreservesAll ();
}

bool Converter64::runOnModule (Module& M) {
	this->pM = &M;
	this->pC = &M.getContext ();

	convertTargetTriple ();
	doubleAllocSize ();

	return false;
}


/* Convert IR target triple to i386 */
void Converter64::convertTargetTriple () {	
	pM->setTargetTriple ("x86_64-unknown-linux-gnu");
}

/* Double allocation size */
void Converter64::doubleAllocSize () {
	for (Module::iterator ifn = pM->begin (); ifn != pM->end (); ++ifn) {
		for (Function::iterator iblk = ifn->begin (); iblk != ifn->end (); ++iblk) {
			for (BasicBlock::iterator iinst = iblk->begin (); iinst != iblk->end (); ++iinst) {
				CallInst *instCall = dyn_cast<CallInst> (&*iinst);
				if (!instCall) continue;
				Function *fnCalled = instCall->getCalledFunction ();

				if (!fnCalled) continue;

				string fnname = fnCalled->getName().str();
				if (fnname == "offload_malloc") {
					Value *valArg0 = instCall->getArgOperand (0);
					IntegerType *tyArg0 = dyn_cast<IntegerType> (valArg0->getType ());
					unsigned bits = tyArg0->getBitWidth ();
					BinaryOperator *bopMul = BinaryOperator::Create (Instruction::Mul,
						valArg0, getConstantInt (bits, 2), "mul", instCall);
					instCall->setArgOperand (0, bopMul);
				}
				else if (fnname == "offload_calloc") {
					Value *valArg0 = instCall->getArgOperand (0);
					IntegerType *tyArg0 = dyn_cast<IntegerType> (valArg0->getType ());
					unsigned bits = tyArg0->getBitWidth ();
					BinaryOperator *bopMul = BinaryOperator::Create (Instruction::Mul,
						valArg0, getConstantInt (bits, 2), "mul", instCall);
					instCall->setArgOperand (0, bopMul);
				}
				else if (fnname == "offload_realloc") {
					Value *valArg1 = instCall->getArgOperand (1);
					IntegerType *tyArg1 = dyn_cast<IntegerType> (valArg1->getType ());
					unsigned bits = tyArg1->getBitWidth ();
					BinaryOperator *bopMul = BinaryOperator::Create (Instruction::Mul,
						valArg1, getConstantInt (bits, 2), "mul", instCall);
					instCall->setArgOperand (1, bopMul);
				}
			}
		}
	}
}
	
//helper function
ConstantInt* Converter64::getConstantInt (unsigned bits, unsigned n) {
	switch (bits) {
		case 1:
			return ConstantInt::get (Type::getInt1Ty (*pC), n);
		case 8:
			return ConstantInt::get (Type::getInt8Ty (*pC), n);
		case 16:
			return ConstantInt::get (Type::getInt16Ty (*pC), n);
		case 32:
			return ConstantInt::get (Type::getInt32Ty (*pC), n);
		case 64:
			return ConstantInt::get (Type::getInt32Ty (*pC), n);
		default:
			return ConstantInt::get (Type::getIntNTy (*pC, bits), n);
	}
}
