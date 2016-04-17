#ifndef LLVM_CORELAB_PORTING64_H
#define LLVM_CORELAB_PORTING64_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <string>
#include <cstdio>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>

#define OUT

using namespace llvm;
using namespace std;

namespace corelab {
	class Porting64 : public ModulePass {
		public:
			typedef map<Value *, Value *> DefMap;
			typedef pair<Value *, Value *> DefPair;
			typedef vector<Value *> OperandVector;
			typedef ArrayRef<Value *> OperandArrayRef;
			typedef vector<Value *> DisposeVector;

			static char ID;

			Porting64 () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "PORTING64"; }

		private:
			Module *pM;
			LLVMContext *pC;

			Function *fnGInitzer;
			Instruction *instGInitzerTerm;

			Function *fnDummy;
			BasicBlock *blkDummy;
			Instruction *instDummy;

			const unsigned REPR_INT_LENGTH_FOR_PTR = 32;

			// FIXME: bundle this by key
			map<StructType *, StructType *> mapStructs;
			map<StructType *, vector<unsigned> > mapStElemIdx;
			map<Function *, Function *> mapFns;											// org to new map
			map<FunctionType *, FunctionType *> mapTyFns; 					// org to new type map 
			map<Function *, DefMap> mapFnArgs;											// XXX key is NEW FUNCTION 
			map<Function *, unsigned> mapFnArgOrgCnt;								// XXX key is NEW FUNCTION
			StructType *tyVaList;
			StructType *tyVaListTag;

			// Pass 1: converting target triple
			void convertTargetTriple ();	

			// Pass 2: porting module
			void portModule ();
	
			// Pass 2.1: loading type data
			vector<StructType *> loadStructData ();
			vector<FunctionType *> loadFunctionTypeData ();	
	
			// Pass 2.2: creating ported type
			StructType* createPortedStructDecl (StructType *tyOStruct);
			vector<unsigned> createPortedStructBody (StructType *tyOStruct); 
			FunctionType* createPortedFunctionType (FunctionType *tyFn);
			pair<vector<Type *>, vector<unsigned> > convertToPackedElems (vector<Type *> vecElems);
			pair<vector<Type *>, vector<unsigned> > convertToNoPackedElems (vector<Type *> vecElems);

			unsigned getTypeByteAlign (Type *type);
			unsigned raiseToNearPowerOf2 (unsigned num); 
			unsigned getStructByteAlign (StructType *tyStruct);
			unsigned raiseToMulPowerOf2 (unsigned num, unsigned unit);
			bool isExcludedStruct (StructType *tyOStruct);
			bool isPackRequiredStruct (StructType *tyOStruct);
			bool isExcludedGlobalVariable (GlobalVariable *gvar);
			bool hasPortedTypeArg (FunctionType *tyFn);	
			bool has64OrLongerBitLengthPrimitive (Type *type);

			// Pass 2.3: replacing to ported/pointer struct
			void replaceToPortedType ();

			void replacePassOnFunctionSign (vector<Function *> vecFns, DisposeVector &vecDisposed,
 						DisposeVector &vecDispFns);
			bool replacePassOnGlobalVariable (GlobalValue *gl, DefMap &mapGlobalDef, 
						DisposeVector &vecDisposed);
			void createConverterWrapper (Function *fn, DefMap &mapGlobalDef);
			void replacePassOnFunction (Function *fn, DefMap &mapGlobalDef, DefMap &mapLocalInit);
			//void replacePostPassOnFunctionSign (vector<Function *> vecTFns, DisposeVector &vecDispFns);

			OperandVector getOperands (User *user);
			void pendDef (Value *oVal, DefMap &mapDef);
			void tryRegistDef (Value *oInst, Value *nInst, DefMap &mapDef);
			bool redirectDef (Value *valFrom, Value *valTo, DefMap &mapDef);
			void removeDef (Value *oVal, DefMap &mapDef);
			vector<Value *> remapIndexList (vector<Value *> &vecIndices, Type *tyStart);
			vector<unsigned> remapUnsignedIndexList (vector<unsigned> &vecIndices, Type *tyStart);
			vector<unsigned> convertValueToUnsigned (vector<Value *> &vecValues);
			void registDispose (Value *inst, DisposeVector &vecDisposed);
			void disposeValues (DisposeVector &vecDisposed);
			void disposeFunctions (DisposeVector &vecDispFns, bool remainDecl);
			vector<Argument *> getFunctionArgs (Function *fn);
			Function* getFnFromCallableVal (Value *val);
			bool hasCallableType (Value *val);
			Value* getGlobalPlaceHolderValue (Type *ty);
			Constant *getDefaultInitzerConstant (Type *type);
			bool hasConstantExpr (Constant *cnst);
			bool isConverterWrapperRequired (Function *fn);
			bool isCoercedVaListArgument (Argument *arg);
			void insertInitzerToGInitzerFn (Value *val, Constant *cnst);

			// Porting method
			Type* portType (Type *type, OUT bool *ported = NULL);
			Type* unportType (Type *type, OUT bool *unported = NULL);
			Value* portDef (Value *oDef, DefMap &mapGlobalDef, DefMap &mapLocalDef, 
						OUT bool *ported = NULL);
			Constant* portConstant (Constant *cnst, DefMap &mapGlobalDef, DefMap &mapLocalDef,
						OUT bool *ported = NULL);
			vector<Instruction *> portInstruction (Instruction *inst, DefMap &mapGlobalDef, 
						DefMap &mapLocalDef, OUT bool *ported = NULL, OUT Value **repl = NULL);

			Type* toNominalizedType (Type *type);
			Constant* toNominalizedConstant (Constant *cnst);
			Type* getReprIntTypeForPtr ();

			PHINode* createPortedPHINodeDecl (PHINode *instPhi);
			void fillPortedPHINode (PHINode *instOPhi, PHINode *instNPhi, DefMap &mapGlobalDef, 
						DefMap &mapLocalDef);
			OperandVector portOperands (OperandVector &vecOpers, DefMap &mapGlobalDef,
						DefMap &mapLocalDef, OUT bool *ported = NULL, OUT bool *error = NULL);
			Value* portCallableValue (Value *val, DefMap &mapGlobalDef, OUT bool *ported = NULL);
			bool isPortedFunction (Function *fn);
			bool isOpaqueTy (Type *type);

			bool insertPortingCode (Value *valTar, Instruction *nextTo, bool reverse, OUT Value *&valRes,
						Value *valOut = NULL); 
			vector<BasicBlock *> generateCopyPortingCode (Value *valVal, Value *valPt, BasicBlock *blkExit, 
						bool reverse);

			// General helper method
			unsigned sizeOf (Type *type);
			ConstantInt* getConstantInt (unsigned bits, unsigned n);
			Constant* getAsConstant (Value *val);
			void printReplaceLog (string domain, Value *oVal, Value *nVal);

			// XXX DEBUG
			vector<Instruction *> getDebugInstructions (Value *val, unsigned fnid, unsigned blkid, unsigned instrid, unsigned tag);
			void dumpType (Type *type, unsigned level = 0);
			// XXX DEBUG
	};
}

#endif
