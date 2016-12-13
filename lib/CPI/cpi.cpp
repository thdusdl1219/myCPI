#include "corelab/CPI/CPI.h"
#include "llvm/PassSupport.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/TargetFolder.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <iostream>


using namespace corelab;
using namespace std;

static void createCPIFunctions(const DataLayout *DL, Module &M, CPIFunctions &CF) {
  LLVMContext &C = M.getContext();
  Type* VoidTy = Type::getVoidTy(C);
  Type* Int8PtrTy = Type::getInt8PtrTy(C);
  Type* Int8PtrPtrTy = Int8PtrTy->getPointerTo(); 

  CF.CPIInit = cast<Function>(M.getOrInsertFunction("__cpi_init", VoidTy, NULL));
  CF.CPISet = cast<Function>(M.getOrInsertFunction("__cpi_set", VoidTy, Int8PtrPtrTy, Int8PtrTy, NULL));
  CF.CPIGet = cast<Function>(M.getOrInsertFunction("__cpi_get", Int8PtrTy, Int8PtrPtrTy, NULL));
  

}

static bool isUsedAsFPtr(Value *FPtr) {
  SmallVector<Value*, 16> WorkList;
  WorkList.push_back(FPtr);

  while(!WorkList.empty()) {
    Value *Val = WorkList.pop_back_val();
    for (Value::user_iterator it = Val->user_begin(), ie = Val->user_end(); it != ie; ++it) {
      User *U = *it;
      if(CastInst *CI = dyn_cast<CastInst>(U)) {
        if(PointerType *Pty = dyn_cast<PointerType>(CI->getType()))
          if(Pty->getElementType()->isFunctionTy())
            return true;
      } else if(isa<CmpInst>(U)) 
        continue;
      else if(isa<PHINode>(U) || isa<SelectInst>(U)) {
        WorkList.push_back(U);
      } else {
        return true;
      }
    }
  }
  return false;
}

static bool IsSafeStackAlloca(AllocaInst *AI, const DataLayout * DL) {
  SmallPtrSet<Value*, 16> Visited;
  SmallVector<Instruction*, 8> WorkList;
  WorkList.push_back(AI);

  while (!WorkList.empty()) {
    Instruction *V = WorkList.pop_back_val();
    for (Value::use_iterator UI = V->use_begin(),
                             UE = V->use_end(); UI != UE; ++UI) {
      Use *U = &*UI;
      Instruction *I = cast<Instruction>(U->getUser());
      assert(V == U->get());

      switch (I->getOpcode()) {
      case Instruction::Load:
        break;
      case Instruction::VAArg:
        break;
      case Instruction::Store:
        if (V == I->getOperand(0))
          return false;
        break;

      case Instruction::GetElementPtr:
        if (!cast<GetElementPtrInst>(I)->hasAllConstantIndices())
          return false;

        if (!isa<ConstantInt>(AI->getArraySize()))
          return false;

      case Instruction::BitCast:
      case Instruction::PHI:
      case Instruction::Select:
        if (Visited.insert(I).second)
          WorkList.push_back(cast<Instruction>(I));
        break;

      case Instruction::Call:
      case Instruction::Invoke: {
        CallSite CS(I);

        if (CS.onlyReadsMemory() &&
            I->getType()->isVoidTy())
          continue;

        CallSite::arg_iterator B = CS.arg_begin(), E = CS.arg_end();
        for (CallSite::arg_iterator A = B; A != E; ++A)
          if (A->get() == V && !CS.doesNotCapture(A - B))
            return false;
        continue;
      }

      default:
        return false;
      }
    }
  }
  return true;
}


char CPIPre::ID = 0;
static RegisterPass<CPIPre> X("cpipre", "cpipre", false, false);

char CPI::ID = 0;
static RegisterPass<CPI> Y("cpi", "cpi", false, false);

