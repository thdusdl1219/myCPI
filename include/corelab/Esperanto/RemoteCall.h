#ifndef LLVM_REMOTE_CALL_H
#define LLVM_REMOTE_CALL_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

namespace corelab {
	using namespace llvm;
	using namespace std;

	class RemoteCall : public ModulePass {
		public:
			bool runOnModule(Module& M);
			void getAnalysisUsage(AnalysisUsage &AU) const;

			static char ID;
			RemoteCall() : ModulePass(ID) {}
			const char *getPassName() const { return "REMOTE_CALL"; }
			std::string deviceName;
			std::map<StringRef, GlobalVariable*> classMatching;
			std::map<Function*,bool> localFunctionTable;
		private:
			Constant* GenerateJobId;
      Constant* ProduceAsyncFunctionArgument;
			Constant* ProduceFunctionArgument;
			Constant* ConsumeReturn;
      Constant* PushArgument;
			void getClassPointer(Module& M);
			void setFunctions(Module& M);
			void substituteRemoteCall(Module& M);
			void generateFunctionTableProfile();
				
			Instruction* createJobId(Function* f, Instruction *insertBefore);
			void createProduceFArgs(Function* f, Instruction* I, Value* jobId, Instruction* insertBefore);
      void createProduceAsyncFArgs(Function* f, Instruction* I, Instruction* insertBefore);
			Instruction* createConsumeReturn(Function* f, Value* JobId, Instruction* I);
			
			vector<Instruction*> removedCallInst;
			vector<Instruction*> substitutedCallInst;
			void removeOriginalCallInst();
      int rc_id = 0;
	};
}

#endif
