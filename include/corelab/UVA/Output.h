/***
 * Output.cpp: Output replacer to dispatcher
 *
 * Replace (standard) output functions to according dispatcher
 * if output function is used without including appropriate
 * header (such as 'putwc' w/o <wchar.h>), output function may be
 * enclosed by bitcast.
 * XXX THIS CASE IS ASSUMED TO BE NOT EXIST XXX
 * written by: gwangmu
 *
 * **/

#ifndef LLVM_CORELAB_OUTPUT_REPL_SERVER_X86_H
#define LLVM_CORELAB_OUTPUT_REPL_SERVER_X86_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <map>
#include <string>

namespace corelab
{
	using namespace llvm;
	using namespace std;
	
	class OutputReplServer : public ModulePass
	{
		public:
			static char ID;

			OutputReplServer() : ModulePass(ID) {}
			const char *getPassName() const { return "OUTPUT_REPL_X86"; }

			virtual bool runOnModule(Module& M);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		private:
			typedef string FunctionName;
			typedef map<FunctionName, Constant *> DispatcherMap;
			typedef pair<FunctionName, Constant *> DispatcherMapElem;

			enum PlatformType { I386, ARM, X86_64, ANDROID, UNSUPPORTED };

			static const unsigned DEFAULT_ADDRESS_SPACE = 0;
			Module *pM;
			LLVMContext *pC;

			// Dispatchers
			DispatcherMap mapDsp;

			// helper method
			void setDispatchers (PlatformType platform);
			StructType* getFileStruct (PlatformType platform);
	};
}
#endif
