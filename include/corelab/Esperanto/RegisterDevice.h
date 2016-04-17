#ifndef LLVM_MAIN_FCN_CREATOR_H
#define LLVM_MAIN_FCN_CREATOR_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "corelab/Esperanto/EspInit.h"
namespace corelab
{
	using namespace llvm;
	using namespace std;

	class DeviceLinker : public ModulePass
	{
		public:

			bool runOnModule(Module& M);
			void setFunctions(Module& M);
			void insertRegisterDevice(Module& M);
			StringRef createConstructorName();

			const char *getPassName() const { return "DeviceLinker"; }
			void getAnalysisUsage(AnalysisUsage& AU) const;
			static char ID;
			DeviceLinker() : ModulePass(ID) {}

			
		private:
			//Function* registerDevice(Instruction* inst);
			Constant* RegisterDevice;

	};
}

#endif

