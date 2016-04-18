/***
 * Output.cpp: Output replacer to dispatcher
 *
 * Replace (standard) output functions to according dispatcher
 * if output function is used without including appropriate
 * header (such as 'putwc' w/o <wchar.h>), output function may be
 * enclosed by bitcast.
 * XXX THIS CASE IS ASSUMED TO BE NOT EXIST XXX
 * XXX ONLY SUPPORTS I386 BC XXX
 * written by: gwangmu
 *
 * **/

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"

#include "llvm/IR/CallSite.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ADT/SmallVector.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/UVA/Output.h"

#include <cstdlib>
#include <iostream>

using namespace std;
using namespace corelab;

char OutputReplServer::ID = 0;

static RegisterPass<OutputReplServer> XA("output-repl", 
	"replace output function to dispatcher", false, false);

void OutputReplServer::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
}

void OutputReplServer::setDispatchers (OutputReplServer::PlatformType platform) {
	// prepare types
	Type *tyVoid = Type::getVoidTy (*pC);
	Type *tyInt8Pt = Type::getInt8PtrTy (*pC);
	Type *tyInt32 = Type::getInt32Ty (*pC);
	Type *tyFile = getFileStruct (platform);
	Type *tyValist = NULL;

	switch (platform) {
		case I386:
			tyValist = Type::getInt8PtrTy (*pC);
			break;
		case ARM:
		case ANDROID:
			tyValist = ArrayType::get (tyInt32, 1);
			break;
		case X86_64:
			// FIXME: fill in right type!
			//tyValist = ???
			assert (0 && "unsupported, because the author is too lazy");
			break;
		default:
			assert (0 && "unsupported target platform");
			break;
	}

	vector<Type *> vecTy_I8pVa;
	vecTy_I8pVa.push_back (tyInt8Pt);

	vector<Type *> vecTy_I8pI8pVa;
	vecTy_I8pI8pVa.push_back (tyInt8Pt);
	vecTy_I8pI8pVa.push_back (tyInt8Pt);
	
	vector<Type *> vecTy_I8pI32I8pVa;
	vecTy_I8pI32I8pVa.push_back (tyInt8Pt);
	vecTy_I8pI32I8pVa.push_back (tyInt32);
	vecTy_I8pI32I8pVa.push_back (tyInt8Pt);

	vector<Type *> vecTy_I8pI32Va;
	vecTy_I8pI32Va.push_back (tyInt8Pt);
	vecTy_I8pI32Va.push_back (tyInt32);
	
	// initialize dispatchers
	mapDsp.insert (DispatcherMapElem ("putchar", 
		pM->getOrInsertFunction ("offload_putchar", tyInt32, tyInt32, NULL)));
	mapDsp.insert (DispatcherMapElem ("printf", 
		pM->getOrInsertFunction ("offload_printf", FunctionType::get(tyInt32, vecTy_I8pVa, true))));
	mapDsp.insert (DispatcherMapElem ("vprintf",
		pM->getOrInsertFunction ("offload_vprintf", tyInt32, tyInt8Pt, tyValist, NULL))); 
	mapDsp.insert (DispatcherMapElem ("puts",
		pM->getOrInsertFunction ("offload_puts", tyInt32, tyInt8Pt, NULL))); 
	mapDsp.insert (DispatcherMapElem ("write",
		pM->getOrInsertFunction ("offload_write", tyInt32, tyInt32, tyInt8Pt, tyInt32, NULL)));
	mapDsp.insert (DispatcherMapElem ("scanf",
		pM->getOrInsertFunction ("offload_scanf", FunctionType::get(tyInt32, vecTy_I8pVa, true))));
	mapDsp.insert (DispatcherMapElem ("getchar",
		pM->getOrInsertFunction ("offload_getchar", tyInt32, NULL)));
	mapDsp.insert (DispatcherMapElem ("remove",
		pM->getOrInsertFunction ("offload_remove", tyInt32, tyInt8Pt, NULL)));
	mapDsp.insert (DispatcherMapElem ("unlink",
		pM->getOrInsertFunction ("offload_unlink", tyInt32, tyInt8Pt, NULL)));
	mapDsp.insert (DispatcherMapElem ("getcwd",
		pM->getOrInsertFunction ("offload_getcwd", tyInt8Pt, tyInt8Pt, tyInt32, NULL)));
	mapDsp.insert (DispatcherMapElem ("system",
		pM->getOrInsertFunction ("offload_system", tyInt32, tyInt8Pt, NULL)));
	mapDsp.insert (DispatcherMapElem ("chdir",
		pM->getOrInsertFunction ("offload_chdir", tyInt32, tyInt8Pt, NULL)));
	mapDsp.insert (DispatcherMapElem ("rename",
		pM->getOrInsertFunction ("offload_rename", tyInt32, tyInt8Pt, tyInt8Pt, NULL)));
	mapDsp.insert (DispatcherMapElem ("open",
		pM->getOrInsertFunction ("offload_open", FunctionType::get(tyInt32, vecTy_I8pI32Va, true))));
	mapDsp.insert (DispatcherMapElem ("close",
		pM->getOrInsertFunction ("offload_close", tyInt32, tyInt32, NULL)));
	mapDsp.insert (DispatcherMapElem ("lseek",
		pM->getOrInsertFunction ("offload_lseek", tyInt32, tyInt32, tyInt32, tyInt32, NULL)));
	mapDsp.insert (DispatcherMapElem ("read",
		pM->getOrInsertFunction ("offload_read", tyInt32, tyInt32, tyInt8Pt, tyInt32, NULL)));

	mapDsp.insert (DispatcherMapElem ("offload_debugsig_handler",
		pM->getOrInsertFunction ("offload_debugsig", tyVoid, NULL))); 

	// if File IO is in use..
	if (tyFile != NULL) {
		// prepare types
		Type *tyFilePt = PointerType::get (tyFile, DEFAULT_ADDRESS_SPACE);

		vector<Type *> vecTy_FpI8pVa;
		vecTy_FpI8pVa.push_back (tyFilePt);
		vecTy_FpI8pVa.push_back (tyInt8Pt);

		// prepare shared dispatcher
		Constant *fnDspPutc = pM->getOrInsertFunction ("offload_putc", tyInt32, tyInt32, tyFilePt, NULL);

		// initialize dispatchers
		mapDsp.insert (DispatcherMapElem ("_IO_putc", fnDspPutc));
		mapDsp.insert (DispatcherMapElem ("putc", fnDspPutc));
		mapDsp.insert (DispatcherMapElem ("fputc", fnDspPutc));
		mapDsp.insert (DispatcherMapElem ("fwrite",
			pM->getOrInsertFunction ("offload_fwrite", tyInt32, tyInt8Pt, tyInt32, tyInt32, tyFilePt, NULL))); 
		mapDsp.insert (DispatcherMapElem ("fputs",
			pM->getOrInsertFunction ("offload_fputs", tyInt32, tyInt8Pt, tyFilePt, NULL))); 
		mapDsp.insert (DispatcherMapElem ("fprintf", 
			pM->getOrInsertFunction ("offload_fprintf", FunctionType::get(tyInt32, vecTy_FpI8pVa, true))));
		mapDsp.insert (DispatcherMapElem ("vfprintf",
			pM->getOrInsertFunction ("offload_vfprintf", tyInt32, tyFilePt, tyInt8Pt, tyValist, NULL))); 
		mapDsp.insert (DispatcherMapElem ("fflush",
			pM->getOrInsertFunction ("offload_fflush", tyInt32, tyFilePt, NULL))); 
	
		mapDsp.insert (DispatcherMapElem ("fopen",
			pM->getOrInsertFunction ("offload_fopen", tyFilePt, tyInt8Pt, tyInt8Pt, NULL)));
		mapDsp.insert (DispatcherMapElem ("fgets",
			pM->getOrInsertFunction ("offload_fgets", tyInt8Pt, tyInt8Pt, tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("fread",
			pM->getOrInsertFunction ("offload_fread", tyInt32, tyInt8Pt, tyInt32, tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("fclose",
			pM->getOrInsertFunction ("offload_fclose", tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("fseek",
			pM->getOrInsertFunction ("offload_fseek", tyInt32, tyFilePt, tyInt32, tyInt32, NULL)));
		mapDsp.insert (DispatcherMapElem ("fseeko",
			pM->getOrInsertFunction ("offload_fseeko", tyInt32, tyFilePt, tyInt32, tyInt32, NULL)));
		mapDsp.insert (DispatcherMapElem ("ftell",
			pM->getOrInsertFunction ("offload_ftell", tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("ftello",
			pM->getOrInsertFunction ("offload_ftello", tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("feof",
			pM->getOrInsertFunction ("offload_feof", tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("fgetc",
			pM->getOrInsertFunction ("offload_fgetc", tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("_IO_getc",
			pM->getOrInsertFunction ("offload_fgetc", tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("ungetc",
			pM->getOrInsertFunction ("offload_ungetc", tyInt32, tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("rewind",
			pM->getOrInsertFunction ("offload_rewind", tyVoid, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("__isoc99_fscanf",
			pM->getOrInsertFunction ("offload_fscanf", FunctionType::get(tyInt32, vecTy_FpI8pVa, true))));
		mapDsp.insert (DispatcherMapElem ("popen",
			pM->getOrInsertFunction ("offload_popen", tyFilePt, tyInt8Pt, tyInt8Pt, NULL)));
		mapDsp.insert (DispatcherMapElem ("pclose",
			pM->getOrInsertFunction ("offload_pclose", tyInt32, tyFilePt, NULL)));
		mapDsp.insert (DispatcherMapElem ("clearerr",
			pM->getOrInsertFunction ("offload_clearerr", tyVoid, tyFilePt, NULL)));
	}
	
	return;
}

bool OutputReplServer::runOnModule(Module& M) {
	// initialize
	this->pM = &M;
	this->pC = &getGlobalContext ();

	// figuring out target
	string strTarget = pM->getTargetTriple ();
	PlatformType platform;

	if (strTarget.find ("android") != string::npos)
		platform = ANDROID;
	else if (strTarget.length () > 3 && strTarget.substr (0, 3) == "arm")
		platform = ARM;
	else if (strTarget.length () > 4 && strTarget.substr (0, 4) == "i386")
		platform = I386;
	else if (strTarget.length () > 6 && strTarget.substr (0, 6) == "x86_64")
		platform = X86_64;
	else {
		assert (0 && "unsupported platform");
		platform = UNSUPPORTED;
	}

	// setting dispatchers
	setDispatchers (platform);

	// replacing process
	// XXX assume selector filtered functions where output return value is in use.
	for (Module::iterator ifn = M.begin(), ifn_end = M.end(); ifn != ifn_end; ++ifn) {

		// XXX for PRESENTATION
		if (ifn->getName().str() == string ("update_and_print_stage")) continue;

		for (Function::iterator ibb = ifn->begin(), ibb_end = ifn->end(); ibb != ibb_end; ++ibb) {
			for (BasicBlock::iterator iinst = ibb->begin(), iinst_end = ibb->end(); iinst != iinst_end; ++iinst) {

				if (CallInst *instCall = dyn_cast <CallInst> (&*iinst)) {
					Value *valCalled = instCall->getCalledValue ();

					//if (Function *fnCalled = dyn_cast <Function> (valCalled)) {
						string name = valCalled->getName().str();

						for (DispatcherMap::iterator it = mapDsp.begin (); it != mapDsp.end (); ++it) {
							if (it->first == name) {
								instCall->setCalledFunction (it->second);
								break;
							}
						}
					//}
				}

			}
		}
	}

	return false;
}

StructType* OutputReplServer::getFileStruct (OutputReplServer::PlatformType platform) {
	switch (platform) {
	case ARM:
	case I386:
	case X86_64:
		return pM->getTypeByName ("struct._IO_FILE");
	case ANDROID:
		return pM->getTypeByName ("struct.__sFILE");
	default:
		assert (0 && "unsupported platform");
	}

	return NULL;
#if 0
	TypeFinder tyFinder;
	
	tyFinder.run (*pM, true);

	for (TypeFinder::iterator it = tyFinder.begin ();	it != tyFinder.end (); ++it) {
		StructType *tyStruct = dyn_cast<StructType> (*it);

		if (tyStruct->getName().str() == "struct._IO_FILE")
			return tyStruct;
	}
	
	return NULL;
#endif
}

