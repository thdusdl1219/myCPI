#ifndef LLVM_CORELAB_OFFLOAD_FUNCTIONPOINTERTRANSLATER_H
#define LLVM_CORELAB_OFFLOAD_FUNCTIONPOINTERTRANSLATER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "corelab/UVA/HoistVariables.h"

namespace corelab {
	using namespace llvm;
	using namespace std;

	class FunctionPointerTranslater {
		public:
			FunctionPointerTranslater(Module& M);
			void installBackTranslator(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout);
			void installTranslator(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout);
			void produceFunctionPointers(Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout);
			void consumeFunctionPointers(Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout);
			void installToAddrRegisters(Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout);
			void setFunctions(Module& M);
			void init(Module& M);
			typedef Module::iterator FI;
			typedef Function::iterator BI;
			typedef BasicBlock::iterator II;
			typedef vector<CallInst*>::iterator CI;

			Constant* ProduceFptr;
			Constant* ConsumeFptr;
			Constant* FunctionList;
			Constant* Translate;
			Constant* TranslateBack;
	};
}

#endif
