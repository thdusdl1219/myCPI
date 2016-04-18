#ifndef LLVM_CORELAB_TRACER_INSTALLER_H
#define LLVM_CORELAB_TRACER_INSTALLER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <set>
#include <string>

#define OUT
#define UNUSED

using namespace llvm;
using namespace std;

namespace corelab {
	class PageTracerInstaller : public ModulePass {
	public:
		static char ID;

		PageTracerInstaller () : ModulePass (ID) {}

		void getAnalysisUsage (AnalysisUsage &AU) const;
		const char* getPassName () const { return "PAGE_TRACER_INSTALLER"; }

		// Pass callback
		bool runOnModule (Module& M);

	private:
		Module *pM;
		LLVMContext *pC;

		Type *tyVoid;
		Type *tyInt8p;
		Type *tyInt16;
		Type *tySizeT;

		void installInitFinal ();
		void installPageTracer ();

		// helper method
		ConstantInt* getConstantInt (unsigned bits, unsigned n);
		Instruction* getNextInstr (Instruction *inst);
		vector<Value *> getOperands (User *user);
	};
}

#endif
