/***
 * FixedGlobal.cpp : Global to address-fixed global
 *
 * Fix internal global variables' addresses.
 * XXX UNSTABLE TO USE LIBRARIES! Use it only for executable IRs. XXX
 * written by : gwangmu
 *
 * **/

#include "llvm/IR/Value.h"
#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/ADT/StringMap.h"

#include "corelab/UVA/FixedGlobalVariable.h"
#include "corelab/UVA/FixedGlobal.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Utilities/InstInsertPt.h"

#include <set>
#include <list>
#include <cstdio>
#include <string>
#include <unistd.h>

#define FIXED_GLOBAL_BASE 			((void *)0x15000000)
#define FIXED_CONST_GLOBAL_BASE ((void *)0x16000000)

//#define DEBUG_FIXGLB

using namespace llvm;
using namespace std;

namespace corelab {
	static RegisterPass<FixedGlobal> X("fix-globals", "Fix global variables' addresses.", false, false);

	char FixedGlobal::ID = 0;

	void FixedGlobal::getAnalysisUsage (AnalysisUsage &AU) const {
		AU.setPreservesAll ();
	}

  // command line argument
  static cl::opt<string> FixGlbDuty("fix_global_duty", 
      cl::desc("Specify Global variable fixing and initializer (1: initializer, 0: not)"), 
      cl::value_desc("global initializer in charge"));

  void FixedGlobal::findGV(Module &M, char *gvar_str, vector<GlobalVariable*> &vecGvars) {
    for (Module::global_iterator igvar = M.global_begin ();
        igvar != M.global_end (); ++igvar) {
      GlobalVariable *gvar = &*igvar;

      // FIXME assume external, if it doesn't have an initializer.
      if ((!gvar->hasExternalLinkage () || gvar->hasInitializer ()) &&
          (gvar->getName().str().length () < 5 ||
           gvar->getName().str().substr (0, 5) != string ("llvm."))) {
        if (!gvar->isConstant ()) {
          //vecGvars.insert (gvar);
          if (strcmp(gvar_str, gvar->getName().data()) == 0) {
#ifdef DEBUG_FIXGLB
            printf("found! %s\n", gvar->getName().data());
#endif
            vecGvars.push_back(gvar);
            break;
          }
#ifdef DEBUG_FIXGLB
          printf("FIXGLB: runOnModule: no const (has init func): %s\n", gvar->getName().data());
#endif
        } else {
          // WORKAROUND: If the global initializer takes functions,
          // it breaks the rule where function pointers of the server
          // must have a function's address of the client, ending
          // up with a devestating result.

          // It would be more ideal to fix the function translator pass,
          // but now, we just add a small workaround code here,
          // so that the initializer can be effectivly overlapped 
          // to the client's one at runtime.
          if (hasFunction (gvar->getInitializer ())) {
            gvar->setConstant (false);
            if (strcmp(gvar_str, gvar->getName().data()) == 0) {
#ifdef DEBUG_FIXGLB
              printf("found! %s\n", gvar->getName().data());
#endif
              vecGvars.push_back(gvar);
              break;
            }
#ifdef DEBUG_FIXGLB
            printf("FIXGLB: runOnModule: const (has init func): %s\n", gvar->getName().data());
#endif
          }	
          else {
            //setConstGvars.insert (gvar);
#ifdef DEBUG_FIXGLB
            printf("FIXGLB: runOnModule: const (NO init func): %s\n",gvar->getName().data());
#endif
          }
        }
      }
    }
  }

	bool FixedGlobal::runOnModule (Module& M) {
		this->pM = &M;
    
    LLVMContext &Context = M.getContext(); 
    if(M.getFunction("__constructor__") == NULL) { // UVA-only module
#ifdef DEBUG_FIXGLB
      printf("FixedGlobal::runOnModule: ctor do not exist (may be UVA-only module)\n");
#endif
      std::vector<Type*> formals(0);
      std::vector<Value*> actuals(0);

      FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false); 

      Function *initForCtr = Function::Create(
          voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M);
      BasicBlock *entry = BasicBlock::Create(Context, "entry", initForCtr);
      BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr); 

