#ifndef LLVM_CORELAB_FN_SELECTOR_H
#define LLVM_CORELAB_FN_SELECTOR_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include "corelab/Metadata/LoadNamer.h"
#include "corelab/UVA/Filter.h"
#include "corelab/UVA/Common.h"

#include <vector>

#define OUT
#define UNUSED

using namespace llvm;
using namespace std;
using namespace corelab::UVA;

namespace corelab {
	class FnSelector : public ModulePass {
		public:
			static char ID;

			FnSelector () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "FN_SELECTOR"; }

		private:
			struct RtnID {
				unsigned 	id;
				unsigned 	type;

				inline bool operator== (const RtnID& rtnID) const {
					return (id == rtnID.id && type == rtnID.type);
				}
			};

			struct HotRtnEntry {
				RtnID 		rtnID;

				unsigned 	tExec;
				unsigned 	cntCall;
				long long gain;
			};

			typedef vector<HotRtnEntry> HotRtnVector;
			typedef pair<CallGraphNode *, CallGraphNode *> CallGraphEdge;
	
			HotRtnVector getHotRoutineProfile ();
			HotRtnVector getOffloadingTargets (HotRtnVector &vecRtnRank, bool takeLoop);
			HotRtnVector functionizeTargets (HotRtnVector &vecTargets);
			HotRtnVector selectJumpResistantTargets (HotRtnVector &vecTargets);
			void printOffloadingTargets (HotRtnVector vecFnTargets);
	
			void printPlausibleCandidates ();
				void insertPathsToMainNodeRec (CallGraphNode *start, vector<CallGraphNode *> path,
						set<CallGraphNode *> &setLabels, set<CallGraphEdge> &setEdges);
				void printCandidateList (set<CallGraphNode *> &setCandid);
					static bool compareNode (CallGraphNode *na, CallGraphNode *nb); 
				void printCandidateDotGraph (set<CallGraphNode *> &setCandid);

			Loop* getLoopFromID (LoopID loopID);
			Function* getParentFunction (Loop *loop);
			bool containsID (HotRtnVector &vecHotRtn, RtnID rtnID);

			Module *pM;
			LLVMContext *pC;
			LoadNamer *loadNamer;
			Namer *namer;
			FnFilter *filter;

			const unsigned PLAUSIBLE_FN_THRESHOLD = 30;
			const unsigned PLAUSIBLE_RANK_THRESHOLD = 3;
			const unsigned CHILD_WIDTH_THRESHOLD = 3;

			const unsigned NODE_HAS_EXCESSIVE_CHILDREN = 0x1;
			const unsigned NODE_INVISIBLE = 0x2;
	};
}

#endif