bool CPIPre::runOnModule(Module &M) {
  const unsigned numCPIFn = sizeof(CPIFunctions) / sizeof(Function*);
  union {
    CPIFunctions CF;
    GlobalValue *GV[numCPIFn];
  };

  createCPIFunctions(DL, M, CF);

  Type* Int8PtrTy = Type::getInt8PtrTy(M.getContext());
/*  for(unsigned i = 0; i < numCPIFn; ++i) {
    if(GV[i]) appendToGlobalArray(M, "llvm.compiler.used", ConstantExpr::getBitCast(GV[i], Int8PtrTy));
  }

  M.getGlobalVariable("llvm.compiler.used")->setSection("llvm.metadata"); */
  return true;
}


bool CPI::doInitialization(Module &M) {

return false;
}

Function *CPI::createGlobalsReload(Module &M, StringRef N) {
  LLVMContext &C = M.getContext();
  Function *F = Function::Create(
      FunctionType::get(Type::getVoidTy(C), false), 
      GlobalValue::InternalLinkage, N, &M);

  TargetFolder TF(*DL);
  IRBuilder<TargetFolder> IRB(C, TF);

  BasicBlock *Entry = BasicBlock::Create(C, "", F);
  IRB.SetInsertPoint(Entry);

  Instruction *CI = IRB.CreateCall(CF.CPIInit);

  IRB.CreateRetVoid();
  IRB.SetInsertPoint(IRB.GetInsertBlock());

  return F;
}

bool CPI::doFinalization(Module &M) {

  return false;
}

bool CPI::externallyCalled(Function* F) {
  SmallSet<Value*, 16> Visited;
  SmallVector<Value*, 16> WorkList;
  WorkList.push_back(F);
  
  do {
    Value *V = WorkList.pop_back_val();

    for(Value::user_iterator i = V->user_begin(); i != V->user_end(); ++i) {
      User *U = *i;
      if(isa<BlockAddress>(U))
        continue;

      CallSite CS(U);
      if(CS) {
        if(CS.getCalledValue() != V && CS.getCalledFunction() && CS.getCalledFunction()->isDeclaration())
          return true;

        continue;
      }

      Operator *OP = dyn_cast<Operator>(U);
      if(OP) {
        switch (OP->getOpcode()) {
          case Instruction::BitCast:
          case Instruction::PHI:
          case Instruction::Select:
            if(Visited.insert(U).second)
              WorkList.push_back(U);
            break;
          default:
            break;
        }

      }
    }
  } while(!WorkList.empty());
  return true;
}

bool CPI::protectType(Type *Ty, bool IsStore, MDNode* TBAATag) {
  cout << Ty->isFunctionTy() << endl;
  cout << Ty->isPointerTy() << endl;
  if(Ty->isFunctionTy() || (Ty->isPointerTy() && printf("hi") && cast<PointerType>(Ty)->getElementType()->isFunctionTy())) {
    return true;
  }
  else if (Ty->isIntegerTy())
    return false;
  else if (PointerType *PTy = dyn_cast<PointerType>(Ty)) {
    if(IsStore && PTy->getElementType()->isStructTy() &&
        cast<StructType>(PTy->getElementType())->getNumElements() == 0 &&
        TBAATag && TBAATag->getNumOperands() > 1 &&
        cast<MDString>(TBAATag->getOperand(0))->getString() == "function pointer")
      return true;
    if(IsStore && PTy->getElementType()->isIntegerTy(8)) {
      bool result = false;
      if(TBAATag) {
        for(unsigned int i = 0; i < TBAATag->getNumOperands(); i++) {
          MDNode* tmp = dyn_cast_or_null<MDNode>(TBAATag->getOperand(i));
          if(tmp) {
            MDString *TagName = cast<MDString>(tmp->getOperand(0));
            if(TagName->getString() == "void pointer" || TagName->getString() == "function pointer")
              result = true;
          }
        }
        
        TBAATag->dump();
        TBAATag->getOperand(1).get()->dump();
        TBAATag->getOperand(0).get()->dump(); /*
        MDString *TagName = cast<MDString>(TBAATag->getOperand(0));
        return TagName->getString() == "void pointer" || TagName->getString() == "function pointer";
        */
        return result;
      }
      
      return true;
    }

    return protectType(PTy->getElementType(), IsStore);
  }
  else if (SequentialType *PTy = dyn_cast<SequentialType>(Ty))
    return protectType(PTy->getElementType(), IsStore);
  else if (StructType *STy = dyn_cast<StructType>(Ty)) {
    return false;
    // TODO handle struct type
  }

  else
    return false;
}