      //CallInst::Create(fnGInitzer, actuals, "", entry); 
      BranchInst::Create(initBB, entry); 
      ReturnInst::Create(Context, 0, initBB);

      callBeforeMain(initForCtr, 0);

      /* finalize */
      Function *finiForDtr = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
      BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
      BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
      actuals.resize(0);
      BranchInst::Create(fini, finiBB);
      ReturnInst::Create(Context, 0, fini);
      callAfterMain(finiForDtr);
    }
    //makeForefrontInGlobalCtorOrder (M, "__constructor__");
    /* FixGlbDuty : it decides who fix global variables and init.  Global
     * variables are shared in multiple clients, thereby we need just only
     * client in charge of fixing and initializing glbs.  Therefore,
     * fix_global_duty value should be 1 only once.  
     **/
    if (FixGlbDuty == "1") 
      this->isFixGlbDuty = true;
    else
      this->isFixGlbDuty = false;

		/* Find all internal global variables. */
		vector<GlobalVariable *> vecGvars;
		//set<GlobalVariable *> setConstGvars;
    
    bool isFileExist = false;
    if(access( "gv_list.dat", F_OK ) != -1) { // gv_list.dat already exist!
      isFileExist = true;
      FILE *fp = fopen("gv_list.dat", "r");
      assert(fp && "gv_list.dat should be exist !");
      char *line = NULL;
      size_t len = 0;
      ssize_t read;

      while ((read = getline(&line, &len, fp)) != -1) {
#ifdef DEBUG_FIXGLB
        //printf("Retrieved line of length %zu :\n", read);
        printf("%s", line);
#endif
        strtok(line,"\n");
        findGV(M, line, vecGvars);
      }
      //while (fscanf (fp, "%s\n", buf) != EOF) {
      //  vecGvarsTmp.push_back(buf);
      //  buf = (char*)realloc(buf, sizeof(char)*1024);
      //}
      //free(line);
      fclose (fp); 
    } else {
      FILE *fp = fopen("gv_list.dat", "w");

      for (Module::global_iterator igvar = M.global_begin ();
          igvar != M.global_end (); ++igvar) {
        GlobalVariable *gvar = &*igvar;

        // FIXME assume external, if it doesn't have an initializer.
        if ((!gvar->hasExternalLinkage () || gvar->hasInitializer ()) &&
            (gvar->getName().str().length () < 5 ||
             gvar->getName().str().substr (0, 5) != string ("llvm."))) {
          if (!gvar->isConstant ()) {
            vecGvars.push_back (gvar);
#ifdef DEBUG_FIXGLB
            printf("$$$$$$$$$$$$$$$$$$ %s\n", gvar->getName().data());
#endif
            fprintf(fp, "%s\n", gvar->getName().data());
#ifdef DEBUG_FIXGLB
            printf("FIXGLB: runOnModule: no const (has init func): %s\n", gvar->getName().data());
#endif
          } else {
            // WORKAROUND: If the global initializer takes functions,
            // it breaks the rule where function pointers of the server
            // must have a function's address of the client, ending
            // up with a devestating result.

            // It would be more ideal to fix the function translator pass,
            // but now, we just add a small workaround code here,
            // so that the initializer can be effectivly overlapped 
            // to the client's one at runtime.
            if (hasFunction (gvar->getInitializer ())) {
              gvar->setConstant (false);
              vecGvars.push_back (gvar);
#ifdef DEBUG_FIXGLB
            printf("$$$$$$$$$$$$$$$$$$ %s\n", gvar->getName().data());
#endif
              fprintf(fp, "%s\n", gvar->getName().data());
#ifdef DEBUG_FIXGLB
              printf("FIXGLB: runOnModule: const (has init func): %s\n", gvar->getName().data());
#endif
            }	
            else {
              //setConstGvars.insert (gvar);
#ifdef DEBUG_FIXGLB
              printf("FIXGLB: runOnModule: const (NO init func): %s\n",gvar->getName().data());
#endif
            }
          }
        }
      }
    }

