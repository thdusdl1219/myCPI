#include "llvm/IR/LLVMContext.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstIterator.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/ObjTrace/ObjTrace.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <inttypes.h>

using namespace std;
using namespace corelab;

char ObjTrace::ID = 0;
static RegisterPass<ObjTrace> X("objtrace", "Object memory allocation tracing", false, false);

// Utils
static bool isUseOfGetElementPtrInst(LoadInst *ld);
static Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl);
static Function *getCalledFunction_aux(Instruction* indCall); // From AliasAnalysis/IndirectCallAnal.cpp
static const Value *getCalledValueOfIndCall(const Instruction* indCall);


void ObjTrace::setFunctions(Module &M) {
  LLVMContext &Context = getGlobalContext();

  objTraceInitialize = M.getOrInsertFunction(
      "objTraceInitialize",
      Type::getVoidTy(Context),
      (Type*)0);

  objTraceFinalize = M.getOrInsertFunction(
      "objTraceFinalize",
      Type::getVoidTy(Context),
      (Type*)0);

  objTraceLoadInstr = M.getOrInsertFunction(
      "objTraceLoadInstr",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt64Ty(Context), /* Instr ID */
      (Type*)0);

  objTraceStoreInstr = M.getOrInsertFunction(
      "objTraceStoreInstr",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt64Ty(Context), /* Instr ID */
      (Type*)0);

  objTraceMalloc = M.getOrInsertFunction(
      "objTraceMalloc",
      Type::getInt8PtrTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* allocation size */
      Type::getInt64Ty(Context), /* Instr ID */
      (Type*)0);

  objTraceCalloc = M.getOrInsertFunction(
      "objTraceCalloc",
      Type::getInt8PtrTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Num */
      Type::getInt64Ty(Context), /* allocation size */
      Type::getInt64Ty(Context), /* Instr ID */
      (Type*)0);

  objTraceRealloc = M.getOrInsertFunction(
      "objTraceRealloc",
      Type::getInt8PtrTy(Context), /* Return type */
      Type::getInt8PtrTy(Context), /* originally allocated address */
      Type::getInt64Ty(Context), /* allocation size */
      Type::getInt64Ty(Context), /* Instr ID */
      (Type*)0);

  objTraceFree = M.getOrInsertFunction(
      "objTraceFree",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt8PtrTy(Context), /* address to free */
      Type::getInt64Ty(Context), /* Instr ID */
      (Type*)0);
}

void ObjTrace::setIniFini(Module& M) {
  LLVMContext &Context = getGlobalContext();
  //LoadNamer &loadNamer = getAnalysis< LoadNamer >();
  //uint64_t maxLoopDepth = (uint64_t)getAnalysis< LoopTraverse >().getMaxLoopDepth();
  std::vector<Type*> formals(0);
  std::vector<Value*> actuals(0);
  FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);

  /* initialize */
  Function *initForCtr = Function::Create(
      voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M);
  BasicBlock *entry = BasicBlock::Create(Context,"entry", initForCtr);
  BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr);
  actuals.resize(0);
  CallInst::Create(objTraceInitialize, actuals, "", entry);
  BranchInst::Create(initBB, entry);
  ReturnInst::Create(Context, 0, initBB);
  callBeforeMain(initForCtr);

  /* finalize */
  Function *finiForDtr = Function::Create(
      voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
  BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
  BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
  actuals.resize(0);
  CallInst::Create(objTraceFinalize, actuals, "", fini);
  BranchInst::Create(fini, finiBB);
  ReturnInst::Create(Context, 0, fini);
  callAfterMain(finiForDtr);
}

