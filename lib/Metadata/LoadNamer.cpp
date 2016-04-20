/***
 * LoadCompareNamer.cpp
 *
 * Module pass to load the metadata from the file "metadata.profile", 
 * which is printed by "corelab/Metadata/Namer.h".
 * 
 * */

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/CallSite.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"

#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstdio>

using namespace corelab;

namespace corelab {

	char LoadNamer::ID = 0;
	static RegisterPass<LoadNamer> X("load-metadata", "load the metadata information from metadata file", false, false);

	bool LoadNamer::runOnModule(Module& M)
	{
		loadMetadata();
		return false;
	}

	void LoadNamer::getAnalysisUsage(AnalysisUsage &AU) const {
		AU.setPreservesAll();
	}
	
	bool LoadNamer::loadMetadata() {
		FILE *meta = fopen ("metadata.profile", "r");
		fseek(meta, 0, SEEK_SET);
		char strbuf[512];

		fgets (strbuf, sizeof(strbuf), meta);
		char asserting[20] = "#REGION metadata";
		assert ((strncmp (strbuf, asserting, strlen(asserting)) == 0) && "metadata is contaminated"); 
		{
			// get the number of units information
			fgets (strbuf, sizeof(strbuf), meta);
			sscanf (strbuf, ":LOAD_COUNT %zd", &numLoads);
			fgets (strbuf, sizeof(strbuf), meta);
			sscanf (strbuf, ":STORE_COUNT %zd", &numStores);
			fgets (strbuf, sizeof(strbuf), meta);
			sscanf (strbuf, ":BASICBLOCK_COUNT %zd", &numBBs);
			fgets (strbuf, sizeof(strbuf), meta);
			sscanf (strbuf, ":FUNCTION_COUNT %zd", &numFuncs);
			fgets (strbuf, sizeof(strbuf), meta);
			sscanf (strbuf, ":LOOP_COUNT %zd", &numLoops);
			fgets (strbuf, sizeof(strbuf), meta);
			sscanf (strbuf, ":CALL_COUNT %zd", &numCalls);
			fgets (strbuf, sizeof(strbuf), meta);
			sscanf (strbuf, ":CONTEXT_COUNT %zd", &numContexts);

			// get the function table
			fgets (strbuf, sizeof(strbuf), meta);
			strcpy (asserting, ":FUNCTION_TABLE");
			assert ((strncmp (strbuf, asserting, strlen(asserting)) == 0) && 
					"meta file is contaminated");

			while (fgets (strbuf, sizeof(strbuf), meta)) {
				if (strncmp (strbuf, ":", 1) == 0)
					break;

				uint16_t id = 0;
				char *name = (char*)malloc(sizeof(char)*256);		
				sscanf (strbuf, "[%" SCNu16 "] %s", &id, name);

				functionTable[id] = name;
			}

			// get the loop table
			strcpy(asserting, ":LOOP_TABLE");
			assert ((strncmp (strbuf, asserting, strlen(asserting)) == 0) && 
					"meta file is contaminated");

			while (fgets (strbuf, sizeof(strbuf), meta)) {
				if (strncmp (strbuf, ":", 1) == 0)
					break;

				uint16_t cid = 0;
				char *name = (char*)malloc(sizeof(char)*256);
				uint16_t fid = 0;
				uint16_t bid = 0;
				sscanf (strbuf, "[%" SCNu16 "] %s in %" SCNu16 ", %" SCNu16	"", &cid, name, &fid, &bid);

				LoopEntry *loopEntry = (LoopEntry*)malloc(sizeof(loopEntry));
				loopEntry->name = name;
				loopEntry->functionId = fid;
				loopEntry->basicBlockId = bid;
				loopTable[cid] = loopEntry;
			}

			// get the context table
			strcpy(asserting, ":CONTEXT_TABLE");
			assert ((strncmp (strbuf, asserting, strlen(asserting)) == 0) && 
					"meta file is contaminated");

			while (fgets (strbuf, sizeof(strbuf), meta)) {
				if (strncmp (strbuf, ":", 1) == 0)
					break;

				uint16_t calling = 0;
				uint16_t include = 0;
				uint16_t cid = 0;
				uint16_t bid = 0;
				CONTEXT_TYPE type;

				if (strstr(strbuf, "call")) {
					sscanf (strbuf, "[%" SCNu16 "] call %" SCNu16 " in %"	SCNu16 ", %" SCNu16 "",
							&cid, &calling, &include, &bid); 
					type = CONTEXT_CALL;
				}
				else {
					sscanf (strbuf, "[%" SCNu16 "] loop in %" SCNu16 ", %" SCNu16 "",
							&cid, &include, &bid); 
					type = CONTEXT_LOOP;
				}

				ContextInfo *contextInfo =
					(ContextInfo*)malloc(sizeof(ContextInfo));
				contextInfo->callingFunctionId = calling;
				contextInfo->includedFunctionId = include;
				contextInfo->contextType = type;
				contextInfo->basicBlockId = bid;

				contextTable[cid] = contextInfo;
			}
		}
		strcpy(asserting, ":INSTR_INFO");
		assert ((strncmp (strbuf, asserting, strlen(asserting)) == 0) && "meta file is contaminated");
			while (fgets (strbuf, sizeof(strbuf), meta)) {
				if (strncmp (strbuf, ":", 1) == 0)
					break;

				uint16_t instrId = 0;
				uint16_t bbId = 0;

				sscanf (strbuf, "[%" SCNu16 "] in %" SCNu16 "", &instrId,	&bbId);
				instructionToBBId[instrId] = bbId;
			}

		fclose(meta);
		return false;
	}
		
	Function* LoadNamer::getFunction(Module &M, uint16_t funcId) {
		for(Module::iterator FI = M.begin(), FE = M.end();
				FI != FE; ++FI) {
			if(getFunctionId(*FI) == funcId)
				return &(*FI);
		}
		return NULL;
	}

	// get Function Id
	uint16_t LoadNamer::getFunctionId(Function &F) {
		for(std::map<uint16_t, const char*>::iterator fi = functionTable.begin(), 
				fe = functionTable.end(); fi != fe; ++fi)
			if(strcmp(fi->second, F.getName().data()) == 0)
				return fi->first;
		return 0;
	}

	// get Function Id
	uint16_t LoadNamer::getFunctionId(const char* fName) {
		for(std::map<uint16_t, const char*>::iterator fi = functionTable.begin(), 
				fe = functionTable.end(); fi != fe; ++fi)
			if(strcmp(fi->second, fName) == 0)
				return fi->first;
		return 0;
	}

	// get Loop Context Id 
	uint16_t LoadNamer::getLoopContextId(Loop *L, uint16_t functionId) {
		BasicBlock *header = L->getHeader();
		for(std::map<uint16_t, LoopEntry*>::iterator li = loopTable.begin(), 
				le = loopTable.end(); li != le; ++li)
			if(li->second->functionId == functionId
					&& strcmp(li->second->name, header->getName().data()) == 0)
				return li->first;
		return 0;
	}

	uint16_t LoadNamer::getCallingFunctionId(uint16_t id) {
		return (contextTable[id])->includedFunctionId;
	}
	
	uint16_t LoadNamer::getCalledFunctionId(uint16_t id) {
		return (contextTable[id])->callingFunctionId;
	}

	void LoadNamer::reload() {
		loadMetadata();
		return;
	}
}