#ifdef DEBUG_FIXGLB
    printf("FIXGLB: vecGvars:\n");
    for (auto SI : vecGvars) {
      printf("%s\n", SI->getName().data());
    }
#endif
		/* Convert non-constant globals */
		size_t sizeGvars = convertToFixedGlobals (vecGvars, FIXED_GLOBAL_BASE);
#ifdef DEBUG_FIXGLB
    printf("FIXGLB: sizeGvars is %ld\n", sizeGvars);
#endif

		/* Convert constant globals */

    /* XXX XXX XXX XXX XXX XXX XXX 
     *
     * I decided that we don't fix "constant" globals. Only "No constant"
     *
     * by bongjun
     *
     * XXX XXX XXX XXX XXX XXX XXX
    **/
		//size_t sizeConstGvars = convertToFixedGlobals (setConstGvars, FIXED_CONST_GLOBAL_BASE);
    //size_t sizeConstGvars = 0;
		//uintptr_t uptConstGvarsBegin = (uintptr_t)FIXED_CONST_GLOBAL_BASE;
		//uintptr_t uptConstGvarsEnd = (uintptr_t)FIXED_CONST_GLOBAL_BASE + sizeConstGvars;
	
#ifdef DEBUG_FIXGLB
    printf("FIXGLB: runOnModule: FIXED_GLOBAL_BASE~bound (%p ~ %p)\n", (void*)FIXED_GLOBAL_BASE, (void*)((uintptr_t)FIXED_GLOBAL_BASE + sizeGvars));
