#include <iostream>
#include <fstream>
#include <sstream>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"

#include "corelab/Esperanto/FcnPtrSelector.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  static RegisterPass<FcnPtrSelector> X("fcnptr-selector", "Function Pointer Selector Pass", false, false);
  char FcnPtrSelector::ID = 0;

  void FcnPtrSelector::getAnalysisUsage (AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }

  bool FcnPtrSelector::runOnModule (Module& M) {
    parseDriverFile();
    setFunctionPointer(M);
    DriverImplMapType::iterator di, dend;
    for(di = DriverImplMap.begin(), dend = DriverImplMap.end(); 
        di != dend; 
        di = DriverImplMap.upper_bound(di->first)){
      makeFcnPtrSelector(M, di->first); 
    }

    return true;
  }

  void FcnPtrSelector::parseDriverFile() {
    ifstream ifs("EspDriver.profile");
    assert(ifs.is_open() && "Error opening driver file");

    string line;    
    while(getline(ifs, line)) {
      istringstream iss(line);
      vector<string> tokens {istream_iterator<string>{iss},
                             istream_iterator<string>{}};
      
      string &PragmaType = tokens[0];
      string &DriverName = tokens[1];
      string &AbstractName = tokens[2];
      string &RealName = tokens[3];

      DriverKeyType key(DriverName, AbstractName);

      if(PragmaType == "EspDriverImpl"){
        // string &Condition = tokens[4];
        vector<string> Condition;
        parseCondition(Condition, tokens[4]);
        DriverImplInfo tempInfo(RealName, Condition);
        pair<DriverKeyType, DriverImplInfo> value(key, tempInfo);
        DriverImplMap.insert(value);
      }
    }
  }

  void FcnPtrSelector::parseCondition(vector<string> &emptyMap, string cond) {
   
    istringstream iss(cond);
    string token;
    int vectorSize = 0;

    while (getline(iss, token, '&')){
      istringstream subiss(token);
      string subtoken; 
    
      int argIndex = 0;
      string argString;

      while (getline(subiss, subtoken, '=')){
        size_t pos = subtoken.find("ARG");
        if(pos == string::npos) argString = subtoken;
        else {
          istringstream ss(subtoken.substr(pos+3));
          ss >> argIndex;
          if(vectorSize < argIndex) emptyMap.resize(argIndex); 
          argIndex--;
        }
      }

      emptyMap[argIndex] = argString;
    }
  }

  void FcnPtrSelector::setFunctionPointer(Module &M) {
    DriverImplMapType::iterator ii;
    
    for(ii = DriverImplMap.begin(); ii != DriverImplMap.end(); ii++){
      DriverImplInfo &info = ii->second;
      for(Module::iterator mi = M.begin(); mi != M.end(); mi++) {
        if (mi->getName().find(info.rawname) != StringRef::npos) {
          info.ptr = &*mi;
          break;
        }
      } 
    }
  }

  void FcnPtrSelector::makeFcnPtrSelector(Module& M, DriverKeyType DeclInfo) {
    LLVMContext &Context = M.getContext();
    
    pair<DriverImplMapType::iterator, DriverImplMapType::iterator> range = DriverImplMap.equal_range(DeclInfo);
  
    int argSize = 0;

    // Find the maximal argument size
    DriverImplMapType::iterator di;
    for(di = range.first; di != range.second; di++) {
      int vectorSize = di->second.cond.size();
      if(argSize < vectorSize)
        argSize = vectorSize;
    }

    // Create Selector Function
    
    // C++ Function Name 
    // int nameSize = DeclInfo.first.size() + DeclInfo.second.size() + 9; 
    //string SelectorName = "_Z";
    //SelectorName += to_string(nameSize) + "get_";
    //SelectorName += DeclInfo.first+"_";
    //SelectorName += DeclInfo.second+"_ptrPKc";

    // C Function Name
    string SelectorName = "get_";
    SelectorName += DeclInfo.first+"_";
    SelectorName += DeclInfo.second+"_ptr";


    vector<Type*> formals;
    for(int i = 0; i < argSize; i++)
      formals.push_back(Type::getInt8PtrTy(Context));
    
    PointerType* retType = range.first->second.ptr->getFunctionType()->getPointerTo();

    FunctionType* SelectorSig = FunctionType::get(retType, formals, false); 
    Function *Selector = (Function*) M.getOrInsertFunction(SelectorName, SelectorSig);
 
    // Fill Basic Blocks
    BasicBlock *compareBB = BasicBlock::Create(Context, "compareBB", Selector);
    BasicBlock *exitBB = BasicBlock::Create(Context, "exitBB", Selector);

    Value* nullPointer = ConstantPointerNull::get(retType); 
    ReturnInst::Create(Context, nullPointer, exitBB);

    for(di = range.first; di != range.second; di++) {
      BasicBlock *trueBB;  
      BasicBlock *falseBB;
      
      if(next(di) == range.second)
        falseBB = exitBB;
      else
        falseBB = BasicBlock::Create(Context, "falseBB", Selector);
      
      // Body of compareBB
      vector<string> &cond = di->second.cond;
      int vectorIndex = 0;
      int vectorSize = cond.size();
      for(Function::arg_iterator ai = Selector->arg_begin(); ai != Selector->arg_end(); ai++) {
        if(vectorIndex < vectorSize && !cond[vectorIndex].empty()) {
          trueBB = BasicBlock::Create(Context, "trueBB", Selector);

          Value* intResult = EmitCallToStrcmp(M, &*ai, cond[vectorIndex], compareBB);
          Value* zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
          Value* boolResult = new ICmpInst(*compareBB, ICmpInst::Predicate::ICMP_EQ, zero, intResult);
          // Instruction* cmpResult = CastInst::CreateIntegerCast(strcmpResult, Type::getInt1Ty(Context), false, "", compareBB); 
          BranchInst::Create(trueBB, falseBB, boolResult, compareBB);
          compareBB = trueBB; 
        }
        vectorIndex++;
      }

      // Body of trueBB
      ReturnInst::Create(Context, di->second.ptr, trueBB);
      compareBB = falseBB;
    }
   
  }

  Value* FcnPtrSelector::EmitCallToStrcmp(Module &M, Value *Arg,
      string str, BasicBlock *end) {
    LLVMContext& Context = M.getContext();
    Constant* strcmpFn = M.getFunction("strcmp");

    if (!strcmpFn) {
      vector<Type*> formals(2);
      formals[0] = Type::getInt8PtrTy(Context);
      formals[1] = Type::getInt8PtrTy(Context);
      FunctionType *strcmpSig = FunctionType::get(
          Type::getInt32Ty(Context),
          formals,
          false);
      strcmpFn = M.getOrInsertFunction("strcmp", strcmpSig);
    }

    Constant *StrConstant = ConstantDataArray::getString(Context, str, true);
    GlobalVariable *GV = new GlobalVariable(M, StrConstant->getType(),
      true, GlobalValue::InternalLinkage, StrConstant, "", 0); 

    Value *zero = ConstantInt::get(Type::getInt32Ty(Context), 0); 
    Value *zeroArgs[] = { zero, zero };
    ArrayRef<Value *> argsRef(zeroArgs, zeroArgs + 2); 
    Value *v = GetElementPtrInst::CreateInBounds(GV, argsRef, "", end);

    vector<Value*> actuals(2);
    actuals[0] = Arg;
    actuals[1] = v;

    return CallInst::Create(strcmpFn, actuals, "", end);
  }
} 

