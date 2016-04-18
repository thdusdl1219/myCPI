#ifndef LLVM_CORELAB_FN_FILTER_H
#define LLVM_CORELAB_FN_FILTER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/LoopInfo.h"

#include "corelab/Metadata/LoadNamer.h"
#include "corelab/Callgraph/Callgraph.h"
#include "corelab/UVA/Common.h"

#include <map>
#include <set>
#include <vector>
#include <string>

#define OUT
#define UNUSED

#define IO_LEGACY 	0x1
#define IO_WIDE 		0x2
#define IO_BUFFER 	0x4

// inspection mode
//#define HOTFN_INSPECTION 10

using namespace llvm;
using namespace std;
using namespace corelab::UVA;

namespace corelab {
	class FnFilter : public ModulePass {
		public:
			static char ID;

			FnFilter () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "FN_FILTER"; }

			// Analysis interfaces
			bool isOffloadableFn (FuncID fnID);
			bool isOffloadableFn (Function *fn);
			bool isStaticAllowedFn (FuncID fnID);
			bool isStaticAllowedFn (Function *fn);
			bool isOffloadableLoop (LoopID loopID);

		private:
			Module *pM;
			LLVMContext *pC;
			CallGraph *grfCall;
			LoadNamer *loadNamer;

			set< string > setStaticAllowExtFns;
			set< string > setAllowExtFns;
			set< string > setJmpFns;
			set< Function * > setUnableFns;
	
		#ifdef HOTFN_INSPECTION
			struct CauseInfo {
				Value *source;
				string desc;

				CauseInfo (Value *_source, string _desc) {
					source = _source;
					desc = _desc;
				}
			};

			map< Function *, vector< CauseInfo > > mapFltCause;
		#endif	

			// function set loader
			void loadAllowedExternalFnSet ();
			void loadJumpFnSet ();

			void registExcludedExternalFns ();

			void dumpApprovedFns ();

			// Offloadable condition checkers
			bool hasOutputDef (BasicBlock *blk, unsigned option);
			bool hasUnhandledExternalFnUsage (BasicBlock *blk);
			bool hasInlineAsm (BasicBlock *blk);
			bool hasFnPointerUsage (BasicBlock *blk);
			UNUSED bool hasSignalHandling (BasicBlock *blk);
			UNUSED bool hasAnnotation (Function *fn);
			UNUSED bool hasThreading (Function *fn);

			bool hasUnoffloadableFnCall (BasicBlock *blk);

		#ifdef HOTFN_INSPECTION
			// inspection result dump
			void dumpInspectionResult ();
			void dumpRuledOutCause (Function *fn, set< Function * > setCausingFns);
		#endif

			// Set manipulator
			void registUnableFns (Function *invRoot, string cause);

			// helper method
			vector<Value *> getOperands (User *user);
			vector<Argument *> getFunctionArgs (Function *fn);
			bool isOutputFn (Function *fn, unsigned option);
			bool isAllowedExternalFn (Function *fn);
			bool isJumpFn (Function *fn);
			bool isOrHasFnPointerType (Type *ty);
			Function* getFnFromCallableVal (Value *val);
			Loop* getLoopFromID (LoopID loopID);

			// helper method auxiliary
			bool isOrHasFnPointerTypeAux (Type *ty, vector<StructType *> vecVisited = vector<StructType *> ());
	
			bool isMatchPattern (string str, string pattern);
			void tokenize (const string& str, vector<string>& tokens, const string& delimiters = " ");
	};
}

#endif