#endif 
		/* Set constant range */
		FunctionType *tyFnVoidVoid = FunctionType::get (
				Type::getVoidTy (pM->getContext ()), false);
		Function *fnDeclCRange = Function::Create (tyFnVoidVoid, GlobalValue::InternalLinkage, 
				"__decl_const_global_range__", pM);
		BasicBlock *blkDeclCRange = BasicBlock::Create (pM->getContext (), "initzer", fnDeclCRange);
		/*Function *fnSendInitCompleteSignal = Function::Create (tyFnVoidVoid, GlobalValue::InternalLinkage, 
				"__send_init_complete_signal__", pM);
		BasicBlock *blkSendInitCompleteSignal = BasicBlock::Create (pM->getContext (), "sendsignal", fnSendInitCompleteSignal);
		*/
		Type *tyVoid = Type::getVoidTy (M.getContext ());
		Type *tyInt8Pt = Type::getInt8PtrTy (M.getContext ());
		Type *tyUintPtr = Type::getIntNTy (M.getContext (),
				M.getDataLayout ().getPointerSizeInBits ());

		Constant *cnstOffSetCRange = M.getOrInsertFunction ("UVAUtilSetConstantRange",
				tyVoid, tyInt8Pt, tyInt8Pt/*, tyInt8Pt, tyInt8Pt*/, NULL);
    Constant *uvaSync = M.getOrInsertFunction ("uva_sync", tyVoid, NULL);
    Constant *cnstSendInitCompleteSignal = M.getOrInsertFunction ("sendInitCompleteSignal", tyVoid, NULL);

		vector<Value *> vecSetCRangeArgs;
		vecSetCRangeArgs.push_back (ConstantExpr::getCast (Instruction::IntToPtr,
					ConstantInt::get (tyUintPtr, (uintptr_t)FIXED_GLOBAL_BASE), tyInt8Pt));
		vecSetCRangeArgs.push_back (ConstantExpr::getCast (Instruction::IntToPtr,
					ConstantInt::get (tyUintPtr, (uintptr_t)FIXED_GLOBAL_BASE + sizeGvars), tyInt8Pt));
		//vecSetCRangeArgs.push_back (ConstantExpr::getCast (Instruction::IntToPtr,
		//			ConstantInt::get (tyUintPtr, uptConstGvarsBegin), tyInt8Pt));
		//vecSetCRangeArgs.push_back (ConstantExpr::getCast (Instruction::IntToPtr,
		//			ConstantInt::get (tyUintPtr, uptConstGvarsEnd), tyInt8Pt));
		CallInst::Create (cnstOffSetCRange, vecSetCRangeArgs, "", blkDeclCRange);

		ReturnInst::Create (M.getContext (), blkDeclCRange);

    if (this->isFixGlbDuty) {
      //callBeforeMain (fnDeclCRange, 0);

      Function *ctor = M.getFunction("__constructor__"); 
      if (ctor != NULL) {
        std::vector<Value*> actuals(0);
        Instruction *EspInitCallInst;
        Instruction *fnGInitzerCallInst;
        InstInsertPt out;
        InstInsertPt out2;
        bool isExistEarlierCallInst;
        for(inst_iterator I = inst_begin(ctor); I != inst_end(ctor); I++) {
          if(isa<CallInst>(&*I)) {
            isExistEarlierCallInst = true;
            CallInst *tarFun = dyn_cast<CallInst>(&*I);
            Function *callee = tarFun->getCalledFunction();
            if(callee->getName() == "EspInit") { // Esperanto-aware
              EspInitCallInst = &*I;
              out = InstInsertPt::After(EspInitCallInst);
            } else if(callee->getName().find("__fixed_global_initializer__") != std::string::npos) {
#ifdef DEBUG_FIXGLB
              printf("fnGInitzer exists!\n");
#endif
              fnGInitzerCallInst = &*I;
              out = InstInsertPt::Before(fnGInitzerCallInst);
              out2 = InstInsertPt::After(fnGInitzerCallInst);
              break;
            }
          }
        }
        if (isExistEarlierCallInst) {
          out << CallInst::Create(fnDeclCRange, actuals, "");
          out2 << CallInst::Create (uvaSync, actuals, "");
          out2 << CallInst::Create(cnstSendInitCompleteSignal, actuals, "");
        } else {
          BasicBlock *bbOfCtor = &(ctor->front());
          CallInst *CI2 = CallInst::Create (uvaSync, actuals, "", bbOfCtor->getFirstNonPHI());
          CallInst *CI = CallInst::Create(fnDeclCRange, actuals, "", bbOfCtor->getFirstNonPHI());
          CallInst::Create(cnstSendInitCompleteSignal, actuals, "", CI);
        }
      }
    } else { // client who have no duty to initalize fixed global variables. But have to call fnDeclCRange.
      Function *ctor = M.getFunction("__constructor__");
      if (ctor != NULL) {
        std::vector<Value*> actuals(0);
        Instruction *EspInitCallInst;
        //Instruction *fnGInitzerCallInst;
        InstInsertPt out;
        bool isExistEarlierCallInst;
        for(inst_iterator I = inst_begin(ctor); I != inst_end(ctor); I++) {
          if(isa<CallInst>(&*I)) {
            isExistEarlierCallInst = true;
            CallInst *tarFun = dyn_cast<CallInst>(&*I);
            Function *callee = tarFun->getCalledFunction();
            if(callee->getName() == "EspInit") { // Esperanto-aware
              EspInitCallInst = &*I;
              out = InstInsertPt::After(EspInitCallInst);
              break;
            }
          }
        }
        if (isExistEarlierCallInst) {
          out << CallInst::Create(fnDeclCRange, actuals, "");
        } else {
          BasicBlock *bbOfCtor = &(ctor->front());
          CallInst::Create(fnDeclCRange, actuals, "", bbOfCtor->getFirstNonPHI());
        }
        
      }
    }

		/* Finalize */
		list<GlobalVariable *> lstDispGvars;
		lstDispGvars.insert (lstDispGvars.begin (), vecGvars.begin (), vecGvars.end ());
	  //lstDispGvars.insert (lstDispGvars.begin (), setConstGvars.begin (), setConstGvars.end ());

		while (!lstDispGvars.empty ()) {
			GlobalVariable *gvar = lstDispGvars.front ();
			lstDispGvars.pop_front ();
#ifdef DEBUG_FIXGLB
      gvar->dump();
#endif

			if (!gvar->user_empty ()) {
				lstDispGvars.push_back (gvar);
				continue;
			}

			gvar->eraseFromParent ();
		}

