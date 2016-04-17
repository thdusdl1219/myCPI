#ifndef ESP_UTILS_H
#define ESP_UTILS_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/IR/DataLayout.h"

namespace corelab{
	using namespace llvm;
	using namespace std;

	class EspUtils{
		public:
			static Value* insertCastingBefore(Value* from, Value* to, const DataLayout* dl, Instruction* before){
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
					cast->insertBefore(before);
					from = cast;
				} 

				// Next, make it have the same size
				if( fromSize < toSize ) {
					Type *integer = IntegerType::get(Context, toSize);
					Instruction *cast = new ZExtInst(from, integer);
					cast->insertBefore(before);
					from = cast;
				} else if ( fromSize > toSize ) {
					Type *integer = IntegerType::get(Context, toSize);
					Instruction *cast = new TruncInst(from, integer);
					cast->insertBefore(before);
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

					cast->insertBefore(before);
					from = cast;
				}

				return from;		
			}

	};
}

#endif
