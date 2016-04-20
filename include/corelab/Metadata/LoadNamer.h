#ifndef LLVM_CORELAB_LOADNAMER_H
#define LLVM_CORELAB_LOADNAMER_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "corelab/Metadata/Metadata.h"

namespace corelab
{
	using namespace llvm;
	using namespace std;
		
	class LoadNamer : public ModulePass
	{
		public:

			bool runOnModule(Module& M);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;
			
			const char *getPassName() const { return "LoadNamer"; }
			static char ID;
			LoadNamer() : ModulePass(ID) {}
			
			/* Metadata Information */
			std::map<uint16_t, uint16_t> instructionToBBId;
			// data structures
			std::map<uint16_t, ContextInfo*> contextTable; // ctxId -> ctxInfo
			std::map<uint16_t, const char*> functionTable; // funcId -> name
			std::map<uint16_t, LoopEntry*> loopTable; // ctxId -> loopInfo

			// number of units
			size_t numLoads;
			size_t numStores;
			size_t numBBs;
			size_t numFuncs;
			size_t numLoops;
			size_t numCalls;
			size_t numContexts;

			bool loadMetadata();

			Function* getFunction(Module& M, uint16_t funcId);
			uint16_t getFunctionId(Function &F);
			uint16_t getFunctionId(const char* fName);
			uint16_t getCallingFunctionId(uint16_t id);
			uint16_t getCalledFunctionId(uint16_t id);
			uint16_t getLoopContextId(Loop *L, uint16_t functionId);
			void reload();
	};
}

#endif

