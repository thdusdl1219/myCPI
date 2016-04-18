/***
 * RTTIClassInfo.h: RTTI Class Info Finder
 *
 * Provides RTTI class information
 * XXX RTTIClass := Struct having RTTI info XXX
 * written by: gwangmu
 *
 * **/

#ifndef LLVM_CORELAB_RTTI_CLASS_INFO_SERVER_X86_H
#define LLVM_CORELAB_RTTI_CLASS_INFO_SERVER_X86_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"

#include <map>
#include <vector>
#include <string>

namespace corelab
{
	using namespace llvm;
	using namespace std;
	
	class RTTIClass {
		friend class RTTIClassInfo;
		private:
			// IR infomation
			StructType *tyIRStruct;										/**< IR struct */
			vector< vector<Function *> > vecVtables;	/**< Virtual tables */
			vector<RTTIClass *> vecBase;							/**< Parent classes */
			string name;															/**< RTTIClass name */

			// Internal management
			map<RTTIClass *, int> mapBaseToVtidx;		 	/**< Maps base classes to Vtable index */
			vector<RTTIClass *> vecDerived;						/**< Derived classes */
			//vector<Type *> vecFieldTypes;						/**< Field types. Must be NULL if not data field. */

			RTTIClass (StructType *rcst);

		public:
			int getNumBases ();
			int getNumDerived ();
			//int getNumFields ();

			vector<Function *> getVtable (RTTIClass *base);
			RTTIClass *getNthBaseRTTIClass (int idx);
			RTTIClass *getNthDerivedRTTIClass (int idx);
			//Type *getNthFieldType (int idx);
	};

	class RTTIClassInfo : public ModulePass
	{
		public:
			static char ID;

			RTTIClassInfo() : ModulePass(ID) {}
			const char *getPassName() const { return "CLASS_INFO"; }

			virtual bool runOnModule(Module& M);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;

		private:
			typedef set<StructType *> StructSet;
			typedef set<RTTIClass *> RClassSet;
			typedef map<StructType *, RTTIClass *> StructToRClassMap;
			typedef pair<StructType *, RTTIClass *> StructRClassPair;

			Module *pM;
			LLVMContext *pC;

	`		StructSet setRCST;		/**< RTTI Class Struct Type(RCST)s */
			RClassSet setRClass;	/**< RTTI Class(RClass)s */
			StructToRClassMap mapRCSTToRClass;

			// Internals (Level 0)
			bool hasRTTI (StructType *tyStruct);
			GlobalVariable *getTVFromModule (StructType *rcst);
			GlobalVariable *getTIFromModule (StructType *rcst);
			bool isVtableDelimiter (Constant *entry);
			Function *getVFnFromVtableEntry (Constant *entry);
			StructType *getBaseRCSTFromTIEntry (Constant *entry);

			// Internals (Level 1)
			string convertRCSTNameToTIName (string rcstname);
			string convertRCSTNameToTVName (string rcstname);
			string convertTINameToRCSTClassName (string tiname);
			string convertTINameToRCSTStructName (string tiname);

			// Internals (Level 2)
			string convertClassNameToRTTIName (string classname);
			string convertRTTINameToClassName (string rttiname);
	};
}
#endif