bool CPI::protectValue(Value *Val, bool IsStore, MDNode *TBAATag) {
  return protectType(Val->getType(), IsStore, TBAATag);
}

bool CPI::protectLoc(Value* Loc, bool IsStore) {
  if (!IsStore && AA->pointsToConstantMemory(Loc))
    return false;

  SmallPtrSet<Value*, 8> Visited;
  SmallVector<Value*, 8> WorkList;
  WorkList.push_back(Loc);
  do {
    Value *P = WorkList.pop_back_val();
    cout << P->getName().data() << endl;
    P = GetUnderlyingObject(P, *DL, 0);
    if(!Visited.insert(P).second)
      continue;
    if(SelectInst *SI = dyn_cast<SelectInst>(P)) {
      WorkList.push_back(SI->getTrueValue());
      WorkList.push_back(SI->getFalseValue());
      continue;
    }

    if(PHINode *PN = dyn_cast<PHINode>(P)) {
      for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i)
        WorkList.push_back(PN->getIncomingValue(i));
      continue;
    }

    if(AllocaInst *AI = dyn_cast<AllocaInst>(P)) {
      //if(!IsSafeStackAlloca(AI, DL))
        return true;
      cout << "SafeStack" << endl;
      //continue;
    } else if (isa<GlobalVariable>(P) &&
        cast<GlobalVariable>(P)->isConstant()) {
      if(IsStore) {
        return true;
      }
    } else {
      if(IsStore || !AA->pointsToConstantMemory(P))
        return true;
    }

  } while(!WorkList.empty());
  return false;
}

bool CPI::pointsToVTable(Value *Ptr) {
  SmallVector<Value*, 8> Objects;
  GetUnderlyingObjects(Ptr, Objects, *DL);
  for (unsigned i = 0, e = Objects.size(); i != e; ++i) {
    Instruction *I = dyn_cast<Instruction>(Objects[i]);
    if(!I)
      return false;

    MDNode *TBAATag = I->getMetadata(LLVMContext::MD_tbaa);
    if(!TBAATag)
      return false;
    bool result = false;
    for(unsigned int i = 0; i < TBAATag->getNumOperands(); i++) {
      MDNode* tmp = dyn_cast_or_null<MDNode>(TBAATag->getOperand(i));
      if(tmp) {
        MDString *TagName = cast<MDString>(tmp->getOperand(0));
        if(TagName->getString() == "vtable pointer")
          result = true;
      }
    }
    if(!result)
      return false;
    /* StringRef T = cast<MDString>(TBAATag->getOperand(0))->getString();
    if(T != "vtable pointer")
      return false; */
  }
  return true;
}