bool ObjTrace::runOnModule(Module& M) {
  setFunctions(M);
  //pLoadNamer = &getAnalysis< LoadNamer >();
  //TargetLibraryInfoWrapperPass *pTLI = &getAnalysis< TargetLibraryInfoWrapperPass >();
  //LoadNamer &loadNamer = *pLoadNamer;
  module = &M;

  DEBUG(errs()<<"############## runOnModule [ObjTrace] START ##############\n");

  for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function &F = *fi;
    LLVMContext &Context = getGlobalContext();
    const DataLayout &dataLayout = module->getDataLayout();
    std::vector<Value*> args(0);
    if (F.isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
      Instruction *instruction = &*I;
      // For each load instructions
      if(LoadInst *ld = dyn_cast<LoadInst>(instruction)) {
        if(isUseOfGetElementPtrInst(ld) == false){
          args.resize (2);
          Value *addr = ld->getPointerOperand();
          Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
          InstInsertPt out = InstInsertPt::Before(ld);
          addr = castTo(addr, temp, out, &dataLayout);

          //InstrID instrId = Namer::getInstrId(instruction);
          //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
          FullID fullId = Namer::getFullId(instruction);
          Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
          //for debug
          //errs()<<"<"<<instrId<<"> "<<*ld<<"\n";

          DEBUG(errs()<< "load instruction id %" << fullId << "\n");

          args[0] = addr;
          args[1] = fullId_;
          CallInst::Create(objTraceLoadInstr, args, "", ld);
        }
      }
      // For each store instructions
      else if (StoreInst *st = dyn_cast<StoreInst>(instruction)) {
        args.resize (2);
        Value *addr = st->getPointerOperand();
        Value *temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
        InstInsertPt out = InstInsertPt::Before(st);
        addr = castTo(addr, temp, out, &dataLayout);

        //InstrID instrId = Namer::getInstrId(instruction);
        //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
        FullID fullId = Namer::getFullId(instruction);
        Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);

        DEBUG(errs()<< "store instruction id %" << fullId << "\n");

        args[0] = addr;
        args[1] = fullId_;
        CallInst::Create(objTraceStoreInstr, args, "", st);
      }
    }
  }

  hookMallocFree();

  DEBUG(errs()<<"############## runOnModule [ObjTrace] END ##############\n");
  setIniFini(M);
  return false;
}