/*		while (!vecGvars.empty ()) {
			GlobalVariable *gvar = vecGvars.front ();
			vecGvars.pop_back ();
#ifdef DEBUG_FIXGLB
      gvar->dump();
#endif

			if (!gvar->user_empty ()) {
				vecGvars.push_back (gvar);
				continue;
			}

			gvar->eraseFromParent ();
		}
    */

		return false;
	}
  static void findAndInsertLoadInstForAllUsesRecursively (Value *V) {
    if (V->user_empty()) return;
    for (auto U : V->users()) {
      if (Instruction *I = dyn_cast<Instruction>(U)) {
        if(isa<LoadInst>(I) || isa<StoreInst>(I)) continue;
#ifdef DEBUG_FIXGLB
        I->dump();
#endif
        Value *LI = new LoadInst(V, "load.glb", I);
        for (unsigned int i = 0; i < I->getNumOperands(); i++) {
          if (I->getOperand(i)->getType() == LI->getType()) {
#ifdef DEBUG_FIXGLB
            printf("in user, this operand (%d) should be replaced\n", i);
            I->getOperand(i)->dump();
#endif
            I->setOperand(i, LI);
          }
        }
        //findAndInsertLoadInstForAllUsesRecursively(I);
      }
    }
  }
	size_t FixedGlobal::convertToFixedGlobals (vector<GlobalVariable *> vecGvars, void *base) {
		typedef map<GlobalVariable *, FixedGlobalVariable *> GlobalToFixedMap;
		typedef pair<GlobalVariable *, FixedGlobalVariable *> GlobalToFixedPair;
		size_t sizeTotalGvars = 0;

    //if (this->isFixGlbDuty)
		  FixedGlobalFactory::begin (pM, base, isFixGlbDuty);

		/* Replace them to fixed globals */
		GlobalToFixedMap mapGlobalToFixed;
		for (vector<GlobalVariable *>::iterator igvar = vecGvars.begin ();
				 igvar != vecGvars.end (); ++igvar) {
			GlobalVariable *gvar = *igvar;

			string gname = gvar->getName().str ();
			gvar->setName ("__disposed__");
			
			Constant *cnstInitzer = NULL;
			if (gvar->hasInitializer ())
				cnstInitzer = gvar->getInitializer ();

			FixedGlobalVariable *fgvar = FixedGlobalFactory::create (gvar->getType()->getElementType (),
					cnstInitzer, gname, isFixGlbDuty);
			mapGlobalToFixed.insert (GlobalToFixedPair (gvar, fgvar));
		}

		/* Correct references to globals */
		for (GlobalToFixedMap::iterator it = mapGlobalToFixed.begin ();
				 it != mapGlobalToFixed.end (); ++it) {
			GlobalVariable *gvar = it->first;
			FixedGlobalVariable *fgvar = it->second;
			
			gvar->replaceAllUsesWith (fgvar);
      //findAndInsertLoadInstForAllUsesRecursively(fgvar);
		}	

		sizeTotalGvars = FixedGlobalFactory::getTotalGlobalSize ();
#ifdef DEBUG_FIXGLB
    printf("FIXGLB: convertToFixedGlobals: sizeTotalGvars: %ld\n", sizeTotalGvars);
#endif
    if (this->isFixGlbDuty)
		  FixedGlobalFactory::end (isFixGlbDuty);

		return sizeTotalGvars;
	}

	bool FixedGlobal::hasFunction (Constant* cnst) {
		if (dyn_cast<Function> (cnst)) 	return true;

		for (User::op_iterator iop = cnst->op_begin ();
				 iop != cnst->op_end (); ++iop) {
			Constant *cnstOper = dyn_cast<Constant> (iop->get ());
			
			if (hasFunction (cnstOper))		return true;
		}

		return false;
	}
}
