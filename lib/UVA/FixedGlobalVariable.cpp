/***
 * FixedGlobalFactory.cpp : address-fixed global variable
 *
 * Simple IR hacking tool to create address-fixed global variables.
 * XXX FixedGlobalVariable cannot be exported outside the module. XXX
 * written by: gwangmu
 *
 * **/

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/StringMap.h"

#include "corelab/UVA/FixedGlobalVariable.h"
#include "corelab/Utilities/GlobalCtors.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <list>

#define DEBUG_HOIST

using namespace llvm;
using namespace corelab;
using namespace std;

static void createAndFillInitializers (Constant *initzer, Value *valPtr, 
		BasicBlock *blkAtEnd, list<Instruction *> &lstInstInitzer);
static void eraseInitializers (list<Instruction *> &lstInstInitzer);

// FixedGlobalVariable information.
// It contains fields, which could have been contained to an extended GlobalAlias class. 
// Damn, LLVM should be more flexible for IR guys than it is now.
struct FGInfo {
	uintptr_t uptVar;
	Constant *cnstInitzer;
	list<Instruction *> lstInstInitzer;		/**< This field must be consistent with the field above! */

	inline FGInfo () {
		uptVar = 0;
		cnstInitzer = NULL;
		lstInstInitzer = list<Instruction *> ();
	}
};

// Integer type, having the same with to pointers.
static Type *tyUintPtr;

// Instance register. All FixedGlobalVariables are registered in this set,
// until the user calls 'end' to finalize the transformation.
static map<FixedGlobalVariable *, FGInfo> mapFGvars;

// Parent module
static Module *pM;

// Base address of the FixedGlobalVariable chunk.
static uintptr_t uptBase;

// Total size of FixedGlobalVariables, reserved to calculate 
// the offset of next (new) FixedGlobalVariable.
static size_t sizeTotal;

// Global initializer basic block. (The only block in the initializer function)
// It sets up the global variable memory before any of the program codes are executed.
static Function *fnGInitzer;
static BasicBlock *blkGInitzer;


/* @detail Begin the IR transformation. 
 * 	Prepare the static manager before starting it. */
void FixedGlobalFactory::begin (Module *module, void *base, bool isFixGlbDuty) {
	/* Minor initializations */
	mapFGvars.clear ();

	tyUintPtr = Type::getIntNTy (module->getContext (),
			module->getDataLayout ().getPointerSizeInBits ());
	pM = module;
	uptBase = (uintptr_t)base;
	sizeTotal = 0;
	
	fnGInitzer = NULL;
	blkGInitzer = NULL;

	/* Create global initializer */
  if (isFixGlbDuty){
	FunctionType *tyFnVoidVoid = FunctionType::get (
			Type::getVoidTy (pM->getContext ()), false);
	fnGInitzer = Function::Create (tyFnVoidVoid, GlobalValue::InternalLinkage, 
			"__fixed_global_initializer__", pM);
	blkGInitzer = BasicBlock::Create (pM->getContext (), "initzer", fnGInitzer);

	callBeforeMain (fnGInitzer, 0);
  }
}


/* @detail End the IR transformation.
 *	Since FixedGlobalVariables are essentially nothing but a constant address,
 *	it doesn't have a real memory space, unless explicitly allocated using 'mmap'
 *	This method installs instructions into the global constructor, 
 *	making it to allocate the memory and store initializers if required. 
 *	The global constructor must be called the very first, even before
 *	other constructors are called, so that other codes cannot notice that
 *	FixedGlobalVariables are actually allocated at runtime.  */