void CPI::insertChecks(DenseMap<Value*, Value*> &BM, Value *V, bool IsDereferenced, 
    SetVector<std::pair<Instruction*, Instruction*> > &ReplMap) {
  if(BM.count(V))
    return;

  BM[V] = NULL;
      cout << "hi0" << endl;
  if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
      cout << "hi1" << endl;
      cout << LI->getType()->isPointerTy() << endl;
      cout << LI->getMetadata("vaarg.load") << endl;
      cout << protectLoc(LI->getPointerOperand(), false) << endl;
      cout << protectValue(LI, false, LI->getMetadata(LLVMContext::MD_tbaa)) << endl;
      cout << pointsToVTable(LI->getPointerOperand()) << endl;
      cout << isUsedAsFPtr(V) << endl;
    if(LI->getType()->isPointerTy() &&
        !LI->getMetadata("vaarg.load") &&
        protectLoc(LI->getPointerOperand(), false) &&
        protectValue(LI, false, LI->getMetadata(LLVMContext::MD_tbaa)) &&
        !pointsToVTable(LI->getPointerOperand()) &&
        isUsedAsFPtr(V)) {
      cout << "hi2" << endl;
      IRBuilder<> IRB(LI->getNextNode());
     
      Instruction *SVal = IRB.CreateCall(CF.CPIGet, IRB.CreatePointerCast(LI->getPointerOperand(), IRB.getInt8PtrTy()->getPointerTo()));
      if(MDNode *TBAA = LI->getMetadata(LLVMContext::MD_tbaa))
        SVal->setMetadata(LLVMContext::MD_tbaa, TBAA);
      SVal = cast<Instruction>(IRB.CreateBitCast(SVal, LI->getType()));

      bool inserted = ReplMap.insert(std::make_pair(LI, SVal));
      
      BM[SVal] = NULL;
    } 
  } else if (PHINode *PHI = dyn_cast<PHINode>(V)) {
    unsigned N = PHI->getNumIncomingValues();
    for(unsigned i = 0; i < N; i++)
      insertChecks(BM, PHI->getIncomingValue(i), IsDereferenced, ReplMap);
  } else if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
    insertChecks(BM, SI->getTrueValue(), IsDereferenced, ReplMap);
    insertChecks(BM, SI->getFalseValue(), IsDereferenced, ReplMap);
  } else if (BitCastInst *CI = dyn_cast<BitCastInst>(V)) {
    insertChecks(BM, CI->getOperand(0), IsDereferenced, ReplMap);
  } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    insertChecks(BM, GEP->getPointerOperand(), IsDereferenced, ReplMap);
  }
}

bool CPI::runOnFunction(Function &F) {
  LLVMContext &C = F.getContext();

  SetVector<Value*> NeedBounds;
  std::vector<std::pair<Instruction*, std::pair<Value*, Value*> > > BoundsSTabStores;
  std::set<Value*> IsDereferenced;
  

  for(inst_iterator it = inst_begin(F); it != inst_end(F); ++it) {
    Instruction *I = &*it;
    if(StoreInst *SI = dyn_cast<StoreInst>(I)) {
    
      if(SI->getValueOperand()->getType()->isPointerTy() &&
         (protectValue(SI->getValueOperand(), true, SI->getMetadata(LLVMContext::MD_tbaa)) ||
          (isa<Operator>(SI->getValueOperand()) &&
           cast<Operator>(SI->getValueOperand())->getOpcode() ==
            Instruction::BitCast &&
            protectValue(cast<Operator>(SI->getValueOperand())->getOperand(0), true)))) {
        NeedBounds.insert(SI->getValueOperand());
        BoundsSTabStores.push_back(std::make_pair(SI, std::make_pair(SI->getPointerOperand(), SI->getValueOperand())));
      }
    } else if(isa<CallInst>(I) || isa<InvokeInst> (I)) {
        CallSite CS(I);
        //if(!isa<Constant>(CS.getCalledValue())) {
          NeedBounds.insert(CS.getCalledValue());
          IsDereferenced.insert(CS.getCalledValue());
        //}

        Function *CF = CS.getCalledFunction();
        
        if(CF && (CF->getName().startswith("llvm.") || CF->getName().startswith("__cpi_")))
          continue;
        
        for (unsigned i = 0; i != CS.arg_size(); ++i) {
          Value *A = CS.getArgument(i);
          if(protectValue(A, true)) {
            NeedBounds.insert(A); 
          }
        }  
      }
      else if(ReturnInst *RI = dyn_cast<ReturnInst>(I)) {
        Value *RV = RI->getReturnValue();
        if(RV && !isa<Constant>(RV) && protectValue(RV, true)) {
          NeedBounds.insert(RV);
        }
      }
    }

  DenseMap<Value*, Value*> BoundsMap;
  SetVector<std::pair<Instruction*, Instruction*> > ReplMap;

  for(unsigned i = 0, e = NeedBounds.size(); i != e; ++i)
    insertChecks(BoundsMap, NeedBounds[i], IsDereferenced.find(NeedBounds[i]) != IsDereferenced.end(), ReplMap);
  
  Type* Int8PtrTy = Type::getInt8PtrTy(C);
  Type* Int8PtrPtrTy = Int8PtrTy->getPointerTo();

  for(unsigned i = 0, e = BoundsSTabStores.size(); i != e; ++i) {
    IRBuilder<> IRB(BoundsSTabStores[i].first);
    
    Value *Loc = IRB.CreateBitCast(BoundsSTabStores[i].second.first, Int8PtrPtrTy);
    Value *Val = IRB.CreateBitCast(BoundsSTabStores[i].second.second, Int8PtrTy);
    IRB.CreateCall(CF.CPISet, {Loc, Val});
  }

  //TODO Memory function Handle
  //
  
  for(unsigned i = 0, e = ReplMap.size();i != e; ++i) {
    Instruction *From = ReplMap[i].first, *To = ReplMap[i].second;
    To->takeName(From);
    From->replaceAllUsesWith(To);
  }
  
  return true;
}