void ObjTrace::hookMallocFree(){
  LLVMContext &Context = getGlobalContext();
  //const DataLayout &dataLayout = module->getDataLayout();
  std::list<Instruction*> listOfInstsToBeErased;
  for(Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
    Function &F = *fi;
    std::vector<Value*> args(0);
    if (F.isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
      Instruction *instruction = &*I;
      bool wasBitCasted = false;
      Type *ty;
      IRBuilder<> Builder(instruction);
      if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){
        Function *callee = getCalledFunction_aux(instruction);
        if(!callee){
          const Value *calledVal = getCalledValueOfIndCall(instruction);
          if(const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())){
            wasBitCasted = true;
            ty = calledVal->getType();
            callee = const_cast<Function *>(tarFun);
          }
        }
        if(!callee) continue;
        if(callee->isDeclaration() == false) continue;


        if(CallInst *callInst = dyn_cast<CallInst>(instruction)){
          if(callee->getName() == "malloc"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            DEBUG(errs()<< "Malloc\n");
            args.resize(2);
            args[0] = instruction->getOperand(0);
            args[1] = fullId_;
            if(wasBitCasted){
              Value * changeTo = Builder.CreateBitCast(objTraceMalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceMalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "calloc"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            DEBUG(errs()<< "Calloc\n");
            args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = fullId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(objTraceCalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceCalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "realloc"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            DEBUG(errs()<< "Realloc\n");
            args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = fullId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(objTraceRealloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceRealloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "free"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            DEBUG(errs()<< "Free\n");
            args.resize(2);
            args[0] = instruction->getOperand(0);
            args[1] = fullId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(objTraceFree, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceFree, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
        }
        else if(InvokeInst *callInst = dyn_cast<InvokeInst>(instruction)){
          if(callee->getName() == "malloc"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            args.resize(2);
            args[0] = instruction->getOperand(0);
            args[1] = fullId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(objTraceMalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceMalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "calloc"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = fullId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(objTraceCalloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceCalloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "realloc"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            args.resize(3);
            args[0] = instruction->getOperand(0);
            args[1] = instruction->getOperand(1);
            args[2] = fullId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(objTraceRealloc, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceRealloc, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
          else if(callee->getName() == "free"){
            //InstrID instrId = Namer::getInstrId(instruction);
            uint64_t fullId = Namer::getFullId(instruction);
            //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
            Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
            args.resize(2);
            args[0] = instruction->getOperand(0);
            args[1] = fullId_;
            if(wasBitCasted){
              Value *changeTo = Builder.CreateBitCast(objTraceFree, ty);
              CallInst *newCallInst = Builder.CreateCall(changeTo, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            } else {
              CallInst *newCallInst = Builder.CreateCall(objTraceFree, args, "");
              makeMetadata(newCallInst, fullId);
              instruction->replaceAllUsesWith(newCallInst);
              listOfInstsToBeErased.push_back(instruction);
            }
          }
        }
        else
          assert(0&&"ERROR!!");
      }// closing bracket for "if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){"
    } // for bb
  } // for ff
  for(auto I: listOfInstsToBeErased) {
    I->eraseFromParent();
  }
}

// From Metadata/Namer
void ObjTrace::makeMetadata(Instruction* instruction, uint64_t Id) {
  LLVMContext &context = getGlobalContext();
  //XXX: Is it okay to cast Value* to Metadata* directly?
  Constant* IdV = ConstantInt::get(Type::getInt64Ty(context), Id);
  Metadata* IdM = (Metadata*)ConstantAsMetadata::get(IdV);
  Metadata* valuesArray[] = {IdM};
  ArrayRef<Metadata *> values(valuesArray, 1);
  MDNode* mdNode = MDNode::get(context, values);
  NamedMDNode *namedMDNode = module->getOrInsertNamedMetadata("corelab.namer");
  namedMDNode->addOperand(mdNode);
  instruction->setMetadata("namer", mdNode);
  return;
}

//Utility
static bool isUseOfGetElementPtrInst(LoadInst *ld){
  // is only Used by GetElementPtrInst ?
  return std::all_of(ld->user_begin(), ld->user_end(), [](User *user){return isa<GetElementPtrInst>(user);});
}

static Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl)
{
  LLVMContext &Context = getGlobalContext();
  const size_t fromSize = dl->getTypeSizeInBits( from->getType() );
  const size_t toSize = dl->getTypeSizeInBits( to->getType() );

  // First make it an integer
  if( ! from->getType()->isIntegerTy() ) {
    // cast to integer of same size of bits
    Type *integer = IntegerType::get(Context, fromSize);
    Instruction *cast;
    if( from->getType()->getTypeID() == Type::PointerTyID )
      cast = new PtrToIntInst(from, integer);
    else {
      cast = new BitCastInst(from, integer);
    }
    out << cast;
    from = cast;
  }

  // Next, make it have the same size
  if( fromSize < toSize ) {
    Type *integer = IntegerType::get(Context, toSize);
    Instruction *cast = new ZExtInst(from, integer);
    out << cast;
    from = cast;
  } else if ( fromSize > toSize ) {
    Type *integer = IntegerType::get(Context, toSize);
    Instruction *cast = new TruncInst(from, integer);
    out << cast;
    from = cast;
  }

  // possibly bitcast it to the approriate type
  if( to->getType() != from->getType() ) {
    Instruction *cast;
    if( to->getType()->getTypeID() == Type::PointerTyID )
      cast = new IntToPtrInst(from, to->getType() );
    else {
      cast = new BitCastInst(from, to->getType() );
    }

    out << cast;
    from = cast;
  }

  return from;
}

//TODO:: where to put this useful function?
//BONGJUN:: From CAMP/ContextTreeBuilder.cpp
static const Value *getCalledValueOfIndCall(const Instruction* indCall){
  if(const CallInst *callInst = dyn_cast<CallInst>(indCall)){
    return callInst->getCalledValue();
  }
  else if(const InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
    return invokeInst->getCalledValue();
  }
  else
    assert(0 && "WTF??");
}

//TODO:: where to put this useful function?
//BONGJUN:: From AliasAnalysis/IndirectCallAnal.cpp
static Function *getCalledFunction_aux(Instruction* indCall){
  if(CallInst *callInst = dyn_cast<CallInst>(indCall)){
    return callInst->getCalledFunction();
  }
  else if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
    return invokeInst->getCalledFunction();
  }
  else
    assert(0 && "WTF??");
}
