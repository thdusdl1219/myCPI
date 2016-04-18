#ifndef LLVM_CORELAB_ENDIAN_CONVERT_H
#define LLVM_CORELAB_ENDIAN_CONVERT_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <map>

#define OUT

using namespace llvm;
using namespace std;

namespace corelab {
	class EndianConvert : public ModulePass {
		public:
			static char ID;

			EndianConvert () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "ENDIAN_CONVERT"; }

		private:
			Module *pM;
			LLVMContext *pC;

			map<Type *, Constant *> mapTyToIdShMap;
			map<Type *, Constant *> mapTyToShMap;
			map<Type *, VectorType *> mapTyToVecTy;
			map<Type *, IntegerType *> mapTyToIntTy;

			// Initializer
			void initialize ();

			vector<Constant *> getShuffleMaskIndices (unsigned n, bool reverse);

			// Installer
			void installEndiannessConverter ();

			vector<Instruction *> getVectorShuffleInsts (Value *valTarget);
			vector<Instruction *> getScalarShuffleInsts (Value *valTarget);

			bool isRequireConverting (Type *type);
			Constant* getIdentityShuffleMask (Type *type);
			Constant* getShuffleMask (Type *type);
			APInt getBitMaskOfNthByte (unsigned n, unsigned len);
			VectorType* getProperVectorType (Type *type);
			IntegerType* getProperIntegerType (Type *type);
	};
}

#endif
