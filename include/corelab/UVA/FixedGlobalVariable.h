/***
 * FixedGlobalVariable.h : address-fixed global variable
 *
 * Simple IR hacking tool to create address-fixed global variables.
 * XXX FixedGlobalVariable cannot be exported outside the module. XXX
 * written by: gwangmu
 *
 * **/

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <map>
#include <inttypes.h>

#define PAGE_SIZE 4096

using namespace llvm;
using namespace std;

namespace corelab {
	// In a nutshell, A FixedGlobalVariable is nothing but a GlobalAlias.
	// Extending GlobalAlias could be a neater solution,
	// but unfortunately GlobalAlias has nothing but the only PRIVATE constructor,
	// so simply we cannot inherit the class.
	typedef GlobalAlias FixedGlobalVariable;

	namespace FixedGlobalFactory {
		// User may explicitly 'begins' and 'ends' the transformation
		// by calling the following interfaces.
		// They prepares and post-processes the IR, respectively.
		void begin (Module *module, void *base, bool isFixGlbDuty);
		void end (bool isFixGlbDuty);	
		
		// Creator method, as in factory pattern.
		FixedGlobalVariable *create (Type *type, Constant *initzer, const Twine &name, bool isFixGlbDuty);
		void erase (FixedGlobalVariable *fgvar);

		// Factory getter, setter, tester. 
		void* getGlobalBaseAddress ();
		size_t getTotalGlobalSize ();

		// Instance getter, setter, and tester.
		void setInitializer (FixedGlobalVariable *fgvar, Constant *cnst);
		Constant* getInitializer (FixedGlobalVariable *fgvar);
		bool hasInitiailzer (FixedGlobalVariable *fgvar);
		void* getFixedAddress (FixedGlobalVariable *fgvar);
	}
}
