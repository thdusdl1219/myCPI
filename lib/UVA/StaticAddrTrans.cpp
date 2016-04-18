/***
 * StaticAddrTrans.cpp : static address translator (bit converter)
 *
 * inserts cast instructions for address bit length conversion
 * written by : gwangmu 
 *
 * **/

#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"

#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/UVA/StaticAddrTrans.h"
#include "corelab/Utilities/Debug.h"

#include <vector>
#include <inttypes.h>

using namespace corelab;
using namespace std;

static RegisterPass<StaticAddrTrans> X("static-addrtrans", "installs static address translator", false, false);

char StaticAddrTrans::ID = 0;

void StaticAddrTrans::getAnalysisUsage (AnalysisUsage &AU) const {
	AU.setPreservesAll ();
}

bool StaticAddrTrans::runOnModule (Module& M) {
	this->pM = &M;
	this->pC = &M.getContext ();

	//convertStructMemberPtrToI32 ();
	installAddrTranslator ();

	return false;
}

//void StaticAddrTrans::convertStructMemberPtrToI32 () {
//	/* Find structure types */
//	TypeFinder tyFinder;
//	vector<StructType *> vecStructs;
//
//	tyFinder.run (*pM, false);
//	vecStructs.insert (vecStructs.begin(), tyFinder.begin(), tyFinder.end());
//
//	/* Create pseudo-pointer types */
//	vector<Type *> vecPPtrElem;
//	vecPPtrElem.push_back (Type::getInt32Ty (*pC));
//
//	map<PointerType *, Type *> mapPtrToPPtr;
//	for (vector<StructType *>::iterator ity = vecStructs.begin (); ity != vecStructs.end (); ity++) {
//		StructType *tyStruct = *ity;
//
//		if (!tyStruct->isOpaque () && !tyStruct->isLiteral ()) {
//			StructType *tyPPtr = StructType::create (*pC, vecPPtrElem, tyStruct->getName().str() + string ("_pt"));
//			mapPtrToPPtr[tyStruct->getPointerTo ()] = tyPPtr;
//		}
//	}
//
//	/* Create structure replacements, which use pseudo-pointers */
//	map<StructType *, StructType *> mapOToNStruct;
//	for (vector<StructType *>::iterator ity = vecStructs.begin (); ity != vecStructs.end (); ity++) {
//		StructType *tyStruct = *ity;
//
//		if (!tyStruct->isOpaque () && !tyStruct->isLiteral ()) {
//			vector<Type *> const vecElems = tyStruct->elements ();
//			vector<Type *> vecNElems;
//
//			for (unsigned i = 0; i < vecElems.size (); i++) {
//				if (PointerType *tyPtElem = dyn_cast<PointerType> (vecElems[i]))
//					vecNElems.push_back (mapPtrToPPtr[tyPtElem]);
//				else
//					vecNElems.push_back (vecElems[i]);
//			}
//		
//			StructType *tyNStruct = StructType::create (*pC, vecNElems, 
//				tyStruct->getName().str() + string ("_repl"), tyStruct->isPacked ());
//
//			mapOToNStruct[tyStruct] = tyNStruct;
//		}
//	}
//
//	/* XXX Mutate struct types to corresponding replacements */
//	for (Module::iterator ifn = pM->begin (); ifn != pM->end(); ifn++) {
//		for (Function::iterator iblk = ifn->begin (); iblk != ifn->end (); iblk++) {
//			for (BasicBlock::iterator iinst = iblk->begin (); iinst != iblk->end (); iinst++) {
//				Value *val = &*iinst;
//				StructType
//
//				if (mapOToNStruct.find (val->getType ()) != mapOToNStruct.end ())
//					val->mutateType (mapOToNStruct [val]);
//			}
//		}
//	}
//
//	return;
//}

void StaticAddrTrans::installAddrTranslator () {
	Type *tySrcWord = Type::getInt32Ty (*pC);			// XXX from 32-bit memory
	Type *tyDstWord = Type::getInt64Ty (*pC);			// XXX to 64-bit memory
	Type *tySrcWordPt = PointerType::get (tySrcWord, 0);

	vector<Instruction *> vecDisposed;

	for (Module::iterator ifn = pM->begin (); ifn != pM->end(); ifn++) {
		for (Function::iterator iblk = ifn->begin (); iblk != ifn->end (); iblk++) {
			for (BasicBlock::iterator iinst = iblk->begin (); iinst != iblk->end (); iinst++) {
				Instruction *inst = &*iinst;

				// (32-bit)pointers on mem. --> (64-bit)pointers on reg.
				if (LoadInst *instLoad = dyn_cast<LoadInst> (inst)) {
					if (dyn_cast<PointerType> (instLoad->getType ())) {
						Type *tyRes = instLoad->getType ();
						Value *valAddr = instLoad->getPointerOperand ();

						Value *valTSrcPt;
						LoadInst *instTSrc;
						Value *valSrc;

						if (valAddr->getType () != tySrcWordPt && tySrcWord != tyRes) {
							valTSrcPt = CastInst::CreateTruncOrBitCast (valAddr, tySrcWordPt, "tsrc_pt", instLoad);
							instTSrc = new LoadInst (valTSrcPt, "tsrc", instLoad);
							valSrc = CastInst::CreateBitOrPointerCast (instTSrc, tyRes, "src", instLoad);

							instLoad->replaceAllUsesWith (valSrc);
							vecDisposed.push_back (instLoad);
						}
					}
				}

				// (64-bit)pointers on reg. --> (32-bit)pointers on mem.
				if (StoreInst *instStore = dyn_cast<StoreInst> (inst)) {
					if (dyn_cast<PointerType> (instStore->getValueOperand()->getType ())) {
						Value *valValue = instStore->getValueOperand ();
						Value *valPt = instStore->getPointerOperand ();

						Value *valTSrc; 
						Value *valTSrcPt;

						if (valValue->getType () != tySrcWord && valPt->getType () != tySrcWordPt) {
							valTSrc = CastInst::CreateBitOrPointerCast (valValue, tySrcWord, "tsrc", instStore);
							valTSrcPt = new BitCastInst (valPt, tySrcWordPt, "tsrc_pt", instStore);
							new StoreInst (valTSrc, valTSrcPt, instStore);
						
							vecDisposed.push_back (instStore);
						}
					}
				}

			}
		}
	}

	for (int i = 0; i < vecDisposed.size (); i++)
		vecDisposed[i]->eraseFromParent ();

	return;
}
