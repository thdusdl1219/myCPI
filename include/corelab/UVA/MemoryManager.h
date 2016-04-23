#ifndef LLVM_CORELAB_MEMORY_MANAGER_H
#define LLVM_CORELAB_MEMORY_MANAGER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

namespace corelab
{
	using namespace llvm;
	using namespace std;
	
	class MemoryManagerX64 : public ModulePass
	{
		public:
			virtual bool runOnModule(Module& M);
			virtual bool runOnFunction(Function *F, bool is32);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;
			virtual void setFunctions(Module &M);

			bool isCppNewOperator(Function *F);
			bool isCppDeleteOperator(Function *F);

			static char ID;
			MemoryManagerX64() : ModulePass(ID) {}
			const char *getPassName() const { return "MEMORY_MANAGER_X64"; }

		protected:
			Constant* Malloc;
			Constant* Calloc;
			Constant* Realloc;
			Constant* Free;
			Constant* Strdup;
			Constant* Mmap;
      Constant* Load;
      Constant* Store;
      Constant* New64;
      Constant* New32;
			Type* voidTy;
			Type* ptrTy;
			Type* intTy;
	};

	class MemoryManagerX64S : public ModulePass
	{
		public:
			virtual bool runOnModule(Module& M);
			virtual bool runOnFunction(Function *F);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;
			virtual void setFunctions(Module &M);

			bool isCppNewOperator(Function *F);
			bool isCppDeleteOperator(Function *F);

			static char ID;
			MemoryManagerX64S() : ModulePass(ID) {}
			const char *getPassName() const { return "MEMORY_MANAGER_X64S"; }

		protected:
			Constant* Malloc;
			Constant* Calloc;
			Constant* Realloc;
			Constant* Free;
			Constant* Strdup;
			Constant* Mmap;
      Constant* Load;
      Constant* Store;
      Constant* New64;
      Constant* New32;
			Type* voidTy;
			Type* ptrTy;
			Type* intTy;
	};
	
  class MemoryManagerArm : public ModulePass
	{
		public:
			virtual bool runOnModule(Module& M);
			virtual bool runOnFunction(Function *F, bool is32);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;
			virtual void setFunctions(Module &M);

			bool isCppNewOperator(Function *F);
			bool isCppDeleteOperator(Function *F);

			static char ID;
			MemoryManagerArm() : ModulePass(ID) {}
			const char *getPassName() const { return "MEMORY_MANAGER_ARM"; }

		protected:
			Constant* Malloc;
			Constant* Calloc;
			Constant* Realloc;
			Constant* Free;
			Constant* Strdup;
			Constant* Mmap;
      Constant* Load;
      Constant* Store;
      //Constant* New64;
      Constant* New32;
			Type* voidTy;
			Type* ptrTy;
			Type* intTy;
	};
}
#endif