void FixedGlobalFactory::end (bool isFixGlbDuty) {
	LLVMContext *pC = &pM->getContext ();
	Type *tyVoid = Type::getVoidTy (*pC);
	Type *tyInt8Pt = Type::getInt8PtrTy (*pC);
	Type *tyInt32 = Type::getInt32Ty (*pC);

	size_t sizeAlloc = (sizeTotal + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

  //if (isFixGlbDuty) {
	/* Create MMAP call in the initializer */
	BasicBlock *blkMmap = BasicBlock::Create (*pC, "mmap", fnGInitzer);
	BasicBlock *blkExcept = BasicBlock::Create (*pC, "except", fnGInitzer);

	blkGInitzer->moveAfter (blkMmap);

	vector<Value *> vecMmapArgs;
	vecMmapArgs.push_back (
			ConstantExpr::getCast (Instruction::IntToPtr,
					ConstantInt::get (tyUintPtr, uptBase), tyInt8Pt));
	vecMmapArgs.push_back (ConstantInt::get (tyUintPtr, sizeAlloc));
	vecMmapArgs.push_back (ConstantInt::get (tyInt32, 3));
	vecMmapArgs.push_back (ConstantInt::get (tyInt32, 50));
	vecMmapArgs.push_back (ConstantInt::get (tyInt32, -1));
	vecMmapArgs.push_back (ConstantInt::get (tyUintPtr, 0));

	Constant *cnstMmap = pM->getOrInsertFunction ("mmap", tyInt8Pt,
			tyInt8Pt, tyUintPtr, tyInt32, tyInt32, tyInt32, tyUintPtr, NULL);
	Instruction *instMmapCall = CallInst::Create (cnstMmap, vecMmapArgs, 
			"mmap.pt", blkMmap);
	Instruction *instCastedMmapRes = CastInst::CreatePointerCast (instMmapCall, tyUintPtr,
			"casted.mmap.pt", blkMmap);
	Instruction *instSuccess = new ICmpInst (*blkMmap, ICmpInst::ICMP_NE, instCastedMmapRes,
			ConstantInt::get (tyUintPtr, -1), "mmap.success");
	BranchInst::Create (blkGInitzer, blkExcept, instSuccess, blkMmap);

	/* Create an exception block */
	Constant *cnstAbort = pM->getOrInsertFunction ("abort",
			tyVoid, NULL);

	CallInst::Create (cnstAbort, "", blkExcept);
	new UnreachableInst (*pC, blkExcept);

	/* Create a terminator of the initializer block. */
	ReturnInst::Create (pM->getContext (), blkGInitzer);
  //}
	/* Reset fields */
	tyUintPtr = NULL;
	mapFGvars.clear ();
	uptBase = 0;
	sizeTotal = 0;

	fnGInitzer = NULL;
	blkGInitzer = NULL;

	return;
}


/* @detail FixedGlobalVariable creator
 * @param[in] initzer Default zero initializer if null */
FixedGlobalVariable* FixedGlobalFactory::create (Type *type, Constant *initzer,
		const Twine &name, bool isFixGlbDuty) {
	if (!initzer) initzer = Constant::getNullValue (type);

	assert (initzer && "The initializer should not be null after this point.");

	FixedGlobalVariable *fgvar =
			GlobalAlias::create (type, 0, GlobalValue::InternalLinkage, name,
					ConstantExpr::getCast (Instruction::IntToPtr, 
							ConstantInt::get (tyUintPtr, (uintptr_t)uptBase + sizeTotal),
						type->getPointerTo ()), pM);

	/* insert the created FGVAR to the instance register. */
	FGInfo fginfo;
	fginfo.uptVar = (uintptr_t)uptBase + sizeTotal;
	fginfo.cnstInitzer = initzer;
	if(isFixGlbDuty) createAndFillInitializers (initzer, fgvar, blkGInitzer, fginfo.lstInstInitzer);

	mapFGvars.insert (pair<FixedGlobalVariable *, FGInfo> (fgvar, fginfo));

	/* update the total chunk size of globals. */
	const DataLayout *dataLayout = &(pM->getDataLayout ());
	sizeTotal += dataLayout->getTypeAllocSize (type);

#ifdef DEBUG_HOIST
  printf("FIXGLB: FixedGlobalFactory::create: name( %s ), size ( %d )\n", name.str().c_str(), dataLayout->getTypeAllocSize(type));
#endif
	return fgvar;
}


/* @detail Remove the FixedGlobalVariable.
 * 	IMPORTANT: Don't use 'eraseFromParent ()' on a FixedGlobalVariable.
 * 	It ends up with messing up the initializer management,
 * 	resulting in the high chance of an invalid bitcode! */
void FixedGlobalFactory::erase (FixedGlobalVariable *fgvar) {
	assert (mapFGvars.find (fgvar) != mapFGvars.end () && "this is not a FixedGlobalVariable!");

	eraseInitializers (mapFGvars[fgvar].lstInstInitzer);
	fgvar->eraseFromParent ();
}


void* FixedGlobalFactory::getGlobalBaseAddress () {
	return (void *)uptBase;
}

// Returns the total size of this global chunk.
size_t FixedGlobalFactory::getTotalGlobalSize () {
	return sizeTotal;
}



void FixedGlobalFactory::setInitializer (FixedGlobalVariable *fgvar, Constant *cnst) {
	assert (mapFGvars.find (fgvar) != mapFGvars.end () && "this is not a FixedGlobalVariable!");

	/* Renew the initializer instruction. */
	eraseInitializers (mapFGvars[fgvar].lstInstInitzer);

	if (cnst) {
		mapFGvars[fgvar].cnstInitzer = cnst;
		createAndFillInitializers (cnst, fgvar, blkGInitzer, mapFGvars[fgvar].lstInstInitzer);
	}
	else {
		Constant *cnstNull = Constant::getNullValue (fgvar->getType()->getElementType ());

		mapFGvars[fgvar].cnstInitzer = cnstNull;
		createAndFillInitializers (cnstNull, fgvar, blkGInitzer, mapFGvars[fgvar].lstInstInitzer);
	}
}

Constant* FixedGlobalFactory::getInitializer (FixedGlobalVariable *fgvar) {
	assert (mapFGvars.find (fgvar) != mapFGvars.end () && "this is not a FixedGlobalVariable!");
	assert (mapFGvars[fgvar].cnstInitzer != NULL && "FixedGlobalVariable has no initializer");

	return mapFGvars[fgvar].cnstInitzer;
}

bool FixedGlobalFactory::hasInitiailzer (FixedGlobalVariable *fgvar) {
	assert (mapFGvars.find (fgvar) != mapFGvars.end () && "this is not a FixedGlobalVariable!");
	return (mapFGvars[fgvar].cnstInitzer != NULL);
}


void* FixedGlobalFactory::getFixedAddress (FixedGlobalVariable *fgvar) {
	assert (mapFGvars.find (fgvar) != mapFGvars.end () && "this is not a FixedGlobalVariable!");
	return (void *)mapFGvars[fgvar].uptVar;
}


static void createAndFillInitializers (Constant *initzer, Value *valPtr, 
		BasicBlock *blkAtEnd, list<Instruction *> &lstInstInitzer) {
	if (ConstantDataArray *cnstArrInitzer = dyn_cast<ConstantDataArray> (initzer)) {
#ifdef DEBUG_HOIST
    printf("FIXGLB: createAndFillInitializers: is ConstantDataArray initzer\n");
#endif
		// WORKAROUND: if INITZER is a constant array initializer,
		// probably it may need splitting to avoid 'vector width exceeded' assertion.
		Type *tyInt8 = Type::getInt8Ty (pM->getContext ());
		Type *tyInt32 = Type::getInt32Ty (pM->getContext ());

		ArrayType *tyArrInitzer = dyn_cast<ArrayType> (cnstArrInitzer->getType ());

		const unsigned CNST_DATASEQ_MAX_WIDTH = 8000;
		if (tyArrInitzer->getNumElements () > CNST_DATASEQ_MAX_WIDTH &&
				tyArrInitzer->getElementType () == tyInt8) {
			vector<Value *> vecIndices;
			vecIndices.push_back (ConstantInt::get (tyInt32, 0));
			vecIndices.push_back (NULL);	/**< will be replaced during the loop */

			for (unsigned i = 0; i < tyArrInitzer->getNumElements (); 
					 i += CNST_DATASEQ_MAX_WIDTH) {
				unsigned sizeRSeq = tyArrInitzer->getNumElements () - i;
				unsigned sizePSeq = (sizeRSeq > CNST_DATASEQ_MAX_WIDTH) ? CNST_DATASEQ_MAX_WIDTH : sizeRSeq;
				
				/* prepare indices */
				vecIndices[1] = ConstantInt::get (tyInt32, i);
				
				/* prepare a partial initializer data sequential */
				vector<uint8_t> vecPDataSeq;
				for (unsigned j = 0; j < sizePSeq; j++) {
					vecPDataSeq.push_back (cnstArrInitzer->getElementAsInteger (j + i));
				}
				Constant *cnstPArrInitzer = ConstantDataArray::get (pM->getContext (), vecPDataSeq);
				
				/* insert partial initializer instructions */
				Instruction *instElemPtr = GetElementPtrInst::Create (valPtr->getType(), valPtr, vecIndices, "arr.elem", blkAtEnd);
				Instruction *instCast = CastInst::CreatePointerCast (instElemPtr,
						ArrayType::get (tyArrInitzer->getElementType (), sizePSeq)->getPointerTo ()
						, "p.ptr", blkAtEnd);
				Instruction *instStore = new StoreInst (cnstPArrInitzer, instCast, blkAtEnd);
				
				lstInstInitzer.push_back (instElemPtr);
				lstInstInitzer.push_back (instCast);
				lstInstInitzer.push_back (instStore);
			}
		}
		else
			goto fallback;
	}
	else {
		goto fallback;
	}

	return;

fallback:
	lstInstInitzer.push_back (new StoreInst (initzer, valPtr, blkAtEnd));
	return;
}

static void eraseInitializers (list<Instruction *> &lstInstInitzer) {
	while (!lstInstInitzer.empty ()) {
		Instruction *inst = lstInstInitzer.back ();
		lstInstInitzer.pop_back ();
		
		if (!inst->user_empty ()) {
			lstInstInitzer.push_front (inst);
			continue;
		}

		inst->eraseFromParent ();
	}
}
