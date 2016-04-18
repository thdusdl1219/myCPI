/***
 * Converter32 : target converter
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

#include "corelab/UVA/Convert32.h"
#include "corelab/Utilities/Debug.h"

#include <inttypes.h>

using namespace corelab;
using namespace std;

static RegisterPass<Converter32> X("converter32", "convert to i386 bitcode", false, false);

char Converter32::ID = 0;

void Converter32::getAnalysisUsage (AnalysisUsage &AU) const {
	AU.setPreservesAll ();
}

bool Converter32::runOnModule (Module& M) {
	this->pM = &M;
	this->pC = &M.getContext ();

	convertTargetTriple ();

	return false;
}


/* Convert IR target triple to i386 */
void Converter32::convertTargetTriple () {	
	pM->setTargetTriple ("i386-unknown-linux-gnu");
}

	
//helper function
ConstantInt* Converter32::getConstantInt (unsigned bits, unsigned n) {
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
