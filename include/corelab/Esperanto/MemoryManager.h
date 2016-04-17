#ifndef LLVM_CORELAB_MEMORY_MANAGER_H
#define LLVM_CORELAB_MEMORY_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

namespace corelab
{
	using namespace llvm;
	using namespace std;
	
	class MemoryManagerX86 : public ModulePass
	{
		public:
			virtual bool runOnModule(Module& M);
			virtual bool runOnFunction(Function *F);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;
			virtual void setFunctions(Module &M);

			static char ID;
			MemoryManagerX86() : ModulePass(ID) {}
			const char *getPassName() const { return "MEMORY_MANAGER_X86"; }

			protected:
			Constant* Malloc;
			Constant* Calloc;
			Constant* Realloc;
			Constant* Free;
			Type* voidTy;
			Type* ptrTy;
			Type* intTy;
	};

	class MemoryManagerArm : public ModulePass
	{
		public:
			virtual bool runOnModule(Module& M);
			virtual bool runOnFunction(Function *F);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;
			virtual void setFunctions(Module &M);

			static char ID;
			MemoryManagerArm() : ModulePass(ID) {}
			const char *getPassName() const { return "MEMORY_MANAGER_ARM"; }

			protected:
			Constant* Malloc;
			Constant* Calloc;
			Constant* Realloc;
			Constant* Free;
			Type* voidTy;
			Type* ptrTy;
			Type* intTy;
	};
}
#endif