bool CPI::runOnModule(Module &M) {
  DL = &M.getDataLayout();
  TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();
  createCPIFunctions(DL, M, CF);

/*   NamedMDNode *STBAA = M.getNamedMetadata("tbaa.structs"); //XXX tbaa.structs?
  for(size_t i = 0, e = STBAA->getNumOperands(); i != e; ++i) {
    MDNode *MD = STBAA->getOperand(i);
    MDNode *TBAATag = dyn_cast_or_null<MDNode>(MD->getOperand(1));
    if(TBAATag)
      StructsTBAA[cast<StructType>(mdconst::extract<ConstantInt>(MD->getOperand(0))->getType())] = TBAATag;
  }

  NamedMDNode *UTBAA = M.getNamedMetadata("clang.tbaa.unions");
  for(size_t i = 0, e = UTBAA->getNumOperands(); i != e; ++i) {
    MDNode *MD = STBAA->getOperand(i);
    MDNode *TBAATag = dyn_cast_or_null<MDNode>(MD->getOperand(1));
    if(TBAATag)
      UnionsTBAA[cast<StructType>(mdconst::extract<ConstantInt>(MD->getOperand(0))->getType())] = TBAATag;
  } */

  for(Module::iterator it = M.begin(); it != M.end(); ++it) {
    Function *F = &*it;
    CalledExternally[F] = externallyCalled(F);
  }

  for(Module::iterator it = M.begin(); it != M.end(); ++it) {
    Function &F = *it;
    if(!F.isDeclaration() && !F.getName().startswith("llvm.") &&
       !F.getName().startswith("__cpi_")) {
      AA = &getAnalysis<AAResultsWrapperPass>(F).getAAResults();
      runOnFunction(F);
    }
  }

  Function *F1 = createGlobalsReload(M, "__cpi_module.init");
  appendToGlobalCtors(M, F1, 0);

  Function *Main = M.getFunction("main");
  if(Main != NULL && !Main->isDeclaration()) {
    Function *F2 = createGlobalsReload(M, "__cpi_module.pre_main");
    F2->addFnAttr(Attribute::NoInline);
    CallInst::Create(F2, Twine(), cast<Instruction>(Main->getEntryBlock().getFirstNonPHI()));
  }
  return true;
}

